#include "Arduino.h"
//#include <PrestonPacket.h>
#include <RHReliableDatagram.h>
#include <RH_RF95.h>
#ifndef ERR_NOTX
  #include <errorcodes.h>
#endif
#include <datatypes.h>


#ifndef PDClient_h
#define PDClient_h


#define RETRIES 20 // for radiohead sending attempts
#define TIMEOUT 30 // for radiohead sending attempts
#define PING 3000 // period between resubscriptions, and period of assumed server timeout

#ifdef MOTEINO_M0
  #define SSPIN A2
//  #define INTPIN 9
#elif defined(ARDUINO_SAMD_ZERO)
  #define SSPIN 8
  #define INTPIN 3
#elif defined(__AVR_ATmega644P__) || defined(__AVR_ATmega1284P__)
  #define SSPIN 4
  #define INTPIN 2
#elif defined(ADAFRUIT_FEATHER_M0) || defined(ADAFRUIT_FEATHER_M0_EXPRESS) || defined(ARDUINO_SAMD_FEATHER_M0)
  // Feather M0 w/Radio
  #define SSPIN 8
  #define INTPIN 3
#else
  #define SSPIN SS
  #define INTPIN 2
#endif


struct command_reply {
  uint8_t replystatus;
  byte* data;
};

//struct message {
//  uint8_t type;
//  uint8_t data[3]; //arbitrary
//  uint8_t datalen;
//};


class PDClient {
  private:
    uint8_t address = 0xF; // start address
    uint8_t channel;
    uint8_t server_address;
    bool final_address = false; // has the server assigned us an address?
    RH_RF95 *driver;
    RHReliableDatagram *manager;
    uint8_t errorstate = 0b11; // start with the assumption that the server is not responding and we have no data
    unsigned char buf[75]; // incoming message (from server) buffer
    uint8_t buflen;
    bool waitforreply = false; // does the message we are currently sending need a reply from the server?
    command_reply response; // holds the most recently received response from the server
    unsigned long timeoflastmessagefromserver = 0;
    unsigned long lastregistration = 0;
    uint8_t substate = 0; // data type currently subscribed to

    // Lens data
    uint16_t iris = 370;
    uint16_t zoom = 78;
    uint32_t focus = 8873;
    uint16_t wfl = 0; // wide end of zoom range
    uint16_t tfl = 0; // tele end of zoom range
    char fulllensname[50] = "0Panavision|Primo Zoom|24-240mm 630ABC";
    char* lensbrand;
    char* lensseries;
    char* lensname;
    char* lensnote;
    bool newlens = false;
    
    bool haveData(); // do we have valid lens data from the server?

    bool sendMessage(uint8_t type, uint8_t data); // send a single datum to the server
    bool sendMessage(uint8_t type, uint8_t* data, uint8_t datalen); // send a data array to the server
    void arrayToCommandReply(byte* input); // convert an incoming char buffer to a structured command reply
    void parseMessage();
    bool processLensName(); // split up lens name into component parts
    void abbreviateName(); // shorten lengthy lens brands

    bool setAddress(uint8_t newAddress);

    bool registerWithServer();
    bool unregisterWithServer();

    // Error handling
    bool error(uint8_t err);
    void clearError();
    bool handleErrors();


  public:
    PDClient(int chan = 0xA);
    void onLoop();

    uint16_t getFocus();
    uint16_t getIris();
    uint16_t getZoom();
    uint16_t getWFl();
    uint16_t getTFl();
    char* getFullLensName();
    char* getLensBrand();
    char* getLensSeries();
    char* getLensName();
    char* getLensNote();
    bool isNewLens();
    bool isZoom();

    uint8_t getAddress();
    uint8_t getChannel();
    bool setChannel(uint8_t newchannel);
    bool changeChannel(uint8_t newchannel); // unregister with old server before switching
    uint8_t getErrorState();

    void mapLater();
    void startMap();
    void mapLens(uint8_t curav);
    void finishMap();

  
};


#endif
