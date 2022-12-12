#include "PDServer.h"

PDServer::PDServer(uint8_t chan, HardwareSerial& mdrSerial) {
  //Serial.begin(115200);
  this->channel = chan;
  Serial.print("Starting server on channel ");
  Serial.println(chan, HEX);
  this->address = chan * 0x10;
  this->ser = &mdrSerial;
  this->mdr = new PrestonDuino(*ser);
  delay(100); // give PD time to connect

  this->flash = new Adafruit_InternalFlash(INTERNAL_FLASH_FILESYSTEM_START_ADDR, INTERNAL_FLASH_FILESYSTEM_SIZE);

  this->mdr->setMDRTimeout(10);

  //this->mdr->mode(0x01, 0x40); // take command of the AUX channel

  this->driver = new RH_RF95(SSPIN, INTPIN);
  this->manager = new RHReliableDatagram(*this->driver, this->address);
  Serial.println(F("Manager created, initializing"));
  int managerstatus = this->manager->init();
  if (!managerstatus) {
    Serial.print(F("RH manager init failed with error: 0x"));
    Serial.println(managerstatus, HEX);
  } else {
    Serial.println(F("RH manager initialized"));
  }

  if (!this->driver->setFrequency(915.0)) {
    Serial.println(F("Driver failed to set frequency"));
  }
  if (!this->driver->setModemConfig(RH_RF95::Bw500Cr45Sf128)) {
    Serial.println(F("Driver failed to configure modem"));
  }


  this->driver->setTxPower(23, false);

  this->driver->setPromiscuous(false);


  Serial.print("Initializing flash...");
  flash->begin();
  
  // Open file system on the flash
  if ( !fatfs.begin(flash) ) {
    Serial.println("Error: filesystem does not exist. Please try SdFat_format example to make one.");
    while(1) yield();
  } else {
    Serial.println("done.");
  }
  
  Serial.print(F("Done with setup, my address is 0x"));
  Serial.println(this->address, HEX);
  Serial.println("Waiting for clients...");
  
}


void PDServer::onLoop() {
  while(!this->manager->waitPacketSent(100));
  if (this->manager->available()) {
    //Serial.println(F("Message available"));
    this->buflen = sizeof(this->buf);
    if (this->manager->recvfromAck(this->buf, &this->buflen, &this->lastfrom)) {
      //Serial.print(F("got message of "));
      //Serial.print(this->buflen);
      //Serial.print(F(" characters from 0x"));
      //Serial.print(this->lastfrom, HEX);
      //Serial.print(": ");
      for (int i = 0; i < this->buflen; i++) {
        //Serial.print(this->buf[i]);
        //Serial.print(" ");
      }
      //Serial.println();
      
      int type = this->buf[0];
      //Serial.print("type of message is ");
      //Serial.println(type);
      
      switch (type) {
        case 0: {
          break;
          /*
          // This message contains raw data for the MDR
          Serial.println("This is data for the MDR");
          PrestonPacket *pak = new PrestonPacket(&this->buf[1], this->buflen-2);
          int replystat = this->mdr->sendToMDR(pak);
          if (replystat == -1) {
            Serial.println(F("MDR acknowledged the packet"));
            // MDR acknowledged the packet, didn't send any other data
            if(!this->manager->sendtoWait((uint8_t*)3, 1, &this->lastfrom)) { // 0x3 is ack
              Serial.println(F("failed to send reply"));
            } else {
              Serial.println("reply sent");
            }
          } else if (replystat > 0) {
            // MDR sent data
            Serial.println(F("got data back from MDR"));
            command_reply reply = this->mdr->getReply();
            if(!this->manager->sendtoWait(this->commandReplyToArray(reply), reply.replystatus + 1, this->lastfrom)) {
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
          */
        }
        case 2: {
          // This message is registering a client for subscription
          //Serial.println(F("Data is being requested"));
          uint8_t datatype = this->buf[1];
          if (datatype == DATA_UNSUB) {
            Serial.println(F("Actually, unsubscription is being requested."));
            this->unsubscribe(this->lastfrom);
          } else {
            if (this->lastfrom & 0xF) {
              // this client needs a new address
              // todo: determine next available address and send to client
            }
            this->subscribe(this->lastfrom, datatype);
            break;
          }

          /*
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
            //Serial.print(F("Subscribing client at 0x"));
            //Serial.print(this->lastfrom, HEX);
            //Serial.print(F(" to receive data of type 0b"));
            //Serial.print(datatype, BIN);
            //Serial.println(F(" every loop"));
            this->subscribe(this->lastfrom, datatype);
          }
          break;
          */
        }

        case 3: {
          // "OK" message, or start mapping
          if (this->curmappingav == -1) {
            this->startMap();
          }
          this->curmappingav = this->buf[1];
          this->mapLens(this->curmappingav);
          break;
        }

        case 4: {
          // "NO" message, or don't map
          this->finishMap();
        }
      }
    } else {
      Serial.println("Couldn't recieve message?");
    }
  }


  if (this->lastupdate + UPDATEDELAY < millis()) {
    // delay sending updates, to allow time for incoming messages to get through
    this->lastupdate = millis();
    if(!this->updateSubs()) {
      Serial.println(F("Failed to update all subscriptions"));
    }
  }

  this->irisToAux();
}

uint8_t PDServer::getData(uint8_t datatype, char* databuf) {
  // Gets up-to-date data from MDR and builds it into an array ready to send to clients
  // Returns length of array

  uint8_t sendlen = 2; // first two bytes are 0x2 and data type
  databuf[0] = 0x2;
  databuf[1] = datatype;
  if (millis() > this->lastmdrupdate + 5) {
    this->iris = mdr->getIris();
    this->focus = mdr->getFocus();
    this->zoom = mdr->getZoom();

    //Serial.print("zoom is ");
    //Serial.println(this->zoom);
    
    this->fulllensname = mdr->getLensName();
    //Serial.print(F("New lens name: "));
    for (int i = 0; i < this->fulllensname[0] - 1 ; i++) {
      if (i == 0) {
        //Serial.print(F("[0x"));
        //Serial.print(this->fulllensname[i], HEX);
        //Serial.print(F("]"));
      } else {
        //Serial.print(this->fulllensname[i]);
      }
      //Serial.print(" ");
    }
    //Serial.println();

    
    this->lastmdrupdate = millis();
    if (this->focus == 0) {
      // No data was recieved, return an error
      Serial.println("Didn't get any data from MDR");
      databuf[0] = 0xF;
      databuf[1] = ERR_NOMDR;
      return sendlen;
    }
  }

  //Serial.print(F("Getting following data: "));
  
  if (datatype & DATA_IRIS) {
    //Serial.print(F("iris "));
    sendlen += snprintf(&databuf[sendlen], 20, "%04lu", (unsigned long)this->iris);
  }
  if (datatype & DATA_FOCUS) {
    //Serial.print(F("focus "));
    sendlen += snprintf(&databuf[sendlen], 20, "%04lu", (unsigned long)this->focus);
  }
  if (datatype & DATA_ZOOM) {
    //Serial.print(F("zoom "));
    sendlen += snprintf(&databuf[sendlen], 20, "%04lu", (unsigned long)this->zoom);
  }
  if (datatype & DATA_AUX) {
    //Serial.print(F("aux (we don't do that yet) "));
  }
  if (datatype & DATA_DIST) {
    //Serial.print(F("distance (we don't do that yet) "));
  }
  if (datatype & DATA_NAME) {
    //Serial.print(F("name "));
    this->lensnamelen = this->fulllensname[0] - 1; // lens name includes length of lens name, which we disregard
    strncpy(mdrlens, &this->fulllensname[1], this->lensnamelen); // copy mdr data, sans length of lens name, to local buffer
    mdrlens[this->lensnamelen] = '\0'; // null character to terminate string
    
    if (strcmp(mdrlens, curlens) != 0) {
      Serial.println("Lens changed");
      // current mdr lens is different from our lens
      this->mapped = false;
      databuf[sendlen++] = '*'; // send an asterisk before lens name
      for (int i = 0; i <= this->lensnamelen; i++) {
        curlens[i] = '\0'; // null-out previous lens name
      }
      strncpy(curlens, mdrlens, this->lensnamelen); // copy our new lens to current lens name
      curlens[this->lensnamelen] = '\0'; // make absolutely sure it ends with a null
    }

    
    int i;
    for (i = 0; i < this->lensnamelen; i++) {
      databuf[sendlen+i] = this->fulllensname[i];
      
    }
    sendlen += i;
  }
  if (sendlen > 0) {
    //Serial.println();
    databuf[sendlen++] = '\0';
  } else {
    //Serial.println(F("...nothing?"));
  }
  for (int i = 0; i < sendlen; i++) {
    //Serial.print(F("0x"));
    //Serial.print(databuf[i], HEX);
    //Serial.print(F(" "));
  }
  //Serial.println();
  
  return sendlen;
}



bool PDServer::updateSubs() {
  char tosend[100];
  uint8_t sendlen = 0;
 
  for (int i = 0; i < 16; i++) {
    sendlen = 0;
    
    if (this->subs[i].data_descriptor == 0) {
      // if the subscription doesn't want any data, skip it
      continue;
    }
    
    if (this->subs[i].keepalive + SUBLIFE < millis()) { // Check if sub has expired
      Serial.print("Subscription of client at 0x");
      Serial.print((this->channel * 0x10) + i, HEX);
      Serial.println(" has expired");
      unsubscribe((this->channel * 0x10)+i);
      continue;
    }
    
    uint8_t desc = this->subs[i].data_descriptor;
    //Serial.print(F("Updating client 0x"));
    //Serial.print((this->channel * 0x10) + i, HEX);
    //Serial.println();
    sendlen = this->getData(desc, tosend);
    //Serial.print(F("Sending "));
    //Serial.print(sendlen);
    //Serial.println(F(" bytes:"));
    for (int i = 0; i < sendlen; i++) {
      //Serial.print("0x");
      //Serial.print(tosend[i], HEX);
      //Serial.print(" ");
    }
    //Serial.println();
    if (!this->manager->sendto((uint8_t*)tosend, sendlen, (this->channel * 0x10) + i)) {
      //Serial.println(F("Failed to send message"));
      return false;
    }
  }
  return true;
}

void PDServer::subscribe(uint8_t addr, uint8_t desc) {
  uint8_t subaddr = addr - (this->channel * 0x10);
  this->subs[subaddr].data_descriptor = desc;
  this->subs[subaddr].keepalive = millis();
  //Serial.print("Subscribed client at address 0x");
  //Serial.print((this->channel * 0x10) + subaddr, HEX);
  //Serial.print(" to data of type ");
  //Serial.println(this->subs[subaddr].data_descriptor, BIN);
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

/*
uint8_t* PDServer::commandReplyToArray(command_reply input) {
  uint8_t output[input.replystatus + 2];
  output[0] = 0;
  output[1] = input.replystatus;
  for (int i = 0; i < input.replystatus+1; i++) {
    output[i+2] = input.data[i];
  }
  return output;
}*/

uint8_t PDServer::getAddress() {
  return this->address;
}

uint8_t PDServer::getChannel() {
  return this->channel;
}

void PDServer::setAddress(uint8_t newaddress) {
  this->address = newaddress;
  Serial.print("new address is 0x");
  Serial.println(this->address, HEX);
}


void PDServer::setChannel(uint8_t newchannel) {
  this->channel = newchannel;
  Serial.print("new channel is ");
  Serial.println(this->channel, HEX);
  setAddress(this->channel * 0x10);
  // todo remove all old subscriptions
}

/*
int PDServer::sendToMDR(PrestonPacket* pak) {
  return this->mdr->sendToMDR(pak);
}

command_reply PDServer::getMDRReply() {
  return this->mdr->getReply();
}*/


/* OneRing */

void PDServer::startMap() {
  Serial.println("Starting map");
  for (int i = 0;i < 10; i++) {
    this->lensmap[i] = 0;
  }

  this->makePath();
  this->fatfs.chdir(); // move to SD root
  this->fatfs.mkdir(this->lenspath, true);
  this->fatfs.chdir(this->lenspath);

  if (fatfs.exists(this->filename)) {
    Serial.println("lens has already been mapped");
    this->lensfile = fatfs.open(this->filename, FILE_READ); // open existing file

    if (this->lensfile) {
      // lens file opened successfully
      for (int i = 0; i < 10; i++) {
        // read through file byte by byte to reconstruct lens table
        this->lensmap[i] = this->lensfile.read();
        this->lensmap[i] += this->lensfile.read() * 0x100;
        Serial.print("[F/");
        Serial.print(this->wholestops[i]);
        Serial.print(": 0x");
        Serial.print(this->lensmap[i], HEX);
        Serial.print("] ");
      }
      Serial.println();
      Serial.println("Finished loading lens data");

      this->lensfile.close();
    } else {
      // lens needs to be mapped

    }
  }
}

void PDServer::makePath() {
  int pipecount = 0;
  int pathlen = 0;

  for (int i = 0; i < this->lensnamelen; i++) {
    if (pipecount < 2) {
      // we are still processing the path, not the filename itself
      if (this->curlens[i] == '|') {
        this->lenspath[i] = '/'; // replace all pipes with directory levels
        pipecount++;
      } else {
        this->lenspath[i] = this->curlens[i];
      }
      pathlen++;
    } else {
      // we are now processing the filename rather than the path
      this->filename[i-pathlen] = this->curlens[i];
    }

    if (this->curlens[i] == '\0') {
      // we have reached the end of the lens name
      return;
    }
  }

  // Remove trailing whitespace
  for (int i = 13; i >= 0; i--) {
    if (this->filename[i] == ' ') {
      this->filename[i] = '\0';
    } else {
      break;
    }
  }

}

void PDServer::mapLens(uint8_t curav) {
  Serial.print("Mapping lens at AV ");
  Serial.print(curav);

  command_reply auxdata = mdr->data(0x58); // 0x58 for Aux, 0x52 for F (for MDR4 testing)
  uint16_t aux = auxdata.data[1] * 0x100;
  aux += auxdata.data[2];

  Serial.print(" to encoder position 0x");
  Serial.println(aux, HEX);
  
  this->lensmap[curav] = aux; // aux encoder position
}

void PDServer::finishMap() {
  Serial.println("Finishing map");
  for (int i = 10; i > this->curmappingav; i--) {
    this->lensmap[i] = 0xFFFF;
  }
  this->mapped = true;
  this->curmappingav = -1;

  for (int i = 0; i < 10; i++) { 
    Serial.print("[F/");
    Serial.print(this->wholestops[i]);
    Serial.print(": 0x");
    Serial.print(this->lensmap[i], HEX);
    Serial.print("] ");
  }
  Serial.println();

  this->lensfile = this->fatfs.open(this->filename, FILE_WRITE);
  int written = this->lensfile.write((uint8_t*)lensmap, 20);
  this->lensfile.close();

  if (written == 20) {
    Serial.println("Wrote lens data to file.");
  } else {
    Serial.println("Failed to write lens data to file");
  }
}


void PDServer::irisToAux() {
  command_reply irisdata = mdr->data(0x41); // get encoder position of iris channel
  uint16_t iris = 0;

  if (irisdata.replystatus > 0) {
    // data was received from mdr
    iris = irisdata.data[1] * 0xFF;
    iris += irisdata.data[2];
  } else if (irisdata.replystatus == 0) {
    Serial.println("Communication error with MDR");
  }

  int ringmapindex = 0;
  for (ringmapindex; ringmapindex < 9; ringmapindex++) { // Find our current position within the ringmap to find our whole AV number
    if (iris >= this->ringmap[ringmapindex] && iris < this->ringmap[ringmapindex+1]) {
      // iris position is greater than ringmapindex and less than the next index...so we have found our place
      break;
    }
  }

  uint8_t avnfloor = ringmapindex; // whole portion of our AV number
  uint8_t avnceil = ringmapindex+1; // next highest AV number

  // Calculate precise position between avnfloor and avnceil to find our fractional AV number
  uint8_t avnfrac = map(iris, this->ringmap[avnfloor], this->ringmap[avnceil], 0, 100);
  double avnumber = avnfloor + (avnfrac/100.0); // complete AV number, with whole and fraction

  // map iris data to aux encoder setting

  uint16_t auxpos;

  if (this->curmappingav == -1 && mapped) {
    // not currently mapping, but the lens has been mapped
    //Serial.print("mapped lens, ");
    auxpos = map(avnumber*100, avnfloor*100, avnceil*100, lensmap[avnfloor], lensmap[avnceil]);
  } else {
    // if mapping is in progress, or if there is no current map, move aux linearly through entire range of encoder (no look up table)
    //Serial.print("unmapped lens, ");
    auxpos = map(avnumber*100, 0, 1000, 0, 0xFFFF);
  }
  
  //Serial.print("Iris pos 0x");
  //Serial.print(iris, HEX);
  //Serial.print("-> Aux pos 0x");
  //Serial.println(auxpos, HEX);

  // set aux motor to encoder setting
  uint8_t auxh = auxpos >> 8;
  uint8_t auxl = auxpos & 0xFF;
  uint8_t auxdata[3] = {0x48, auxh, auxl}; //0x48 is AUX, 0x42 is F
  mdr->data(auxdata, 3);
}
