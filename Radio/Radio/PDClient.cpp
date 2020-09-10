#include "PDClient.h"


PDClient::PDClient(int chan) {
  this->setChannel(chan);
  this->address += this->server_address;

  Serial.print(F("My start address is 0x"));
  Serial.println(this->address, HEX);
  
  this->manager = new RHReliableDatagram(this->driver, this->address);
  Serial.println(F("Manager created, initializing"));
  if (!this->manager->init()) {
    Serial.println(F("RH manager init failed"));
  } else {
    Serial.println(F("RH manager initialized"));
  }

  if (!this->driver.setFrequency(915.0)) {
    Serial.println(F("Driver failed to set frequency"));
  }
  this->driver.setModemConfig(RH_RF95::Bw500Cr45Sf128);

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

  Serial.print(F("Sending message: "));
  Serial.print(type);
  Serial.print(F(" "));
  for (int i = 0; i < datalen; i++) {
    Serial.print(data[i]);
    Serial.print(F(" "));
  }
  Serial.print(F(" to server at 0x"));
  Serial.println(this->server_address, HEX);
  
  if(this->manager->sendtoWait(tosend, datalen+1, this->server_address)) {
    // Got an acknowledgement of our message
    Serial.println(F("Message was received"));
    if (!this->waitforreply) {
      // Don't need a reply
      Serial.println(F("No reply needed"));
      this->errorstate = 0x0;
      return true;
      
    } else {
      // A reply is expected, let's wait for it
      Serial.println(F("Awaiting reply"));
      if (this->manager->waitAvailableTimeout(2000)) {
        // got a message
        uint8_t len = sizeof(this->buf);
        uint8_t from;
        if (this->manager->recvfrom(this->buf, &len, &from)) {
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
            this->buf[len] = "\0";
  
            Serial.println();
          } else {
            Serial.print(F("Server sent back an error code: 0x"));
            Serial.println(this->buf[1]);
            this->errorstate = this->buf[1];
          }
          return true;

        }
      } else {
        Serial.println(F("No reply received (timeout)"));
        // Reply was not received (timeout)
        this->errorstate = 0x1;
        return false;
      }
    }


    
  } else {
    this->errorstate = 0x1; //server not responding
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


uint8_t* PDClient::getFIZData() {
  this->waitforreply = true;
  if (this->sendMessage(2, 7)) {
    // Message acknowledged
    return (uint8_t*)buf;
  }
}

uint32_t PDClient::getFocusDistance() {
  Serial.println(F("Asking for focus distance"));
  this->waitforreply = true;
  if (this->sendMessage(2, 2)) {
    // Message acknowledged
    return strtoul(buf, NULL, 10);
  }
}

uint16_t PDClient::getAperture() {
  this->waitforreply = true;
  if (this->sendMessage(2, 1)) {
    // Message acknowledged
    Serial.println((char*)buf);
    Serial.println(atoi(buf));
    return strtol(buf, NULL, 10);
  }
}


uint16_t PDClient::getFocalLength() {
  this->waitforreply = true;
  if (this->sendMessage(2, 4)) {
    // Message acknowledged
    return strtoul(buf, NULL, 10);
  }
}

char* PDClient::getLensName() {
  this->waitforreply = true;
  if (this->sendMessage(2, 16)) {
    // Message acknowledged
    return (char*)buf;
  }
}

void PDClient::onLoop() {
  if (!this->final_address) {
    this->findAddress();
  }
  if (this->manager->available()) {
    Serial.println();
    Serial.println(F("Message available"));
    this->buflen = sizeof(this->buf);
    uint8_t from;
    if (manager->recvfromAck(this->buf, &this->buflen, &from)) {
      // Do nothing for now, all we need is the acknowledgment of the ping
    }
  }

  if (this->errorstate > 0) {
    this->handleErrors();
  }
}

bool PDClient::handleErrors() {
  Serial.print(F("Error! 0x"));
  Serial.println(this->errorstate);
  switch (this->errorstate) {
    default:
      this->errorstate = 0;
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
  Serial.print("Server address is 0x");
  Serial.println(this->server_address, HEX);
  this->final_address = false;
}

uint8_t PDClient::getErrorState() {
  return this->errorstate;
}

void PDClient::findAddress() {
  for (int i = 1; i <= 0xF; i++) {
    Serial.print(F("Trying address 0x"));
    Serial.println(this->server_address + i, HEX);
    if (!this->manager->sendtoWait("", 0, this->server_address + i)) {
      Serial.println(F("Didn't get a response from this address, taking it for myself"));
      // Did not get an ack from this address, so this address is available
      this->address = this->server_address + i;
      this->manager->setThisAddress(this->address );
      this->final_address = true;
      break;
    }
    Serial.println(F("Got a reply, trying the next address"));
  }

  Serial.print(F("My final address is 0x"));
  Serial.println(this->address, HEX);
}
