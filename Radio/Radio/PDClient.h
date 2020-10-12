#include "Arduino.h"
#include <PrestonPacket.h>
#include <RHReliableDatagram.h>
#include <RH_RF95.h>


#ifndef PDClient_h
#define PDClient_h


#define NUMRETRIES 500

struct command_reply {
  uint8_t replystatus;
  byte* data;
};


class PDClient {
  private:
    uint8_t address = 0xF; // start address
    uint8_t channel = 0x3; // hard-coded for now, eventually determined by pot
    uint8_t server_address = 0x0;
    bool final_address = false;
    RH_RF95 *driver;
    RHReliableDatagram *manager;
    uint8_t errorstate = 0x0;
    unsigned char buf[50];
    uint8_t buflen;
    bool waitforreply = false;
    command_reply response;
    
    bool sendMessage(uint8_t type, uint8_t data);
    bool sendMessage(uint8_t type, uint8_t* data, uint8_t datalen);
    void findAddress();
    void arrayToCommandReply(byte* input);
    bool handleErrors();

    uint16_t iris;
    uint16_t flength;
    uint32_t focus;
    char fulllensname[50];
    char* lensbrand;
    char* lensseries;
    char* lensname;
    char* lensnote;
    
    byte* parseMessage();
    bool processLensName(char* newname, uint8_t len);
    void shiftArrayBytesRight(uint8_t* toshift, uint8_t len, uint8_t num);


  public:
    PDClient(int chan = 0);
    void onLoop();
    command_reply sendPacket(PrestonPacket *pak); // Send a PrestonPacket, get a command_reply in return
    command_reply sendCommand(uint8_t command, uint8_t* args, uint8_t len); // Send an MDR command with arguments, get a command_reply in return
    command_reply sendCommand(uint8_t command); // Same as above, for commands with no arguments
    uint8_t* getFIZDataOnce(); // Ask for FIZ data
    uint32_t getFocusDistance();
    uint16_t getAperture();
    uint16_t getFocalLength();
    uint32_t getFocusDistanceOnce();
    uint16_t getApertureOnce();
    uint16_t getFocalLengthOnce();
    char* getLensNameOnce();
    char* getFullLensName();
    char* getLensBrand();
    char* getLensSeries();
    char* getLensName();
    char* getLensNote();
    bool isZoom();
    uint8_t getAddress();
    uint8_t getChannel();
    void setChannel(uint8_t newchannel);
    uint8_t getErrorState();

    bool subscribe(uint8_t type);
    bool subAperture();
    bool subFocus();
    bool subZoom();
    bool unsub();


  
};


#endif
