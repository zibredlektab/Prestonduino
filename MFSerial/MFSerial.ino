#include "PrestonDuino.h"

#define REPEAT_DELAY 10
#define SETBUTTONPIN 5
#define MFPIN A0
#define LEDPIN 13
#define MESSAGE_DELAY 12

bool firstrun = true;
PrestonDuino *mdr;
int mfoutput = 0; // current velocity specified by MicroForce
uint16_t curzoom; // current zoom position
bool setpressed = false; // is the "Set" button depressed?
uint32_t timesetpressed; // when was the "Set" button depressed?
uint32_t timelastsent = 0; // when was the last mdr message sent?
int count; // number of microforce samples taken in this averaging

bool limits = false; // is the zoom currently limited
uint16_t firstlimit; // initial position when set button is first pressed
uint16_t secondlimit; // ending position when set button is released
uint16_t widelimit = 0; // encoder position of wide zoom limit
uint16_t tightlimit = 0xFFFF; // encoder position of tight zoom limit


void setup() {
  Serial.begin(9600);
  while(!Serial){};
  Serial.println("---Start---");

  mdr = new PrestonDuino(Serial1);
  delay(100);

  pinMode(SETBUTTONPIN, INPUT_PULLUP);
  pinMode(MFPIN, INPUT);
  pinMode(LEDPIN, OUTPUT);

  mdr->shutUp();
  mdr->mode(0x9, 0x4); // commanded motor positions, zoom position, streaming, controlling zoom
  mdr->data(0x44); // request only zoom position
  byte zoomdata[3] = {0x3, 0x7F, 0xFF}; // establish a known starting position, halfway through range
  curzoom = 0x7FFF;
  mdr->data(zoomdata, 3);

  timelastsent = millis();
}

void loop() {
  mdr->onLoop();

  if (firstrun && timelastsent + 1000 > millis()) { // give the mdr a little time to get caught up at the beginning
    return;
  }

  firstrun = false;

  if (count == 0 || timelastsent + MESSAGE_DELAY > millis()) { // average input from microforce until message delay period is reached
    mfoutput += getMFOutput();
    count++;
  } else {
    mfoutput /= count;
    count = 0;

    uint16_t newzoom = curzoom;
    //curzoom = mdr->getZoom();
    
    Serial.print("Current zoom position is 0x");
    Serial.print(curzoom, HEX);

    int stepsize = mfoutput; // eventually this will be scaled for soft stops
    
    Serial.print(", step size is ");
    Serial.print(stepsize);

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

    byte zoomdata[3] = {0x4, highByte(newzoom), lowByte(newzoom)};

    if (newzoom != curzoom) {
      mdr->data(zoomdata, 3); // send to mdr
      curzoom = newzoom;
    }

    timelastsent = millis();
    mfoutput = 0;
  }
/*
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
  }*/
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
