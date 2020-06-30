/*
  PrestonPacket.cpp
  Created by Max Batchelder, June 2020.
*/

#include "Arduino.h"
#include "PrestonPacket.h"

PrestonPacket::PrestonPacket(byte cmd_mode, byte* cmd_data, int cmd_datalen) {
  // Initializer for creating a new packet from component parts
  
  this->mode = cmd_mode;
  Serial.print("Mode is ");
  Serial.println(this->mode, HEX);
  Serial.println("Data is:");
  for (int i = 0; i < cmd_datalen; i++) {
    this->data[i] = cmd_data[i];
    Serial.println(this->data[i]);
  }
  this->datalen = cmd_datalen;  
  Serial.print("Data length is ");
  Serial.println(this->datalen);
  this->corelen = this->datalen + 2; // mode, size, data
  this->packetlen = (this->corelen * 2) + 4; // STX, ETX, 2 sum bytes
  this->compilePacket();
}


PrestonPacket::PrestonPacket(byte* inputbuffer, int len) {
  // Initializer for creating a packet from a recieved set of bytes
  
  this->packetlen = len;
  for (int i = 0; i < len; i++){
    this->packet_ascii[i] = inputbuffer[i];
  }
  this->parseInput(this->packet_ascii, len);
}

void PrestonPacket::parseInput(byte* inputbuffer, int len) {  
  if (inputbuffer[0] != STX) {
    Serial.print("Packet to parse doesn't start with STX, instead starts with ");
    Serial.println(inputbuffer[0]);
    return;
  } else {
    byte decoded[len/2-1];
    this->asciiDecode(inputbuffer, len, decoded);
    
    
    // set mode
    this->mode = decoded[0];
    

    // set datalen, corelen
    this->datalen = decoded[1];
    this->corelen = this->datalen + 2;


    // set data
    for (int i = 0; i < this->datalen; i++) {
      this->data[i] = decoded[i+2];
    }


    // set sum
    this->checksum = decoded[len/2-2];
  }
}

void PrestonPacket::compilePacket() {
  /*  1) build core (mode + size + data)
   *    core pre-encoding should be hex numbers, no padding
   *  2) ascii encode core
   *    ascii encoding first adds padding, then encodes each character
   *  3) compute sum
   *    sum should include 2 for STX
   *  4) ascii encode sum
   *  5) STX + core in ascii + sum in ascii + ETX
   * 
   * 
   */


  // Build the core
  byte core[this->corelen];
  core[0] = this->mode; // Mode is first
  core[1] = this->datalen; // Size of data


  for (int i = 0; i < this->datalen; i++) {
    // Iterate through the data array
    core[2+i] = this->data[i]; // Make sure not to overwrite the mode & size
  }
  // Finished building core

  // Encode the core
  int coreasciilen = (this->corelen * 2); // Every byte in core becomes 2 bytes, as we 0-pad everything
  byte coreascii[coreasciilen];
  this->asciiEncode(core, this->corelen, coreascii);

  for (int i = 0; i <= coreasciilen; i++) {
    //delay(1);
  }

  // Finished encoding core

  // Compute sum, encode sum
  int coresum = this->computeSum(coreascii, coreasciilen);
  byte sumascii[2];
  sprintf(sumascii, "%02X", coresum);
  // Finished with sum


  // Put it all together
  int ioutput = 0;
  
  this->packet_ascii[ioutput++] = 0x02; // STX

  
  for (int i = 0; i < coreasciilen; i++) {
    // Iterate through coreascii
    this->packet_ascii[ioutput++] = coreascii[i]; // Don't overwrite STX
  }

  for (int i = 0; i < 2; i++) {
    // Iterate through sumascii 
    this->packet_ascii[ioutput++] = sumascii[i]; // Don't overwrite core
  }
  
  this->packet_ascii[ioutput++] = 0x03; // ETX
}


int PrestonPacket::computeSum(byte* input, int len) {
  // byte* input is an ascii-encoded array
  
  int sum = 2; // STX is included in sum
  for (int i = 0; i < len; i++) {
    // iterate through the ascii core
    sum += input[i];
  }
  
  this->checksum = sum % 0x100;
  return this->checksum;
}


void PrestonPacket::asciiEncode(byte* input, int len, byte* output) {
  /*  duplicate input array, but 0-pad every byte to get double-digit values
   * 
   * 
   */
  
  for (int i = 0; i < len ; i++) {
    byte holder[2]; // 2-byte intermediate array to hold newly-formatted number (one digit per byte)
    
    sprintf(holder, "%02X", input[i]);
    
    output[i*2] = holder[0]; // populate output, two bytes to represent what was previously one hex byte
    output[(i*2)+1] = holder[1];
    
  }
}

void PrestonPacket::asciiDecode(byte* input, int len, byte* output) {
  int outputlen = (len/2)-1;
  for (int i = 0; i < outputlen; i++) {  // need to iterate over 1, 3, 5, 7, etc so i*2+1
    // input[0] is stx, input[len-1] is etx
    static char holder[2];
    int j = (i*2)+1;

    sprintf(holder, "%c%c", input[j], input[j+1]);

    output[i] = strtol(holder, NULL, 16);
  }
}



unsigned int PrestonPacket::getFocusDistance() {
  int res = 0;
  int i = 0;
  static char focusdist[6];
  if (this->mode == 0x04) {
    focusdist[4] = "\0";
    i = 1; //starting at 1 because the first byte in a mode 4 response is the identifier of the data set
  }
  int j = 0;
  for (i; i < this->datalen; i++) { 
    sprintf(&focusdist[j++], "%02X", this->data[i]);
  }

  return strtol(focusdist, NULL, HEX);
}



byte PrestonPacket::getMode() {
  return this->mode;
}


byte* PrestonPacket::getData() {
  return this->data;
}


int PrestonPacket::getDataLen() {
  return this->datalen;
}


int PrestonPacket::getSum() {
  return this->checksum;
}


byte* PrestonPacket::getPacket() {
  for (int i = 0; i < this->packetlen; i++) {
    //Serial.println(packet_ascii[i]);
  }
  return this->packet_ascii;
}


int PrestonPacket::getPacketLen() {
  return this->packetlen;
}
