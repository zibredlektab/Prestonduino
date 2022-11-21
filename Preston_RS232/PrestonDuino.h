/*
  PrestonDuino.h - Library for interfacing with a Preston MDR 2/3/4 over Arduio Serial port
  Created by Max Batchelder, June 2020.
*/

#include "Arduino.h"
#include "PrestonPacket.h"
#ifndef PrestonDuino_h
#define PrestonDuino_h

#define DEFAULTTIMEOUT 2000

struct command_reply {
  uint8_t replystatus;
  byte* data;
};



class PrestonDuino {

  
  
  private:
    // variables
    HardwareSerial *ser; // serial port connected to MDR
    bool connectionopen = false; // flag that we have a line to the MDR
    byte rcvbuf[100]; // buffer for incoming data from MDR (100 is arbitrary but should be large enough)
    int rcvlen = 0; // length of incoming packet info
    PrestonPacket* sendpacket = NULL; // outgoing packet to MDR
    PrestonPacket* rcvpacket = NULL; // incoming packet from MDR
    int mdrtype = 0; // 2, 3, or 4 depending on what kind of MDR
    command_reply reply; // most recently received reply from MDR
    const byte dummydata[7] = {0,0,0,0,0,0,0};
    
    unsigned long time_now = 0; // used for scheduling packets (Caution: this will overflow if the program runs for over 49.7 days. Remember to reboot once a month or so)
    int period = 6; // milliseconds to wait between sending packets (lens data is updated every 6ms)
    int timeout = DEFAULTTIMEOUT; // milliseconds to wait for a response


    // MDR Data
    // Basic lens data - can be assumed to be up to date
    uint16_t iris = 0;
    uint16_t focus = 0;
    uint16_t zoom = 0;
    uint16_t aux = 0;
    uint16_t distance = 0; // rangefinder distance
    
    // Advanced data, not updated automatically
    char lensname[50];

    // methods
    void sendACK();
    void sendNAK();
    bool waitForRcv(); // returns true if response was recieved
    bool rcv(); // true if usable data received, false if not
    int parseRcv(); // >=0 result is length of data received, -1 if ACK, -2 if NAK, -3 if error
    command_reply sendCommand(PrestonPacket* pak, bool withreply); // Generic command. See description below for the list of commands and returned array format. If withreply is true, attempts to get a reply from MDR after ACK.
    bool validatePacket(); // true if packet validates with checksum

  public:

    void onLoop();

    PrestonDuino(HardwareSerial& serial);
    int sendToMDR(byte* tosend, int len); // sends raw bytes to MDR. >0 result is length of data received, 0 if timeout, -1 if ACK, -2 if NAK, -3 if error
    int sendToMDR(PrestonPacket* packet); // sends a constructed PrestonPacket to MDR, returns same as above. Does not retry on NAK
    int sendToMDR(PrestonPacket* packet, bool retry); // same as above, with option to retry on NAK
    void setMDRTimeout(int newtimeout); // sets the timeout
    bool readyToSend();
    command_reply getReply();
    
    /* All of the following are according to the Preston protocol.
     * The first byte is a signed int identifying the type of the response:
     * 0 if timeout, -1 if ACK with no further reply, -2 if NAK, -3 if error, >0 result is length of data section in the MDR reply
     * The following array is the data section of the MDR reply
     */
    command_reply mode(byte datah, byte datal);
    command_reply stat();
    command_reply who();
    command_reply data(byte datadescription);
    command_reply data(byte* datadescription, int datalen);
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
    command_reply dist(byte type, uint32_t dist);
    command_reply err(); // Here for completeness but unused as a client command


    // The following are helper methods, simplifying common tasks
    // Lens data requires lens to be calibrated (mapped) from the hand unit

    // Getters
    uint32_t getFocus(); // Focus distance, in mm (1mm precision)
    int getZoom(); // Focal length, in mm (1mm precision)
    int getIris(); // Iris (*100, ex T-5.6 returns as 560)
    char* getLensName(); // Lens name, as assigned in hand unit. 0-terminated string
};

#endif
