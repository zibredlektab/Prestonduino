#include "Arduino.h"
#include <PrestonPacket.h>
#include <RHReliableDatagram.h>
#include <RH_RF95.h>
#include <errorcodes.h>
#include <datatypes.h>


#ifndef PDClient_h
#define PDClient_h


#define RETRIES 0
#define PING 5000

#ifdef MOTEINO_M0
  #define SSPIN A2
  #define INTPIN 9
#elif defined(ARDUINO_SAMD_ZERO)
  #define SSPIN 8
  #define INTPIN 3
#elif defined(__AVR_ATmega644P__) || defined(__AVR_ATmega1284P__)
  #define SSPIN 4
  #define INTPIN 2
#else
  #define SSPIN SS
  #define INTPIN 2
#endif


struct command_reply {
  uint8_t replystatus;
  byte* data;
};

struct message {
  uint8_t type;
  uint8_t data[3]; //arbitrary
  uint8_t datalen;
};


class PDClient {
  private:
    uint8_t address = 0xF; // start address
    uint8_t channel;
    uint8_t server_address;
    bool final_address = false;
    RH_RF95 *driver;
    RHReliableDatagram *manager;
    uint8_t errorstate = 0b11; //start with the assumption that the server is not responding and we have no data
    unsigned char buf[75];
    uint8_t buflen;
    bool waitforreply = false;
    command_reply response;
    unsigned long timeoflastmessagefromserver = 0;
    unsigned long lastping = 0;
    message lastsent;
    uint8_t substate = 0;
    
    bool sendMessage(uint8_t type, uint8_t data);
    bool sendMessage(uint8_t type, uint8_t* data, uint8_t datalen);
    bool resend();
    void findAddress();
    void arrayToCommandReply(byte* input);
    bool handleErrors();

    uint16_t iris = 620;
    uint16_t flength = 240;
    uint32_t focus = 12863;
    uint16_t wfl = 0;
    uint16_t tfl = 0;
    char fulllensname[50] = "0Angenieux|Optimo|24-290mm 630";
    char* lensbrand;
    char* lensseries;
    char* lensname;
    char* lensnote;
    
    void parseMessage();
    bool processLensName();
    void shiftArrayBytesRight(uint8_t* toshift, uint8_t len, uint8_t num);
    bool haveData();
    bool error(uint8_t err);
    void clearError();


  public:
    PDClient(int chan = 0xA);
    void onLoop();
    command_reply sendPacket(PrestonPacket *pak); // Send a PrestonPacket, get a command_reply in return
    command_reply sendCommand(uint8_t command, uint8_t* args, uint8_t len); // Send an MDR command with arguments, get a command_reply in return
    command_reply sendCommand(uint8_t command); // Same as above, for commands with no arguments
    uint8_t* getFIZDataOnce(); // Ask for FIZ data
    uint32_t getFocusDistance();
    uint16_t getAperture();
    uint16_t getFocalLength();
    uint16_t getWFl();
    uint16_t getTFl();
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
    bool setChannel(uint8_t newchannel);
    uint8_t getErrorState();

    bool subscribe(uint8_t type);
    bool subAperture();
    bool subFocus();
    bool subZoom();
    bool unsub();

  
};


#endif
