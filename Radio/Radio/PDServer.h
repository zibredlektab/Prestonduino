#include "Arduino.h"
#include <PrestonDuino.h>
#include <RHReliableDatagram.h>
#include <RH_RF95.h>
#ifndef PDServer_h
#define PDServer_h


/*
 * First byte of message will always be uint8_t describing the type of the message:
 * 0x0 = Command reply
 * 0x1 = PrestonPacket
 * 0x2 = Requesting data, first byte is data type (see bit map below), second byte is whether to subscribe or not
 * 0x3 = ACK from mdr
 * 0x4 = uint16_t data (2 bytes)
 * 0x5 = uint32_t data (4 bytes)
 * 0x6 = char* data (length stored in first index)
 * 
 * 0xF = error, second byte determines error type
 * 
 * Data request bits as follows:
 * 1 = Iris
 * 2 = Focus
 * 4 = Zoom
 * 8 = Aux
 * 16 = Lens Name
 * 32 = Distance
 * 
 * Error states:
 * 0x0 = no error
 * 0x1 = server not responding
 * 0x2 = MDR not responding
 * 0x3 = NAK from MDR
 * 0x4 = ERR from MDR (error data follows)
 * 0xF = other error
 */

struct subscription {
  uint8_t client_address;
  uint8_t data_descriptor;
};

class PDServer {
  private:
    uint8_t channel = 0xA; //hard-coded for now, will eventually be determined by pot
    uint8_t address = 0x0;
    HardwareSerial *ser;
    PrestonDuino *mdr;
    RH_RF95 *driver;
    RHReliableDatagram *manager;
    uint8_t buf[30];
    uint8_t buflen;
    uint8_t lastfrom;
    subscription subs[16];
    uint8_t subcount = 0;
    unsigned long lastupdate = 0;

    uint8_t getData(uint8_t datatype, char* databuf);
    void subscribe(uint8_t addr, uint8_t desc);
    bool unsubscribe(uint8_t addr); // returns true if the subscription was removed
    bool updateSubs(); // returns false if a message failed to send
    uint8_t* replyToArray(command_reply input);

  public:
    PDServer(int chan = 0, HardwareSerial& mdrserial = Serial);
    void onLoop();
    uint8_t getAddress();
  
};


#endif
