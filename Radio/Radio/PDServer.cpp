#include "PDServer.h"

PDServer::PDServer(int chan, HardwareSerial& mdrserial) {
  Serial.begin(115200);
  this->channel = chan;
  this->address = chan * 0x10;
  this->ser = &mdrserial;
  this->mdr = new PrestonDuino(*ser);
  delay(100); // give PD time to connect

  this->driver = new RH_RF95(4);
  this->manager = new RHReliableDatagram(*this->driver, this->address);
  Serial.println(F("Manager created, initializing"));
  if (!this->manager->init()) {
    Serial.println(F("RH manager init failed"));
  } else {
    Serial.println(F("RH manager initialized"));
  }

  if (!this->driver->setFrequency(915.0)) {
    Serial.println(F("Driver failed to set frequency"));
  }
  this->driver->setModemConfig(RH_RF95::Bw500Cr45Sf128);
  
  Serial.println("done with setup");
  Serial.print("My address is 0x");
  Serial.println(this->address, HEX);
  
}

void PDServer::onLoop() {
  if (this->manager->available()) {
    Serial.println("Message available");
    this->buflen = sizeof(this->buf);
    uint8_t from;
    if (manager->recvfromAck(this->buf, &this->buflen, &from)) {
      Serial.print("got request of ");
      Serial.print(this->buflen);
      Serial.print(" characters from 0x");
      Serial.print(from, HEX);
      Serial.print(": ");
      for (int i = 0; i < this->buflen; i++) {
        Serial.print(this->buf[i]);
      }
      Serial.println();
      if (this->buf[0] == 1) {
        // This message contains a PrestonPacket
        PrestonPacket *pak = new PrestonPacket(&this->buf[1], this->buflen-1);
        int replystat = this->mdr->sendToMDR(pak);
        if (replystat == -1) {
          // MDR acknowledged the packet, didn't send any other data
          if(!this->manager->sendtoWait((uint8_t*)3, 1, from)) { // 0x3 is ack
            Serial.println("failed to send reply");
          }
        } else if (replystat > 0) {
          // MDR sent data
          command_reply reply = this->mdr->getReply();
          if(!this->manager->sendtoWait(this->replyToArray(reply), reply.replystatus + 1, from)) {
            Serial.println("failed to send reply");
          }
        } else if (replystat == 0) {
          // MDR didn't respond
          uint8_t err[] = {0xF, 0x2};
          if (!this->manager->sendtoWait(err, 2, from)) {
            Serial.println("failed to send reply");
          }
        }
      } else if (this->buf[0] == 2) {
        // This message is requesting data
        Serial.println("Data is being requested");
        uint8_t datatype = this->buf[1];
        char tosend[20];
        uint8_t sendlen;
        if (datatype == 1 || datatype == 2 || datatype == 4) {
          Serial.println("Only one type of data is needed");
          uint32_t mdrdata;
          switch(datatype) {
            case 1:
              tosend[0] = 4;
              mdrdata = mdr->getAperture();
              Serial.print("Aperture is ");
              break;
            case 2:
              tosend[0] = 5;
              Serial.print("Focus distance is ");
              mdrdata = mdr->getFocusDistance();
              break;
            case 4:
              tosend[0] = 4;
              Serial.print("Zoom is ");
              mdrdata = mdr->getFocalLength();
              break;
          }
          Serial.println(mdrdata);
          
          sendlen = snprintf(tosend+1, sizeof(tosend)+1 , "%lu", (unsigned long)mdrdata) + 1;

        } else if (datatype == 7) {
          Serial.println("Full FIZ data is being requested");
          tosend[0] = 6;
          sendlen = 8;
          uint8_t* fizdata = mdr->getLensData();
          for (int i = 0; i < sendlen-1; i++) {
            tosend[i+1] = fizdata[i];
          }
        }

        Serial.print("Sending ");
        for (int i = 0; i <= sendlen; i++) {
          Serial.print(tosend[i]);
        }
        Serial.print(" back to 0x");
        Serial.println(from, HEX);
        if(!this->manager->sendto((char*)tosend, sendlen, from)) {
          Serial.println("Failed to send reply");
        } else {
          Serial.println("Reply sent");
        }
      }
    }
  }
}

uint8_t* PDServer::replyToArray(command_reply input) {
  uint8_t output[input.replystatus + 2];
  output[0] = 0;
  output[1] = input.replystatus;
  for (int i = 0; i < input.replystatus+1; i++) {
    output[i+2] = input.data[i];
  }
  return output;
}

uint8_t PDServer::getAddress() {
  return this->address;
}
