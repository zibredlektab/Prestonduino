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

    void sendACK();
    void sendNAK();

    
  public:
    PrestonDuino(HardwareSerial& serial);
    int sendToMDR(PrestonPacket packet); // 1 if ACK, 0 if ERR, -1 if NAK. Does not retry on NAK
    int sendToMDR(PrestonPacket packet, bool retry); // same as above, with option to retry on NAK
    int rcvFromMDR(byte* buf); // >0 result is length of data received, -1 if ACK, -2 if ERR, -3 if NAK
    
    // All of the following are according to the Preston protocol.
    // Methods that return data will return a byte or byte array, the length of which is determined by the protocol.
    void mode(byte datah, byte datal);
    byte* stat();
    byte who();
    byte* data(byte datadescription);
    byte* data(byte* datadescription, int datalen); // not sure what is returned for this command
    byte* rtc(byte select, byte* data); // this is called "Time" in the protocol. Time is a reserved name, hence rtc instead.
    void setl(byte motors);
    byte ct(); // MDR2 only
    void ct(byte cameratype); // MDR2 only
    byte* mset(byte mseth, byte msetl); //MDR3/4 only
    byte* mstat(byte motor);
    void rs(bool camerarun);
    byte tcstat();
    byte* ld(); // MDR3/4 only, first element of array is size of payload
    byte* info(byte type); // MDR3/4 only, first element of array is size of payload
    void dist(byte type, int dist);
    //void err(); // Here for completeness but unused as a client command

    // Methods for 
};

#endif
