#include <PrestonDuino.h>

PrestonDuino *mdr;

long unsigned int time = 0;
uint8_t i = 0;

void setup() {

  Serial.begin(115200);
  while(!Serial);

  mdr = new PrestonDuino(Serial1);
  mdr->mode(0x01,0x08);
}

void loop() {
  mdr->onLoop();

  if (time + 100 < millis()) {

    Serial.print("iris is: ");
    Serial.println(mdr->getAperture());
    time = millis();
    uint16_t iris = map(time, 0, 10000, 0, 0xFFFF); // full lens rack in 10 seconds...nice and smooth
    byte irish = iris >> 8;
    byte irisl = iris & 0xFF;
    byte irisdata[3] = {0x1, irish, irisl};
    //mdr->data(irisdata, 3);
  }

  if (millis() > 10000) {
    while(1);
  }

}