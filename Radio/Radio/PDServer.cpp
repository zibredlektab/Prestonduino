#include "PDServer.h"

PDServer::PDServer(int chan, HardwareSerial& mdrSerial) {
  Serial.begin(115200);
  this->channel = chan;
  this->address = chan * 0x10;
  this->ser = &mdrSerial;
  this->mdr = new PrestonDuino(*ser);
  delay(100); // give PD time to connect

  this->mdr->setMDRTimeout(10);

  this->driver = new RH_RF95(SSPIN, INTPIN);
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
  while(!this->manager->waitPacketSent(100));
  if (this->manager->available()) {
    Serial.println(F("Message available"));
    this->buflen = sizeof(this->buf);
    if (this->manager->recvfromAck(this->buf, &this->buflen, &this->lastfrom)) {
      Serial.print(F("got message of "));
      Serial.print(this->buflen);
      Serial.print(F(" characters from 0x"));
      Serial.print(this->lastfrom, HEX);
      Serial.print(": ");
      for (int i = 0; i < this->buflen; i++) {
        Serial.print(this->buf[i]);
        Serial.print(" ");
      }
      Serial.println();
      
      int type = this->buf[0];
      Serial.print("type of message is ");
      Serial.println(type);
      
      switch (type) {
        case 1: {
          // This message contains raw data for the MDR
          Serial.println("This is data for the MDR");
          PrestonPacket *pak = new PrestonPacket(&this->buf[1], this->buflen-2);
          int replystat = this->mdr->sendToMDR(pak);
          if (replystat == -1) {
            Serial.println(F("MDR acknowledged the packet"));
            // MDR acknowledged the packet, didn't send any other data
            /*if(!this->manager->sendtoWait((uint8_t*)3, 1, &this->lastfrom)) { // 0x3 is ack
              Serial.println(F("failed to send reply"));
            } else {
              Serial.println("reply sent");
            }*/
          } else if (replystat > 0) {
            // MDR sent data
            Serial.println(F("got data back from MDR"));
            command_reply reply = this->mdr->getReply();
            if(!this->manager->sendtoWait(this->commandReplyToArray(reply), reply.replystatus + 1, &this->lastfrom)) {
              Serial.println(F("failed to send reply"));
            } else {
              Serial.println("reply sent");
            }
          } else if (replystat == 0) {
            // MDR didn't respond, something has gone wrong
            Serial.println(F("MDR didn't respond..."));
            uint8_t err[] = {0xF, ERR_NOMDR};
            if (!this->manager->sendtoWait(err, 2, this->lastfrom)) {
              Serial.println(F("failed to send reply"));
            } else {
              Serial.println("reply sent");
            }
          }
          break;
        }
        case 2: {
          // This message is requesting processed data
          Serial.println(F("Data is being requested"));
          uint8_t datatype = this->buf[1];
          if (datatype == DATA_UNSUB) {
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
            if(!this->manager->sendto((uint8_t*)tosend, sendlen, this->lastfrom)) {
              Serial.println(F("Failed to send reply"));
            } else {
              Serial.println(F("Reply sent"));
            }
          } else {
            // Client wants to subscribe to this data
            Serial.print(F("Subscribing client at 0x"));
            Serial.print(this->lastfrom, HEX);
            Serial.print(F(" to receive data of type 0b"));
            Serial.print(datatype, BIN);
            Serial.println(F(" every loop"));
            this->subscribe(this->lastfrom, datatype);
          }
          break;
        }
      }
    } else {
      Serial.println("Couldn't recieve message?");
    }
  }

  if(!this->updateSubs()) {
    Serial.println(F("Failed to update all subscriptions"));
  }
}

uint8_t PDServer::getData(uint8_t datatype, char* databuf) {
  uint8_t sendlen = 2; // first two bytes are 0x2 and data type
  databuf[0] = 0x2;
  databuf[1] = datatype;
  if (millis() > this->lastupdate + 5) {
    this->iris = mdr->getAperture();
    this->focus = mdr->getFocusDistance();
    this->zoom = mdr->getFocalLength();
    this->fulllensname = mdr->getLensName();
    Serial.print(F("New lens name: "));
    for (int i = 0; i < this->fulllensname[0] - 1 ; i++) {
      if (i == 0) {
        Serial.print(F("[0x"));
        Serial.print(this->fulllensname[i], HEX);
        Serial.print(F("]"));
      } else {
        Serial.print(this->fulllensname[i]);
      }
      Serial.print(" ");
    }
    Serial.println();

    
    this->lastupdate = millis();
    if (this->iris == 0 || this->focus == 0 || this->zoom == 0) {
      // No data was recieved, return an error
      Serial.println("Didn't get any data from MDR");
      databuf[0] = 0xF;
      databuf[1] = ERR_NOMDR;
      return sendlen;
    }
  }

  Serial.print(F("Getting following data: "));
  
  if (datatype & DATA_IRIS) {
    Serial.print(F("iris "));
    sendlen += snprintf(&databuf[sendlen], 20, "%04lu", (unsigned long)this->iris);
  }
  if (datatype & DATA_FOCUS) {
    Serial.print(F("focus "));
    sendlen += snprintf(&databuf[sendlen], 20, "%08lu", (unsigned long)this->focus);
  }
  if (datatype & DATA_ZOOM) {
    Serial.print(F("zoom "));
    sendlen += snprintf(&databuf[sendlen], 20, "%04lu", (unsigned long)this->zoom);
  }
  if (datatype & DATA_AUX) {
    Serial.print(F("aux (we don't do that yet) "));
  }
  if (datatype & DATA_DIST) {
    Serial.print(F("distance (we don't do that yet) "));
  }
  if (datatype & DATA_NAME) {
    Serial.print(F("name "));
    char* lensname = this->fulllensname;
    
    int i;
    for (i = 0; i < lensname[0] - 1; i++) {
      databuf[sendlen+i] = lensname[i];
    }
    sendlen += i;
  }
  if (sendlen > 0) {
    Serial.println();
    databuf[sendlen++] = '\0';
  } else {
    Serial.println(F("...nothing?"));
  }
  for (int i = 0; i < sendlen; i++) {
    Serial.print(F("0x"));
    Serial.print(databuf[i], HEX);
    Serial.print(F(" "));
  }
  Serial.println();
  
  return sendlen;
}



bool PDServer::updateSubs() {
  char tosend[100];
  uint8_t sendlen = 0;
 
  for (int i = 0; i < 16; i++) {
    sendlen = 0;
    if (this->subs[i].data_descriptor == 0) {
      continue;
    }
    
    if (this->subs[i].keepalive + SUBLIFE < millis()) {
      Serial.print("Subscription of client at 0x");
      Serial.print((this->channel * 0x10) + i, HEX);
      Serial.println(" has expired");
      unsubscribe((this->channel * 0x10)+i);
      continue;
    }
    
    uint8_t desc = this->subs[i].data_descriptor;
    Serial.print(F("Updating client 0x"));
    Serial.print((this->channel * 0x10) + i, HEX);
    Serial.println();
    sendlen = this->getData(desc, tosend);
    Serial.print(F("Sending "));
    Serial.print(sendlen);
    Serial.println(F(" bytes:"));
    for (int i = 0; i < sendlen; i++) {
      Serial.print("0x");
      Serial.print(tosend[i], HEX);
      Serial.print(" ");
    }
    Serial.println();
    if (!this->manager->sendto((char*)tosend, sendlen, (this->channel * 0x10) + i)) {
      Serial.println(F("Failed to send message"));
      return false;
    }
  }
  return true;
}

void PDServer::subscribe(uint8_t addr, uint8_t desc) {
  uint8_t subaddr = addr - (this->channel * 0x10);
  this->subs[subaddr].data_descriptor = desc;
  this->subs[subaddr].keepalive = millis();
  Serial.print("Subscribed client at address 0x");
  Serial.print((this->channel * 0x10) + subaddr, HEX);
  Serial.print(" to data of type ");
  Serial.println(this->subs[subaddr].data_descriptor, BIN);
}

bool PDServer::unsubscribe(uint8_t addr) {
  Serial.print(F("0x"));
  Serial.print(addr, HEX);
  Serial.println(F(" wants to be unsubscribed"));
  
  uint8_t subaddr = addr - (this->channel * 0x10);
  
  if (this->subs[subaddr].data_descriptor) {
    this->subs[subaddr].data_descriptor = 0;
    Serial.println(F("Done unsubscribing"));
    return true;
  } else {
    Serial.println(F("Couldn't find that subscription"));
    return false;
  }
}

uint8_t* PDServer::commandReplyToArray(command_reply input) {
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
