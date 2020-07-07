#include "PrestonPacket.h"
#include "PrestonDuino.h"

command_reply lensdata;

PrestonDuino *mdr;

int time_now = 0;
int period = 50;

int count = 0;

void setup() {

  Serial.begin(115200); //open communication with computer

  mdr = new PrestonDuino(Serial1);

  delay(100);
}


void loop() {
  //Serial.println(millis());
  
  if (millis() >= time_now + period) {
    Serial.print(count++);
    Serial.print(": ");
    
    time_now = millis();
    lensdata = mdr->info(0x1);
    Serial.println(lensdata.replystatus);
  }
  
}
