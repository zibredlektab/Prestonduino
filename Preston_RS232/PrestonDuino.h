/*
  PrestonDuino.h - Library for interfacing with a Preston MDR 2/3/4 over Arduio Serial port
  Created by Max Batchelder, June 2020.
*/

#include "Arduino.h"
#include "PrestonPacket.h"
#ifndef PrestonDuino_h
#define PrestonDuino_h

class PrestonDuino {
  private:
    HardwareSerial *ser;
    byte rcvdata[100]; // buffer for incoming data from MDR (100 is arbitrary but should be large enough)
    bool rcvreadytoprocess = false; // flag that we have received a complete packet from the MDR to process
    int rcvdatalen = 0; // length of incoming packet info
    PrestonPacket* rcvpacket; // most recently received packet from MDR
    
    unsigned long time_now = 0; // used for scheduling packets
    int period = 6; // milliseconds to wait between sending packets (lens data is updated every 6ms)

    
  public:
    PrestonDuino(HardwareSerial& serial);
    int sendToMDR(PrestonPacket packet); // 1 if ACK, 0 if ERR, -1 if NAK. Does not retry on NAK
    int sendToMDR(PrestonPacket packet, bool retry); // same as above, with option to retry on NAK
    int rcvFromMDR(byte* buf); // >0 result is length of data received, -1 if ACK, -2 if ERR, -3 if NAK
    
    // All of the following are according to the Preston protocol, returning 1 if an ACK is recieved, 0 if ERR, and -1 if NAK
    int mode(byte datah, byte datal);
    int stat();
    int who();
    int data(byte datadescription);
    int data(byte* datadescription, int datalen);
    int rtc(byte select, byte* data); // this is called "Time" in the protocol. Time is a reserved name, hence rtc instead.
    int setl(byte motors);
    int ct();
    int ct(byte cameratype);
    int mset(byte mseth, byte msetl);
    int mstat(byte motor);
    int rs(bool camerarun);
    int tcstat();
    int ld();
    int info(byte type);
    int dist(byte type, int dist);
    //int err(); // Here for completeness but unused as a client command
};

#endif
