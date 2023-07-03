#include "PDClient.h"


PDClient::PDClient(int chan) {
  randomSeed(analogRead(0));
  this->driver = new RH_RF95(SSPIN,INTPIN);
  this->channel = chan;
  Serial.print(F("Starting up on channel "));
  Serial.println(this->channel, HEX);
  this->server_address = this->channel * 0x10;
  Serial.print(F("Server address is 0x"));
  Serial.println(this->server_address, HEX);
  this->address += this->server_address;

  Serial.print(F("My start address is 0x"));
  Serial.println(this->address, HEX);
  
  this->manager = new RHReliableDatagram(*this->driver, this->address);
  Serial.print("Manager created, initializing...");
  if (!this->manager->init()) {
    Serial.println("RH manager init failed");
    this->error(ERR_RADIO);
  } else {
    Serial.println("done");
    this->manager->setRetries(RETRIES);
    this->manager->setTimeout(TIMEOUT);
  }

  this->driver->setPromiscuous(false);

  if (!this->driver->setFrequency(915.0)) {
    Serial.println(F("Driver failed to set frequency"));
    this->error(ERR_RADIO);
  }
  if (!this->driver->setModemConfig(RH_RF95::Bw500Cr45Sf128)) {
    Serial.println(F("Driver failed to configure modem"));
    this->error(ERR_RADIO);
  }

  this->driver->setTxPower(23, false);

  this->setChannel(chan);

  Serial.print("Setting initial name...");
  this->processLensName();
  Serial.println("done");
  Serial.println("Setup complete, waiting for data...");
}

bool PDClient::sendMessage(uint8_t type, uint8_t data) {
  uint8_t dataarray[] = {data};
  return this->sendMessage(type, dataarray, 1);
}


bool PDClient::sendMessage(uint8_t msgtype, uint8_t* data, uint8_t datalen) {
  
  uint8_t messagelength = datalen + 1; // type + data
  uint8_t tosend[messagelength];
  
  tosend[0] = msgtype;
  for (int i = 0; i < datalen; i++) {
    tosend[i+1] = data[i];
  }

  /*Serial.print(F("Sending message: "));
  for (int i = 0; i < messagelength; i++) {
    Serial.print("0x");
    Serial.print(tosend[i], HEX);
    Serial.print(F(" "));
  }
  Serial.print(F(" to server at 0x"));
  Serial.println(this->server_address, HEX);*/
  
  if (this->manager->sendtoWait(tosend, messagelength, this->server_address)) {
    // Got an acknowledgement of our message
    //Serial.println("Message was received");
    if (!this->waitforreply) {
      // Don't need a reply
      //Serial.println(F("No reply needed"));
      return true;
      
    } else {
      // A reply is expected, let's wait for it
      //Serial.println(F("Awaiting reply"));
      if (this->manager->waitAvailableTimeout(2000)) {
        //Serial.println(F("Reply is available"));
        // got a message
        uint8_t len = sizeof(this->buf);
        uint8_t from;
        if (this->manager->recvfromAck((uint8_t*)this->buf, &len, &from)) {
          this->clearError();
          this->timeoflastmessagefromserver = millis();
          Serial.print(F("Got a reply: "));
          for (int i = 0; i < len; i++) {
            Serial.print(" 0x");
            Serial.print(this->buf[i], HEX);
          }
          Serial.println();
          // Reply was received
          this->waitforreply = false;
          this->parseMessage();
          return true;
        } else {
          Serial.println("failed to receive message, even though one is available");
        }
      } else {
        Serial.println("No reply received (timeout)");
        // Reply was not received (timeout)
        this->error(ERR_NOTX);
        return false;
      }
    }


    
  } else {
    this->error(ERR_NOTX); //server not responding
    Serial.println("Message was not received");
    // Did not get an acknowledgement of message
    return false;
  }
  
  return true;
  
}

void PDClient::onLoop() {
  if (this->manager->available()) {
    //Serial.print("\nMessage available, ");
    uint8_t from;
    this->buflen = sizeof(this->buf);
    if (manager->recvfrom((uint8_t*)this->buf, &this->buflen, &from)) {
      //Serial.print(this->buflen);
      //Serial.print(" bytes:");
      this->timeoflastmessagefromserver = millis();
      this->clearError(); // Server is responding
      
      
      /*
      for (int i = 0; i < this->buflen; i++) {
        if (i < 2) {
          Serial.print(" 0x");
          Serial.print(this->buf[i], HEX);
        } else {
          Serial.print((char)this->buf[i]);
        }
      }
      Serial.println();
      */
      this->parseMessage();

    }
  }

  if (millis() > this->timesinceiriscommand + IRISCOMMANDDELAY && this->newiris != this->iris) {
    uint8_t dataset[2] = {highByte(this->newiris), lowByte(this->newiris)};
    this->sendMessage(0x5, dataset, 2);
    this->timesinceiriscommand = millis();
  }

/*
  if (this->timeoflastmessagefromserver + PING < millis()) {
    Serial.println("Haven't heard from the server in a while...");
    this->error(ERR_NOTX);
    this->registerWithServer();
  }

  if (0 && this->lastregistration + PING < millis()) {
    Serial.println("Time to resubscribe");
    this->registerWithServer();
  }

  if (this->errorstate > 0) {
    this->handleErrors();
  }*/

}

void PDClient::parseMessage() {
  // Determine message type
  uint8_t messagetype = this->buf[0];
  //Serial.print(F("Message is of type 0x"));
  //Serial.println(messagetype, HEX);
  switch (messagetype) {
    case 0xF:
      Serial.print("Message is an error, of type 0x");
      Serial.println(this->buf[1], HEX);
      this->error(this->buf[1]);
      break;
      
    case 0x0: {// new address
      this->address = this->buf[1];
      if (this->setAddress(this->buf[1])) { // set our address to the newly specified address
        this->final_address = true;
      }
      Serial.print("Set new address to 0x");
      Serial.println(this->address, HEX);
      break;
    }
    case 0x1:{ // data
      //Serial.println("Reply is data");
      // Response is a data set

      this->buf[this->buflen] = 0;

      uint8_t index = 1;
      
      uint8_t datatype = this->buf[index++];
      //Serial.print("data type of data is 0x");
      //Serial.println(datatype, HEX);
      
      if (datatype & DATA_IRIS) {
        // Iris data is AV number * 100 if the lens is mapped, or raw encoder data if not

        Serial.print("Received data includes iris: ");
        char data[5];
        strncpy(data, &this->buf[index], 4);
        data[4] = 0;
        index += 4;

        sscanf(data, "%4hx", &this->iris);

        Serial.println(this->iris);
      }

      if (datatype & DATA_FOCUS) {
        //Serial.print("Received data includes focus: ");
        char data[5];
        strncpy(data, &this->buf[index], 4);
        data[4] = 0;
        index += 4;
        //Serial.println(data);


        sscanf(data, "%4hx", &this->focus);


        //Serial.print("Focus is ");
        //Serial.println(this->focus);     
      }

      if (datatype & DATA_ZOOM) {
        //Serial.print("Received data includes zoom: ");
        char data[5];
        strncpy(data, &this->buf[index], 4);
        data[4] = 0;
        index += 4;
        //Serial.println(data);


        sscanf(data, "%4hx", &this->zoom);


        //Serial.print("Zoom is ");
        //Serial.println(this->zoom); 
      }

      if (datatype & DATA_NAME) {
        Serial.print("Recieved data includes lens name: ");
        uint8_t namelen = this->buf[index]; // first byte of name is length of name
        for (int i = 0; i < sizeof(this->fulllensname); i++) {
          // null out the name before recording the new one
          this->fulllensname[i] = 0;
        }
        strncpy(this->fulllensname, &this->buf[index + 1], namelen); // skip the name length for processing
        Serial.println(this->fulllensname);
        this->processLensName();
      
      }
      break;
    }

    return;
  }
}


bool PDClient::processLensName() {
  // Format of name is: [status symbol][brand]|[series]|[name] [note]
  // status symbols: '.' = standard mapped lens, '!' = lens needs mapping, '&' = currently mapping, '%' = mapping delayed

  /*Serial.print("full name is ");
  Serial.println(this->fulllensname);

  Serial.print("status symbol is ");
  Serial.print(this->fulllensname[0]);
  Serial.print(", which means ");*/

  switch(this->fulllensname[0]) {
    case '.': {
      //Serial.println("this lens info needs no further processing.");
      this->mapping = false;
      this->mapped = true;
      this->maplater = false;
      break;
    }
    case '!': {
      //Serial.println("this lens needs to be mapped.");
      this->mapped = false;
      this->mapping = false;
      this->maplater = false;
      break;
    }
    case '&': {
      //Serial.println("this lens is currently being mapped.");
      this->mapped = false;
      this->mapping = true;
      this->maplater = false;
      break;
    }
    case '%': {
      //Serial.println("this lens needs to be mapped, but we're not mapping it yet.");
      this->mapping = false;
      this->mapped = false;
      this->maplater = true;
      break;
    }
  }

  int processfrom = 1; // index from which to start processing lens name

  if (strcmp(&this->fulllensname[processfrom], this->curlens) != 0) {
    // This is a new lens
    this->lenschanged = true;
    this->setIris(0x1F4); // reset newiris (to AV 5) now that we're dealing with a mapped lens

    Serial.println("Lens has changed");

    strcpy(this->curlens, &this->fulllensname[processfrom]);

    strcpy(this->lensbrand, this->curlens);
    this->lensseries = strchr(this->lensbrand, '|') + 1; // find separator between brand and series
    this->lensseries[-1] = 0; // null terminator for brand
    
    Serial.print("brand is ");
    Serial.println(this->lensbrand);
    
    this->lensname = strchr(this->lensseries, '|') + 1; // find separator between series and name
    this->lensname[-1] = 0; // null terminator for series
    
    Serial.print("series is ");
    Serial.println(this->lensseries);
    
    this->lensnote = strchr(this->lensname, ' '); // find separator between name and note
    if (this->lensnote == NULL) {
      // Lens name does not always include further information (no serial number / note)
      Serial.println("Lens name does not contain a serial number or note");
      this->lensnote = strchr(this->lensname, 0);
    } else {
      this->lensnote = &this->lensnote[1];
      this->lensnote[-1] = 0; // if there is further information, name needs a null terminator
    }

    
    Serial.print("name is ");
    Serial.println(this->lensname);
    Serial.print("note is ");
    Serial.println(this->lensnote);

    this->abbreviateName();
    
    if (this->isZoom()) {
      sscanf (this->lensname, "%hu-%hu", &this->wfl, &this->tfl);
    } else {
      uint8_t i;
      sscanf(this->lensname, "%hu", &this->zoom); // prime focal length is not reported with standard lens data
      this->zoom *= 100; // need to convert to "high res" focal length format
      this->wfl = this->zoom;
      this->tfl = this->zoom;
    }
  } else {
    this->lenschanged = false;
  }

  return true;
}

#define SWAPCT 5

void PDClient::abbreviateName() {
  const char* swaps[SWAPCT][2] = {{"Panavision", "PV\0"}, {"Angenieux", "Ang\0"}, {"Angeniux", "Ang\0"}, {"Servicevision", "SV"}, {"Other", "\0"}};

  for (int i = 0; i < SWAPCT; i++) {

    char* ptr = strstr(this->lensbrand, swaps[i][0]);
    if (!ptr) ptr = strstr(this->lensseries, swaps[i][0]);
    if (ptr) {
      memcpy(ptr, swaps[i][1], sizeof(swaps[i][1]));
      i--;
    }
  }
}

/*
 * Error handling
 */


bool PDClient::error(uint8_t err) {
  if (this->errorstate == 0 || this->errorstate > err) { // only change error states if we aren't already flagging a more severe error
    this->errorstate = err;
    return true;
  } else {
    return false;
  }
}

void PDClient::clearError() {
  this->errorstate = 0;
}


bool PDClient::handleErrors() {
  Serial.print(F("Error state is "));
  Serial.println(this->errorstate);
  
  return true;
}


uint8_t PDClient::getErrorState() {
  return this->errorstate;
}


uint8_t PDClient::getAddress() {
  return this->address;
}

bool PDClient::setAddress(uint8_t newaddress) {
  Serial.print("Setting address to 0x");
  Serial.print(newaddress, HEX);
  Serial.print("...");

  this->manager->setThisAddress(newaddress);
  Serial.println("done");
  return true;
}

uint8_t PDClient::getChannel() {
  return this->channel;
}


bool PDClient::setChannel(uint8_t newchannel) {
  this->channel = newchannel;
  this->server_address = this->channel * 0x10;
  this->address = this->server_address + 0x0F;
  this->setAddress(this->address);
  this->final_address = false;
  Serial.print("Server address is 0x");
  Serial.println(this->server_address, HEX);
  return this->registerWithServer();
}

bool PDClient::changeChannel(uint8_t newchannel) {
  if (!this->unregisterWithServer()) {
    return false;
  }
  return this->setChannel(newchannel);  
}

bool PDClient::registerWithServer() {
  // Say hello to the server.
  // If this is the first time we're talking, server should reply with next available client address
  // Either way, server should (re)subscribe this client at that address

  if (!this->final_address) {
    this->waitforreply = true;
    if (this->sendMessage(0, 0)) {
      Serial.print("Established contact with server at address 0x");
      Serial.println(this->server_address, HEX);
      return true;
    } else {
      this->error(ERR_NOTX);
      Serial.print("Failed to establish contact with server at address 0x");
      Serial.println(this->server_address, HEX);
      return false;
    }
  } else {
    this->lastregistration = millis();
    this->waitforreply = false;
    if (this->sendMessage(1, 0x17)) {
      Serial.print("Subscribed to data with server at address 0x");
      Serial.println(this->server_address, HEX);
      return true;
    } else {
      Serial.print("Failed to subscribe to data with server at address 0x");
      Serial.println(this->server_address, HEX);
      return false;
    }
  }

}

bool PDClient::unregisterWithServer() {
  this->waitforreply = false;
  if (this->sendMessage(2, 0)) {
    Serial.println(F("Sent unsubscription request"));
    return true;
  } else {
    Serial.println(F("Failed to send unsub request"));
    return false;
  }
}


bool PDClient::haveData() {
  return this->errorstate == 0;
}

/*
 * Subscription
 */
/*
bool PDClient::subscribe(uint8_t subtype) {
  Serial.print("Attempting to subscribe to data of type ");
  Serial.println(subtype);
  this->substate = subtype;

  int messagelen = 3;
  
  uint8_t tosend[messagelen];
  
  tosend[0] = 2;
  tosend[1] = subtype;
  tosend[2] = 1;
  
  
  this->lastregistration = millis();

  
  this->manager->setTimeout(1);
  this->manager->sendtoWait(tosend, messagelen, this->server_address);
  this->manager->setTimeout(TIMEOUT);

  return true;
  
}

bool PDClient::subAperture() {
  this->subscribe(1);
  return true;
}

bool PDClient::subFocus() {
  this->subscribe(2);
  return true;
}

bool PDClient::subZoom() {
  this->subscribe(4);
  return true;
}
*/


/*
*  Subscribed data retrieval
*/

uint16_t PDClient::getIris() {
  return this->iris;
}

uint16_t PDClient::getZoom() {
  return this->zoom;
}

uint16_t PDClient::getWFl() {
  return this->wfl;
}

uint16_t PDClient::getTFl() {
  return this->tfl;
}

uint16_t PDClient::getFocus() {
  return this->focus;
}

char* PDClient::getFullLensName() {
  return this->fulllensname;
}

char* PDClient::getLensBrand() {
  return this->lensbrand;
}

char* PDClient::getLensSeries() {
  return this->lensseries;
}

char* PDClient::getLensName() {
  return this->lensname;
}

char* PDClient::getLensNote() {
  return this->lensnote;
}

bool PDClient::isZoom() {
  if (strchr(this->lensname, '-')) {
    //Serial.println("This is a zoom");
    return true;
  } else {
    //Serial.println("This is not a zoom");
  }

  return false;
}

bool PDClient::didLensChange() {
  return this->lenschanged;
}

bool PDClient::isLensMapped() {
  return this->mapped;
}

bool PDClient::isLensMapping() {
  return this->mapping;
}

bool PDClient::isMapLater() {
  return this->maplater;
}

/* OneRing */

void PDClient::mapLater() {
  Serial.println("Telling server to map later");
  this->sendMessage(4, 0);
  this->maplater = true; // this feels kludgy but it will help
}

void PDClient::startMap() {
  Serial.println("Telling server to start map");
  this->sendMessage(3, '*');
}

void PDClient::mapLens(uint8_t curav) {
  Serial.print("Telling server to save AV ");
  Serial.print(curav);
  this->sendMessage(3, curav);
}

void PDClient::finishMap() {
  Serial.println("Telling server to finish mapping");
  this->sendMessage(4, 0);
}

/* IrisBuddy */
void PDClient::setIris(uint16_t newiris) {
  if (!this->mapped) {
    Serial.print("Setting newiris position to 0x");
    Serial.println(newiris, HEX);
  } else {
    Serial.print("Setting newiris AV to ");
    Serial.println(newiris);
  }
  this->newiris = newiris;
}

void PDClient::changeIris(int16_t delta) {
  int32_t signediris = this->iris;
  int limit;
  if (this->mapped) {
    limit = 900;
  } else {
    limit = 0xFFFF;
  }
  Serial.print("Iris is currently ");
  Serial.print(signediris);
  Serial.print(", requested new iris is ");
  Serial.print(signediris + delta);
  Serial.print(", limit is between 0 and ");
  Serial.print(limit);
  if (signediris + delta > limit) {
    Serial.println(", requested iris is too high");
    this->setIris(limit);
  } else if (signediris + delta < 0) {
    Serial.println(", requested iris is too low");
    this->setIris(0);
  } else {
    Serial.println(", requested iris is okay");
    this->setIris(this->iris + delta);
  }
}
