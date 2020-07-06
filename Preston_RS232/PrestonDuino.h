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
    bool connectionopen = false; // flag that we have a line to the MDR
    byte rcvbuf[100]; // buffer for incoming data from MDR (100 is arbitrary but should be large enough)
    bool rcving = false; // flag that we are in the middle of receiving a packet
    bool rcvreadytoprocess = false; // flag that we have received a complete packet from the MDR to process
    int rcvlen = 0; // length of incoming packet info
    PrestonPacket* rcvpacket; // most recently received packet from MDR
    int mdrtype = 0; // 2, 3, or 4 depending on what kind of MDR
    
    unsigned long time_now = 0; // used for scheduling packets (Caution: this will overflow if the program runs for over 49.7 days. Remember to reboot once a month or so)
    int period = 6; // milliseconds to wait between sending packets (lens data is updated every 6ms)
    int timeout = 2000; // milliseconds to wait for a response


    // methods
    void sendACK();
    void sendNAK();
    bool waitForRcv(); // returns true if response was recieved
    bool rcv(); // true if usable data received, false if not
    int parseRcv(); // >=0 result is length of data received, -1 if ACK, -2 if NAK, -3 if error
    byte* sendCommand(PrestonPacket* pak, bool withreply); // Generic command. See description below for the list of commands and returned array format. If withreply is true, attempts to get a reply from MDR after ACK.


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
    byte* mode(byte datah, byte datal);
    byte* stat();
    byte* who();
    byte* data(byte datadescription);
    byte* data(byte* datadescription, int datalen); // not sure what is returned in this case, I think just ACK?
    byte* rtc(byte select, byte* data); // this is called "Time" in the protocol. time is a reserved name, hence rtc instead.
    byte* setl(byte motors);
    byte* ct(); // MDR2 only
    byte* ct(byte cameratype); // MDR2 only
    byte* mset(byte mseth, byte msetl); //MDR3/4 only
    byte* mstat(byte motor);
    byte* r_s(bool rs);
    byte* tcstat();
    byte* ld(); // MDR3/4 only
    byte* info(byte type); // MDR3/4 only, first element of array is size of payload
    byte* dist(byte type, int dist);
    byte* err(); // Here for completeness but unused as a client command


    // The following are helper methods, simplifying common tasks
    // Lens data requires lens to be calibrated (mapped) from the hand unit
    byte* getLensData(); // checks for previously-recieved (still fresh) lens data, then runs ld if not found
    int getFocusDistance(); // Focus distance, in mm *10 (.1mm precision, 334.3mm returns as 3343)
    int getFocalLength(); // Focal length, in mm (1mm precision)
    int getAperture(); // Aperture (*100, ex T-5.6 returns as 560)
    char* getLensName(); // Lens name, as assigned in hand unit. 0-terminated string

};

#endif
