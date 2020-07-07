/*
  PrestonDuino.h - Library for interfacing with a Preston MDR 2/3/4 over Arduio Serial port
  Created by Max Batchelder, June 2020.
*/

#include "Arduino.h"
#include "PrestonPacket.h"
#ifndef PrestonDuino_h
#define PrestonDuino_h

struct command_reply {
  int replystatus;
  byte* data;
};

class PrestonDuino {

  
  
  private:
    // variables
    HardwareSerial *ser; // serial port connected to MDR
    bool firstpacket = true;
    bool connectionopen = false; // flag that we have a line to the MDR
    byte rcvbuf[100]; // buffer for incoming data from MDR (100 is arbitrary but should be large enough)
    bool rcving = false; // flag that we are in the middle of receiving a packet
    bool rcvreadytoprocess = false; // flag that we have received a complete packet from the MDR to process
    int rcvlen = 0; // length of incoming packet info
    PrestonPacket* rcvpacket; // most recently received packet from MDR
    int mdrtype = 0; // 2, 3, or 4 depending on what kind of MDR
    command_reply reply;
    
    unsigned long time_now = 0; // used for scheduling packets (Caution: this will overflow if the program runs for over 49.7 days. Remember to reboot once a month or so)
    int period = 6; // milliseconds to wait between sending packets (lens data is updated every 6ms)
    int timeout = 2000; // milliseconds to wait for a response


    // methods
    void sendACK();
    void sendNAK();
    bool waitForRcv(); // returns true if response was recieved
    bool rcv(); // true if usable data received, false if not
    int parseRcv(); // >=0 result is length of data received, -1 if ACK, -2 if NAK, -3 if error
    command_reply sendCommand(PrestonPacket* pak, bool withreply); // Generic command. See description below for the list of commands and returned array format. If withreply is true, attempts to get a reply from MDR after ACK.


  public:  
    PrestonDuino(HardwareSerial& serial);
    int sendToMDR(byte* tosend, int len); // sends raw bytes to MDR. >0 result is length of data received, 0 if timeout, -1 if ACK, -2 if NAK, -3 if error
    int sendToMDR(PrestonPacket* packet); // sends a constructed PrestonPacket to MDR, returns same as above. Does not retry on NAK
    int sendToMDR(PrestonPacket* packet, bool retry); // same as above, with option to retry on NAK
    void setMDRTimeout(int timeout); // sets the timeout
    bool readyToSend();
    
    /* All of the following are according to the Preston protocol.
     * The first byte is a signed int identifying the type of the response:
     * 0 if timeout, -1 if ACK with no further reply, -2 if NAK, -3 if error, >0 result is length of data section in the MDR reply
     * The following array is the data section of the MDR reply
     */
    command_reply mode(byte datah, byte datal);
    command_reply stat();
    command_reply who();
    command_reply data(byte datadescription);
    command_reply data(byte* datadescription, int datalen); // not sure what is returned in this case, I think just ACK?
    command_reply rtc(byte select, byte* data); // this is called "Time" in the protocol. time is a reserved name, hence rtc instead.
    command_reply setl(byte motors);
    command_reply ct(); // MDR2 only
    command_reply ct(byte cameratype); // MDR2 only
    command_reply mset(byte mseth, byte msetl); //MDR3/4 only
    command_reply mstat(byte motor);
    command_reply r_s(bool rs);
    command_reply tcstat();
    command_reply ld(); // MDR3/4 only
    command_reply info(byte type); // MDR3/4 only, first element of array is size of payload
    command_reply dist(byte type, int dist);
    command_reply err(); // Here for completeness but unused as a client command


    // The following are helper methods, simplifying common tasks
    // Lens data requires lens to be calibrated (mapped) from the hand unit
    byte* getLensData(); // checks for previously-recieved (still fresh) lens data, then runs ld if not found
    uint32_t getFocusDistance(); // Focus distance, in mm (1mm precision)
    int getFocalLength(); // Focal length, in mm (1mm precision)
    int getAperture(); // Aperture (*100, ex T-5.6 returns as 560)
    char* getLensName(); // Lens name, as assigned in hand unit. 0-terminated string

};

#endif
