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

  //this->mdr->setMDRTimeout(10);

  //this->mdr->mode(0x01, 0x40); // take command of the AUX channel (disabled for now)

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

  /*
  this->flash = new Adafruit_InternalFlash(INTERNAL_FLASH_FILESYSTEM_START_ADDR, INTERNAL_FLASH_FILESYSTEM_SIZE);
  Serial.print("Initializing flash..."); // TODO Flash stuff
  flash->begin();
  
  // Open file system on the flash
  if ( !fatfs.begin(flash) ) {
    Serial.println("Error: filesystem does not exist. Please try SdFat_format example to make one.");
    while(1) yield();
  } else {
    Serial.println("done.");
  }*/
  
  Serial.print(F("Done with setup, my address is 0x"));
  Serial.println(this->address, HEX);
  Serial.println("Waiting for clients...");
  
}


void PDServer::onLoop() {

  mdr->onLoop();

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
        Serial.print(" 0x");
        Serial.print(this->buf[i], HEX);
      }
      Serial.println();
      
      int type = this->buf[0];
      Serial.print("type of message is 0x");
      Serial.println(type, HEX);
      
      switch (type) {
        case 0: { // request for a new address
          Serial.print("Sending new address (0x");
          Serial.print(this->nextavailaddress, HEX);
          Serial.print(") to client...");

          uint8_t newaddr[2] = {0x0, this->nextavailaddress};
          if (!this->manager->sendtoWait(newaddr, 2, this->lastfrom)) {
            Serial.println("Failed.");
          } else {
            Serial.println("Done.");
            this->subscribe(this->nextavailaddress, 0x17);
          }
          break;
        }

        case 1: { // This message is registering a client for subscription
          Serial.print(F("This is a subsciption request for data type 0x"));
          uint8_t datatype = this->buf[1];
          Serial.println(datatype, HEX);
          this->subscribe(this->lastfrom, datatype);
          break;
        }

        case 2: { // unsubscription
          Serial.println(F("Actually, unsubscription is being requested."));
          this->unsubscribe(this->lastfrom);
          break;
        }

        case 3: { // "OK" message, or start mapping
          if (this->curmappingav == -1) {
            this->startMap();
          }
          this->curmappingav = this->buf[1];
          this->mapLens(this->curmappingav);
          break;
        }

        case 4: { // "NO" message, or don't map
          this->finishMap();
        }

        default: {
          Serial.println("Unknown message type");
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

  //this->irisToAux(); // (for now)
}

uint8_t PDServer::getData(uint8_t datatype, char* databuf) {
  // Gets up-to-date data from MDR and builds it into an array ready to send to clients
  // Returns length of array

  uint8_t sendlen = 2; // first two bytes are 0x1 and data type
  databuf[0] = 0x1; // message type is 0x1 for data
  databuf[1] = datatype;

  this->iris = mdr->getIris();
  this->focus = mdr->getFocus();
  this->zoom = mdr->getZoom();
  this->fulllensname = mdr->getLensName();

  
  if (0 && this->focus == 0) { //TODO
    // No data was recieved, return an error
    Serial.println("Didn't get any data from MDR");
    databuf[0] = 0xF;
    databuf[1] = ERR_NOMDR;
    return sendlen;
  }


  Serial.print(F("Getting following data: "));
  
  if (datatype & DATA_IRIS) {
    Serial.print(F("iris ("));
    Serial.print(this->iris);
    Serial.print(") ");
    sendlen += snprintf(&databuf[sendlen], 20, "%04lX", (unsigned long)this->iris);

  }
  if (datatype & DATA_FOCUS) {
    Serial.print(F("focus ("));
    Serial.print(this->focus);
    Serial.print(") ");
    sendlen += snprintf(&databuf[sendlen], 20, "%04lX", (unsigned long)this->focus);
  }
  if (datatype & DATA_ZOOM) {
    Serial.print(F("zoom ("));
    Serial.print(this->zoom);
    Serial.print(") ");
    sendlen += snprintf(&databuf[sendlen], 20, "%04lX", (unsigned long)this->zoom);
  }
  if (datatype & DATA_AUX) {
    Serial.print(F("aux (we don't do that yet) "));
  }
  if (datatype & DATA_DIST) {
    Serial.print(F("distance (we don't do that yet) "));
  }
  if (datatype & DATA_NAME) {
    Serial.print(F("name ("));
    this->lensnamelen = this->fulllensname[0] - 1; // lens name includes length of lens name, which we disregard
    Serial.print(this->lensnamelen);
    Serial.print(" characters long, ");


    //strncpy(mdrlens, &this->fulllensname[1], this->lensnamelen); // copy mdr data, sans length of lens name, to local buffer
    //mdrlens[this->lensnamelen] = 0; // null character to terminate string, just in case
    

    //strncpy(curlens, mdrlens, this->lensnamelen);
    /*
    if (strcmp(mdrlens, curlens) != 0) {
      Serial.println("Lens changed");
      // current mdr lens is different from our lens
      this->mapped = false; (todo)
      databuf[sendlen++] = '*'; // send an asterisk before lens name
      for (int i = 0; i <= this->lensnamelen; i++) {
        curlens[i] = '\0'; // null-out previous lens name
      }
      strncpy(curlens, mdrlens, this->lensnamelen); // copy our new lens to current lens name
      curlens[this->lensnamelen] = '\0'; // make absolutely sure it ends with a null
    }*/

    strncpy(&databuf[sendlen], this->fulllensname, this->lensnamelen);
    sendlen += this->lensnamelen;
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

  for (int i = 0x0E; i > 0; i--) {
    sendlen = 0;
    
    if (this->subs[i].data_descriptor == 0) {
      // if the subscription doesn't want any data, skip it
      this->nextavailaddress = (this->channel * 0x10) + i;
      continue;
    }
    
    if (0 && this->subs[i].keepalive + SUBLIFE < millis()) { // TODO Check if sub has expired
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
    Serial.print(F("Sending "));
    Serial.print(sendlen);
    Serial.println(F(" bytes:"));
    for (int i = 0; i < sendlen; i++) {
      Serial.print(" 0x");
      Serial.print(tosend[i], HEX);
    }
    Serial.println();
    
    if (!this->manager->sendto((uint8_t*)tosend, sendlen, (this->channel * 0x10) + i)) {
      Serial.println(F("Failed to send message"));
      return false;
    } else {
      Serial.println("Update sent.");
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
  Serial.print(" to data of type 0x");
  Serial.println(this->subs[subaddr].data_descriptor, HEX);
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
  /*Serial.print("Mapping lens at AV ");
  Serial.print(curav);

  command_reply auxdata = mdr->data(0x58); // 0x58 for Aux, 0x52 for F (for MDR4 testing)
  uint16_t aux = auxdata.data[1] * 0x100;
  aux += auxdata.data[2];

  Serial.print(" to encoder position 0x");
  Serial.println(aux, HEX);
  
  this->lensmap[curav] = aux; // aux encoder position*/
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
  /*command_reply irisdata = mdr->data(0x41); // get encoder position of iris channel
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
  mdr->data(auxdata, 3);*/
}
