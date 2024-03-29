

#ifndef PDServer_h
  #define PDServer_h
  
#include "Arduino.h"
#include <PrestonDuino.h>
#include <RHReliableDatagram.h> // RH_ENABLE_EXPLICIT_RETRY_DEDUP must be redefined as 1 in this file
#include <RH_RF95.h>
#include <SD.h>


#ifndef ERR_NOTX
  #include <errorcodes.h>
#endif
#include <datatypes.h>

#define SUBLIFE 10000 // how long to keep subscriptions active before purging them
#define UPDATEDELAY 100 // how long to wait between updating subs
#define REPEATSEND 5 // how many times to repeat a motor command to the mdr when the command hasn't changed


#ifdef MOTEINO_M0
  #define SSPIN A2
  #define INTPIN 9
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



/*
 * First byte of message will always be uint8_t describing the mode of the message:
 * 0x0 = Request for new address
 * 0x1 = Subscription data request, second byte is data type (see datatypes.h)
 * 0x2 = Unsubscription request
 * 0x3 = OK (start or continue lens mapping)
 * 0x4 = NO (do not map lens or cancel mapping)
 * 0x5 = New data value for mdr - second byte is data type, then uint16 for new data (only one axis at a time for now, please)
 * 0xF = error, second byte determines error type (see errorcodes.h)
 * 
 */

struct subscription {
  uint8_t data_descriptor = 0; // type of data requested
  unsigned long keepalive = 0; // time of last subscription update
};


class PDServer {
  private:
    
    File lensfile;

    uint8_t channel; // A is default
    uint8_t address; // should always be channel * 0x10
    HardwareSerial *ser;
    PrestonDuino *mdr;
    RH_RF95 *driver;
    RHReliableDatagram *manager;
    char buf[75]; // incoming message buffer from clients
    uint8_t buflen;
    uint8_t lastfrom; // address of sender of last-received message
    subscription subs[16]; // array of currently active clients and what data they want
    uint8_t nextavailaddress = 0xF;

    /* lens data */
    uint16_t iris = 0;
    uint16_t zoom = 0;
    uint16_t focus = 0;
    char* fulllensname;
    uint8_t lensnamelen = 0;
    
    char statussymbol = '.'; // . (normal), ! (lens needs mapping), & (currently mapping), % (mapping delayed)
    char mdrlens[40]; // active lens name as reported by MDR
    char curlens[40]; // last-known active lens name
    char filedirectory[25]; // file path where the current lens map is stored
    char filefullname[25]; // file name the current lens map is stored in
    unsigned long long lastupdate = 0; // last time we sent updated data to subs
    unsigned long long lastmotorcommand = 0; // last time we sent a motor command

    uint8_t getData(uint8_t datatype, char* databuf);
    void subscribe(uint8_t addr, uint8_t desc);
    bool unsubscribe(uint8_t addr); // returns true if the subscription was removed
    bool updateSubs(); // returns false if a message failed to send
    //uint8_t* commandReplyToArray(command_reply input);
  

    /* OneRing */
    bool onering = false;
    bool irisbuddy = true;
    const uint16_t stops[10] = {100, 140, 200, 280, 400, 560, 800, 1100, 1600, 2200}; // standard T stops, index of this array is the corresponding AV number
    const uint16_t ringmap[10] = {0xFEFC, 0xE203, 0xCA70, 0xABC0, 0x8E40, 0x7370, 0x529C, 0x371C, 0x19C0, 0x0000};//{0x0000, 0x19C0, 0x371C, 0x529C, 0x7370, 0x8E40, 0xABC0, 0xCA70, 0xE203, 0xFEFC}; // map of actual encoder positions for linear iris, T1 to T22
    
    uint16_t lensmap[10]; // currently active lens mapping
    bool mapped = false; // do we have a valid lens map?
    bool mapping = false; // are we currently building a lens map?
    bool maplater = false; // are we delaying mapping until later?
    int8_t curmappingav = -1;

    void processLensName();
    void makeStatusSymbol();
    void startMap();
    void mapLater();
    void mapLens(uint8_t curav);
    void finishMap();
    void makePath();
    void irisToAux();

    uint16_t AVToPosition(uint16_t avnumber);
    uint16_t positionToAV(uint16_t position);

  public:
    PDServer(uint8_t chan = 0xA, HardwareSerial& mdrserial = Serial1);
    void onLoop();
    uint8_t getAddress();
    uint8_t getChannel();
    void setAddress(uint8_t newaddress);
    void setChannel(uint8_t newchannel);

    //int sendToMDR(PrestonPacket* pak);
    //command_reply getMDRReply();
  
};


#endif
