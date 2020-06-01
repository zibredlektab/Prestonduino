#ifndef PrestonPacket_h
#define PrestonPacket_h

#include "Arduino.h"

class PrestonPacket {
  private:
    byte mode;
    byte* data; // pointer to array containing data
    int checksum;
    byte packet_ascii[100]; // ascii-encoded packet, currently limited to 100 bytes (arbitrary "big" limit)
    int computeSum(byte* input, int len);
    void asciiEncode(byte* input, int len, byte* output);
    void asciiDecode(byte* input, int len, byte* output);
    void compilePacket();
    void parseInput(byte* inputbuffer, int len);
    
    int datalen; // length of data portion of core
    int corelen; // length of core of packet
    int packetlen; // length of full packet, ascii encoded
    

  public:
    PrestonPacket(byte cmd_mode, byte* cmd_data, int datalen);
    PrestonPacket(byte* inputbuffer, int len);
    byte getMode();
    int setMode(byte cmd_mode);
    byte* getData();
    int setData(byte* cmd_data);
    byte* getPacket();
    int getPacketLength();
    int getSum();
};

#endif
