#include "PDClient.h"


PDClient::PDClient(int chan) {
  this->driver = new RH_RF95(A2,9);
  this->setChannel(chan);
  this->address += this->server_address;

  SerialUSB.print(F("My start address is 0x"));
  SerialUSB.println(this->address, HEX);
  
  this->manager = new RHReliableDatagram(*this->driver, this->address);
  SerialUSB.println(F("Manager created, initializing"));
  if (!this->manager->init()) {
    SerialUSB.println(F("RH manager init failed"));
  } else {
    SerialUSB.println(F("RH manager initialized"));
    this->manager->setRetries(NUMRETRIES);
    this->manager->setTimeout(10);
  }

  //this->driver.setPromiscuous(false);

  if (!this->driver->setFrequency(915.0)) {
    SerialUSB.println(F("Driver failed to set frequency"));
  }
  if (!this->driver->setModemConfig(RH_RF95::Bw500Cr45Sf128)) {
    SerialUSB.println(F("Driver failed to configure modem"));
  }
}

bool PDClient::sendMessage(uint8_t type, uint8_t data) {
  uint8_t dataarray[] = {data};
  return this->sendMessage(type, dataarray, 1);
}

bool PDClient::sendMessage(uint8_t type, uint8_t* data, uint8_t datalen) {
  uint8_t tosend[datalen+1];
  tosend[0] = type;
  for (int i = 0; i <= datalen; i++) {
    tosend[i+1] = data[i];
  }

  SerialUSB.print(F("Sending message: "));
  for (int i = 0; i < datalen+1; i++) {
    SerialUSB.print(tosend[i]);
    SerialUSB.print(F(" "));
  }
  SerialUSB.print(F(" to server at 0x"));
  SerialUSB.println(this->server_address, HEX);
  if(this->manager->sendtoWait(tosend, datalen+1, this->server_address)) {
    // Got an acknowledgement of our message
    SerialUSB.println(F("Message was received"));
    if (!this->waitforreply) {
      // Don't need a reply
      SerialUSB.println(F("No reply needed"));
      this->errorstate = 0x0;
      return true;
      
    } else {
      // A reply is expected, let's wait for it
      SerialUSB.println(F("Awaiting reply"));
      if (this->manager->waitAvailableTimeout(2000)) {
        SerialUSB.println(F("Reply is available"));
        // got a message
        uint8_t len = sizeof(this->buf);
        uint8_t from;
        if (this->manager->recvfrom(this->buf, &len, &from)) {
          SerialUSB.print(F("Got a reply: "));
          for (int i = 0; i < len; i++) {
            SerialUSB.print(this->buf[i]);
            SerialUSB.print(F(" "));
          }
          SerialUSB.println();
          // Reply was received
          waitforreply = false;
          
          if (this->buf[0] == 0) {
            SerialUSB.println(F("Reply is a CR"));
            // Reply message is a commandreply
            this->arrayToCommandReply(this->buf);
            
          } else if (this->buf[0] == 3) {
            SerialUSB.println(F("MDR acknowledges command"));
            // Response is an MDR ack, no further processing needed
            
          } else if (this->buf[0] > 3 && this->buf[0] != 0xF) {
            SerialUSB.print(F("Reply is data: "));
            // Response is a data set
            for (int i = 0; i < len-1; i++) {
              this->buf[i] = this->buf[i+1]; // shift buf elements by one (no longer need type identifier)
              SerialUSB.print(this->buf[i]);
              SerialUSB.print(F(" "));
            }
  
            len -= 1;
            this->buf[len] = (uint8_t)'\0';
  
            SerialUSB.println();
          } else {
            SerialUSB.print(F("Server sent back an error code: 0x"));
            SerialUSB.println(this->buf[1]);
            this->errorstate = this->buf[1];
          }
          return true;

        }
      } else {
        SerialUSB.println(F("No reply received (timeout)"));
        // Reply was not received (timeout)
        this->errorstate = 0x1;
        return false;
      }
    }


    
  } else {
    this->errorstate = 0x1; //server not responding
    SerialUSB.println(F("Message was not received"));
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
    return this->response;
  }
}

command_reply PDClient::sendCommand(uint8_t command, uint8_t* args, uint8_t len) {
  PrestonPacket *pak = new PrestonPacket(command, args, len);
  return this->sendPacket(pak);
}

command_reply PDClient::sendCommand(uint8_t command) {
  PrestonPacket *pak = new PrestonPacket(command);
  return this->sendPacket(pak);
}


uint8_t* PDClient::getFIZDataOnce() {
  this->waitforreply = true;
  if (this->sendMessage(2, 7)) {
    // Message acknowledged
    return (uint8_t*)buf;
  }
}

uint32_t PDClient::getFocusDistanceOnce() {
  SerialUSB.println(F("Asking for focus distance"));
  this->waitforreply = true;
  if (this->sendMessage(2, 2)) {
    // Message acknowledged
    return strtoul((const char*)this->buf, NULL, 10);
  }
}

uint16_t PDClient::getAperture() {
  return this->iris;
}

uint16_t PDClient::getFocalLength() {
  return this->flength;
}

uint32_t PDClient::getFocusDistance() {
  return this->focus;
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


bool PDClient::subscribe(uint8_t type) {
  this->waitforreply = false;
  uint8_t data[2] = {type, 1};
  if (this->sendMessage(2, data, 2)) {
    SerialUSB.print(F("Sent subscription request for "));
    SerialUSB.println(type);
  }
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
    SerialUSB.println(F("Sent unsubscription request"));
  }
}


void PDClient::onLoop() {
  if (!this->final_address) {
    this->findAddress();
  } else {
    if (this->manager->available()) {
      SerialUSB.println();
      SerialUSB.print(F("Message available, this long: "));
      uint8_t from;
      this->buflen = sizeof(this->buf);
      if (manager->recvfrom(this->buf, &this->buflen, &from)) {
        this->errorstate = 0x0;
        SerialUSB.println(this->buflen);
        
        for (int i = 0; i < this->buflen; i++) {
          if (i < 2) {
            SerialUSB.print(F("0x"));
            SerialUSB.print(this->buf[i], HEX);
            SerialUSB.print(F(" "));
          } else {
            SerialUSB.print((char)this->buf[i]);
          }
        }
        
        SerialUSB.println();
        
        this->parseMessage();

      }
    }
  
    if (this->errorstate > 0) {
      this->handleErrors();
    }
  }
}

byte* PDClient::parseMessage() {
  // Determine message type
  uint8_t messagetype = this->buf[0];
  SerialUSB.print(F("Message is of type 0x"));
  SerialUSB.println(messagetype, HEX);
  switch (messagetype) {
    case 0xF:
      SerialUSB.print(F("Message is an error, of type 0x"));
      this->errorstate = this->buf[1];
      SerialUSB.println(this->errorstate);
      return;
      
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
      
      if (datatype & 0b00000001) {
        SerialUSB.println(F("Received data includes iris"));
        byte data[5];
        for (int i = 0; i < 4; i++) {
          data[i] = this->buf[index + i];
          SerialUSB.print(data[i]);
          SerialUSB.print(F(" "));
        }
        data[4] = "\0";
        SerialUSB.println();
        this->iris = atoi(data);
        SerialUSB.print(F("Iris is "));
        SerialUSB.println(this->iris);
        index += 4;
      }

      if (datatype & 0b00000010) {
        SerialUSB.println(F("Received data includes focus"));
        byte data[9];
        for (int i = 0; i < 8; i++) {
          data[i] = this->buf[index + i];
          SerialUSB.print(data[i]);
          SerialUSB.print(F(" "));
        }
        data[8] = "\0";
        SerialUSB.println();
        this->focus = strtoul(data, NULL, 10);
        SerialUSB.print(F("Focus is "));
        SerialUSB.println(this->focus);
        index += 8;
        
      }

      if (datatype & 0b0000100) {
        SerialUSB.println(F("Received data includes zoom"));
        byte data[5];
        for (int i = 0; i < 4; i++) {
          data[i] = this->buf[index + i];
          SerialUSB.print(data[i]);
          SerialUSB.print(F(" "));
        }
        data[4] = "\0";
        SerialUSB.println();
        this->flength = atoi(data);
        SerialUSB.print(F("Zoom is "));
        SerialUSB.println(this->flength);
        index += 4;
      }

      if (datatype & 0b00100000) {
        SerialUSB.println(F("Recieved data includes lens name"));
        uint8_t namelen = atoi(this->buf[index]); // first byte of name is length of name
        byte data[namelen];
        this->processLensName(this->buf[index], namelen);

        index += namelen;
      }
      break;

      

    default:
      return NULL;
  }
}


bool PDClient::processLensName(char* newname, uint8_t len) {
  // Format of name is: [length of name][brand]|[series]|[name] [note]
  
  strncpy(this->fulllensname, newname, len); // copy new name into fullensname
  
  this->lensbrand = this->fulllensname[1]; // first element of name is length of full name
  this->lensseries = strchr(this->lensbrand, '|') + 1; // find separator between brand and series
  strncpy(this->lensseries-1, '\0', 1); // replace separator with null
  this->lensname = strchr(this->lensseries, '|') + 1; // find separator between series and name
  strncpy(this->lensname-1, '\0', 1); // replace separator with null
  this->lensnote = strchr(this->lensname, ' ') + 1; // find separator between name and note
  strncpy(this->lensnote-1, '\0', 1); // replace separator with null


  
  Serial.print(F("Lens brand is "));
  Serial.println(this->lensbrand);
  Serial.print(F("Lens series is "));
  Serial.println(this->lensseries);
  Serial.print(F("Lens name is "));
  Serial.println(this->lensname);
  Serial.print(F("Lens note is "));
  Serial.println(this->lensnote);
  
  return true;
}

bool PDClient::handleErrors() {
  SerialUSB.print(F("Error! 0x"));
  SerialUSB.println(this->errorstate);
  switch (this->errorstate) {
    default:
      //this->errorstate = 0;
      return true;
  }
}


uint8_t PDClient::getAddress() {
  return this->address;
}

uint8_t PDClient::getChannel() {
  return this->channel;
}


void PDClient::setChannel(uint8_t newchannel) {
  this->channel = newchannel;
  this->server_address = this->channel * 0x10;
  SerialUSB.print("Server address is 0x");
  SerialUSB.println(this->server_address, HEX);
  this->final_address = false;
}


uint8_t PDClient::getErrorState() {
  return this->errorstate;
}


void PDClient::findAddress() {
  for (int i = 1; i <= 0xF; i++) {
    SerialUSB.print(F("Trying address 0x"));
    SerialUSB.println(this->server_address + i, HEX);
    this->manager->setRetries(2);
    if (!this->manager->sendtoWait("ping", 4, this->server_address + i)) {
      SerialUSB.println(F("Didn't get a response from this address, taking it for myself"));
      // Did not get an ack from this address, so this address is available
      this->address = this->server_address + i;
      this->manager->setThisAddress(this->address);
      this->final_address = true;
      this->manager->setRetries(NUMRETRIES);
      break;
    }
    SerialUSB.println(F("Got a reply, trying the next address"));
  }

  this->manager->setRetries(NUMRETRIES);
  SerialUSB.print(F("My final address is 0x"));
  SerialUSB.println(this->address, HEX);
}
