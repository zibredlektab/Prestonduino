#include "PDServer.h"

PDServer::PDServer(int chan, HardwareSerial& mdrserial) {
  //Serial.begin(115200);
  this->channel = chan;
  this->address = chan * 0x10;
  this->ser = &mdrserial;
  this->mdr = new PrestonDuino(*ser);
  delay(100); // give PD time to connect
  
  this->manager = new RHReliableDatagram(this->driver, this->address);
  this->manager->init();

  this->driver.setFrequency(915.0);
  this->driver.setModemConfig(RH_RF95::Bw500Cr45Sf128);
  //Serial.println("done with setup");
  
}

void PDServer::onLoop() {
  if (this->manager->available()) {
    this->buflen = sizeof(this->buf);
    uint8_t from;
    if (manager->recvfromAck(this->buf, &this->buflen, &from)) {
      //Serial.print("got request of ");
      //Serial.print(this->buflen);
      //Serial.print(" characters from 0x");
      //Serial.print(from, HEX);
      //Serial.print(": ");
      for (int i = 0; i < this->buflen; i++) {
        //Serial.print(this->buf[i]);
      }
      //Serial.println();
      if (this->buf[0] == 1) {
        // This message contains a PrestonPacket
        PrestonPacket *pak = new PrestonPacket(&this->buf[1], this->buflen-1);
        int replystat = this->mdr->sendToMDR(pak);
        if (replystat == -1) {
          // MDR acknowledged the packet, didn't send any other data
          if(!this->manager->sendtoWait((uint8_t*)3, 1, this->address)) { // 0x3 is ack
            //Serial.println("failed to send reply");
          }
        } else if (replystat > 0) {
          // MDR sent data
          command_reply reply = this->mdr->getReply();
          if(!this->manager->sendtoWait(this->replyToArray(reply), reply.replystatus + 1, this->address)) {
            //Serial.println("failed to send reply");
          }
        }
      } else if (this->buf[0] == 2) {
        // This message is requesting data
        //Serial.println("Data is being requested");
        uint8_t datatype = this->buf[1];
        uint32_t mdrdata;
        char tosend[10];
        switch (datatype) {
          case 1:
            mdrdata = mdr->getAperture();
            //Serial.print("Aperture is ");
            break;
          case 2:
            //Serial.print("Focus distance is ");
            mdrdata = mdr->getFocusDistance();
            break;
          case 4:
            //Serial.print("Zoom is ");
            mdrdata = mdr->getFocalLength();
            break;
          default:
            // Client is asking for an unsupported data type
            //Serial.print("Requested data type (");
            //Serial.print(datatype);
            //Serial.println(") is unsupported");
            break;
        }

        //Serial.println(mdrdata);
        //Serial.print("Sending ");
        uint8_t sendlen = snprintf(tosend, sizeof(tosend), "%lu", (unsigned long)mdrdata);
        for (int i = 0; i <= sendlen; i++) {
          //Serial.print(tosend[i]);
        }
        //Serial.print(" back to 0x");
        //Serial.println(from, HEX);
        if(!this->manager->sendtoWait(tosend, sendlen+1, from)) {
          //Serial.println("Failed to send reply");
        } else {
          //Serial.println("Reply sent");
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
