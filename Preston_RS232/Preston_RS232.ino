#include "PrestonPacket.h"
#include "PrestonDuino.h"
#include "MaxTools.h"



bool rcving = false;
bool packetcomplete = false; // flag for whether there is data available to be processed
char rcvbuffer[100]; // buffer for storing incoming data, currently limited to 100 bytes since that seems like more than enough?
int packetlen = 0;

MaxTools *tools = new MaxTools();
PrestonDuino *mdr = new PrestonDuino(Serial1);

unsigned long time_now = 0;
int period = 5;

void setup() {

  
  
  Serial.begin(115200); //open communication with computer
  while (!Serial) {
    ; // wait for serial port to connect. Needed for Native USB only
  }
  Serial1.begin(115200); //open communication with MDR
  while (!Serial1) {
    ;
  }
  
  byte initdata[] = {0x00, 0x00};
  int initlen = 2;
  byte initmode = 0x01;
  PrestonPacket *init = new PrestonPacket(initmode, initdata, initlen);
  byte* initpacket = init->getPacket();
  
  sendPacketToPreston(initpacket, init->getPacketLength());
  delete init;
    
}

void loop() {
  if (millis() >= time_now + period) {
    time_now = millis();
    askPrestonForData();
  }

  
  rcvData();
  if (packetcomplete) {

    PrestonPacket *rcv = new PrestonPacket(rcvbuffer, packetlen);
    packetcomplete = false;
    int rcvdatalen = rcv->getDataLen();
    byte *rcvdata = rcv->getData();

    Serial.print("start");
    Serial.print(rcv->getFocusDistance());
    Serial.println("end");
    
    delete rcv;
  }
}


void askPrestonForData() {
  byte data[] = {0x2}; //0x1 iris, 0x2 focus, 0x4 zoom, 0x8 aux
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
  char currentchar;
  
  while (Serial1.available() > 0 && !packetcomplete) { //only receive if there is something to be received and no data in our buffer
    currentchar = Serial1.read();
    //Serial.print("received character ");
    //Serial.print(currentchar);
    if (rcving) {
      rcvbuffer[i++] = currentchar;
      if (currentchar == ETX) { // We have received etx, stop reading
        //Serial.println("ETX");
        rcving = false;
        packetlen = i;
        packetcomplete = true;
        Serial1.write(ACK); // Polite thing to do
        return 1;
      } else {
        //Serial.println(currentchar);
      }
      
    } else if (currentchar == STX) {
      //Serial.println("STX");
      i = 0;
      rcvbuffer[i++] = currentchar;
      rcving = true;
      
    } else if (currentchar == NAK) {
      //Serial.println("NAK");
      // NAK received from MDR
      break;
      
    } else if (currentchar == ACK) {
      //Serial.println("ACK");
      // ACK received from MDR
      break;
  
    } else {
      Serial.print("(I have no idea what this means) ");
      Serial.println(currentchar);
      // something inexplicable was received from MDR
      while (Serial1.available()) {
        Serial.print("flushed: ");
        Serial.println(Serial1.read(), HEX);
      }
      Serial1.write(NAK); // tell the MDR that we don't understand
      
      return -1;
    }
  }

  return 0;
}
