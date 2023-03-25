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
  Serial.println(F("Manager created, initializing"));
  if (!this->manager->init()) {
    Serial.println(F("RH manager init failed"));
    this->error(ERR_RADIO);
  } else {
    Serial.println(F("RH manager initialized"));
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
  this->processLensName();
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

  Serial.print(F("Sending message: "));
  for (int i = 0; i < messagelength; i++) {
    Serial.print("0x");
    Serial.print(tosend[i], HEX);
    Serial.print(F(" "));
  }
  Serial.print(F(" to server at 0x"));
  Serial.println(this->server_address, HEX);
  
  if (this->manager->sendtoWait(tosend, messagelength, this->server_address)) {
    // Got an acknowledgement of our message
    Serial.println(F("Message was received"));
    if (!this->waitforreply) {
      // Don't need a reply
      Serial.println(F("No reply needed"));
      return true;
      
    } else {
      // A reply is expected, let's wait for it
      Serial.println(F("Awaiting reply"));
      if (this->manager->waitAvailableTimeout(2000)) {
        Serial.println(F("Reply is available"));
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
        Serial.println(F("No reply received (timeout)"));
        // Reply was not received (timeout)
        this->error(ERR_NOTX);
        return false;
      }
    }


    
  } else {
    this->error(ERR_NOTX); //server not responding
    Serial.println(F("Message was not received"));
    // Did not get an acknowledgement of message
    return false;
  }
  
  return true;
  
}

void PDClient::onLoop() {
  
  if (this->manager->available()) {
    Serial.println();
    Serial.print("Message available, this long: ");
    uint8_t from;
    this->buflen = sizeof(this->buf);
    if (manager->recvfrom((uint8_t*)this->buf, &this->buflen, &from)) {
      Serial.println(this->buflen);
      this->timeoflastmessagefromserver = millis();
      this->clearError(); // Server is responding
      
      for (int i = 0; i < this->buflen; i++) {
        if (i < 2) {
          Serial.print(F("0x"));
          Serial.print(this->buf[i], HEX);
          Serial.print(F(" "));
        } else {
          Serial.print((char)this->buf[i]);
        }
      }
      
      Serial.println();
      
      this->parseMessage();

    }
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
  Serial.print(F("Message is of type 0x"));
  Serial.println(messagetype, HEX);
  switch (messagetype) {
    case 0xF:
      Serial.print(F("Message is an error, of type "));
      Serial.println(this->buf[1]);
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
      Serial.println("Reply is data");
      // Response is a data set

      this->buf[this->buflen] = 0;

      uint8_t index = 1;
      
      uint8_t datatype = this->buf[index++];
      Serial.print("data type of data is 0x");
      Serial.println(datatype, HEX);
      
      if (datatype & DATA_IRIS) {
        Serial.print("Received data includes iris: ");
        char data[5];
        strncpy(data, &this->buf[index], 4);
        data[4] = 0;
        index += 4;
        Serial.println(data);

        //static uint16_t tempiris = 0;

        sscanf(data, "%4hx", &this->iris);

        //this->iris = tempiris;

        Serial.print("Iris is ");
        Serial.println(this->iris);
      }

      if (datatype & DATA_FOCUS) {
        Serial.print("Received data includes focus: ");
        char data[5];
        strncpy(data, &this->buf[index], 4);
        data[4] = 0;
        index += 4;
        Serial.println(data);


        sscanf(data, "%4hx", &this->focus);


        Serial.print("Focus is ");
        Serial.println(this->focus);     
      }

      if (datatype & DATA_ZOOM) {
        Serial.print("Received data includes zoom: ");
        char data[5];
        strncpy(data, &this->buf[index], 4);
        data[4] = 0;
        index += 4;
        Serial.println(data);


        sscanf(data, "%4hx", &this->zoom);


        Serial.print("Zoom is ");
        Serial.println(this->zoom); 
      }

      if (datatype & DATA_NAME) {
        Serial.println(F("Recieved data includes lens name"));
        uint8_t namelen = this->buf[index]; // first byte of name is length of name

        
        for (int i = 0; i < namelen; i++) {
          this->fulllensname[i] = this->buf[index++];
        }
        
        this->processLensName();
      }
      break;
    }

    return;
  }
}


bool PDClient::processLensName() {
  // Format of name is: [asterisk for new lens][length of name][brand]|[series]|[name] [note]
  //Serial.print("full name is ");
  //Serial.println(this->fulllensname);

  int processfrom = 1;

  if (this->fulllensname[0] == '*') {
    Serial.println("this is a new lens");
    this->newlens = true;
    processfrom = 2;
  }
  
  this->lensbrand = &this->fulllensname[processfrom]; // first element of name is length of full name
  this->lensseries = strchr(this->lensbrand, '|') + 1; // find separator between brand and series
  this->lensseries[-1] = '\0'; // null terminator for brand
  
  //Serial.print("brand is ");
  //Serial.println(this->lensbrand);
  
  this->lensname = strchr(this->lensseries, '|') + 1; // find separator between series and name
  this->lensname[-1] = '\0'; // null terminator for series
  
  //Serial.print("series is ");
  //Serial.println(this->lensseries);
  
  this->lensnote = strchr(this->lensname, ' ') + 1; // find separator between name and note
  this->lensnote[-1] = '\0'; // null terminator for name

  
  //Serial.print("name is ");
  //Serial.println(this->lensname);  
  //Serial.print("note is ");
  //Serial.println(this->lensnote);

  this->abbreviateName();
  
  if (this->isZoom()) {
    long int w, t, i;
    sscanf (this->lensname, "%lu-%lumm %lu", &w, &t, &i);
    this->wfl = w;
    this->tfl = t;
  } else {
    this->wfl = this->zoom;
    this->tfl = this->zoom;
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
    Serial.println("This is a zoom");
    return true;
  } else {
    Serial.println("This is not a zoom");
  }

  return false;
}

bool PDClient::isNewLens() {
  return this->newlens;
}

/* OneRing */

void PDClient::mapLater() {
  Serial.println("mapping later");
  this->newlens = false;
  this->sendMessage(4, 0);
}

void PDClient::startMap() {
  this->newlens = false;
}

void PDClient::mapLens(uint8_t curav) {
  Serial.print("mapping now, starting av is ");
  Serial.println(curav);
  this->sendMessage(3, curav);
}

void PDClient::finishMap() {
  Serial.println("Finishing map");
  this->sendMessage(4, 0);
}
