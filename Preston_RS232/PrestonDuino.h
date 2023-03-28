/*
  PrestonDuino.h - Library for interfacing with a Preston MDR 2/3/4 over Arduio Serial port
  Created by Max Batchelder, June 2020.
*/

#include "Arduino.h"
#include "PrestonPacket.h"
#ifndef PrestonDuino_h
#define PrestonDuino_h

#define DEFAULTTIMEOUT 20
#define PERIOD 6
#define NAMECHECK 2000
#define MSGQUEUELIMIT 10
#define NORMALDATAMODE 0x17 // high resolution focus, iris, and zoom position data

struct command_reply {
  int8_t replystatus;
  byte* data;
};

struct mdr_message {
  mdr_message* nextmsg = NULL;
  uint8_t msglen;
  byte data[100];
};



class PrestonDuino {

  
  
  private:
    // variables
    HardwareSerial *ser; // serial port connected to MDR
    byte rcvbuf[100]; // buffer for incoming data from MDR (100 is arbitrary but should be large enough)
    int rcvlen = 0; // length of incoming packet info
    mdr_message* rootmsg = NULL;
    uint8_t totalmessages = 0; // number of queued messages
    PrestonPacket* sendpacket = NULL; // most recent outgoing packet to MDR
    PrestonPacket* rcvpacket = NULL; // most recent incoming packet from MDR
    command_reply reply; // most recent received reply from MDR (not currently used)
    int timeout = DEFAULTTIMEOUT; // milliseconds to wait for a response
    uint32_t lastsend = 0; // last time a message was sent to MDR
    uint32_t lastnamecheck = 0; // last time we asked MDR for a lens name

    // MDR Data
    // Basic lens data - can be assumed to be up to date
    uint16_t iris = 0;
    uint16_t focus = 0;
    uint16_t zoom = 0;
    uint16_t aux = 0;
    uint16_t distance = 0; // rangefinder distance
    
    // Advanced data, not updated automatically
    char lensname[50];
    char fwname[50];

    uint8_t lensnamelen = 0;

    // methods
    void sendACK();
    void sendNAK();
    bool waitForRcv(); // returns true if response was recieved
    bool rcv(); // true if usable data received, false if not 
    int parseRcv(); // >=0 result is length of data received, -1 if ACK, -2 if NAK, -3 if error
    void sendCommand(PrestonPacket* pak); // Generic command. See description below for the list of commands and returned array format.
    bool validatePacket(); // true if packet validates with checksum
    bool queueForSend(byte* tosend, int len); // adds raw bytes to the current send buffer
    void sendBytesToMDR(); // sends the current send buffer to MDR.
    void sendPacketToMDR(PrestonPacket* packet, bool retry = false); // sends a constructed PrestonPacket to MDR
    void zoomFromLensName(); // in the case of a prime lens, need to extract zoom data from lens name

  public:

    void onLoop();

    PrestonDuino(HardwareSerial& serial);
    void setMDRTimeout(int newtimeout); // sets the timeout on waiting for incoming data from the mdr
    
    /* All of the following are according to the Preston protocol.
     * reply_status is a signed int identifying the type of the response:
     * 0 if timeout, -1 if ACK with no further reply, -2 if NAK, -3 if error, >0 result is length of data section in the MDR reply
     * data is a byte array representing the data section of the MDR reply
     */

    void mode(byte datah, byte datal);
    void stat();
    void who();
    void data(byte datadescription);
    void data(byte* datadescription, int datalen);
    void rtc(byte select, byte* data); // this is called "Time" in the protocol. time is a reserved name, hence rtc instead.
    void setl(byte motors);
    void ct(); // MDR2 only
    void ct(byte cameratype); // MDR2 only
    void mset(byte mseth, byte msetl); //MDR3/4 only
    void mstat(byte motor);
    void r_s(bool rs);
    void tcstat();
    void ld(); // MDR3/4 only
    void info(byte type); // MDR3/4 only, first element of array is size of payload
    void dist(byte type, uint32_t dist);
    void err(); // Here for completeness but unused as a client command

    // The following are helper methods, simplifying common tasks
    // Lens data requires lens to be calibrated (mapped) from the hand unit

    // Getters
    uint16_t getFocus(); // Focus distance, in mm (1mm precision)
    uint16_t getZoom(); // Focal length, in mm (1mm precision)
    uint16_t getIris(); // Iris (*100, ex T-5.6 returns as 560)
    uint16_t getAux();
    char* getLensName(); // Lens name, as assigned in hand unit. 0-terminated string
    uint8_t getLensNameLen(); // Length of lens name
};

#endif
