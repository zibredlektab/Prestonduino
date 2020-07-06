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
  
  lensdata = mdr->ld();
  int lensdatalen = lensdata[0];
  
  Serial.print("Data is this many bytes long: ");
  Serial.println(lensdatalen);
  
  Serial.print("start");
  for (int i = 0; i < lensdatalen; i++) {
    Serial.print(lensdata[i], HEX);
    Serial.print(" ");
  }
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
