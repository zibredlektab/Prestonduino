#include <Adafruit_ADS1X15.h>
#include <Scheduler.h>
#include "PrestonDuino.h"
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>

#define REPEAT_DELAY 10 // debounce for buttons
#define SETBUTTONPIN 9
#define ZEROBUTTONPIN 5
#define SOFTBUTTONPIN 6
#define MFPIN A0 // output from microforce
#define LEDPIN 10
#define MESSAGE_DELAY 6 // delay in sending messages to MDR, to not overwhelm it
#define DEADZONE 10 // any step size +- this value is ignored, to avoid drift
#define DEFAULTZERO 260 // value of "zero" point on zoom
#define DEFAULTSOFT 0x5000
#define MAXSOFT 0x7FFF

bool firstrun = true;
PrestonDuino *mdr;
int mfoutput = 0; // current velocity specified by MicroForce
uint16_t curzoom; // current zoom position
bool setpressed = false; // is the "Set" button depressed?
bool softpressed = false;
uint32_t timesetpressed; // when was the "Set" button depressed?
uint32_t timesoftpressed; // when was the "Soft" button depressed?
uint32_t timezeropressed; // when was the "Zero" button depressed?
uint32_t timelastsent = 0; // when was the last mdr message sent?
int count; // number of microforce samples taken in this averaging

bool limits = false; // is the zoom currently limited
uint16_t firstlimit; // initial position when set button is first pressed
uint16_t secondlimit; // ending position when set button is released
uint16_t widelimit = 0; // encoder position of wide zoom limit
uint16_t tightlimit = 0xFFFF; // encoder position of tight zoom limit
int zeropoint = DEFAULTZERO; // point on the microforce at which the lens should not move at all
int softlevel = DEFAULTSOFT; // amount of feathering to apply at limits, 0-2000

// Metadata follows

uint16_t widelimitmeta = 0; // metadata position (focal length in 0.01mm)
uint16_t tightlimitmeta = 0;
uint16_t firstlimitmeta = 0;
uint16_t secondlimitmeta = 0;
uint16_t lenswide = 0;
uint16_t lenstight = 0;
char fulllensname[50];
char lensbrand[50];
char* lensseries;
char* lensname;

int16_t adcval = 0;

Adafruit_SH1107 oled(64, 128, &Wire);
Adafruit_ADS1115 adc;


void setup() {
  oled.begin(0x3C, true);
  oled.setTextWrap(true);
  oled.setRotation(1);
  oled.clearDisplay();
  oled.setTextColor(SH110X_WHITE);
  oled.setCursor(0, 30);
  
  Serial.begin(9600);
  oled.print("Starting Serial...\n");
  oled.display();
  while(!Serial && millis() < 3000);
  Serial.println();
  oled.print("Serial started, or timed out.\n");
  oled.display();

  Serial.println("---Start---");

  mdr = new PrestonDuino(Serial1);
  delay(100);

  pinMode(SETBUTTONPIN, INPUT_PULLUP);
  pinMode(MFPIN, INPUT);
  pinMode(LEDPIN, OUTPUT);
  pinMode(ZEROBUTTONPIN, INPUT_PULLUP);
  pinMode(SOFTBUTTONPIN, INPUT_PULLUP);

  mdr->shutUp();
  mdr->mode(0x19, 0x4); // commanded motor positions, zoom position, streaming, controlling zoom
  mdr->data(0x14); // request only zoom position
  zeroOut();

  adc.begin();
  adc.startADCReading(ADS1X15_REG_CONFIG_MUX_SINGLE_1, true);

  timelastsent = millis();
  Scheduler.startLoop(metaLoop);
}

void zeroOut() {
  byte zoomdata[3] = {0x3, 0x7F, 0xFF}; // establish a known starting position, halfway through range
  curzoom = 0x7FFF;
  mdr->data(zoomdata, 3);
  zeropoint = analogRead(MFPIN);
}

void metaLoop() {
  if (strcmp(mdr->getLensName(), fulllensname) != 0) { // lens has changed
    Serial.println("Lens name has changed");
    strncpy(fulllensname, mdr->getLensName(), mdr->getLensNameLen());
    Serial.print("New lens name is ");
    Serial.println(fulllensname);
    Serial.print("Lens name is ");
    Serial.print(mdr->getLensNameLen());
    Serial.println(" chars long");
    if (strchr(fulllensname, '-')) { // this is a zoom

      for (int i = 0; i < 50; i++) {
        lensbrand[i] = 0;
      }

      strncpy(lensbrand, fulllensname, mdr->getLensNameLen());
      lensseries = strchr(lensbrand, '|') + 1; // find separator between brand and series
      lensseries[-1] = 0; // null terminator for brand
      
      lensname = strchr(lensseries, '|') + 1; // find separator between series and name
      lensname[-1] = 0; // null terminator for series

      sscanf(lensname, "%hu-%hu", &lenswide, &lenstight); // store the wide & tight focal lengths
      lenswide *= 100;
      lenstight *= 100;
      Serial.print("Lens range is ");
      Serial.print(lenswide/100);
      Serial.print(" to ");
      Serial.println(lenstight/100);
    } else { // this is a prime
      lenswide = mdr->getZoom() * 100;
      lenstight = mdr->getZoom() * 100;
    }

    resetLimits();
  }

  oled.clearDisplay();
  oled.setCursor(10,10);
  oled.print(mdr->getZoom()/100);
  oled.print(" mm");

  if (lenswide != lenstight) { // range is only displayed for zooms, not primes
    oled.setCursor(10, 20);
    oled.print(widelimitmeta/100);
    oled.print(" - ");
    oled.print(tightlimitmeta/100);
  }

  oled.setCursor(60, 10);
  oled.print("Soft: ");
  oled.print(map(softlevel, 0, 0x7FFF, 0, 100));
  oled.print("%");


  oled.setCursor(10, 30);
  oled.print(adcval);

  oled.setCursor(60,40);
  oled.print(millis());
  oled.display();
  
  //yield();
}

void loop() {
  mdr->onLoop();

  if (firstrun && timelastsent + 1000 > millis()) { // give the mdr a little time to get caught up at the beginning
    return;
  }

  firstrun = false;

  if (softpressed) { // adjusting soft stop value rather than actually zooming
    softlevel += getMFOutput() * .1;
    if (softlevel > MAXSOFT) softlevel = MAXSOFT;
    if (softlevel < 0) softlevel = 0;
    Serial.print("soft now ");
    Serial.println(softlevel);
  } else {

    if (count == 0 || timelastsent + MESSAGE_DELAY > millis()) { // average input from microforce until message delay period is reached
      mfoutput += getMFOutput();
      count++;
    } else {
      mfoutput /= count;
      count = 0;

      uint16_t newzoom = curzoom;
      
      //Serial.print("Current zoom position is 0x");
      //Serial.print(curzoom, HEX);

      int stepsize = mfoutput * 2; // -1024 - 1024, eventually this will be scaled dynamically for soft stops

      if (abs(stepsize) < DEADZONE) stepsize = 0;

      if (stepsize < 0) { // zooming out
        int distance = curzoom - widelimit; // find remaining distance in the move
        if (distance <= softlevel) { // distance is within the slowdown zone
          stepsize = map(distance, 0, softlevel, 0, stepsize); // scale the stepsize accordingly
        }
      } else if (stepsize > 0) { // zooming in
        int distance = tightlimit - curzoom;
        if (distance <= softlevel) {
          stepsize = map(distance, 0, softlevel, 0, stepsize);
        }
      }
      
      //Serial.print(", step size is ");
      //Serial.print(stepsize);

      if (mfoutput < 0) { // zooming out
        if (curzoom + stepsize >= widelimit) {
          newzoom = curzoom + stepsize;
        } else {
          newzoom = widelimit;
        }
      } else if (mfoutput > 0) { // zooming in
        if (curzoom + stepsize <= tightlimit) {
          newzoom = curzoom + stepsize;
        } else {
          newzoom = tightlimit;
        }
      }

      //Serial.print(", new zoom should be 0x");
      //Serial.println(newzoom, HEX);

      byte zoomdata[3] = {0x4, highByte(newzoom), lowByte(newzoom)};

      if (newzoom != curzoom) {
        mdr->data(zoomdata, 3); // send to mdr
        curzoom = newzoom;
      }

      timelastsent = millis();
      mfoutput = 0;
    }
  }

  // check if set button is pressed
  if (!digitalRead(SETBUTTONPIN)) { // set button is pressed
    if (!setpressed) {
      timesetpressed = millis();
      setpressed = true;
      if (!limits) {
        firstlimit = curzoom;
        firstlimitmeta = mdr->getZoom();
      }
    }
  } else if (timesetpressed + REPEAT_DELAY < millis()) { // set button is not pressed, and it's been long enough to debounce input!
    if (setpressed) { // set button *was* pressed on the last loop, so it was just released
      setpressed = false;
      if (!limits) { // zoom is not currently limited, so set new limits based on the current position
        secondlimit = curzoom;
        secondlimitmeta = mdr->getZoom();
        limits = true;

        // assign our temp limits as the zoom limits
        if (firstlimit < secondlimit) {
          widelimit = firstlimit;
          tightlimit = secondlimit;
          widelimitmeta = firstlimitmeta;
          tightlimitmeta = secondlimitmeta;
        } else {
          widelimit = secondlimit;
          tightlimit = firstlimit;
          widelimitmeta = secondlimitmeta;
          tightlimitmeta = firstlimitmeta;
        }
      } else { // zoom is currently limited, so release the limits
        resetLimits();
      }
    }
  }


  // check if soft button is pressed
  if (!digitalRead(SOFTBUTTONPIN)) { // soft button is pressed
    if (!softpressed) {
      timesoftpressed = millis();
      softpressed = true;
    }
  } else if (timesetpressed + REPEAT_DELAY < millis()) { // soft button is not pressed, and it's been long enough to debounce input!
    if (softpressed) { // soft button *was* pressed on the last loop, so it was just released
      softpressed = false;
      if (softlevel > MAXSOFT) softlevel = MAXSOFT;
      if (softlevel < 1) softlevel = 1;
      Serial.print("new soft level is ");
      Serial.print(softlevel);
    }
  }
  

  if (limits) {
    digitalWrite(LEDPIN, HIGH);
  } else {
    digitalWrite(LEDPIN, LOW);
  }

  if (!digitalRead(ZEROBUTTONPIN) && timezeropressed + REPEAT_DELAY < millis()) {
    timezeropressed = millis();
    zeroOut();
  }
  //yield();
}

void resetLimits() {
    limits = false;
    widelimit = 0;
    tightlimit = 0xFFFF;
    widelimitmeta = lenswide; // wide limit is now the wide end of the lens
    tightlimitmeta = lenstight;

    Serial.println("Limits reset");
}

int getMFOutput() {
  // zero = 5.02v
  // full speed in ~ 7.7v
  // full speed out ~ 2.0v
  // scale factor = 2.464
  int signal = analogRead(MFPIN); // 10-bit, so 0-1024
  //signal *= 2464; // scale up to mv...but do we actually care about the voltage?

  signal -= zeropoint; // need to find the actual zero point in the scale, as it isn't actually evenly split


  //Serial.print("Reading ADC...");

  adcval = adc.getLastConversionResults(); // adc.readADC_Differential_0_1();

  //Serial.print("ADC value is ");
  //Serial.println(adcval);

  return signal * -1;

}
