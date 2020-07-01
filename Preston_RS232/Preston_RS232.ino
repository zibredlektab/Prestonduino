#include "PrestonPacket.h"
#include "PrestonDuino.h"



//bool rcving = false;
//bool packetcomplete = false; // flag for whether there is data available to be processed
//char rcvbuffer[100]; // buffer for storing incoming data, currently limited to 100 bytes since that seems like more than enough?
//int packetlen = 0;

byte* lensdata;

PrestonDuino *mdr = new PrestonDuino();

unsigned long time_now = 0;
int period = 5;

void setup() {
  
  Serial.begin(115200); //open communication with computer
  while (!Serial) {
    ; // wait for serial port to connect. Needed for Native USB only
  }
  
  mdr->init(Serial1);

}

void loop() {
  if (millis() >= time_now + period) {
    time_now = millis();
    lensdata = mdr->ld();
  }
  
    Serial.print("start");
    for (int i = 0; i < 3; i++) {
      Serial.print(lensdata[i], HEX);
    }
    Serial.println("end");
}
