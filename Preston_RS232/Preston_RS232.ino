#include "SoftwareSerial.h"
#include "PrestonPacket.h"

SoftwareSerial prestonSerial(2, 3); //2 is RX, 3 is TX

bool newdata = false; // flag for whether there is data available to be processed
char rcvbuffer[100]; // buffer for storing incoming data, currently limited to 100 bytes since that seems like more than enough?
int packetlen = 0;

void setup() {
  Serial.begin(9600); //open communication with computer
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
  
  for (int i = 0; i < packetlen; i++) {
    Serial.println(packet[i], HEX);
  }

  /*prestonSerial.begin(115200);
  while (!prestonSerial) {
    ; // wait for Preston to connect
  }
  Serial.println("Connected to Preston");

  */
  Serial.println("----");
}

void loop() {
  rcvData();
  if (newdata) {
    PrestonPacket rcv = PrestonPacket(rcvbuffer, packetlen);
  }
}

bool sendPacketToPreston(byte* packet, int packetlen) {
  for (int i = 0; i < packetlen; i++) {
    prestonSerial.print(packet[i], HEX);
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

  return true;


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
