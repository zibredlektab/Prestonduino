#include <PDServer.h>

#define PIN1 5
#define PIN2 6
#define PIN4 7
#define PIN8 8

PDServer *pd;

void setup() {
  pd = new PDServer(readSwitch(), Serial1);
  
  pinMode(PIN1, INPUT_PULLUP);
  pinMode(PIN2, INPUT_PULLUP);
  pinMode(PIN4, INPUT_PULLUP);
  pinMode(PIN8, INPUT_PULLUP);
}

void loop() {
  pd->onLoop();
}

uint8_t readSwitch() {
  uint8_t value = 0;
  for (int i = 0; i < 4; i++) {
    if (!digitalRead(i + PIN1)) {
      value += 1 << i;
    }
  }

  return value;
}
