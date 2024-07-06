#include <FlashAsEEPROM.h>
#include <FlashStorage.h>
#include <Adafruit_ADS1X15.h>
#include <Scheduler.h>
#include <PrestonDuino.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>

#define REPEAT_DELAY 10 // debounce for buttons
#define SETBUTTONPIN 9
#define ZEROBUTTONPIN 5 // deprecated
#define SOFTBUTTONPIN 6
#define MFPIN A0 // output from microforce through voltage divider
#define LEDPIN A5 // "set" LED

#define OLED_DC 2
#define OLED_CS 5
#define OLED_RST 4

#define MESSAGE_DELAY 6 // delay in sending messages to MDR, to not overwhelm it
#define DEADZONE 3 // any step size +- this value is ignored, to avoid drift
#define DEFAULTZERO 679  // value of "zero" point on zoom
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
int zeropoint = 0; // point on the microforce at which the lens should not move at all
int softlevel = 0; // amount of feathering to apply at limits, 0-2000
bool soft = true; // softening/dampening/feathering is enabled

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

//Adafruit_SH1107 oled(64, 128, &Wire);
//Adafruit_ADS1115 adc;
Adafruit_SH1106G oled = Adafruit_SH1106G(128, 64, &SPI, OLED_DC, OLED_RST, OLED_CS);

FlashStorage(zeropoint_flash, int); // Reserve a portion of flash memory to store current zero point
FlashStorage(softlevel_flash, int); // and same for soft setting

void setup() {
  oled.begin(0x3C, true);
  oled.setTextWrap(true);
  oled.setRotation(0);
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
  //pinMode(ZEROBUTTONPIN, INPUT_PULLUP);
  pinMode(SOFTBUTTONPIN, INPUT_PULLUP);

  mdr->shutUp();
  mdr->mode(0x19, 0x4); // commanded motor positions, zoom position, streaming, controlling zoom
  mdr->data(0x14); // request only zoom position

  //adc.begin();
  //adc.startADCReading(ADS1X15_REG_CONFIG_MUX_DIFF_0_1, true);

  zeropoint = zeropoint_flash.read();
  if (!zeropoint) zeropoint = DEFAULTZERO;

  softlevel = softlevel_flash.read();
  if (!softlevel) softlevel = DEFAULTSOFT;

  zeroOut(!digitalRead(SETBUTTONPIN)); // zero everything out, and save the current stick value as a new zero point if the set button is pressed

  timelastsent = millis();
  Scheduler.startLoop(metaLoop);
  oled.clearDisplay();
  oled.display();
}

void zeroOut(bool newzero) {
  byte zoomdata[3] = {0x3, 0x7F, 0xFF}; // establish a known starting position, halfway through range
  curzoom = 0x7FFF;
  mdr->data(zoomdata, 3);
  if (newzero) {
    zeropoint = analogRead(MFPIN);//adc.getLastConversionResults(); // current stick position is the new zero point
    zeropoint_flash.write(zeropoint); // save this zero point for future use
  }
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
  oled.setCursor(0,10);
  oled.print(mdr->getZoom()/100);
  oled.print(" mm");

  if (lenswide != lenstight) { // range is only displayed for zooms, not primes
    oled.setCursor(0, 20);
    oled.print("Range: ");
    oled.print(widelimitmeta/100);
    oled.print(" - ");
    oled.print(tightlimitmeta/100);
  }

  oled.setCursor(60, 10);
  oled.print("Soft: ");
  if (soft) {
    oled.print(map(softlevel, 0, 0x7FFF, 0, 100));
    oled.print("%");
  } else {
    oled.print("off");
  }


  oled.setCursor(10, 30);
  oled.print("MF Value: ");
  oled.print(adcval);
  oled.setCursor(10, 40);
  oled.print("Velocity: ");
  oled.print(zeropoint - adcval);

  oled.setCursor(10,50);
  oled.print("Time: ");
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
    softlevel += getMFOutput() * 0.1;
    if (softlevel > MAXSOFT) softlevel = MAXSOFT;
    if (softlevel < 0) softlevel = 0;
    Serial.print("soft now ");
    Serial.println(softlevel);
  } else {

    if (count == 0 || timelastsent + MESSAGE_DELAY > millis()) { // average input from microforce until message delay period is reached
      int toadd = getMFOutput();
      mfoutput += toadd;
      Serial.print("count #");
      Serial.print(count);
      Serial.print(" at time ");
      Serial.print(millis());
      Serial.print(": ");
      Serial.print(toadd);
      Serial.print(" (Sending at ");
      Serial.print(timelastsent + MESSAGE_DELAY);
      Serial.println(")");
      count++;
    } else {
      mfoutput /= count;
      count = 0;

      uint16_t newzoom = curzoom;
      
      Serial.print("\nCurrent zoom position is 0x");
      Serial.print(curzoom, HEX);


      int stepsize = mfoutput; // get current output from microforce, this is the step size

      Serial.print(", step size is ");
      Serial.print(stepsize);

      if (abs(stepsize) < DEADZONE) {
        stepsize = 0; // extremely small steps are below the noise floor, so zero them out
      } else {
        bool zoomingout = stepsize < 0;

        // Scale stepsize exponentially (high step values should move motor further than low step values)
        stepsize = (1000 + (abs(stepsize) * abs(stepsize) * 9)) / 500;
        if (zoomingout) stepsize *= -1;

        Serial.print(", scaled step size is ");
        Serial.print(stepsize);

        if (soft) {
          // Dampen extremes of zoom range by scaling stepsize down as it approaches the end
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
          
          Serial.print(", softened step size is ");
          Serial.print(stepsize);
        }
      }

      // Determine new zoom position using stepsize
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

      Serial.print(", new zoom should be 0x");
      Serial.println(newzoom, HEX);

      byte zoomdata[3] = {0x4, highByte(newzoom), lowByte(newzoom)}; // build the mdr command

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
  } else if (timesoftpressed + REPEAT_DELAY < millis()) { // soft button is not pressed, and it's been long enough to debounce input!
    if (softpressed) { // soft button *was* pressed on the last loop, so it was just released
      softpressed = false;
      if (millis() - timesoftpressed < 1000) { // soft button was pressed & released, not held
        soft = !soft;
        Serial.print("softening is ");
        Serial.println(soft);
      } else {
        if (softlevel > MAXSOFT) softlevel = MAXSOFT;
        if (softlevel < 1) softlevel = 1;
        Serial.print("new soft level is ");
        Serial.println(softlevel);
        softlevel_flash.write(softlevel);
      }
    }
  }
  

  if (limits) {
    digitalWrite(LEDPIN, HIGH);
  } else {
    digitalWrite(LEDPIN, LOW);
  }

/*  if (!digitalRead(ZEROBUTTONPIN) && timezeropressed + REPEAT_DELAY < millis()) {
    timezeropressed = millis();
    zeroOut(true);
  }*/
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

  Serial.print("Reading ADC...");

  int signal = analogRead(MFPIN); // 10-bit, so 0-1024
  adcval = signal;


  Serial.print("ADC value is ");
  Serial.print(adcval);

  //adcval = adc.getLastConversionResults();
  //int signal = adcval;
  signal -= zeropoint; // Get a velocity from the raw sensor value

  Serial.print(" minus zero point of ");
  Serial.print(zeropoint);
  Serial.print(" = signal of ");
  Serial.println(signal * -1);

  return signal * -1; // Value is reversed from what we would expect, for some reason

}
