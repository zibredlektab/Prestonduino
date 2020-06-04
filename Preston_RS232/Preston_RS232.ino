#include <AltSoftSerial.h>
#include "PrestonPacket.h"

AltSoftSerial prestonSerial(11,12);

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
  
  Serial.begin(115200); //open communication with computer
  while (!Serial) {
    ; // wait for serial port to connect. Needed for Native USB only
  }
  Serial.println();
  Serial.println("Hello!");

  byte data[] = {0x00, 0x1c};
  int datalen = 2;
  byte mode = 0x01;
  PrestonPacket foo = PrestonPacket(mode, data, datalen);

  packetlen = foo.getPacketLength();

  byte* packet = foo.getPacket();

  PrestonPacket bar = PrestonPacket(packet, packetlen);
  Serial.println(bar.getPacketLength());
  Serial.println(bar.getMode(), HEX);
  int bardatalen = bar.getDataLen();
  Serial.println(bardatalen, HEX);
  byte *bardata = bar.getData();
  for (int i = 0; i < bardatalen; i++) {
    Serial.println(bardata[i], HEX);
  }
  Serial.println(bar.getSum(), HEX);

  //prestonSerial.begin(115200);

  //sendPacketToPreston(packet, packetlen);

    
  Serial.println("----");
}

void loop() {
  /*byte packet[] = {0x02,0x30,0x34,0x30,0x31,0x30,0x33,0x32,0x41,0x03};

  for (int i = 0; i < 10; i++) {
    prestonSerial.write(packet[i]);
  }

  //if (prestonSerial.available() > 0) {
    char c = prestonSerial.read();
    Serial.print(c);
  //}
  
  rcvData();
  if (newdata) {
    Serial.print("Received from Preston: ");
    PrestonPacket rcv = PrestonPacket(rcvbuffer, packetlen);
    for (int i=0; i < packetlen; i++) {
      Serial.print(rcvbuffer[i]);
    }
    Serial.println();
  }*/
}


bool sendPacketToPreston(byte* packet, int packetlen) {
  for (int i = 0; i < packetlen; i++) {
    prestonSerial.write(packet[i]);
    delay(10);
  }
}

int rcvData() {
  /* returns 1 if there is a new packet
   * returns 0 if NAK was received
   * returns -1 if anything else was received 
   */
  
  int i = 0;
  bool rcving = false;
  char currentchar;
  char stx = 0x02;
  char etx = 0x03;
  char nak = 0x15;
  
  while (prestonSerial.available() > 0 && !newdata) { //only receive if there is something to be received and no data in our buffer
    Serial.println("data available");
    currentchar = Serial.read();

    if (rcving) {
      rcvbuffer[i++] = currentchar;
      if (currentchar == etx) { // We have received etx, stop reading
        rcving = false;
        packetlen = i;
        newdata = true;
      }
      
    } else if (currentchar == stx) {
      rcvbuffer[i] = currentchar;
      rcving = true;
      
    } else if (currentchar == nak) {
      // NAK received from MDR
      return 0;
      
    } else {
      // something inexplicable was received from MDR
      return -1;
    }
  }

  return 1;


/*
  byte response[
  if (prestonSerial.available()) {
    byte response[100] = prestonSerial.readBytes();
  }
  
  if (response == char(06)) {
    Serial.println("ACK recieved from Preston");
    return true;
  } else if (response == char(15)) {
    Serial.println("NAK recieved from Preston");
    return false;
  } else {
    Serial.print(response);
    Serial.println(" recieved from Preston");
    return false;
  }*/
}
