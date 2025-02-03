#include <Time.h>
#include <FlashAsEEPROM.h>
#include <FlashStorage.h>
#include <Adafruit_ADS1X15.h>
#include <Scheduler.h>
#include <PrestonDuino.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>

#include "Fonts/pixelmix4pt7b.h"
#include "Fonts/Roboto_Medium_26.h"
#include "Fonts/Roboto_34.h"
#define XLARGE_FONT &Roboto_34
#define LARGE_FONT &Roboto_Medium_26
#define SMALL_FONT &pixelmix4pt7b

#define REPEAT_DELAY 10 // debounce for buttons
#define SETBUTTONPIN SETBTN
#define SOFTBUTTONPIN SOFTBTN
#define MFPIN ZSIG // output from microforce through voltage divider
#define LEDPIN LED_BUILTIN // "set" LED

#ifndef OLED_DC
#define OLED_DC 2
#define OLED_CS 5
#define OLED_RST 4
#endif

#define MESSAGE_DELAY 6 // delay in sending messages to MDR, to not overwhelm it
#define DEADZONE 3 // any step size +- this value is ignored, to avoid drift
#define DEFAULTZERO 679  // value of "zero" point on zoom
#define DEFAULTSOFT 0x5000
#define MAXSOFT 0x7FFF

#define DATA_SETTING 0x14 //zoom position data (we don't care about focus or iris)

bool firstrun = true; // this is set to false after the first time the program is run
PrestonDuino *mdr; // object for communicating with the MDR
int mfoutput = 0; // current velocity specified by MicroForce
uint16_t curzoom; // current zoom position, as reported by MDR
bool setpressed = false; // is the "Set" button depressed?
bool softpressed = false;
uint32_t timesetpressed; // when was the "Set" button depressed?
uint32_t timesoftpressed; // when was the "Soft" button depressed?
uint32_t timelastsent = 0; // when was the last mdr message sent?
uint32_t timelastupdatedname = 0; // when was the lens name last updated?
int samplecount; // number of microforce samples taken in this averaging
int16_t adcval = 0; // current value of ADC



// Zoom limits

bool limits = false; // is the zoom currently limited?
uint16_t firstlimit; // initial position when set button is first pressed
uint16_t secondlimit; // ending position when set button is released
uint16_t widelimit = 0; // encoder position of wide zoom limit
uint16_t tightlimit = 0xFFFF; // encoder position of tight zoom limit
int zeropoint = 0; // point on the microforce at which the lens should not move at all (read from flash during setup)
int softlevel = 0; // amount of feathering to apply at limits, 0-2000 (read from flash during setup)
bool soft = true; // softening/dampening/feathering is enabled



// Metadata

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

//Adafruit_SH1107 oled(64, 128, &Wire);
//Adafruit_ADS1115 adc;
Adafruit_SH1106G oled = Adafruit_SH1106G(128, 64, &SPI, OLED_DC, OLED_RST, OLED_CS); // object for communicating with oled screen

FlashStorage(zeropoint_flash, int); // Reserve a portion of flash memory to store current zero point
FlashStorage(softlevel_flash, int); // and same for soft setting

void setup() {
  delay(500);

  while(!Serial && millis() < 3000); // give Serial time to start up

  Serial.println("---Start---");

  // OLED setup
  oled.begin(0x3C, true);
  oled.setTextWrap(true);
  oled.setRotation(0);
  oled.clearDisplay();
  oled.setTextColor(SH110X_WHITE);
  oled.setCursor(0, 10);
  
  // Beginning message
  Serial.begin(9600);
  oled.print("MFSerial Starting...\n\n");
  oled.print("Compiled ");
  oled.print(__DATE__);
  oled.print(" ");
  oled.println(__TIME__);
  oled.println("Connecting to MDR...");
  oled.display();

  mdr = new PrestonDuino(Serial1);
  while(!mdr->isMDRReady()) {
    delay(10);
    mdr->onLoop(); // gotta loop if you want to receive a message
  }
  Serial.print("Connected to ");
  Serial.println(mdr->getMDRType());

  oled.print("Connected to ");
  oled.print(mdr->getMDRType());
  oled.display();
  
  delay(1000);

  pinMode(SETBUTTONPIN, INPUT_PULLUP);
  pinMode(MFPIN, INPUT);
  pinMode(LEDPIN, OUTPUT);
  pinMode(SOFTBUTTONPIN, INPUT_PULLUP);

  mdr->shutUp();
  delay(MESSAGE_DELAY);
  mdr->onLoop();
  mdr->info(0x1); // Get starting lens name
  delay(MESSAGE_DELAY);
  mdr->onLoop();
  mdr->mode(0x19, 0x4); // Request "commanded" motor positions (not actual position), streaming zoom position (not velocity), and controlling zoom axis
  delay(MESSAGE_DELAY);
  mdr->onLoop();
  mdr->data(DATA_SETTING); // request only zoom position data (we don't care about focus or iris)
  delay(MESSAGE_DELAY);
  mdr->onLoop();

  zeropoint = zeropoint_flash.read(); // attempt to read zero point from flash
  if (!zeropoint) zeropoint = DEFAULTZERO;

  softlevel = softlevel_flash.read(); // attempt to read soft level from flash
  if (!softlevel) softlevel = DEFAULTSOFT;

  zeroOut(true);//!digitalRead(SETBUTTONPIN)); // zero everything out, and save the current stick value as a new zero point if the set button is pressed

  timelastsent = millis();
  Scheduler.startLoop(metaLoop); // multi threading begin!
  oled.clearDisplay();
  oled.display();
}

void zeroOut(bool newzero) {
  byte zoomdata[3] = {0x3, 0x7F, 0xFF}; // establish a known starting position, halfway through range
  curzoom = 0x7FFF;
  mdr->data(zoomdata, 3);
  delay(MESSAGE_DELAY);
  if (newzero) {
    zeropoint = analogRead(MFPIN); // current stick position is the new zero point
    zeropoint_flash.write(zeropoint); // save this zero point for future use
    digitalWrite(LEDPIN, HIGH);
    delay(250);
    digitalWrite(LEDPIN, LOW);
    delay(250);
    digitalWrite(LEDPIN, HIGH);
    delay(250);
    digitalWrite(LEDPIN, LOW);
  }
}

void metaLoop() {
  // metaLoop handles all metadata interactions, including getting the lens name & running the OLED screen

  if (strcmp(mdr->getLensName(), fulllensname) != 0) { // lens has changed
    Serial.println("Lens name has changed");
    strncpy(fulllensname, mdr->getLensName(), mdr->getLensNameLen()); // store the new lens name
    fulllensname[mdr->getLensNameLen()-1] = 0; // null terminate
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

  // display current focal length
  oled.clearDisplay();
  oled.setCursor(10,30);
  oled.setFont(XLARGE_FONT);
  oled.print(mdr->getZoom()/100);
  oled.setFont(LARGE_FONT);
  oled.print(" mm");

  oled.setFont(SMALL_FONT);

  // display total zoom range available
  if (lenswide != lenstight) { // range is only displayed for zooms, not primes
    oled.setCursor(10, 45);
    oled.print("Range: ");
    oled.print(widelimitmeta/100);
    oled.print(" - ");
    oled.print(tightlimitmeta/100);
  }

  // display softening
  oled.setCursor(10, 60);
  if (soft) {
    oled.print(map(softlevel, 0, 0x7FFF, 0, 100));
    oled.print("%");
  } else {
    oled.print("off");
  }

  
  // debug info
  oled.setCursor(40, 60);
  oled.print(adcval);
  oled.print("/");
  oled.print(zeropoint - adcval);

  oled.setCursor(80,60);
  oled.print(millis());
  

  oled.display();

  yield();
}

void loop() {
  //Serial.println("----- Loop Start -----");
  mdr->onLoop(); // receive any incoming messages from mdr

  if (firstrun && timelastsent + 1000 > millis()) { // give the mdr a little time to get caught up at the beginning
    return;
  }

  firstrun = false;

  if (softpressed) { // adjusting soft stop value rather than actually zooming
    softlevel += getMFOutput() * 0.1;
    if (softlevel > MAXSOFT) softlevel = MAXSOFT;
    if (softlevel < 0) softlevel = 0;
    Serial.print("adjusted soft level is now ");
    Serial.println(softlevel);

  } else { // controlling zoom (not changing soft level)

    if (samplecount == 0 || timelastsent + MESSAGE_DELAY > millis()) { // average input from microforce until message delay period is reached
      int toadd = getMFOutput();
      mfoutput += toadd;
      //Serial.print("samplecount #");
      //Serial.print(samplecount);
      //Serial.print(" at time ");
      //Serial.print(millis());
      //Serial.print(": ");
      //Serial.print(toadd);
      //Serial.print(" (Sending at ");
      //Serial.print(timelastsent + MESSAGE_DELAY);
      //Serial.println(")");
      samplecount++;
    } else {
      mfoutput /= samplecount;
      samplecount = 0;

      uint16_t newzoom = curzoom;
      
      //Serial.print("\nCurrent zoom position is 0x");
      //Serial.print(curzoom, HEX);

      int stepsize = mfoutput; // get current output from microforce, this is the step size (amount to increment zoom position)

      //Serial.print(", step size is ");
      //Serial.print(stepsize);

      if (abs(stepsize) < DEADZONE) {
        stepsize = 0; // extremely small steps are below the noise floor, so zero them out
      } else {
        bool zoomingout = stepsize < 0; // negative step size means zooming out

        // Scale stepsize exponentially (high step values should move motor further than low step values)
        stepsize = (1000 + (abs(stepsize) * abs(stepsize) * 9)) / 500; // hard coded values based on my experimentation
        if (zoomingout) stepsize *= -1;

        //Serial.print(", scaled step size is ");
        //Serial.print(stepsize);

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
          
          //Serial.print(", softened step size is ");
          //Serial.print(stepsize);
        }
      }

      // Determine new zoom position using stepsize
      if (mfoutput < 0) { // zooming out
        if (curzoom + stepsize >= widelimit) { // Only continue to change zoom if the new value will be within limits
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

      byte zoomdata[3] = {0x4, highByte(newzoom), lowByte(newzoom)}; // build the mdr command

      if (newzoom != curzoom) { // only send new data if the data is new!
        mdr->data(zoomdata, 3); // send to mdr
        //delay(MESSAGE_DELAY);
        //mdr->data(DATA_SETTING);
        timelastsent = millis();
        curzoom = newzoom;
      } else {
        if (millis() > timelastupdatedname + 100) {
          Serial.println("Requesting lens name during zoom downtime"); 
          // if it has been a while since the last zoom command was sent, use the downtime to update lens name
          mdr->info(0x1);
          timelastsent = millis();
          timelastupdatedname = millis();
        } else if (millis() > timelastsent + MESSAGE_DELAY) {
          mdr->data(DATA_SETTING);
          timelastsent = millis();
        }

      }

      mfoutput = 0;
    }
  }

  // check if set button is pressed
  if (!digitalRead(SETBUTTONPIN)) { // set button is pressed
    if (!setpressed) { // ...and it was not pressed on the last loop, so it was just pressed
      timesetpressed = millis();
      setpressed = true;
      if (!limits) { // ...and limits have not been set yet, so we are beginning the limit-setting process
        firstlimit = curzoom;
        firstlimitmeta = mdr->getZoom(); // get the metadata value of the focal length as well
      }
    }
  } else if (timesetpressed + REPEAT_DELAY < millis()) { // set button is not pressed, and it's been long enough to debounce input
    if (setpressed) { // set button *was* pressed on the last loop, so it was just released
      setpressed = false;
      if (!limits) { // zoom is not currently limited, so set new limits based on the current position
        secondlimit = curzoom;
        secondlimitmeta = mdr->getZoom();
        limits = true;

        // assign our zoom limits from the temp limits
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
    if (!softpressed) { // ...and it was not pressed on the last loop
      timesoftpressed = millis();
      softpressed = true;
    } else {
      Serial.println(" still");
    }
  } else if (timesoftpressed + REPEAT_DELAY < millis()) { // soft button is not pressed, and it's been long enough to debounce input
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
        softlevel_flash.write(softlevel); // store soft level in flash
      }
    }
  }
  

  // LED control
  if (limits) {
    digitalWrite(LEDPIN, HIGH);
  } else {
    digitalWrite(LEDPIN, LOW);
  }
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

  //Serial.print("Reading ADC...");

  int signal = analogRead(MFPIN); // 10-bit, so 0-1024
  adcval = signal;

  //Serial.print("ADC value is ");
  //Serial.print(adcval);

  signal -= zeropoint; // Get a velocity from the raw sensor value

  //Serial.print(" minus zero point of ");
  //Serial.print(zeropoint);
  //Serial.print(" = signal of ");
  //Serial.println(signal * -1);

  return signal * -1; // Value is reversed from what we would expect, for some reason

}
