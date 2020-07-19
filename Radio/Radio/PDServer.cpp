#include "PDServer.h"


PDServer::PDServer(uint8_t addr, HardwareSerial& mdrserial) {
  this->address = addr;
  this->ser = &mdrserial;
  this->mdr = new PrestonDuino(*ser);
  delay(100); // give PD time to connect

  if (!this->driver.setFrequency(RF95_FREQ)) {
    Serial.println("setFrequency failed");
    while (1);
  }
  
  this->manager = new RHReliableDatagram(this->driver, this->address);
  if (!this->manager->init()) {
    Serial.println("RH manager init failed");
  }
  
}

void PDServer::onLoop() {
  if (this->manager->available()) {
    this->buflen = sizeof(this->buf);
    uint8_t from;
    if (manager->recvfromAck(this->buf, &this->buflen, &from)) {
      Serial.print("got request from : 0x");
      Serial.print(from, HEX);
      Serial.print(": ");
      Serial.println((char*)this->buf);
      if (this->buf[0] = "0x1") {
        // This message contains a PrestonPacket
        PrestonPacket *pak = new PrestonPacket(&this->buf[1], this->buflen-1);
        int replystat = this->mdr->sendToMDR(pak);
        if (replystat >= 0 || replystat == -1) {
          command_reply reply = this->mdr->getReply();
          if(!this->manager->sendtoWait((uint8_t*)&reply, reply.replystatus + 1, this->address)) {
            Serial.println("failed to send reply");
          }
        }
      }
    }
  }
}

byte* PDServer::replyToArray(command_reply input) {
  byte output[input.replystatus + 1];
  output[0] = input.replystatus;
  for (int i = 0; i < input.replystatus; i++) {
    output[i+1] = input.data[i];
  }
  return output;
}
