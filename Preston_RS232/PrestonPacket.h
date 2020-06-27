/*
  PrestonPacket.h
  Created by Max Batchelder, June 2020.
*/

#ifndef PrestonPacket_h
#define PrestonPacket_h

#define STX 0x02
#define ETX 0x03
#define ACK 0x06
#define NAK 0x15


#include "Arduino.h"

class PrestonPacket {
  private:
    byte mode;
    byte data[10]; // not encoded, 10 for now as arbitrary limit
    int checksum;
    byte packet_ascii[50]; // ascii-encoded packet, currently limited to 50 bytes (arbitrary "big" limit)
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
    int getDataLen();
    byte* getPacket();
    int getPacketLen();
    int getSum();
    unsigned int getFocusDistance();
};

#endif
