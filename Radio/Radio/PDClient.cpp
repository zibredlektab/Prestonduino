#include "PDClient.h"


PDClient::PDClient(uint8_t addr) {
  this->address = addr;

  if (!this->driver.setFrequency(RF95_FREQ)) {
    Serial.println("setFrequency failed");
    while (1);
  }
  
  this->manager = new RHReliableDatagram(this->driver, this->address);
  if (!this->manager->init()) {
    Serial.println("RH manager init failed");
  }
}

bool PDClient::sendMessage(uint8_t* data, uint8_t datalen) {
  if(this->manager->sendtoWait(data, datalen, this->server_address)) {
    // Got an acknowledgement of our message
    
    if (!waitforreply) {
      // Don't need a reply
      return true;
    } else {
      // A reply is expected, let's wait for it
      uint8_t len = sizeof(this->buf);
      uint8_t from;
      if (this->manager->recvfromAckTimeout(this->buf, &len, 2000, &from)) {
        // Reply was received
        this->arrayToCommandReply(this->buf);
        waitforreply = false;
        return true;
      } else {
        // Reply was not received
        waitforreply = false;
        return false;
      }
    }
  } else {
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
  if (this->sendMessage(pak->getPacket(), pak->getPacketLen())){
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
