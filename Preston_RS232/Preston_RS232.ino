#include "PrestonDuino.h"

PrestonDuino *mdr;

long unsigned int time = 0;
long unsigned int time1 = 0;
uint8_t i = 0;
bool continuelooping = true;

void setup() {

  Serial.begin(115200);
  while(!Serial);

  Serial.println("start");

  mdr = new PrestonDuino(Serial1);
  delay(100);
  Serial.println("Done with mdr setup");
  //mdr->mode(0x01,0x42);
  Serial.println("Done setting mode");
}

void loop() {
  if (continuelooping) {
    mdr->onLoop();

    if (time1 + 100 < millis()) {
      Serial.println("*****time to print lens info*****");
      time1 = millis();
      Serial.print("iris: ");
      Serial.print(mdr->getIris());
      Serial.print(" focus: ");
      uint16_t focus = mdr->getFocus(); // as mm
      double focusin = (double)focus / 25.4; // to inches
      double focusft = focusin / 12.0; // to ft and fractional ft
      focusft = floor(focusft); // just feet
      focusin -= (focusft * 12.0); // get just the remaining inches
      Serial.print((int)focusft);
      Serial.print("' ");
      Serial.print(focusin);
      Serial.print("\" ");
      Serial.print(focus);
      Serial.print("mm zoom: ");
      Serial.println(mdr->getZoom());
      //mdr->info(1);
      Serial.println(mdr->getLensName());
    }
  /*
    if (time + 6 < millis()) {

      time = millis();
      //command_reply lens = mdr->info(1);
      //Serial.print("current lens name is: ");
      //Serial.println(mdr->getLensName());
      //Serial.println("time to update aux");

      uint16_t iris = map(time, 0, 10000, 0, 0xFFFF); // full lens rack in 10 seconds...nice and smooth
      byte irish = iris >> 8;
      byte irisl = iris & 0xFF;
      byte irisdata[3] = {0x2, irish, irisl};
      mdr->data(irisdata, 3);
    }*/

    if (time + 3000 < millis()) {
      mdr->mode(0,0);
      delay(500);
      continuelooping = false;
      Serial.println("Debug time's up, shutting down.");
    }
  }

  
}