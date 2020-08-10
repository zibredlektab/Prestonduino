#include <RH_RF95.h>
#include <RHReliableDatagram.h>

#define SERVER_ADDRESS 0
#define CLIENT_ADDRESS 1


RH_RF95 driver;
RHReliableDatagram manager(driver, CLIENT_ADDRESS);

void setup() {
  Serial.begin(115200);
  while (!Serial);
  if (!manager.init()) {
    Serial.println("init failed");
  }
}

uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
int pot = 1023;
uint8_t data[5];

void loop() {
  Serial.println("Sending!");
  pot = analogRead(0);

  sprintf(data, "%d", pot);

  if (manager.sendto(data, sizeof(data), SERVER_ADDRESS)) {
/*
    uint8_t len = sizeof(buf);
    uint8_t fromaddress;
    
    if (manager.recvfromAckTimeout(buf, &len, 10, &fromaddress)) {
      Serial.print("got a reply from: 0x");
      Serial.print(fromaddress, HEX);
      Serial.print(": ");
      Serial.println((char*)buf);
    } else {
      Serial.println("No reply");
    }
  } else {
    Serial.println("sendtoWait failed");*/
  }

  //delay(10);

}
