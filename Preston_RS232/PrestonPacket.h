#ifndef PrestonPacket_h
#define PrestonPacket_h

#include "Arduino.h"

class PrestonPacket {
  private:
    byte mode;
    byte* data;
    int checksum;
    byte* packet_hex;
    byte* packet_ascii; // ascii encoded
    int computeSum(byte* input, int len);
    void asciiEncode(byte* input, byte* output);
    void compilePacket();

    
    int datalen; // length of data portion of core
    int corelen; // length of core of packet
    int packetlen; // length of full packet, ascii encoded
    

  public:
    PrestonPacket(byte cmd_mode, byte* cmd_data, int datalen);
    byte getMode();
    int setMode(byte cmd_mode);
    byte* getData();
    int setData(byte* cmd_data);
    byte* getPacket();
    int getPacketLength();
    int getSum();
};

#endif
