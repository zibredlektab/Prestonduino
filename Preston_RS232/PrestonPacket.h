/*
  PrestonPacket.h
  Created by Max Batchelder, June 2020.
*/

#ifndef PrestonPacket_h
#define PrestonPacket_h

#define STX 2
#define ETX 3
#define ACK 6
#define NAK 15
#define printline Serial.print(__LINE__); Serial.print(": ");


#include "Arduino.h"

/* [STX][Mode][Len][Data 1]...[Data n][sum 1][sum 2][etx]
   [                    Packet                          ]
        [                Core        ]
                   [     Data        ]
*/

class PrestonPacket {
  private:
    byte mode = 0;
    byte data[100]; // not encoded, 100 for now as arbitrary limit
    int checksum = 0;
    byte packet_ascii[100]; // ascii-encoded packet, currently limited to 100 bytes (arbitrary "big" limit)
    void asciiEncode(byte* input, int len, byte* output);
    void asciiDecode(byte* input, int len, byte* output);
    void compilePacket();
    void parseInput(byte* inputbuffer, int len);
    
    int datalen; // length of data portion of core
    int corelen; // length of core of packet
    int packetlen; // length of full packet, ascii encoded


    

  public:
    PrestonPacket();

    bool packetFromCommand(byte cmd_mode); // For commands that don't take arguments - returns true if successfully created the packet
    bool packetFromCommandWithData(byte cmd_mode, byte* cmd_data, int datalen); // For commands that do take arguments
    bool packetFromBuffer(byte* inputbuffer, int len); // For parsing incoming bytes into a PrestonPacket

    int computeSum(byte* input, int len);
    void zeroOut();

    byte getMode();
    byte* getData();
    int getDataLen();
    byte* getPacket();
    int getPacketLen();
    int getSum();
};

#endif
