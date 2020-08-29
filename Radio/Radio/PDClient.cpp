#include "PDClient.h"


PDClient::PDClient() {
  this->server_address = this->channel * 0x10;
  this->address += this->server_address;

  Serial.print("My start address is 0x");
  Serial.println(this->address, HEX);
  
  this->manager = new RHReliableDatagram(this->driver, this->address);
  Serial.println("Manager created, initializing");
  if (!this->manager->init()) {
    Serial.println("RH manager init failed");
  } else {
    Serial.println("RH manager initialized");
  }

  if (!this->driver.setFrequency(915.0)) {
    //Serial.println("Driver failed to set frequency");
  }
  this->driver.setModemConfig(RH_RF95::Bw500Cr45Sf128);

  this->findAddress();
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

  //Serial.print("Sending message: ");
  //Serial.print(type);
  //Serial.print(" ");
  for (int i = 0; i < datalen; i++) {
    //Serial.print(data[i]);
    //Serial.print(" ");
  }
  //Serial.println();
  
  if(this->manager->sendtoWait(tosend, datalen+1, this->server_address)) {
    // Got an acknowledgement of our message
    //Serial.println("Message was received");
    if (!this->waitforreply) {
      // Don't need a reply
      //Serial.println("No reply needed");
      return true;
      
    } else {
      // A reply is expected, let's wait for it
      uint8_t len = sizeof(this->buf);
      uint8_t from;
      //Serial.println("Awaiting reply");
      if (this->manager->recvfromAckTimeout(this->buf, &len, 2000, &from)) {
        //Serial.print("Got a reply: ");
        for (int i = 0; i < len; i++) {
          //Serial.print(this->buf[i]);
          //Serial.print(" ");
        }
        //Serial.println();
        // Reply was received
        waitforreply = false;
        
        if (this->buf[0] == 0) {
          //Serial.println("Reply is a CR");
          // Reply message is a commandreply
          this->arrayToCommandReply(this->buf);
          
        } else if (this->buf[0] == 3) {
          //Serial.println("MDR acknowledges command");
          // Response is an MDR ack, no further processing needed
          
        } else if (this->buf[0] > 3) {
          //Serial.print("Reply is data: ");
          // Response is a data set
          for (int i = 0; i < len; i++) {
            this->rcvdata[i] = this->buf[i];
            //Serial.print(this->rcvdata[i]);
            //Serial.print(" ");
          }
          //Serial.println();
        }
        return true;
        
      } else {
        //Serial.println("No reply received (timeout)");
        // Reply was not received (timeout)
        return false;
      }
    }


    
  } else {
    //Serial.println("Message was not received");
    // Did not get an acknowledgement of message
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
  if (this->sendMessage(2, 7)) {
    // Message acknowledged
    return (uint8_t*)rcvdata;
  }
}

uint32_t PDClient::getFocusDistance() {
  //Serial.println("Asking for focus distance");
  this->waitforreply = true;
  if (this->sendMessage(2, 2)) {
    // Message acknowledged
    return strtoul(rcvdata, NULL, 10);
  }
}

uint16_t PDClient::getAperture() {
  this->waitforreply = true;
  if (this->sendMessage(2, 1)) {
    // Message acknowledged
    return strtoul(rcvdata, NULL, 10);
  }
}

uint16_t PDClient::getFocalLength() {
  this->waitforreply = true;
  if (this->sendMessage(2, 4)) {
    // Message acknowledged
    return strtoul(rcvdata, NULL, 10);
  }
}

char* PDClient::getLensName() {
  this->waitforreply = true;
  if (this->sendMessage(2, 16)) {
    // Message acknowledged
    return (char*)rcvdata;
  }
}

void PDClient::onLoop() {
  if (!this->final_address) {
    this->findAddress();
  }
  if (this->manager->available()) {
    Serial.println();
    Serial.println("Message available");
    this->buflen = sizeof(this->buf);
    uint8_t from;
    if (manager->recvfromAck(this->buf, &this->buflen, &from)) {
      // Do nothing for now, all we need is the acknowledgment of the ping
    }
  }
}

uint8_t PDClient::getAddress() {
  return this->address;
}

void PDClient::findAddress() {
  for (int i = 1; i <= 0xF; i++) {
    Serial.print("Trying address 0x");
    Serial.println(this->server_address + i, HEX);
    if (!this->manager->sendtoWait("", 0, this->server_address + i)) {
      Serial.println("Didn't get a response from this address, taking it for myself");
      // Did not get an ack from this address, so this address is available
      this->address = this->server_address + i;
      this->manager->setThisAddress(this->address );
      this->final_address = true;
      break;
    }
    Serial.println("Got a reply, trying the next address");
  }

  Serial.print("My final address is 0x");
  Serial.println(this->address, HEX);
}
