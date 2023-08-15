#include "PrestonDuino.h"

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


void setup() {
  Serial.begin(9600);
  while(!Serial){};
  Serial.println("---Start---");

  mdr = new PrestonDuino(Serial1);
  delay(100);

  pinMode(SETBUTTONPIN, INPUT_PULLUP);
  pinMode(MFPIN, INPUT);
  pinMode(LEDPIN, OUTPUT);
  pinMode(ZEROBUTTONPIN, INPUT_PULLUP);
  pinMode(SOFTBUTTONPIN, INPUT_PULLUP);

  mdr->shutUp();
  mdr->mode(0x9, 0x4); // commanded motor positions, zoom position, streaming, controlling zoom
  mdr->data(0x44); // request only zoom position
  zeroOut();

  timelastsent = millis();
}

void zeroOut() {
  byte zoomdata[3] = {0x3, 0x7F, 0xFF}; // establish a known starting position, halfway through range
  curzoom = 0x7FFF;
  mdr->data(zoomdata, 3);
  zeropoint = analogRead(MFPIN);
}

void loop() {
  mdr->onLoop();

  if (firstrun && timelastsent + 1000 > millis()) { // give the mdr a little time to get caught up at the beginning
    return;
  }

  firstrun = false;

  if (softpressed) { // adjusting soft stop value rather than actually zooming
    softlevel += getMFOutput() * .1;
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
      //curzoom = mdr->getZoom();
      
      Serial.print("Current zoom position is 0x");
      Serial.print(curzoom, HEX);

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
  }

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
}

int getMFOutput() {
  // zero = 5.02v
  // full speed in ~ 7.7v
  // full speed out ~ 2.0v
  // scale factor = 2.464
  int signal = analogRead(MFPIN); // 10-bit, so 0-1024
  //signal *= 2464; // scale up to mv...but do we actually care about the voltage?

  signal -= zeropoint; // need to find the actual zero point in the scale, as it isn't actually evenly split
  return signal * -1;

}
