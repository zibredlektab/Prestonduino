#include "PDServer.h"

PDServer::PDServer(uint8_t chan, HardwareSerial& mdrSerial) {

  for (int i = 0; i < 40; i++) {
    this->curlens[i] = 0;
    this->mdrlens[i] = 0;
  }
  //Serial.begin(115200);
  this->channel = chan;
  Serial.print("Starting server on channel ");
  Serial.println(chan, HEX);
  this->address = chan * 0x10;
  this->ser = &mdrSerial;
  this->mdr = new PrestonDuino(*ser);
  delay(100); // give PD time to connect


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

  
  // Open file system on the SD card
  Serial.print("Intializing SD card...");

  // first, disable the radio
  pinMode(SSPIN, OUTPUT);
  digitalWrite(SSPIN, HIGH);

  if ( !SD.begin(19) ) { // 19 was chosen at random and is not the default for Adalogger boards
    Serial.println("SD card error");
    while(1) yield();
  } else {
    Serial.println("done.");
  }
  
  digitalWrite(SSPIN, LOW); // renable radio

  this->irisbuddy = true;
  this->onering = false;

  if (this->onering) this->mdr->mode(0x01, 0x40); // onering mode needs control of the AUX channel (disabled for now)

  if (this->irisbuddy) {
    Serial.println("Setting up IrisBuddy");
    this->mdr->shutUp();
    this->mdr->mode(0x01, 0x01);
    this->mdr->data(0x41); // irisbuddy only cares about iris data, and only encoder counts
    this->mdr->setIris(0x8888);
  }

  Serial.println("Getting initial data");
  char tempdatabuf[100];
  this->getData(0x3F, tempdatabuf);

  Serial.print(F("Done with setup, my address is 0x"));
  Serial.println(this->address, HEX);
  Serial.println("Waiting for clients...");
  
}


void PDServer::onLoop() {

  mdr->onLoop();
  this->processLensName();

  while(!this->manager->waitPacketSent(100));
  if (this->manager->available()) {
    //Serial.println("Message available");
    this->buflen = sizeof(this->buf);
    if (this->manager->recvfromAck((uint8_t*)this->buf, &this->buflen, &this->lastfrom)) {
      /*Serial.print(F("got message of "));
      Serial.print(this->buflen);
      Serial.print(F(" characters from 0x"));
      Serial.print(this->lastfrom, HEX);
      Serial.print(": ");
      for (int i = 0; i < this->buflen; i++) {
        Serial.print(" 0x");
        Serial.print(this->buf[i], HEX);
      }
      Serial.println();*/
      
      int type = this->buf[0];
      
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
            this->subscribe(this->nextavailaddress, 0x11); //TODO, client needs to manually subscribe
          }
          break;
        }

        case 1: { // This message is registering a client for subscription
          Serial.print("This is a subsciption request for data type 0x");
          uint8_t datatype = this->buf[1];
          Serial.println(datatype, HEX);
          this->subscribe(this->lastfrom, datatype);
          break;
        }

        case 2: { // unsubscription
          Serial.println("Unsubscription is being requested.");
          this->unsubscribe(this->lastfrom);
          break;
        }

        case 3: { // "OK" message, or start mapping
          Serial.print("Got an OK message from client 0x");
          Serial.println(this->lastfrom, HEX);
          if (this->buf[1] == '*') { // starting aperture value will be an asterisk
            this->startMap();
          } else {
            this->mapLens(this->curmappingav);
            this->curmappingav = this->buf[1];
          }
          break;
        }

        case 4: { // "NO" message, or don't map
          Serial.print("Got a NO message from client 0x");
          Serial.println(this->lastfrom, HEX);
          this->maplater = 
          this->finishMap();
          break;
        }

        case 5: { // new data
          // TODO determine if we control desired axis & take control if needed
          // for now, assuming iris axis and assuming we have control
          Serial.print("Got a data message from client 0x");
          Serial.println(this->lastfrom, HEX);
          if (this->irisbuddy) {
            char newdata[5];
            uint16_t newiris = 0;
            newiris = this->buf[1] << 8;
            newiris += this->buf[2];

            uint16_t newpos = 0;
            if (this->mapped) {
              newpos = AVToPosition(newiris); // get encoder position from parsed AV
              Serial.print("Moving iris to AV ");
              Serial.println(newpos);
            } else {
              // if mapping is not complete, this data is a raw encoder value so it can be passed straight along
              newpos = newiris;
              Serial.print("Moving iris to position 0x");
              Serial.println(newpos, HEX);
            }

            byte dataset[3] = {0x1, highByte(newpos), lowByte(newpos)};
            mdr->data(dataset, 3); // spin the iris motor to that position
          }

          break;
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

  if (onering) {
    this->irisToAux();
  }
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

  
  if (0 && !this->iris && !this->focus && !this->zoom) { // TODO
    // No data was recieved, return an error
    Serial.println("Didn't get any data from MDR");
    databuf[0] = 0xF;
    databuf[1] = ERR_NOMDR;
    return sendlen;
  }


  //Serial.print(F("Getting following data: "));
  
  if (datatype & DATA_IRIS) {
    //Serial.print(F("iris ("));
    //Serial.print(this->iris);
    //Serial.print(") ");

    // if lens has not been mapped, we send encoder values (this->iris)
    // otherwise, we get the mapped iris value and send that instead
    // uint16_t either way

    if (this->mapped) {
      uint16_t av = this->positionToAV(this->iris);
      sendlen += snprintf(&databuf[sendlen], 20, "%04lX", (unsigned long)av);
    } else {
      sendlen += snprintf(&databuf[sendlen], 20, "%04lX", (unsigned long)this->iris);
    }


  }
  if (datatype & DATA_FOCUS) {
    //Serial.print(F("focus ("));
    //Serial.print(this->focus);
    //Serial.print(") ");
    sendlen += snprintf(&databuf[sendlen], 20, "%04lX", (unsigned long)this->focus);
  }
  if (datatype & DATA_ZOOM) {
    //Serial.print(F("zoom ("));
    //Serial.print(this->zoom);
    //Serial.print(") ");
    sendlen += snprintf(&databuf[sendlen], 20, "%04lX", (unsigned long)this->zoom);
  }
  if (datatype & DATA_AUX) {
    //Serial.print(F("aux (we don't do that yet) "));
  }
  if (datatype & DATA_DIST) {
    //Serial.print(F("distance (we don't do that yet) "));
  }
  if (datatype & DATA_NAME) {
    
    databuf[sendlen++] = this->lensnamelen; // add length of lens name to buffer
    databuf[sendlen++] = this->statussymbol;
    strcpy(&databuf[sendlen], curlens);
    sendlen += this->lensnamelen;
    if (this->newlens) {
      Serial.println("this is no longer a new lens.");
      this->newlens = false;
    }
  }

  if (sendlen > 0) {
    //Serial.println();
    databuf[sendlen++] = '\0';
  } else {
    Serial.println(F("...nothing?"));
  }
  
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
    sendlen = this->getData(desc, tosend);
    /*
    Serial.print(F("Updating client 0x"));
    Serial.print((this->channel * 0x10) + i, HEX);
    Serial.println();
    */
    Serial.print("Sending ");
    Serial.print(sendlen);
    Serial.print(" bytes:");
    for (int i = 0; i < sendlen; i++) {
      Serial.print(" 0x");
      Serial.print(tosend[i], HEX);
    }
    Serial.println();
    
    
    if (!this->manager->sendto((uint8_t*)tosend, sendlen, (this->channel * 0x10) + i)) {
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

  this->mapping = true;

  if (onering) mdr->data(0x49); // Switch MDR over to reading iris and aux encoder positions (irisbuddy mode is already reading the correct info)

  for (int i = 0; i < 10; i++) {
    this->lensmap[i] = 0;
  }
}

void PDServer::processLensName() {
  if (strcmp(this->fulllensname, this->curlens) != 0) {
    // current mdr lens is different from our lens
    // this is a new lens
    Serial.print("Lens changed, from ");
    Serial.print(this->curlens);
    Serial.print(" to ");
    Serial.println(this->fulllensname);
    
    this->newlens = true;
    this->statussymbol = '*';

    strcpy(this->curlens, this->fulllensname); // copy our new lens to current lens name, including the null
    this->lensnamelen = this->mdr->getLensNameLen();

    this->makePath();

    if (SD.exists(this->filefullname)) { // check if this new lens has been mapped
      Serial.println("lens has already been mapped");
      this->lensfile = SD.open(this->filefullname, FILE_READ); // open existing file

      if (this->lensfile) {
        // lens file opened successfully
        for (int i = 0; i < 10; i++) {
          // read through file byte by byte to reconstruct lens table
          this->lensmap[i] = this->lensfile.read();
          this->lensmap[i] += this->lensfile.read() * 0x100;
          Serial.print("[T");
          Serial.print(this->stops[i]);
          Serial.print(": 0x");
          Serial.print(this->lensmap[i], HEX);
          Serial.print("] ");
        }
        Serial.println();
        Serial.println("Finished loading lens data");
        this->lensfile.close();

        this->mapping = false;
        this->mapped = true;

      } else {
        Serial.println("There was an error opening lens data file");
      }
    } else {
      Serial.println("This lens has not been mapped.");
      this->mapped = false;
      this->mapping = false;
    }

    return;
  }

  if (!this->newlens) { // don't change the status symbol until the clients have been notified of the new lens
    this->statussymbol = '.';
    if (!this->mapped) this->statussymbol = '!';
    if (this->mapping) this->statussymbol = '&';
  }

}

void PDServer::makePath() {
  // step through lens name, swapping pipes for directory levels
  // once we're two levels deep, store that as the directory name, and keep processing for the full path

  for (int i = 0; i < sizeof(filedirectory); i++) {
    this->filedirectory[i] = 0;
    this->filefullname[i] = 0;
  }

  int directoryendindex = 0;

  for (int i = 0; i < this->lensnamelen; i++) {
    if (this->curlens[i] == '|') {
      this->filefullname[i] = '/'; // replace all pipes with directory levels
      directoryendindex = i;
    } else {
      this->filefullname[i] = this->curlens[i];
    }

    if (this->curlens[i] == '\0') {
      // we have reached the end of the lens name
      break;
    }

  }

  strncpy(this->filedirectory, this->filefullname, directoryendindex);

  // Remove trailing whitespace
  for (int i = 13; i >= 0; i--) { // lens names are at most 14 characters long
    if (this->filefullname[i] == ' ') {
      this->filefullname[i] = 0;
    } else {
      break;
    }
  }

}

void PDServer::mapLens(uint8_t curav) {
  Serial.print("Mapping lens at AV ");
  Serial.print(curav);

  uint16_t position = 0;
  if (onering) {
    position = mdr->getAux();
  } else if (irisbuddy) {
    position = mdr->getIris();
  }

  Serial.print(" to encoder position 0x");
  Serial.println(position, HEX);
  
  this->lensmap[curav] = position;
}

void PDServer::finishMap() {
  Serial.println("Finishing map");
  for (int i = 10; i >= this->curmappingav; i--) { // the rest of the (unmapped) stops should be assumed to be at the far end of the lens
    this->lensmap[i] = 0xFFFF;
  }
  this->mapped = true;
  this->mapping = false;
  this->curmappingav = -1;

  for (int i = 0; i < 10; i++) { 
    Serial.print("[F/");
    Serial.print(this->stops[i]);
    Serial.print(": 0x");
    Serial.print(this->lensmap[i], HEX);
    Serial.print("] ");
  }
  Serial.println();

  if (!SD.exists(this->filedirectory)) {
    Serial.print("Directory ");
    Serial.print(this->filedirectory);
    Serial.println(" doesn't exist, making directory...");
    SD.mkdir(this->filedirectory);
  }

  this->lensfile = SD.open(this->filefullname, FILE_WRITE);
  int written = this->lensfile.write((uint8_t*)lensmap, 20);
  this->lensfile.close();

  if (written == 20) {
    Serial.println("Wrote lens data to file.");
  } else {
    Serial.print("Wrote an unexpected number of bytes to file (");
    Serial.print(written);
    Serial.println(")");
  }

  if (onering) this->mdr->data(NORMALDATAMODE); // restore normal data operation
}


void PDServer::irisToAux() {
  // turn aux motor to mapped iris position, for OneRing mode

  if (millis() < this->lastmotorcommand + PERIOD) {
    // only send aux commands once per period
    return;    
  }

  uint16_t curiris = mdr->getIris();

  uint8_t avnfloor, avnceil, avnfrac;
  double avnumber;

  if (!mapping) {

    // determine AV number
    int ringmapindex = 0;
    for (ringmapindex; ringmapindex < 9; ringmapindex++) { // Find our current position within the ringmap to find our whole AV number
      if (curiris >= this->stops[ringmapindex] && curiris < this->stops[ringmapindex+1]) {
        // iris position is greater than ringmapindex and less than the next index...so we have found our place
        break;
      }
    }

    avnfloor = ringmapindex; // whole portion of our AV number
    avnceil = ringmapindex+1; // next highest AV number

    // Calculate precise position between avnfloor and avnceil to find our fractional AV number
    avnfrac = map(curiris, this->stops[avnfloor], this->stops[avnceil], 0, 100);
    
    avnumber = avnfloor + (avnfrac/100.0); // complete AV number, with whole and fraction

    Serial.print(", AV number is ");
    Serial.println(avnumber);
  }

  // map iris data to aux encoder setting

  static uint16_t curaux = 0;
  uint16_t newaux = 0;
  static uint8_t repeatsendct = REPEATSEND;

  if (mapped) {
    // The lens has been mapped, aux maps to iris AV number using lut
    Serial.print("mapped lens, ");
    newaux = map(avnumber*100, avnfloor*100, avnceil*100, lensmap[avnfloor], lensmap[avnceil]);

  } else if (!mapped && mapping) {
    // Currenly mapping a lens, aux should directly match iris encoder (no lut)
    Serial.print("mapping lens, ");
    newaux = curiris;

  } else { // default state
    // Not mapping a lens, and don't already have a map, aux maps directly to linear iris position (no lut)
    newaux = map(curiris, 140, 2200, 0, 0xFFFF);
  }
  
  Serial.print("Iris pos 0x");
  Serial.print(mdr->getIris(), HEX);
  Serial.print("-> Aux pos 0x");
  Serial.println(newaux, HEX);

  if (curaux == newaux) {
    //Serial.print("Aux position hasn't changed,");
    if (repeatsendct <= 0) {
      //Serial.println(" not sending to MDR anymore.");
      return;
    } else {
      //Serial.print(" but we'll send it ");
      //Serial.print(repeatsendct);
      //Serial.println(" more time(s) to be safe.");
      repeatsendct--;
    }
  } else {
    repeatsendct = REPEATSEND;
  }

  // set aux motor to encoder setting
  uint8_t auxh = newaux >> 8;
  uint8_t auxl = newaux & 0xFF;
  uint8_t auxdata[3] = {0x48, auxh, auxl}; //0x48 is AUX, 0x42 is F
  mdr->data(auxdata, 3);
  curaux = newaux;
  this->lastmotorcommand = millis();
}

uint16_t PDServer::AVToPosition(uint16_t avnumber) {
  Serial.print("Finding position from AV ");
  Serial.print(avnumber);
  Serial.print("...");
  if (!this->mapped) {
    Serial.println("Lens is not mapped, cannot find position");
    return 0;
  }
  if (avnumber >= 9) { // we don't support above t22
    return this->lensmap[9];
  } else if (avnumber <= 0) { // nor do we support below t1.0
    return this->lensmap[0];
  }

  // find whole AV
  uint16_t avnfloor, avnceil;
  avnfloor = ((avnumber + 1) / 100) * 100;
  avnceil = avnfloor + 100;
  uint16_t newposition = map(avnumber, avnfloor, avnceil, this->lensmap[avnfloor], this->lensmap[avnceil]);

  Serial.print("0x");
  Serial.println(newposition, HEX);
  return newposition;
}

uint16_t PDServer::positionToAV(uint16_t position) {
  Serial.print("Finding AV from position 0x");
  Serial.print(position, HEX);
  Serial.print("...");
  if (!this->mapped) {
    Serial.println("Lens is not mapped, cannot find AV");
    return 0;
  }
  
  uint16_t avnfloor, avnceil, avnfrac;
  uint16_t avnumber;
  // determine AV number
  int lensmapindex = 0;
  for (lensmapindex; lensmapindex < 9; lensmapindex++) { // Find our current position within the ringmap to find our whole AV number
    if (position >= this->lensmap[lensmapindex] && position < this->lensmap[lensmapindex+1]) {
      // iris position is greater than ringmapindex and less than the next index...so we have found our place
      break;
    }
  }

  avnfloor = lensmapindex; // whole portion of our AV number
  avnceil = lensmapindex + 1; // next highest AV number

  // Calculate precise position between avnfloor and avnceil to find our fractional AV number
  avnfrac = map(position, this->lensmap[avnfloor], this->lensmap[avnceil], 0, 100);
  
  avnumber = (avnfloor * 100) + avnfrac; // complete AV number, with whole and fraction

  Serial.println(avnumber);

  return avnumber;
}