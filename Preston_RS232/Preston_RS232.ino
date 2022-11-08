#include "PrestonDuino.h"

PrestonDuino *mdr;

long unsigned int time = 0;
long unsigned int time1 = 0;
uint8_t i = 0;

void setup() {

  Serial.begin(115200);
  while(!Serial);

  Serial.println("start");

  mdr = new PrestonDuino(Serial1);
  //mdr->mode(0x01,0x01);
}

void loop() {
  mdr->onLoop();

  if (time1 + 100 < 1) {
    time1 = millis();
    Serial.print("iris: ");
    Serial.print(mdr->getIris());
    Serial.print(" focus: ");
    Serial.println(mdr->getFocus());
  }

  if (time + 1000 < millis()) {

    time = millis();
    Serial.println("asking for lens name...");
    command_reply lens = mdr->info(1);
    Serial.print("current lens name is: ");
    for (int i = 2; i < lens.replystatus; i++) {
      Serial.print((char)lens.data[i]);
    }
    Serial.println();

/*
    uint16_t iris = map(time, 0, 10000, 0, 0xFFFF); // full lens rack in 10 seconds...nice and smooth
    byte irish = iris >> 8;
    byte irisl = iris & 0xFF;
    byte irisdata[3] = {0x1, irish, irisl};
    //mdr->data(irisdata, 3);*/
  }

  
}