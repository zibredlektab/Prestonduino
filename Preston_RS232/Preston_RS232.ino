#include "PrestonDuino.h"
#include <Arduino.h>   // required before wiring_private.h
#include "wiring_private.h" // pinPeripheral() function


PrestonDuino *mdr;

unsigned long long lastdata = 0;

void setup() {

  Serial.begin(115200);
  while(!Serial);
  Serial.print("start");


  mdr = new PrestonDuino(Serial1, 1);

  delay(100);
}


void loop() {

  mdr->onLoop();

  if (millis() > lastdata + 6) {
    mdr->data(0x87);
    lastdata = millis();
  }
}