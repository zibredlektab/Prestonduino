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
#define DEFAULT_DEBUG_LVL 0


class PrestonDuino {

  private:
    int debuglvl = DEFAULT_DEBUG_LVL;
    
    // variables
    HardwareSerial *ser; // serial port connected to MDR
    byte rcvbuf[100]; // buffer for incoming data from MDR (100 is arbitrary but should be large enough)
    int rcvlen = 0; // length of incoming packet info
    PrestonPacket* sendpacket = NULL; // most recent outgoing packet to MDR
    PrestonPacket* rcvpacket = NULL; // most recent incoming packet from MDR
    int timeout = PDDEFAULTTIMEOUT; // milliseconds to wait for a response
    uint32_t lastsend = 0; // last time a message was sent to MDR
    bool shouldwaitforACK = true; // do we wait for ACK after sending messages or no?

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
    bool waitForAck(int timeout); // discards all serial input until an ack is received (or timeout elapses)


    bool waitForRcv(); // returns true if response was recieved
    bool rcv(); // true if usable data received, false if not 
    int parseRcv(); // >=0 result is length of data received, -1 if ACK, -2 if NAK, -3 if error
    bool validatePacket(); // true if packet validates with checksum

    int sendCommand(byte cmd); // create a preston packet for the selected command and send it
    int sendCommandWithData(byte cmd, byte* data, int len); // same as above, but with a data payload
    void sendBytesToMDR(byte* bytestosend, int sendlen); // sends raw bytes to MDR
    int sendPacketToMDR(PrestonPacket* packet, bool retry = false); // sends a constructed PrestonPacket to MDR

    void zoomFromLensName(); // in the case of a prime lens, need to extract zoom data from lens name

    void debugPrint(char* toprint);
    void debugPrintln(char* toprint);

  public:

    void onLoop();

    PrestonDuino(HardwareSerial& serial, int debuglvl = DEFAULT_DEBUG_LVL);
    bool isMDRReady(); // returns false until we get our first response from MDR
    void setMDRTimeout(int newtimeout); // sets the timeout on waiting for incoming data from the mdr
    void shutUp(); // hard-coded packet to get the MDR to stop streaming data

    void shouldWaitForACK(bool wait); // in general, should we wait for ACK after sending a message or just continue?

    /* All of the following are according to the Preston protocol.
     * reply_status is a signed int identifying the type of the response:
     * 0 if timeout, -1 if ACK with no further reply, -2 if NAK, -3 if error, >0 result is length of data section in the MDR reply
     * data is a byte array representing the data section of the MDR reply
     */

    int mode(byte datah, byte datal);
    int stat();
    int who();
    int data(byte datadescription);
    int data(byte* dataset, int datalen);
    int rtc(byte select, byte* data); // this is called "Time" in the protocol. time is a reserved name, hence rtc instead.
    int setl(byte motors);
    int ct(); // MDR2 only
    int ct(byte cameratype); // MDR2 only
    int mset(byte mseth, byte msetl); //MDR3/4 only
    int mstat(byte motor);
    int r_s(bool rs); // run the camera
    int tcstat();
    int ld(); // MDR3/4 only
    int info(byte type); // MDR3/4 only, first element of array is size of payload
    int dist(byte type, uint32_t dist);
    int err(); // Here for completeness but unused as a client command

    void raw(byte* packet, int packetlen); // send raw data to mdr

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
    void setIris(uint16_t newiris, bool meta = true);  // experimental, Iris must be configured for control with mode() first
    void setAux(uint16_t newaux);
};

#endif
