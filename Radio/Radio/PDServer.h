#include "Arduino.h"
#include "PrestonPacket.h"
#include "PrestonDuino.h"
#include <RHReliableDatagram.h>
#include <RH_RF95.h>
#ifndef PDServer_h
#define PDServer_h

#define RF95_FREQ 915.0

class PDServer {
  private:
    uint8_t address;
    HardwareSerial *ser;
    PrestonDuino *mdr;
    RH_RF95 driver;
    RHReliableDatagram *manager;
    uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
    uint8_t buflen;

    byte* replyToArray(command_reply input);

  public:
    PDServer(uint8_t addr, HardwareSerial& mdrserial);
    void onLoop();
  
};


#endif
