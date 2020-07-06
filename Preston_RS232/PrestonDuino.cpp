/*
  PrestonDuino.cpp - Library for talking to a Preston MDR 2/3/4 over RS-232
  Created by Max Batchelder, June 2020.
*/

#include "PrestonDuino.h"


PrestonDuino::PrestonDuino(HardwareSerial& serial) {
  // Open a connection to the MDR on Serial port 'serial'
  
  ser = &serial;
  ser->begin(115200);

  // Connection to MDR theoretically open, testing with WHO

  while (!ser) {
    ; // wait until we get a reply
  }

  Serial.println("MDR connection is open");
  // TODO slightly more robust way of checking for connection?
  this->connectionopen = true;

  // TODO get & store MDR number 
  /*byte* mdrinfo = this->info(0x0);

  int infolen = mdrinfo[0];

  this->mdrtype = mdrinfo[5];
  //Serial.println(this->mdrtype);
  */
}


bool PrestonDuino::readyToSend() {
  return this->connectionopen;
}



int PrestonDuino::sendToMDR(byte* tosend, int len) {
  /* The most basic send function, simply writes byte array out to ser and returns the immediate MDR response
   *  
   * Returns: >0 is length of data received, 0 if timeout, -1 if ACK, -2 if NAK, -3 if error message
   * 
   * MDR should always (apparently) respond with ACK or NAK before sending a reply packet, so this function 
   * should always return 0, -1, or -2. YMMV.
   */

  for (int i = 0; i < len; i++) {
    Serial.print("Sending ");
    Serial.println(char(tosend[i]));
    ser->write(tosend[i]);
    ser->flush(); // wait for byte to finish sending
  }

  
  int response = 0;
  
  if (this->waitForRcv()) {
    // response received
    Serial.println("Got a response");
    response = this->parseRcv();
  }

  return response;
}





bool PrestonDuino::waitForRcv() {
  this->time_now = millis();
  
  while(millis() <= this->time_now + this->timeout) {
    if (ser->available()) {
      Serial.println("Response is available");
      // Response is available
      return this->rcv();
    }
  }

  // If it gets this far, there was a timeout
  Serial.println("Timeout");
  return false;
}





bool PrestonDuino::rcv() {
  /* Check if there is available data, then check the first char for following:
   *    ACK/NAK: add to rcv and return
   *    STX: add all bytes to rcv until ETX is found (if second STX etc is found, see next line)
   *    Anything else: treat as garbage data. Send NAK to MDR and toss the buffer
   * Returns true if we got usable data, false if not
   */
  
  static int i = 0;
  char currentchar;
  bool gotgoodreply = false;

  
  while (ser->available() > 0 && !this->rcvreadytoprocess) { //only receive if there is something to be received and no data in our buffer
    currentchar = ser->read();
    Serial.print("Received ");
    Serial.print(currentchar, HEX);
    Serial.println(" from MDR");
    
    if (this->rcving) {
      // Currently in the process of receiving an incoming packet
      this->rcvbuf[i++] = currentchar; // Add current incoming character to rcvbuf
      if (currentchar == ETX) {
        // We have received ETX, stop reading
        this->rcving = false;
        this->rcvlen = i;
        this->rcvreadytoprocess = true;
        gotgoodreply = true;
        
        this->sendACK(); // Polite thing to do
        
        break;
        
      } else if (currentchar == STX || currentchar == NAK || currentchar == ACK) {
        // This should not happen, and means that our packet integrity is compromised (we started reading into a new packet somehow)
        break;
      }
      
    } else if (currentchar == STX) {
      // STX received from MDR
      i = 0;
      this->rcvbuf[i++] = currentchar;
      this->rcving = true; // On the next go-round, start adding characters to rcvbuf
      
    } else if (currentchar == NAK || currentchar == ACK) {
      // Incoming character is either NAK or ACK, right now we don't care which
      this->rcvbuf[0] = currentchar;
      this->rcvreadytoprocess = true;
      this->rcvlen = 1;
      this->rcving = false;
      gotgoodreply = true; // NAK still counts as a good reply in this context!
      break;
  
    } else {
      // Incoming character is some kind of garbage
      gotgoodreply = false;
      break;
    }
  }

  if (gotgoodreply) {
    return true;
  } else {
    // Some kind of garbage was received from the MDR
    while (ser->available()) {
      // Dump the buffer
      // TODO this could result in well-structured data being tossed as well, if a good packet arrives while we are processing a bad packet.
      ser->read();
    }
    this->sendNAK(); // tell the MDR that we don't understand
  
    this->rcving = false;
    this->rcvlen = 0;
    this->rcvreadytoprocess = false;
    
    return false; // :(
  }

}




int PrestonDuino::parseRcv() {
  // >0 result is length of data received, -1 if ACK, -2 if error, -3 if NAK
  int response = 0;
  
  if (this->rcvreadytoprocess) {
    // TODO This should always be true, parseRcv() shouldn't be called unless we're ready to process...
    if (rcvbuf[0] == ACK) {
      // Reply is ACK
      response = -1;
      
    } else if (rcvbuf[0] == NAK) {
      // Reply is NAK
      response = -2;
      
    } else if (rcvbuf[0] == STX) {
      // Reply is a packet
      PrestonPacket *pak = new PrestonPacket(this->rcvbuf, this->rcvlen);
      this->rcvpacket = pak;
      
      if (this->rcvpacket->getMode() == 0x11) {
        // Reply is an error message
        response = -3;
        // TODO actual error handling
        
      } else {
        // Reply is a response packet
        response = this->rcvpacket->getDataLen();
      }
    }

    // Done processing
    this->rcvreadytoprocess = false;
  }

  return response;
}



void PrestonDuino::sendACK() {
  // Send ACK, don't wait for a reply (there shouldn't be one)
  Serial.println("Sending ACK to MDR");
  ser->write(ACK);
  ser->flush();
}



void PrestonDuino::sendNAK() {
  // Send NAK, don't wait for a reply (that will be handled elsewhere)
  Serial.println("Sending NAK to MDR");
  ser->write(NAK);
  ser->flush();
}



int PrestonDuino::sendToMDR(PrestonPacket* packet, bool retry) {
  // Send a PrestonPacket to the MDR, byte by byte. If "retry" is true, packet will be re-sent upon timout or NAK, up to 3 times
  int packetlen = packet->getPacketLen();
  byte* packetbytes = packet->getPacket();
  
  int response = this->sendToMDR(packetbytes, packetlen);

  if (response == -2) {
    // NAK received, MDR didn't understand us. Resend packet.
    // TODO retry
  }


  Serial.print("Response status is ");
  Serial.println(response);
  delete packet;
  return response;

}



int PrestonDuino::sendToMDR(PrestonPacket* packet) {
  return this->sendToMDR(packet, false);
}



byte* PrestonDuino::sendCommand(PrestonPacket* pak, bool withreply) {
  // Send a PrestonPacket to the MDR, optionally wait for a reply packet in response
  // Return an array of the reply, first element of which is an indicator of the reply type
  
  int stat = this->sendToMDR(pak); // MDR's receipt of the command packet
  
  if (withreply && stat == -1) {
    // Packet was acknowledged by MDR

    Serial.println("Command packet acknowledged, awaiting reply");
    
    if (this->waitForRcv()) {
      // Response packet was received
      Serial.println("Got a reply packet");
      stat = this->parseRcv(); // MDR's response to the command packet
      Serial.print("Status of reply packet is ");
      Serial.println(stat);
    }
  }

  Serial.println("Reply data is as follows");
  for (int i = 0; i < 100; i++) {
    Serial.println(this->rcvpacket->getData()[i], HEX);
  }
  Serial.println("End of reply array");

  static byte out[100];
  out[0] = stat; // First index of return array is the reply type
  
 // memcpy(&out[1], this->rcvpacket->getData(), this->rcvpacket->getDataLen());

  return out;
}



/*
 *  Preston-specified commands
 */


byte* PrestonDuino::mode(byte modeh, byte model) {
  byte data[2] = {modeh, model};
  PrestonPacket *pak = new PrestonPacket(0x01, data, 2);
  return this->sendCommand(pak, false);
}

byte* PrestonDuino::stat() {
  PrestonPacket *pak = new PrestonPacket(0x02);
  return this->sendCommand(pak, true);
}

byte* PrestonDuino::who() {
  PrestonPacket *pak = new PrestonPacket(0x03);
  byte* data = this->sendCommand(pak, true);
  return data[0];
}

byte* PrestonDuino::data(byte datadescription) {
  PrestonPacket *pak = new PrestonPacket(0x04, &datadescription, 1);
  return this->sendCommand(pak, true);
}

byte* PrestonDuino::data(byte* datadescription, int datalen) {
  PrestonPacket *pak = new PrestonPacket(0x04, datadescription, datalen);
  return this->sendCommand(pak, false);
}

byte* PrestonDuino::rtc(byte select, byte* data) {
  int len = 0;
  if (select == 0x01) {
    len = 8;
  } else if (select == 0x02 || select == 0x04) {
    len = 5;
  } else {
    len = 1;
  }
  PrestonPacket *pak = new PrestonPacket(0x05, data, len);
  return this->sendCommand(pak, true);
}

byte* PrestonDuino::setl(byte motors) {
  PrestonPacket *pak = new PrestonPacket(0x06, &motors, 1);
  return this->sendCommand(pak, false);
}

byte* PrestonDuino::ct() {
  PrestonPacket *pak = new PrestonPacket(0x07);
  return this->sendCommand(pak, true);
}

byte* PrestonDuino::ct(byte cameratype) {
  PrestonPacket *pak = new PrestonPacket(0x07, &cameratype, 1);
  return this->sendCommand(pak, false);
}

byte* PrestonDuino::mset(byte mseth, byte msetl) {
  byte data[2] = {mseth, msetl};
  PrestonPacket *pak = new PrestonPacket(0x08, data, 2);
  return this->sendCommand(pak, true);
}

byte* PrestonDuino::mstat(byte motor) {
  PrestonPacket *pak = new PrestonPacket(0x09, &motor, 1);
  return this->sendCommand(pak, true);
}

byte* PrestonDuino::r_s(bool rs) {
  PrestonPacket *pak = new PrestonPacket(0x0A, byte(rs), 1);
  return this->sendCommand(pak, false);
}

byte* PrestonDuino::tcstat() {
  PrestonPacket *pak = new PrestonPacket(0x0B);
  return this->sendCommand(pak, true);
}

byte* PrestonDuino::ld() {
  PrestonPacket *pak = new PrestonPacket(0x0C);
  return this->sendCommand(pak, true);
}

byte* PrestonDuino::info(byte type) {
  PrestonPacket *pak = new PrestonPacket(0x0E, &type, 0x01);
  return this->sendCommand(pak, true);
}

byte* PrestonDuino::dist(byte type, int dist) {
  //TODO properly format distance (should be 3 bytes)
  
  byte data[4] = {type, dist};
  PrestonPacket *pak = new PrestonPacket(0x10, data, 4);
  return this->sendCommand(pak, false);
}



/*  Helper Commands 
 *  Note that lenses must be properly calibrated for any of this to work
 */


byte* PrestonDuino::getLensData() {
 // if (this->rcvpacket->getMode() != 0x0C) {
    // Ensures that the data we're accessing is from an ld command, not something else
    this->ld();
 // }

  Serial.println("Lens data:");
  for (int i = 0; i < this->rcvpacket->getDataLen(); i++) {
    Serial.println(this->rcvpacket->getData()[i], HEX);
  }
  Serial.println("----");
  return this->rcvpacket->getData();
}



int PrestonDuino::getFocusDistance() {
  byte* lensdata = this->getLensData();
  byte dist[4];
  dist[3] = 0;
  Serial.println("Focus data:");
  for (int i = 0; i < 3; i++) {
    Serial.println(lensdata[i], HEX);
  }
  Serial.println("----");
  
  for (int i = 2; i >= 0; i--) {
    dist[i] = lensdata[2-i];
  }

  Serial.println("Focus data isolated:");
  for (int i = 0; i < 4; i++) {
    Serial.println(dist[i], HEX);
  }

  Serial.println("----");
  Serial.print("Focus data as int: ");
  Serial.println(((uint32_t*)dist)[0], DEC);

  return ((uint32_t*)dist)[0];
}



int PrestonDuino::getFocalLength() {
  byte* lensdata = this->getLensData();
  byte flength[2];


  Serial.println("Focal length data:");
  for (int i = 0; i < 2; i++) {
    Serial.println(lensdata[5+i], HEX);
  }
  Serial.println("----");
  
  for (int i = 1; i >= 0; i--) {
    flength[i] = lensdata[6-i];
  }

  Serial.println("Focal Length data isolated:");
  for (int i = 0; i < 2; i++) {
    Serial.println(flength[i], HEX);
  }

  Serial.println("----");
  Serial.print("Focal length data as int: ");
  Serial.println(((uint16_t*)flength)[0], DEC);

  return ((uint16_t*)flength)[0];
}



int PrestonDuino::getAperture() {
  byte* lensdata = this->getLensData();
  byte iris[2];


  Serial.println("Iris data:");
  for (int i = 0; i < 2; i++) {
    Serial.println(lensdata[3+i], HEX);
  }
  Serial.println("----");
  
  for (int i = 1; i >= 0; i--) {
    iris[i] = lensdata[4-i];
  }

  Serial.println("Iris data isolated:");
  for (int i = 0; i < 2; i++) {
    Serial.println(iris[i], HEX);
  }

  Serial.println("----");
  Serial.print("Iris data as int: ");
  Serial.println(((uint16_t*)iris)[0], DEC);

  return ((uint16_t*)iris)[0];
}



char* PrestonDuino::getLensName() {
  byte* lensinfo = this->info(0x1);

  int lensnamelen = lensinfo[0];

  Serial.print("Lens name is this long: ");
  Serial.println(lensnamelen);
  
  char lensname[lensnamelen];

  for (int i = 0; i < lensnamelen; i++) {
    Serial.println(char(lensinfo[i]));
  }

  lensname[lensnamelen] = "\0";

  for (int i = 0; i < lensnamelen; i++) {
    Serial.println(lensname[i]);
  }
  
  return lensname;
}
