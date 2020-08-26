#include "Arduino.h"
#include <PrestonPacket.h>
#include <PrestonDuino.h>
#include <RHReliableDatagram.h>
#include <RH_RF95.h>
#ifndef PDServer_h
#define PDServer_h

/*
 * First byte of message will always be uint8_t describing the type of the message:
 * 0x0 = Command reply
 * 0x1 = PrestonPacket
 * 0x2 = Requesting data (see bit map for data byte)
 * 0x3 = ack from mdr
 * 0x4 = uint16_t data (2 bytes)
 * 0x5 = uint32_t data (4 bytes)
 * 0x6 = char* data (length stored in first index)
 * 
 * Bits as follows:
 * 1 - Iris
 * 2 - Focus
 * 4 - Zoom
 * 8 - Aux
 * 16 - Lens Name
 */

class PDServer {
  private:
    uint8_t address;
    HardwareSerial *ser;
    PrestonDuino *mdr;
    RH_RF95 driver;
    RHReliableDatagram *manager;
    uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
    uint8_t buflen;

    uint8_t* replyToArray(command_reply input);

  public:
    PDServer(uint8_t addr, HardwareSerial& mdrserial);
    void onLoop();
  
};


#endif
