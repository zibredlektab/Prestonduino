#include "PrestonPacket.h"


bool newdata = false; // flag for whether there is data available to be processed
char rcvbuffer[100]; // buffer for storing incoming data, currently limited to 100 bytes since that seems like more than enough?
int packetlen = 0;

unsigned long time_now = 0;
int period = 12;

void setup() {
  Serial.begin(115200); //open communication with computer
  while (!Serial) {
    ; // wait for serial port to connect. Needed for Native USB only
  }
  Serial1.begin(115200); //open communication with MDR
  while (!Serial1) {
    ;
  }


  byte initdata[] = {0x0, 0x0};
  int initlen = 2;
  byte initmode = 0x01;
  PrestonPacket *init = new PrestonPacket(initmode, initdata, initlen);
  byte* initpacket = init->getPacket();
  
  sendPacketToPreston(initpacket, init->getPacketLength());
  delete init;
    
}

void loop() {  
  if (millis() >= time_now + period) {
    time_now += period;
    askPrestonForData();
  }

  
  rcvData();
  if (newdata) {
    /*Serial.print("Received from MDR: ");
    for (int i=0; i < packetlen; i++) {
      Serial.print(rcvbuffer[i], HEX);
    }
    Serial.println();*/
    PrestonPacket *rcv = new PrestonPacket(rcvbuffer, packetlen);
    newdata = false;
    int rcvdatalen = rcv->getDataLen();
    byte *rcvdata = rcv->getData();

    
      
    Serial.print("start");
    static char outbuf[10];
    /*
    sprintf(&outbuf[0], "%02X", 0x1D);
    sprintf(&outbuf[2], "%02X", 0x1);
    Serial.print(outbuf);
    */
   
    for (int i = 1; i < rcvdatalen; i++) { //starting at 1 because the first byte in a mode 4 response is the identifier of the data set
      sprintf(&outbuf[(i-1)*2], "%02X", rcvdata[i]);
      //Serial.print(rcvdata[i], HEX);
    }
    Serial.print(outbuf);
    Serial.println("end");

    delete rcv;
  }
}

void askPrestonForData() {
  byte data[] = {0x2};
  int datalen = 1;
  byte mode = 0x04;
  PrestonPacket *reqfordata = new PrestonPacket(mode, data, datalen);
  
  packetlen = reqfordata->getPacketLength();
  
  byte* reqfordatapacket = reqfordata->getPacket();
  
  sendPacketToPreston(reqfordatapacket, packetlen);
  delete reqfordata;
}

bool sendPacketToPreston(byte* packet, int packetlen) {
  for (int i = 0; i < packetlen; i++) {
    //Serial.print("sending ");
    //Serial.println(packet[i], HEX);
    
    Serial1.write(packet[i]);
    Serial1.flush(); // wait for the packet to finish sending
  }
}

int rcvData() {
  /* returns 1 if there is a new packet
   * returns 0 if NAK or ACK was received
   * returns -1 if anything else was received 
   */
  
  static int i = 0;
  bool rcving = false;
  char currentchar;
  
  while (Serial1.available() > 0 && !newdata) { //only receive if there is something to be received and no data in our buffer
    currentchar = Serial1.read();
    delay(10);
    if (rcving) {
      rcvbuffer[i++] = currentchar;
      if (currentchar == ETX) { // We have received etx, stop reading
        rcving = false;
        packetlen = i;
        newdata = true;
      }
      
    } else if (currentchar == STX) {
      i = 0;
      rcvbuffer[i++] = currentchar;
      rcving = true;
      
    } else if (currentchar == NAK) {
      Serial.println("Received NAK from MDR");
      // NAK received from MDR
      return 0;
      
    } else if (currentchar == ACK) {
      // ACK received from MDR
      delay(10);
      return 0;
  
    } else {
      // something inexplicable was received from MDR
      Serial1.write(NAK); // tell the MDR that we don't understand
      return -1;
    }
  }

  return 1;
}
