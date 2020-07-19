#include <RH_RF95.h>
#include <RHReliableDatagram.h>

#define SERVER_ADDRESS 0
#define CLIENT_ADDRESS 1

RH_RF95 driver;
RHReliableDatagram manager(driver, SERVER_ADDRESS);

void setup() {
  Serial.begin(9600);
  while (!Serial);
  if (!manager.init()) {
    Serial.println("init failed");
  }
}

uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];

void loop() {
  if (manager.available()) {
    uint8_t len = sizeof(buf);
    uint8_t fromaddress;
    if (manager.recvfromAck(buf, &len, &fromaddress)){
      Serial.print("got a request from: 0x");
      Serial.print(fromaddress, HEX);
      Serial.print(": ");
      Serial.println((char*)buf);
    }
  }

}
