#include "PrestonPacket.h"


bool newdata = false; // flag for whether there is data available to be processed
char rcvbuffer[100]; // buffer for storing incoming data, currently limited to 100 bytes since that seems like more than enough?
int packetlen = 0;

void setup() {
 // pinMode(rx, INPUT);
 // pinMode(tx, OUTPUT);
 // digitalWrite(tx, HIGH);
 // delay(2);
 // digitalWrite(13, HIGH);

//  SWprint();
  
  Serial.begin(9600); //open communication with computer
  while (!Serial) {
    ; // wait for serial port to connect. Needed for Native USB only
  }
  Serial1.begin(115200); //open communication with MDR
  while (!Serial1) {
    ;
  }
  Serial.println("--Setup begins--");
  Serial.println("Hello!");

  byte initdata[] = {0x10, 0x0};
  int initlen = 2;
  byte initmode = 0x01;
  PrestonPacket init = PrestonPacket(initmode, initdata, initlen);
  byte* initpacket = init.getPacket();
  
  sendPacketToPreston(initpacket, init.getPacketLength());


  byte data[] = {0x03};
  int datalen = 1;
  byte mode = 0x04;
  PrestonPacket foo = PrestonPacket(mode, data, datalen);

  packetlen = foo.getPacketLength();

  byte* packet = foo.getPacket();
  
  sendPacketToPreston(packet, packetlen);

    
  Serial.println("--Setup complete, loop begins below--");
}

void loop() {
  /*byte packet[] = {0x02,0x30,0x34,0x30,0x31,0x30,0x33,0x32,0x41,0x03};

  for (int i = 0; i < 10; i++) {
    Serial1.print(packet[i], HEX);
  }
  while (Serial1.available()) {
    Serial.println(Serial1.read(), HEX);
  }*/
  
    
  rcvData();
  if (newdata) {
    Serial.print("Received from MDR: ");
    for (int i=0; i < packetlen; i++) {
      Serial.print(rcvbuffer[i], HEX);
    }
    Serial.println();
    PrestonPacket rcv = PrestonPacket(rcvbuffer, packetlen);
    newdata = false;


    int rcvdatalen = rcv.getDataLen();
    byte *rcvdata = rcv.getData();
    for (int i = 0; i < rcvdatalen; i++) {
      char buf[20];
      sprintf(buf, "%02X", rcvdata[i]);
      Serial.println(buf);
    }
  }
}


bool sendPacketToPreston(byte* packet, int packetlen) {
  for (int i = 0; i < packetlen; i++) {
    Serial.print("sending ");
    Serial.println(packet[i], HEX);
    
    Serial1.write(packet[i]);
  }
}

int rcvData() {
  /* returns 1 if there is a new packet
   * returns 0 if NAK or ACK was received
   * returns -1 if anything else was received 
   */
  
  int i = 0;
  bool rcving = false;
  char currentchar;
  char stx = 0x02;
  char etx = 0x03;
  char ack = 0x06;
  char nak = 0x15;
  
  while (Serial1.available() > 0 && !newdata) { //only receive if there is something to be received and no data in our buffer
    currentchar = Serial1.read();

    if (rcving) {
      rcvbuffer[i++] = currentchar;
      if (currentchar == etx) { // We have received etx, stop reading
        rcving = false;
        packetlen = i;
        newdata = true;
      }
      
    } else if (currentchar == stx) {
      rcvbuffer[i++] = currentchar;
      rcving = true;
      
    } else if (currentchar == nak) {
      Serial.println("Received NAK from MDR");
      // NAK received from MDR
      return 0;
      
    } else if (currentchar == ack) {
      Serial.println("Received ACK from MDR");
      return 0;
  
    } else {
      // something inexplicable was received from MDR
      return -1;
    }
  }

  return 1;
}
