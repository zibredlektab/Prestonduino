#include "PrestonDuino.h"

PrestonDuino *mdr;

bool running = false;
bool switchreleased = true;
uint32_t timeswitchreleased = 0;

bool runswitch = false;

#define DEBOUNCE 1000

void setup() {

  pinMode(RUN, INPUT_PULLUP);

  Serial.begin(9600);
  while(!Serial);

  mdr = new PrestonDuino(Serial1);
  delay(100);
  while(!mdr->isMDRReady()) {
    delay(10);
    mdr->onLoop();
  }
  mdr->shutUp();
  delay(6);
  mdr->mode(0x80,0x0);
  Serial.println("Done with mdr setup");
}

void loop() {
  mdr->onLoop();
  mdr->stat();
  delay(6);
  if (!digitalRead(RUN)) { // switch is pressed
    if (!runswitch) { // ...and it was not pressed on the last loop, so it was just pressed
      runswitch = true;
      if (!running) {
        mdr->r_s(true);
        running = true;
        delay(6);
      } else {
        mdr->r_s(false);
        running = false;
        delay(6);
      }
    }
  } else { // switch is not pressed
    if (runswitch) { // ...but it was pressed on the last loop, so it was just released
      runswitch = false;
    }
  }
}