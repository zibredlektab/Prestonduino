/*
  PrestonPacket.cpp
  Created by Max Batchelder, June 2020.
*/

#include "Arduino.h"
#include "PrestonPacket.h"


PrestonPacket::PrestonPacket() {

}

bool PrestonPacket::packetFromCommand(byte cmd_mode) {
  // Initializer for creating a new packet for a command with no arguments
  this->mode = cmd_mode;

  this->zeroOut();
  this->datalen = 0;

  this->compilePacket();

  return true;
}



bool PrestonPacket::packetFromCommandWithData(byte cmd_mode, byte* cmd_data, int cmd_datalen) {
  // Initializer for creating a new packet from component parts
  ////Serial.println("PP: Making a packet");
  this->mode = cmd_mode;


  for (int i = 0; i < cmd_datalen; i++) {
    this->data[i] = cmd_data[i];
  }
  
  this->datalen = cmd_datalen;
  this->compilePacket();

  return true;
}



bool PrestonPacket::packetFromBuffer(byte* inputbuffer, int len) {
  // Initializer for creating a packet from a recieved set of bytes
  
  if (len < 9) {
    //Serial.println("PP: warning: buffer is too short to be valid!");
    return false; // minimum length for a valid packet
  }

  this->packetlen = len;
  //Serial.print("PP: Setting up a packet from a buffer ");
  //Serial.print(this->packetlen);
  //Serial.println(" bytes long");

  if (len > 100) {
    //Serial.println("PP: WARNING: incoming buffer is too long!");
  }

  for (int i = 0; i < len; i++) {
    this->packet_ascii[i] = inputbuffer[i];
  }
  this->parseInput(inputbuffer, len);
  
  //this->compilePacket();

  //Serial.print ("PP: Packet is : ");
  for (int i = 0; i < this->packetlen; i++) {
    //Serial.print(" 0x");
    //Serial.print(this->packet_ascii[i], HEX);
  }
  //Serial.println();

  return true;
}






void PrestonPacket::parseInput(byte* inputbuffer, int len) {
  if (inputbuffer[0] != STX) {
    ////Serial.print("PP: Packet to parse doesn't start with STX, instead starts with ");
    ////Serial.println(inputbuffer[0]);
    return;
  } else {
    /*
    //Serial.print("PP: Bytes to parse:");
    for (int i = 0; i < len; i++) {
      //Serial.print(" 0x");
      //Serial.print(inputbuffer[i], HEX);
    }
    //Serial.println();
    */

    // Ascii decode the packet header
    byte decodedheader[2];
    this->asciiDecode(&inputbuffer[1], 4, decodedheader);

    /*
    //Serial.print("PP: Decoded header:");
    for (int i = 0; i < 2; i++) {
      //Serial.print(" 0x");
      //Serial.print(decodedheader[i], HEX);
    }
    //Serial.println();
    */
    
    // set mode
    this->mode = decodedheader[0];

    //Serial.print("PP: Mode of reply is 0x");
    //Serial.println(this->mode, HEX);

    // set datalen, corelen
    this->datalen = decodedheader[1];
    this->corelen = this->datalen + 2;

    int decodeddatalen = this->datalen;
    

    if (this->mode == 0x0E || this->mode == 0x1F) {
      // Data should be treated as literal ascii, not encoded ascii
      for (int i = 0; i < this->datalen; i++) {
        this->data[i] = inputbuffer[i+5]; // data starts at index 5
      }
    } else {
      // Data needs to be decoded from encoded ascii
      byte decodedcore[this->datalen];
      this->asciiDecode(&inputbuffer[5], this->datalen*2, decodedcore);
      // set data
      
      //Serial.print("PP: Decoded core is as follows:");
      for (int i = 0; i < this->datalen; i++) {
        this->data[i] = decodedcore[i];
        //Serial.print(" 0x");
        //Serial.print(this->data[i], HEX);
        //Serial.print(" (");
        //Serial.print((char)this->data[i]);
        //Serial.print(")");
      }
      //Serial.println();
      

      decodeddatalen = this->datalen*2;
    }

    //Serial.print("PP: Decoded data length is ");
    //Serial.println(decodeddatalen);
    
    // set sum
    byte decodedsum[1];
    this->asciiDecode(&inputbuffer[5+(decodeddatalen)], 2, decodedsum);
    this->checksum = decodedsum[0];
    //Serial.print("PP: Checksum is 0x");
    //Serial.println(this->checksum, HEX);
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


  this->corelen = this->datalen + 2; // mode, size, data
  this->packetlen = (this->corelen * 2) + 4; // STX, ETX, 2 sum bytes


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

  // Finished encoding core

  // Compute sum, encode sum
  /*byte tosum[coreasciilen+1];
  tosum[0] = 2; // STX;
  memcpy(&tosum[1], coreascii, coreasciilen);
  this->checksum = this->computeSum(tosum, coreasciilen+1);
  char sumascii[3];
  sprintf(sumascii, "%02X", this->checksum);*/
  // Finished with sum


  // Put it all together
  int outputlen = 0;
  this->packet_ascii[outputlen++] = 0x02; // STX

  
  for (int i = 0; i < coreasciilen; i++) {
    // Iterate through coreascii
    this->packet_ascii[outputlen++] = coreascii[i]; // Don't overwrite STX
  }

  // Compute sum from packet assembled thus far
  this->checksum = this->computeSum(this->packet_ascii, outputlen);
  char sumascii[3];
  sprintf(sumascii, "%02X", this->checksum);

  for (int i = 0; i < 2; i++) {
    // Iterate through sumascii
    this->packet_ascii[outputlen++] = sumascii[i]; // Don't overwrite core
  }
  
  this->packet_ascii[outputlen++] = 0x03; // ETX

  if (outputlen != this->packetlen) {
    //Serial.print("PP: during prestonpacket creation, outputlen (");
    //Serial.print(outputlen);
    //Serial.print(") does not match packetlen (");
    //Serial.print(this->packetlen);
    //Serial.println("), which it should.");
  }
}






int PrestonPacket::computeSum(byte* input, int len) {
  // byte* input is an ascii-encoded array, including STX
  
  int sum = 0; 
  for (int i = 0; i < len; i++) {
    // iterate through the ascii core
    sum += input[i];
  }
  
  return sum % 0x100;
}






void PrestonPacket::asciiEncode(byte* input, int len, byte* output) {
  /*  duplicate input array, but 0-pad every byte to get double-digit values
   * 
   * 
   */
  
  for (int i = 0; i < len ; i++) {
    char holder[3]; // 2-byte intermediate array to hold newly-formatted number (one digit per byte) (with a trailing 0)
    
    sprintf(holder, "%02X", input[i]);
    
    output[i*2] = holder[0]; // populate output, two bytes to represent what was previously one hex byte
    output[(i*2)+1] = holder[1];
    
  }
}





void PrestonPacket::asciiDecode(byte* input, int inputlen, byte* output) {
  int outputlen = inputlen/2;
  
  //Serial.print("PP: Length of decoded byte array is ");
  //Serial.println(outputlen);
  
  //Serial.print("PP: Bytes given to asciiDecode:");
  for (int i = 0; i < inputlen; i++) {
    //Serial.print(" 0x");
    //Serial.print(input[i], HEX);
  }
  //Serial.println();
  
  for (int i = 0; i < outputlen; i++) {  // need to iterate over 1, 3, 5, 7, etc so i*2+1
    static char holder[3]; // don't forget the trailing 0
    int j = (i*2);

    sprintf(holder, "%c%c", input[j], input[j+1]);

    output[i] = strtol(holder, NULL, 16);
  }
}

void PrestonPacket::zeroOut(){
  for (int i = 0; i < 100; i++) {
    this->data[i] = 0;
    this->packet_ascii[i] = 0;
  }
}

/*
 *  Getters & setters
 */




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
  return this->packet_ascii;
}


int PrestonPacket::getPacketLen() {
  return this->packetlen;
}
