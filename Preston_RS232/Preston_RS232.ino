#include "PrestonPacket.h"
#include "PrestonDuino.h"

byte* lensdata;

PrestonDuino *mdr;

unsigned long time_now = 0;
int period = 5;


void setup() {

  Serial.begin(115200); //open communication with computer

  mdr = new PrestonDuino(Serial1);

  delay(100);
  
  byte* lens = mdr->who();
  
  Serial.print("start");
  Serial.print(lens[0]);
  Serial.println("end");
}


void loop() {
 /* if (mdr->readyToSend()) {
    if (millis() >= time_now + period) {
      time_now = millis();
      lensdata = mdr->ld();
    }
    
    Serial.print("");
    for (int i = 1; i < lensdata[0]; i++) {
      Serial.print(lensdata[i], HEX);
      Serial.print(" ");
    }
    Serial.println();
  }*/
}
