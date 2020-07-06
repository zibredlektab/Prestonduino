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

  while (this->who()[0] == 0) {
    ; // wait until we get a reply
  }

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
    
    ser->write(tosend[i]);
    ser->flush(); // wait for byte to finish sending
  }

  
  int response = 0;
  
  if (this->waitForRcv()) {
    // response received
    response = this->parseRcv();
  }

  return response;
}





bool PrestonDuino::waitForRcv() {
  this->time_now = millis();
  
  while(millis() <= this->time_now + this->timeout) {
    if (ser->available()) {
      // Response is available
      return this->rcv();
    }
  }

  // If it gets this far, there was a timeout
  
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
        response = rcvlen;
      }
    }

    // Done processing
    this->rcvreadytoprocess = false;
  }

  return response;
}



void PrestonDuino::sendACK() {
  this->sendToMDR({ACK}, 1);
}



void PrestonDuino::sendNAK() {
  this->sendToMDR({NAK}, 1);
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

  delete packet;
  return response;

}



int PrestonDuino::sendToMDR(PrestonPacket* packet) {
  return this->sendToMDR(packet, false);
}



byte* PrestonDuino::sendCommand(PrestonPacket* pak, bool withreply) {
  // Send a PrestonPacket to the MDR, optionally wait for a reply packet in response
  // Return an array of the reply, first element of which is an indicator of the reply type
  
  int8_t stat = this->sendToMDR(pak); // MDR's receipt of the command packet
  
  if (withreply && stat == -1) {
    // Packet was acknowledged by MDR
    
    if (this->waitForRcv()) {
      // Response packet was received
      stat = this->parseRcv(); // MDR's response to the command packet
    }
  }

  byte out[this->rcvpacket->getDataLen()+1];
  out[0] = stat; // First index of return array is the reply type
  memcpy(out[1], this->rcvpacket->getData(), this->rcvpacket->getDataLen());
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
  byte* lensdata;

  if (this->rcvpacket->getMode() == 0x0C) {
    lensdata = this->rcvpacket->getData();
  } else {
    lensdata = this->ld();
  }

  return lensdata;
}



int PrestonDuino::getFocusDistance() {
  byte* lensdata = this->getLensData();
  byte dist[4];
  memcpy(&dist[1], &lensdata, 3);
  return uint32_t(dist);
}



int PrestonDuino::getFocalLength() {
  byte* lensdata = this->getLensData();
  byte flength[2];
  memcpy(&flength, &lensdata[3], 2);
  return uint16_t(flength);
}



int PrestonDuino::getAperture() {
  byte* lensdata = this->getLensData();
  byte aperture[2];
  memcpy(&aperture, &lensdata[5], 2);
  return uint16_t(aperture);
}



char* PrestonDuino::getLensName() {
  byte* lensinfo;

  if (this->rcvpacket->getMode() == 0x0E) {
    lensinfo = this->rcvpacket->getData();
  } else {
    lensinfo = this->info(0x1);
  }

  char lensname[15];

  memcpy(&lensname, &lensinfo[1], 14);

  lensname[14] = "\0";
  
  return lensname;
}
