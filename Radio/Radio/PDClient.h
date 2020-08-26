#include "Arduino.h"
#include <PrestonPacket.h>
#include <PrestonDuino.h>
#include <RHReliableDatagram.h>
#include <RH_RF95.h>
#ifndef PDClient_h
#define PDClient_h


#define RF95_FREQ 915.0

class PDClient {
  private:
    uint8_t address;
    uint8_t server_address = 0; // hard-coded for now
    RH_RF95 driver;
    RHReliableDatagram *manager;
    uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
    uint8_t buflen;
    bool waitforreply = false;
    bool sendMessage(uint8_t type, uint8_t data);
    bool sendMessage(uint8_t type, uint8_t* data, uint8_t datalen);
    command_reply response;
    byte rcvdata[50];
    
    void arrayToCommandReply(byte* input);


  public:
    PDClient(uint8_t addr);
    command_reply sendPacket(PrestonPacket *pak); // Send a PrestonPacket, get a command_reply in return
    command_reply sendCommand(uint8_t command, uint8_t* args, uint8_t len); // Send an MDR command with arguments, get a command_reply in return
    command_reply sendCommand(uint8_t command); // Same as above, for commands with no arguments
    uint8_t* getFIZData(); // Ask for FIZ data
    uint32_t getFocusDistance();
    uint16_t getAperture();
    uint16_t getFocalLength();
    char* getLensName();


  
};


#endif
