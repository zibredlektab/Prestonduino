#include "PrestonPacket.h"
#include "PrestonDuino.h"

char* lensdata;

PrestonDuino *mdr;

unsigned long long time_now = 0;
int period = 5;

int count = 0;

void setup() {

  Serial.begin(115200); //open communication with computer

  mdr = new PrestonDuino(Serial1);

  delay(100);
}


void loop() {

  if (millis() >= time_now + period) {
    Serial.print(count++);
    Serial.print(": ");
    
    time_now = millis();
    lensdata = mdr->getLensName();
    Serial.println(lensdata);
  }
  
}
