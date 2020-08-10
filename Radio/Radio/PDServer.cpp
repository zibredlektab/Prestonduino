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
      if (this->buf[0] = 1) {
        // This message contains a PrestonPacket
        PrestonPacket *pak = new PrestonPacket(&this->buf[1], this->buflen-1);
        int replystat = this->mdr->sendToMDR(pak);
        if (replystat >= 0 || replystat == -1) {
          command_reply reply = this->mdr->getReply();
          if(!this->manager->sendtoWait((uint8_t*)&reply, reply.replystatus + 1, this->address)) {
            Serial.println("failed to send reply");
          }
        }
      } else if (this->buf[0] = 2) {
        // This message is requesting data
        uint8_t datatype = this->buf[1];
        switch (datatype) {
          case 1:
            if(!this->manager->sendtoWait((uint8_t*)mdr->getAperture(), 2, this->address)) {
              Serial.println("failed to send reply");
            }
            break;
          case 2:
            if(!this->manager->sendtoWait((uint8_t*)mdr->getFocusDistance(), 4, this->address)) {
              Serial.println("failed to send reply");
            }
            break;
          case 4:
            if(!this->manager->sendtoWait((uint8_t*)mdr->getFocalLength(), 2, this->address)) {
              Serial.println("failed to send reply");
            }
            break;
          default:
            // Client is asking for an unsupported data type
            break;
        }
      }
    }
  }
}

byte* PDServer::replyToArray(command_reply input) {
  byte output[input.replystatus + 2];
  output[0] = 0;
  output[1] = input.replystatus;
  for (int i = 0; i < input.replystatus+1; i++) {
    output[i+2] = input.data[i];
  }
  return output;
}
