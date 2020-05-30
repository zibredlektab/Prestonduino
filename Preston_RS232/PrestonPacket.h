#ifndef PrestonPacket_h
#define PrestonPacket_h

#include "Arduino.h"

class PrestonPacket {
  private:
    byte mode;
    byte* data;
    int checksum;
    byte* packet;
    
    int computeSum();
    byte* compilePacket();
    

  public:
    PrestonPacket(byte cmd_mode, byte* cmd_data);
    byte getMode();
    int setMode(byte cmd_mode);
    byte* getData();
    int setData(byte* cmd_data);
    byte* getPacket();
    int getSum();
};

#endif
