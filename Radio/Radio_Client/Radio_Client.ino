#include <RH_RF95.h>
#include <PrestonDuino.h>
#include <PrestonPacket.h>


RH_RF95 driver;

PrestonDuino *mdr;

uint32_t dist;
char buf[10];
uint8_t buflen = 0;

void setup() {
  mdr = new PrestonDuino(Serial);
  if (driver.init()) {
    driver.setModemConfig(RH_RF95::Bw500Cr45Sf128);
  }

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(10);
  digitalWrite(LED_BUILTIN, LOW);
}


void loop() {
  
  dist = mdr->getFocusDistance();
  
  buflen = snprintf(buf, sizeof(buf), "%lu", (unsigned long)dist);

  driver.send(buf, buflen+1);
}
