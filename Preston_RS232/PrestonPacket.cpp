#include "Arduino.h"
#include "PrestonPacket.h"

PrestonPacket::PrestonPacket(byte cmd_mode, byte* cmd_data, int cmd_datalen) {
  this->mode = cmd_mode;
  this->data = cmd_data;
  this->datalen = cmd_datalen;
  this->corelen = this->datalen + 2; // mode, size, data
  this->packetlen = (this->corelen * 2) + 4; // STX, ETX, 2 sum bytes
  compilePacket();
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
  Serial.println("Building the core");
  byte core[this->corelen];
  core[0] = this->mode; // Mode is first
  core[1] = this->datalen; // Size of data
  
  for (int i = 0; i < this->datalen; i++) {
    // Iterate through the data array
    core[2+i] = this->data[i]; // Make sure not to overwrite the mode & size
  }

  Serial.print("Core is ");
  for (int i = 0; i < this->corelen; i++) {
    Serial.print(core[i], HEX);
  }
  Serial.println();
  // Finished building core

  // Encode the core
  int coreasciilen = (this->corelen * 2); // Every byte in core becomes 2 bytes, as we 0-pad everything
  byte coreascii[coreasciilen];
  this->asciiEncode(core, this->corelen, coreascii);
  Serial.print("Encoded core is: ");
  for (int i=0; i<coreasciilen; i++) {
    Serial.print(coreascii[i], HEX);
  }
  Serial.println();

  // Finished encoding core

  // Compute sum, encode sum
  int coresum = this->computeSum(coreascii, coreasciilen);
  Serial.print("Sum is ");
  Serial.println(coresum, HEX);
  byte sumascii[2];
  sprintf(sumascii, "%02X", coresum);

  Serial.print("Encoded sum is: ");
  for (int i = 0; i < 2; i++) {
    Serial.print(sumascii[i], HEX);
  }
  Serial.println();
  // Finished with sum


  // Put it all together
  byte output[this->packetlen];
  int ioutput = 0;
  output[ioutput++] = 0x02; // STX
  for (int i = 0; i < coreasciilen; i++) {
    // Iterate through coreascii
    output[ioutput++] = coreascii[i]; // Don't overwrite STX
  }

  for (int i = 0; i < 2; i++) {
    // Iterate through sumascii 
    output[ioutput++] = sumascii[i]; // Don't overwrite core
  }
  
  output[ioutput++] = 0x03; // ETX
  this->packet_ascii = output;
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
    
    Serial.println(holder[0]);
    Serial.println(holder[1]);
    
    output[i*2] = holder[0]; // populate output, two bytes to represent what was previously one hex byte
    output[(i*2)+1] = holder[1];
  }
}







byte PrestonPacket::getMode() {
  return this->mode;
}


int PrestonPacket::setMode(byte cmd_mode) {
  this->mode = cmd_mode;
  return 1;
}


byte* PrestonPacket::getData() {
  return this->data;
}


int PrestonPacket::setData(byte* cmd_data) {
  this->data = cmd_data;
  return 1;
}


int PrestonPacket::getSum() {
  return this->checksum;
}



byte* PrestonPacket::getPacket() {
  return this->packet_ascii;
}


int PrestonPacket::getPacketLength() {
//  Serial.print("packetlen is ");
//  Serial.println(this->packetlen);
  return this->packetlen;
}
