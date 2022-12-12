

#ifndef PDServer_h
  #define PDServer_h
  
#include "Arduino.h"
#include <PrestonDuino.h>
#include <RHReliableDatagram.h> // RH_ENABLE_EXPLICIT_RETRY_DEDUP must be redefined as 1 in this file
#include <RH_RF95.h>
#include <SdFatConfig.h>
#include <sdios.h>
#include <FreeStack.h>
#include <MinimumSerial.h>
#include <SdFat.h>
//#include <BlockDriver.h>
//#include <SysCall.h>
#include <Adafruit_InternalFlash.h>


#ifndef ERR_NOTX
  #include <errorcodes.h>
#endif
#include <datatypes.h>

#define REFRESHRATE 5
#define SUBLIFE 6000 // how long to keep subscriptions active before purging them
#define UPDATEDELAY 10 // how long to wait between updating subs

#define INTERNAL_FLASH_FILESYSTEM_SIZE (64*1024)
#define INTERNAL_FLASH_FILESYSTEM_START_ADDR (0x00040000 - 256 - 0 - INTERNAL_FLASH_FILESYSTEM_SIZE)

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
 * 0x0 = Raw data request/reply (not supported at this time)
 * 0x1 = Single-time data request, second byte is data type (see datatypes.h) (not supported at this time)
 * 0x2 = Subscription data request, second byte same as 0x1
 * 0x3 = OK (usually meaning start lens map or continue lens mapping)
 * 0x4 = NO (do not map lens or cancel mapping)
 * 0xF = error, second byte determines error type (see errorcodes.h)
 * 
 */

struct subscription {
  uint8_t data_descriptor = 0; // type of data requested
  unsigned long keepalive = 0; // time of last subscription update
};


class PDServer {
  private:
    
    Adafruit_InternalFlash *flash;
    FatFileSystem fatfs;
    File lensfile;

    uint8_t channel; // A is default
    uint8_t address; // should always be channel * 0x10
    HardwareSerial *ser;
    PrestonDuino *mdr;
    RH_RF95 *driver;
    RHReliableDatagram *manager;
    uint8_t buf[75]; // incoming message buffer from clients
    uint8_t buflen;
    uint8_t lastfrom; // address of sender of last-received message
    subscription subs[16]; // array of currently active clients and what data they want

    /* lens data */
    uint16_t iris = 0;
    uint16_t zoom = 0;
    uint16_t focus = 0;
    char* fulllensname;
    uint8_t lensnamelen = 0;
    
    char mdrlens[40]; // active lens name as reported by MDR
    char curlens[40]; // last-known active lens name
    char lenspath[25]; // file path where the current lens map is stored
    char filename[25]; // file name the current lens map is stored in
    unsigned long long lastmdrupdate = 0; // last time we asked MDR for updated data
    unsigned long long lastupdate = 0; // last time we sent updated data to subs

    uint8_t getData(uint8_t datatype, char* databuf);
    void subscribe(uint8_t addr, uint8_t desc);
    bool unsubscribe(uint8_t addr); // returns true if the subscription was removed
    bool updateSubs(); // returns false if a message failed to send
    //uint8_t* commandReplyToArray(command_reply input);
  

    /* OneRing */
    const char wholestops[10][4] = {"1.0", "1.4", "2.0", "2.8", "4.0", "5.6", "8.0", "11\0", "16\0", "22\0"};
    const uint16_t ringmap[10] = {0x0000, 0x19C0, 0x371C, 0x529C, 0x7370, 0x8E40, 0xABC0, 0xCA70, 0xE203, 0xFEFC}; // map of actual encoder positions for linear iris, t/1 to t/22
    uint16_t lensmap[10]; // currently active lens mapping
    bool mapped = false;
    int8_t curmappingav = -1;

    void startMap();
    void mapLens(uint8_t curav);
    void finishMap();
    void makePath();
    void irisToAux();

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
