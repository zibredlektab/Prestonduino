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
  if (!this->driver->setModemConfig(RH_RF95::Bw500Cr45Sf128)) {
    Serial.println(F("Driver failed to configure modem"));
  }

  this->driver->setPromiscuous(false);
  
  Serial.print(F("Done with setup, my address is 0x"));
  Serial.println(this->address, HEX);
  
}

void PDServer::onLoop() {
  if (this->manager->waitAvailableTimeout(15)) {
    Serial.println(F("Message available"));
    this->buflen = sizeof(this->buf);
    if (this->manager->recvfromAck(this->buf, &this->buflen, &this->lastfrom)) {
      Serial.print(F("got request of "));
      Serial.print(this->buflen);
      Serial.print(F(" characters from 0x"));
      Serial.print(this->lastfrom, HEX);
      Serial.print(": ");
      for (int i = 0; i < this->buflen; i++) {
        Serial.print(this->buf[i]);
      }
      Serial.println();
      
      if (this->buf[0] == 1) {
        // This message contains a PrestonPacket (deprecated)
        /*PrestonPacket *pak = new PrestonPacket(&this->buf[1], this->buflen-1);
        int replystat = this->mdr->sendToMDR(pak);
        if (replystat == -1) {
          // MDR acknowledged the packet, didn't send any other data
          if(!this->manager->sendtoWait((uint8_t*)3, 1, from)) { // 0x3 is ack
            Serial.println(F("failed to send reply"));
          }
        } else if (replystat > 0) {
          // MDR sent data
          command_reply reply = this->mdr->getReply();
          if(!this->manager->sendtoWait(this->replyToArray(reply), reply.replystatus + 1, from)) {
            Serial.println(F("failed to send reply"));
          }
        } else if (replystat == 0) {
          // MDR didn't respond
          uint8_t err[] = {0xF, 0x2};
          if (!this->manager->sendtoWait(err, 2, from)) {
            Serial.println(F("failed to send reply"));
          }
        }*/
      } else if (this->buf[0] == 2) {
        // This message is requesting data
        Serial.println(F("Data is being requested"));
        uint8_t datatype = this->buf[1];
        if (datatype == 0) {
          Serial.println(F("Actually, unsubscription is being requested."));
          this->unsubscribe(this->lastfrom);
          return;
        }
        bool subscribe = false;
        if (this->buflen > 2) {
          subscribe = this->buf[2];
        }
        
        if (!subscribe) {
          // Client only wants this data once, now
          char tosend[20];
          uint8_t sendlen = getData(datatype, tosend);
          
  
          Serial.print(F("Sending "));
          for (int i = 0; i <= sendlen; i++) {
            Serial.print(tosend[i]);
          }
          Serial.print(F(" back to 0x"));
          Serial.println(this->lastfrom, HEX);
          if(!this->manager->sendto((char*)tosend, sendlen, this->lastfrom)) {
            Serial.println(F("Failed to send reply"));
          } else {
            Serial.println(F("Reply sent"));
          }
        } else {
          // Client wants to subscribe to this data
          Serial.print(F("Subscribing client at 0x"));
          Serial.print(this->lastfrom, HEX);
          Serial.print(F(" to receive data of type (binary) "));
          Serial.print(datatype, BIN);
          Serial.println(F(" every loop"));
          this->subscribe(this->lastfrom, datatype);
        }
      }
    }
  }

  if(!this->updateSubs()) {
    Serial.println(F("Failed to update all subscriptions"));
  }
}

uint8_t PDServer::getData(uint8_t datatype, char* databuf) {
  uint8_t sendlen = 2; // first two bytes are 0x4 and data type
  databuf[0] = 0x4;
  databuf[1] = datatype;
  if (millis() > this->lastupdate + 5) {
    this->iris = mdr->getAperture();
    this->focus = mdr->getFocusDistance();
    this->zoom = mdr->getFocalLength();
    this->lastupdate = millis();
    if (this->iris == 0 || this->focus == 0 || this->zoom == 0) {
      databuf[0] = 0xF;
      databuf[1] = 0x2;
      return sendlen;
    }
  }

  Serial.print(F("Getting following data: "));
  
  if (datatype & 0b000001) {
    Serial.print(F("iris "));
    sendlen += snprintf(&databuf[sendlen], 20, "%lu", (unsigned long)this->iris);
  }
  if (datatype & 0b000010) {
    Serial.print(F("focus "));
    sendlen += snprintf(&databuf[sendlen], 20, "%lu", (unsigned long)this->focus);
  }
  if (datatype & 0b000100) {
    Serial.print(F("zoom "));
    sendlen += snprintf(&databuf[sendlen], 20, "%lu", (unsigned long)this->zoom);
  }
  if (datatype & 0b001000) {
    Serial.print(F("aux (we don't do that yet) "));
  }
  if (datatype & 0b010000) {
    Serial.print(F("distance (we don't do that yet) "));
  }
  if (sendlen > 0) {
    Serial.println();
    databuf[sendlen++] = "\0";
  } else {
    Serial.println(F("...nothing?"));
  }
  for (int i = 0; i < sendlen; i++) {
    Serial.print((char)databuf[i]);
  }
  Serial.println();
  
  return sendlen;
}



bool PDServer::updateSubs() {
  char tosend[20];
  uint8_t sendlen = 0;
  
  if (subcount > 0) {
    Serial.println(F("Subscriptions are being updated..."));
    Serial.print(subcount);
    Serial.println(F(" total sub(s)"));
    for (int i = 0; i < this->subcount; i++) {
      uint8_t desc = this->subs[i].data_descriptor;
      Serial.print(F("Updating client 0x"));
      Serial.print(this->subs[i].client_address, HEX);
      Serial.println();
      sendlen = this->getData(desc, tosend);
      if (!this->manager->sendto((char*)tosend, sendlen, this->subs[i].client_address)) {
        Serial.println(F("Failed to send message"));
        return false;
      }
    }
  } else {
    return true;
  }
}

void PDServer::subscribe(uint8_t addr, uint8_t desc) {
  this->subs[subcount].client_address = addr;
  this->subs[subcount].data_descriptor = desc;
  subcount++;
}

bool PDServer::unsubscribe(uint8_t addr) {
  Serial.print(F("0x"));
  Serial.print(addr, HEX);
  Serial.println(F(" wants to be unsubscribed."));
  for (int i = 0; i < subcount; i++) {
    if (this->subs[i].client_address == addr) {
      for (int j = i; j < subcount; j++) {
        this->subs[j].client_address = this->subs[j+1].client_address;
        this->subs[j].data_descriptor = this->subs[j+1].data_descriptor;
      }
      subcount--;
      Serial.println(F("Done unsubscribing."));
      return true;
    }
  }
  Serial.println(F("Couldn't find that subscription!"));
  return false;
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
