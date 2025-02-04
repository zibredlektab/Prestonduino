/*
  PrestonDuino.h - Library for interfacing with a Preston MDR 2/3/4 over Arduio Serial port
  Created by Max Batchelder, June 2020.
*/

#include "Arduino.h"
#include "PrestonPacket.h"
#ifndef PrestonDuino_h
#define PrestonDuino_h

#define PDDEFAULTTIMEOUT 20
#define PDPERIOD 6
#define PDNORMALDATAMODE 0x17 // high resolution focus, iris, and zoom position data

struct command_reply {
  int8_t replystatus;
  byte* data;
};


class PrestonDuino {

  
  
  private:
    // variables
    HardwareSerial *ser; // serial port connected to MDR
    byte rcvbuf[100]; // buffer for incoming data from MDR (100 is arbitrary but should be large enough)
    int rcvlen = 0; // length of incoming packet info
    PrestonPacket* sendpacket = NULL; // most recent outgoing packet to MDR
    PrestonPacket* rcvpacket = NULL; // most recent incoming packet from MDR
    command_reply reply; // most recent received reply from MDR (not currently used)
    int timeout = PDDEFAULTTIMEOUT; // milliseconds to wait for a response
    uint32_t lastsend = 0; // last time a message was sent to MDR

    // MDR Data
    // Basic lens data - can be assumed to be up to date
    uint16_t iris = 0;
    uint16_t focus = 0;
    int16_t zoom = 0;
    uint16_t aux = 0;
    uint16_t distance = 0; // rangefinder distance
    bool running = false;
    
    // Advanced data, not updated automatically
    char lensname[50];
    char mdrtype[5];

    uint8_t lensnamelen = 0;

    // methods
    void sendACK();
    void sendNAK();

    bool waitForRcv(); // returns true if response was recieved
    bool rcv(); // true if usable data received, false if not 
    int parseRcv(); // >=0 result is length of data received, -1 if ACK, -2 if NAK, -3 if error
    bool validatePacket(); // true if packet validates with checksum

    void sendCommand(byte cmd); // create a preston packet for the selected command and send it
    void sendCommandWithData(byte cmd, byte* data, int len); // same as above, but with a data payload
    void sendBytesToMDR(byte* bytestosend, int sendlen); // sends raw bytes to MDR
    void sendPacketToMDR(PrestonPacket* packet, bool retry = false); // sends a constructed PrestonPacket to MDR

    void zoomFromLensName(); // in the case of a prime lens, need to extract zoom data from lens name

  public:

    void onLoop();

    PrestonDuino(HardwareSerial& serial);
    bool isMDRReady(); // returns false until we get our first response from MDR
    void setMDRTimeout(int newtimeout); // sets the timeout on waiting for incoming data from the mdr
    void shutUp(); // hard-coded packet to get the MDR to stop streaming data

    /* All of the following are according to the Preston protocol.
     * reply_status is a signed int identifying the type of the response:
     * 0 if timeout, -1 if ACK with no further reply, -2 if NAK, -3 if error, >0 result is length of data section in the MDR reply
     * data is a byte array representing the data section of the MDR reply
     */

    void mode(byte datah, byte datal);
    void stat();
    void who();
    void data(byte datadescription);
    void data(byte* dataset, int datalen);
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
    // Based on latest info received from MDR using data() or ld(), or streamed if that is enabled

    // Getters
    uint16_t getFocus(); // Focus distance, in mm (1mm precision)
    int16_t getZoom(); // Focal length, in mm (1mm precision)
    uint16_t getIris(); // Iris (*100, ex T-5.6 returns as 560)
    uint16_t getAux();
    char* getLensName(); // Lens name, as assigned in hand unit. 0-terminated string
    uint8_t getLensNameLen(); // Length of lens name
    char* getMDRType(); // MDR name and version number. First four chars will be "MDRx" (x = 2 - 5) for easy model recognition
    bool getRunning(); // Is the camera running?


    // Setters
    void setIris(uint16_t newiris);  // experimental, Iris must be configured for control with mode() first
};

#endif
