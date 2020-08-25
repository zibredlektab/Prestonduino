//#include <LowPower.h>

#include <RH_RF95.h>
#include <PrestonDuino.h>
#include <PrestonPacket.h>


RH_RF95 driver;

PrestonDuino *mdr;
uint32_t dist;
char buf[10];
uint8_t buflen;

void setup() {
  
  mdr = new PrestonDuino(Serial);
  if (driver.init()) {
    //driver.setModemConfig(RH_RF95::Bw31_25Cr48Sf512);
    driver.setFrequency(915.0);
  }

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(10);
  digitalWrite(LED_BUILTIN, LOW);
}


void loop() {
  
  //LowPower.powerDown(SLEEP_15MS, ADC_OFF, BOD_OFF); 
  
  dist = mdr->getFocusDistance();
  
  buflen = snprintf(buf, sizeof(buf), "%lu", (unsigned long)dist);

  driver.send(buf, buflen+1);

 
}
