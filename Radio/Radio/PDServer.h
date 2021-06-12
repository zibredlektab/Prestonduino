#ifndef PDServer_h
  #define PDServer_h  
  
#include "Arduino.h"
#include <PrestonDuino.h>
#include <RHReliableDatagram.h> // RH_ENABLE_EXPLICIT_RETRY_DEDUP must be redefined as 1 in this file
#include <RH_RF95.h>

#include errorcodes.h
#include datatypes.h

#define REFRESHRATE 5
#define SUBLIFE 6000

#ifdef MOTEINO_M0
  #define SSPIN A2
  #define INTPIN 9
#elif defined(__AVR_ATmega644P__) || defined(__AVR_ATmega1284P__)
  #define SSPIN 4
  #define INTPIN 2
#else
  #define SSPIN SS
  #define INTPIN 2
#endif


/*
 * Last byte of message is a random int, 0-255, to allow for repeated otherwise identical messages
 *  (ie resubscribing the same node to the same data after unsusbscribing)
 * First byte of message will always be uint8_t describing the mode of the message:
 * 0x0 = Raw data request/reply
 * 0x1 = Single-time data request, second byte is data type (see datatypes.h)
 * 0x2 = Subscription data request, second byte same as 0x1
 * 0xF = error, second byte determines error type (see errorcodes.h)
 * 
 */

struct subscription {
  uint8_t data_descriptor = 0;
  unsigned long keepalive = 0;
};

class PDServer {
  private:
    uint8_t channel = 0xA; //hard-coded for now, will eventually be determined by pot
    uint8_t address = 0x0;
    HardwareSerial *ser;
    PrestonDuino *mdr;
    RH_RF95 *driver;
    RHReliableDatagram *manager;
    uint8_t buf[75];
    uint8_t buflen;
    uint8_t lastfrom;
    subscription subs[16];
    unsigned long lastupdate = 0;
    uint16_t iris = 0;
    uint16_t zoom = 0;
    uint32_t focus = 0;
    char* fulllensname;

    uint8_t getData(uint8_t datatype, char* databuf);
    void subscribe(uint8_t addr, uint8_t desc);
    bool unsubscribe(uint8_t addr); // returns true if the subscription was removed
    bool updateSubs(); // returns false if a message failed to send
    uint8_t* commandReplyToArray(command_reply input);

  public:
    PDServer(int chan = 0xA, HardwareSerial& mdrserial = Serial);
    void onLoop();
    uint8_t getAddress();
  
};


#endif
