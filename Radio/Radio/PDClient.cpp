#include "PDClient.h"


PDClient::PDClient(int chan) {
  randomSeed(analogRead(0));
  this->driver = new RH_RF95(SSPIN,INTPIN);
  this->channel = chan;
  Serial.print(F("channel is "));
  Serial.println(this->channel, HEX);
  this->server_address = this->channel * 0x10;
  Serial.print(F("server address is 0x"));
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

  
  this->setChannel(chan);
  this->processLensName();
}

bool PDClient::sendMessage(uint8_t type, uint8_t data) {
  uint8_t dataarray[] = {data};
  return this->sendMessage(type, dataarray, 1);
}


bool PDClient::sendMessage(uint8_t msgtype, uint8_t* data, uint8_t datalen) {
  
  uint8_t messagelength = datalen+1; // type + data
  uint8_t tosend[messagelength];
  
  tosend[0] = msgtype;
  for (int i = 0; i < datalen; i++) {
    tosend[i+1] = data[i];
  }


  /*
  Serial.print(F("Sending message: "));
  for (int i = 0; i < messagelength; i++) {
    Serial.print("0x");
    Serial.print(tosend[i], HEX);
    Serial.print(F(" "));
  }
  Serial.print(F(" to server at 0x"));
  Serial.println(this->server_address, HEX);
  */
  
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
        if (this->manager->recvfrom(this->buf, &len, &from)) {
          this->clearError();
          Serial.print(F("Got a reply: "));
          for (int i = 0; i < len; i++) {
            Serial.print(this->buf[i]);
            Serial.print(F(" "));
          }
          Serial.println();
          // Reply was received
          waitforreply = false;
          
          if (this->buf[0] == 0) {
            Serial.println(F("Reply is a CR"));
            // Reply message is a commandreply
            this->arrayToCommandReply(this->buf);
            
          } else if (this->buf[0] == 3) {
            Serial.println(F("MDR acknowledges command"));
            // Response is an MDR ack, no further processing needed
            
          } else if (this->buf[0] > 3 && this->buf[0] != 0xF) {
            Serial.print(F("Reply is data: "));
            // Response is a data set
            for (int i = 0; i < len-1; i++) {
              this->buf[i] = this->buf[i+1]; // shift buf elements by one (no longer need type identifier)
              Serial.print(this->buf[i]);
              Serial.print(F(" "));
            }
  
            len -= 1;
            this->buf[len] = (uint8_t)'\0';
  
            Serial.println();
          } else {
            Serial.print(F("Server sent back an error code: 0x"));
            Serial.println(this->buf[1]);
            this->error(this->buf[1]);
          }
          return true;

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
  
  
}

void PDClient::arrayToCommandReply(byte* input) {
  this->response.replystatus = input[0];
  for (int i = 0; i < input[0]; i++) {
    this->response.data[i] = input[i+1];
  }
}

command_reply PDClient::sendPacket(PrestonPacket *pak) {
  if (this->sendMessage(1, pak->getPacket(), pak->getPacketLen())){
    delete pak;
    return this->response;
  }
}

command_reply PDClient::sendCommand(uint8_t command, uint8_t* args, uint8_t len) {
  PrestonPacket *pak = new PrestonPacket(command, args, len);
  Serial.println("Packet created");
  return this->sendPacket(pak);
}

command_reply PDClient::sendCommand(uint8_t command) {
  PrestonPacket *pak = new PrestonPacket(command);
  return this->sendPacket(pak);
}

void PDClient::onLoop() {
  
  if (!this->final_address) {
    this->findAddress();
    
  } else {
    if (this->manager->available()) {
      Serial.println();
      Serial.print(F("Message available, this long: "));
      uint8_t from;
      this->buflen = sizeof(this->buf);
      if (manager->recvfrom(this->buf, &this->buflen, &from)) {
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


    if (this->timeoflastmessagefromserver + PING < millis()) {
      Serial.println("Haven't heard from the server in a while...");
      this->error(ERR_NOTX);
    }

    if (this->lastsubscriptionattempt + PING < millis()) {
      Serial.println("Time to resubscribe");
      this->subscribe(this->substate);
    }
  
    if (this->errorstate > 0) {
      this->handleErrors();
    }
  }
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
      
    case 0x0:
      // Raw data reply from MDR
      break;
    case 0x1:
      // Single-time data reply
      break;
    case 0x2:
      // Subscription update
      uint8_t datatype = this->buf[1];
      uint8_t index = 2;
      
      if (datatype & DATA_IRIS) {
        Serial.println(F("Received data includes iris"));
        char data[5];
        for (int i = 0; i < 4; i++) {
          data[i] = this->buf[index + i];
          Serial.print(data[i]);
          Serial.print(F(" "));
        }
        data[4] = '\0';
        Serial.println();
        this->iris = atoi(data);
        Serial.print(F("Iris is "));
        Serial.println(this->iris);
        index += 4;
      }

      if (datatype & DATA_FOCUS) {
        Serial.println(F("Received data includes focus"));
        char data[9];
        for (int i = 0; i < 8; i++) {
          data[i] = this->buf[index + i];
          Serial.print(data[i]);
          Serial.print(F(" "));
        }
        data[8] = '\0';
        Serial.println();
        this->focus = strtoul(data, NULL, 10);
        Serial.print(F("Focus is "));
        Serial.println(this->focus);
        index += 8;
        
      }

      if (datatype & DATA_ZOOM) {
        Serial.println(F("Received data includes zoom"));
        char data[5];
        for (int i = 0; i < 4; i++) {
          data[i] = this->buf[index + i];
          Serial.print(data[i]);
          Serial.print(F(" "));
        }
        data[4] = '\0';
        Serial.println();
        this->flength = atoi(data);
        Serial.print(F("Zoom is "));
        Serial.println(this->flength);
        index += 4;
      }

      if (datatype & DATA_NAME) {
        Serial.println(F("Recieved data includes lens name"));
        uint8_t namelen = this->buf[index]; // first byte of name is length of name

        
        for (int i = 0; i < namelen; i++) {
          this->fulllensname[i] = this->buf[index+i];
        }
        
        this->processLensName();

        index += namelen;
      }
      break;

    return;
  }
}


bool PDClient::processLensName() {
  // Format of name is: [asterisk for new lens][length of name][brand]|[series]|[name] [note]
  Serial.print("full name is ");
  Serial.println(this->fulllensname);

  int processfrom = 1;

  if (this->fulllensname[0] == '*') {
    Serial.println("this is a new lens");
    processfrom = 2;
  }
  
  this->lensbrand = &this->fulllensname[processfrom]; // first element of name is length of full name
  this->lensseries = strchr(this->lensbrand, '|') + 1; // find separator between brand and series
  this->lensseries[-1] = '\0'; // null terminator for brand
  
  Serial.print("brand is ");
  Serial.println(this->lensbrand);
  
  this->lensname = strchr(this->lensseries, '|') + 1; // find separator between series and name
  this->lensname[-1] = '\0'; // null terminator for series
  
  Serial.print("series is ");
  Serial.println(this->lensseries);
  
  this->lensnote = strchr(this->lensname, ' ') + 1; // find separator between name and note
  this->lensnote[-1] = '\0'; // null terminator for name

  
  Serial.print("name is ");
  Serial.println(this->lensname);  
  Serial.print("note is ");
  Serial.println(this->lensnote);

  this->abbreviateName();
  
  if (this->isZoom()) {
    long int w, t, i;
    sscanf (this->lensname, "%lu-%lumm %lu", &w, &t, &i);
    this->wfl = w;
    this->tfl = t;
  } else {
    this->wfl = this->flength;
    this->tfl = this->flength;
  }
  
  return true;
}

#define SWAPCT 6

void PDClient::abbreviateName() {
  char* swaps[SWAPCT][2] = {{"Panavision", "PV\0"}, {"Angenieux", "Ang\0"}, {"Servicevision", "SV"}, {"Prime", "Pr\0"}, {"Zoom", "Zm\0"}, {"Other", "\0"}};

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


uint8_t PDClient::getAddress() {
  return this->address;
}

uint8_t PDClient::getChannel() {
  return this->channel;
}


bool PDClient::setChannel(uint8_t newchannel) {
  this->channel = newchannel;
  this->server_address = this->channel * 0x10;
  Serial.print("Server address is 0x");
  Serial.println(this->server_address, HEX);
  this->findAddress();
  return true;
}


uint8_t PDClient::getErrorState() {
  return this->errorstate;
}


void PDClient::findAddress() {
  for (int i = 1; i <= 0xF; i++) {
    Serial.print(F("Trying address 0x"));
    Serial.println(this->server_address + i, HEX);
    this->manager->setRetries(2);
    if (!this->manager->sendtoWait((uint8_t*)"ping", 4, this->server_address + i)) {
      Serial.println(F("Didn't get a response from this address, taking it for myself"));
      // Did not get an ack from this address, so this address is available
      this->address = this->server_address + i;
      this->manager->setThisAddress(this->address);
      this->final_address = true;
      this->manager->setRetries(RETRIES);
      break;
    }
    Serial.println(F("Got a reply, trying the next address"));
  }

  this->manager->setRetries(RETRIES);
  Serial.print(F("My final address is 0x"));
  Serial.println(this->address, HEX);
}




bool PDClient::haveData() {
  return this->errorstate == 0;
}

/*
 * Subscription
 */

bool PDClient::subscribe(uint8_t subtype) {
  Serial.print("Attempting to subscribe to data of type ");
  Serial.println(subtype);
  this->substate = subtype;

  int messagelen = 3;
  
  uint8_t tosend[messagelen];
  
  tosend[0] = 2;
  tosend[1] = subtype;
  tosend[2] = 1;
  
  
  this->lastsubscriptionattempt = millis();

  
  this->manager->setTimeout(1);
  this->manager->sendtoWait(tosend, messagelen, this->server_address);
  this->manager->setTimeout(TIMEOUT);

  return true;
  
  /*
  uint8_t data[2] = {type, 1};
  if (this->sendMessage(2, data, 2)) {

    Serial.print(F("Requested subscription for "));
    Serial.println(type);
    return true;
  } else {
    Serial.println("Subscription request failed");
    return false;
  }*/
}

bool PDClient::subAperture() {
  this->subscribe(1);
}

bool PDClient::subFocus() {
  this->subscribe(2);
}

bool PDClient::subZoom() {
  this->subscribe(4);
}

bool PDClient::unsub() {
  this->waitforreply = false;
  if (this->sendMessage(2, 0)) {
    Serial.println(F("Sent unsubscription request"));
    return true;
  } else {
    Serial.println(F("Failed to send unsub request"));
    return false;
  }
}

/* 
 *  Single-time data retrieval
 */

uint8_t* PDClient::getFIZDataOnce() {
  this->waitforreply = true;
  if (this->sendMessage(2, 7)) {
    // Message acknowledged
    return (uint8_t*)buf;
  }
}

uint32_t PDClient::getFocusDistanceOnce() {
  Serial.println(F("Asking for focus distance"));
  this->waitforreply = true;
  if (this->sendMessage(2, 2)) {
    // Message acknowledged
    return strtoul((const char*)this->buf, NULL, 10);
  }
}

uint16_t PDClient::getApertureOnce() {
  this->waitforreply = true;
  if (this->sendMessage(1, 1)) {
    // Message acknowledged
    return atoi((const char*)this->buf);
  }
}

uint16_t PDClient::getFocalLengthOnce() {
  this->waitforreply = true;
  if (this->sendMessage(1, 4)) {
    // Message acknowledged
    return atoi((const char*)this->buf);
  }
}

char* PDClient::getLensNameOnce() {
  this->waitforreply = true;
  if (this->sendMessage(1, 16)) {
    // Message acknowledged
    return (char*)buf;
  }
}


/*
*  Subscribed data retrieval
*/

uint16_t PDClient::getAperture() {
  return this->iris;
}

uint16_t PDClient::getFocalLength() {
  return this->flength;
}

uint16_t PDClient::getWFl() {
  return this->wfl;
}

uint16_t PDClient::getTFl() {
  return this->tfl;
}

uint32_t PDClient::getFocusDistance() {
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
