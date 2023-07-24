#include "PrestonDuino.h"

#define REPEAT_DELAY 10
#define SETBUTTONPIN 5
#define MFPIN 6
#define LEDPIN 13

PrestonDuino *mdr;
int mfoutput = 0; // current velocity specified by MicroForce
uint16_t curzoom; // current zoom position
bool setpressed = false; // is the "Set" button depressed?
uint32_t timesetpressed; // when was the "Set" button depressed?

bool limits = false; // is the zoom currently limited
uint16_t firstlimit; // initial position when set button is first pressed
uint16_t secondlimit; // ending position when set button is released
uint16_t widelimit = 0; // encoder position of wide zoom limit
uint16_t tightlimit = 0xFFFF; // encoder position of tight zoom limit


void setup() {
  mdr = new PrestonDuino(Serial1);
  pinMode(SETBUTTONPIN, INPUT_PULLUP);
  pinMode(MFPIN, INPUT);
  pinMode(LEDPIN, OUTPUT);

  mdr->mode(0x19, 0x4);
}

void loop() {
  mdr->onLoop();
  curzoom = mdr->getZoom();
  uint16_t newzoom;

  // determine step size by microforce output
  mfoutput = getMFOutput();
  int stepsize = mfoutput;

  if (mfoutput < 0) {
    if (curzoom + stepsize >= widelimit) {
      newzoom = curzoom + stepsize;
    } else {
      newzoom = widelimit;
    }
  } else if (mfoutput > 0) {
    if (curzoom + stepsize <= tightlimit) {
      newzoom = curzoom + stepsize;
    } else {
      newzoom = tightlimit;
    }
  }

  byte zoomdata[3] = {0x4, highByte(newzoom), lowByte(newzoom)};

  if (newzoom != curzoom) mdr->data(zoomdata, 3); // send to mdr


  // check if set button is pressed
  if (!digitalRead(SETBUTTONPIN)) { // set button is pressed
    if (!setpressed) {
      timesetpressed = millis();
      setpressed = true;
      if (!limits) firstlimit = curzoom;
    }
  } else if (timesetpressed + REPEAT_DELAY < millis()) { // set button is not pressed, and it's been long enough to debounce input!
    if (setpressed) { // set button *was* pressed on the last loop, so it was just released
      setpressed = false;
      if (!limits) { // zoom is not currently limited, so set new limits based on the current position
        secondlimit = curzoom;
        limits = true;

        // assign our temp limits as the zoom limits
        if (firstlimit < secondlimit) {
          widelimit = firstlimit;
          tightlimit = secondlimit;
        } else {
          widelimit = secondlimit;
          tightlimit = firstlimit;
        }
      } else { // zoom is currently limited, so release the limits
        limits = false;
        widelimit = 0;
        tightlimit = 0xFFFF;
      }
    }
  }

  if (limits) {
    digitalWrite(LEDPIN, HIGH);
  } else {
    digitalWrite(LEDPIN, LOW);
  }
}

int getMFOutput() {
  // zero = 5.02v
  // full speed in ~ 7.7v
  // full speed out ~ 2.0v
  // scale factor = 2.464
  int signal = analogRead(MFPIN); // 10-bit, so 0-1024
  //signal *= 2464; // scale up to mv...but do we actually care about the voltage?

  signal -= 512; // need to find the actual zero point in the scale, as it isn't actually evenly split
  return signal;

}
