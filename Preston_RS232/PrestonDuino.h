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
    // variables
    HardwareSerial *ser; // serial port connected to MDR
    byte rcvbuf[100]; // buffer for incoming data from MDR (100 is arbitrary but should be large enough)
    bool rcving; // flag that we are in the middle of receiving a packet
    bool rcvreadytoprocess = false; // flag that we have received a complete packet from the MDR to process
    int rcvlen = 0; // length of incoming packet info
    PrestonPacket* rcvpacket; // most recently received packet from MDR
    
    unsigned long time_now = 0; // used for scheduling packets (Caution: this will overflow if the program runs for over 49.7 days. Remember to reboot once a month or so)
    int period = 6; // milliseconds to wait between sending packets (lens data is updated every 6ms)
    int timeout = 2000; // milliseconds to wait for a response

    // methods
    void sendACK();
    void sendNAK();

    bool waitForRcv(); // returns true if response was recieved
    bool rcv(); // true if usable data received, false if not
    int parseRcv(); // >0 result is length of data received, -1 if ACK, -2 if NAK, -3 if error
    byte* commandWithReply(PrestonPacket* pak);
    bool command(PrestonPacket* pak); // true if ACK


  public:
    PrestonDuino(HardwareSerial& serial);
    int sendToMDR(byte* tosend, int len); // >0 result is length of data received, 0 if timeout, -1 if ACK, -2 if NAK, -3 if error
    int sendToMDR(PrestonPacket* packet); // Same as sendToMDR above, but takes a packet. Does not retry on NAK
    int sendToMDR(PrestonPacket* packet, bool retry); // same as above, with option to retry on NAK
    void setMDRTimeout(int timeout); // returns the timeout
    
    // All of the following are according to the Preston protocol.
    // Methods that return data will return a byte or byte array, the length of which is determined by the protocol.
    void mode(byte datah, byte datal);
    byte* stat();
    byte who();
    byte* data(byte datadescription);
    void data(byte* datadescription, int datalen); // not sure what is returned in this case, I think just ACK?
    byte* rtc(byte select, byte* data); // this is called "Time" in the protocol. time is a reserved name, hence rtc instead.
    void setl(byte motors);
    byte ct(); // MDR2 only
    void ct(byte cameratype); // MDR2 only
    byte* mset(byte mseth, byte msetl); //MDR3/4 only
    byte* mstat(byte motor);
    void r_s(bool rs);
    byte tcstat();
    byte* ld(); // MDR3/4 only
    byte* info(byte type); // MDR3/4 only, first element of array is size of payload
    void dist(byte type, int dist);
    //void err(); // Here for completeness but unused as a client command

};

#endif
