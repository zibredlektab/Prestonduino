/*
  PrestonDuino.cpp - Library for talking to a Preston MDR 2/3/4 over RS-232
  Created by Max Batchelder, June 2020.
*/

#include "PrestonDuino.h"


PrestonDuino::PrestonDuino(HardwareSerial& serial) {
  // Open a connection to the MDR on Serial port 'serial'
  ser = &serial;
  ser->begin(115200);

  //Serial.println("Connection to MDR theoretically open, testing with WHO");

  while (this->who() == 0x0) {
    ;
  }
  
  this->connectionopen = true;
  //Serial.println("--Connected to MDR--");
 
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
  // >0 result is length of data received, 0 if timeout, -1 if ACK, -2 if NAK, -3 if error message
  //Serial.print("Initiating new send action, total of ");
  //Serial.print(len);
  //Serial.println(" bytes");
  for (int i = 0; i < len; i++) {
    //Serial.print("Sending ");
    //Serial.print(tosend[i], HEX);
    //Serial.println(" to MDR");
    
    ser->write(tosend[i]);
    ser->flush(); // wait for byte to finish sending
  }
  //Serial.println("Send complete");

  
  int response = 0;
  
  if (this->waitForRcv()) {
    // response received
    response = this->parseRcv();
  } else {
    //Serial.println("No response received");
  }

  return response;
}



bool PrestonDuino::waitForRcv() {
  //Serial.print("Waiting for response, the time is now ");
  this->time_now = millis();
  //Serial.println(this->time_now);
  
  while(millis() <= this->time_now + this->timeout) {
    if (ser->available()) {
      //Serial.print("Response is available, time is now ");
      //Serial.println(millis());
      return this->rcv();
    }
  }
  
  //Serial.print("Timeout, the time is now ");
  //Serial.println(millis());
  return false;
}




bool PrestonDuino::rcv() {
  // check if available data, if so check the first char for following:
  //  ACK/NAK: add to rcv and return
  //  STX: add all bytes to rcv until ETX is found (if second STX etc is found, see next line)
  //  Anything else: treat as garbage data. Send NAK to MDR and toss the buffer
  // return true if we got usable data, false if not
  
  static int i = 0;
  char currentchar;
  bool out = false;

  
  while (ser->available() > 0 && !this->rcvreadytoprocess) { //only receive if there is something to be received and no data in our buffer
    currentchar = ser->read();
    //Serial.print("Received ");
    //Serial.print(char(currentchar));
    //Serial.println(" from MDR");
    if (this->rcving) {
      this->rcvbuf[i++] = currentchar;
      if (currentchar == ETX) { // We have received etx, stop reading
        //Serial.println("Char is ETX");
        this->rcving = false;
        this->rcvlen = i;
        this->rcvreadytoprocess = true;
       // this->sendACK(); // Polite thing to do
        out = true;
        break;
      } else if (currentchar == STX || currentchar == NAK || currentchar == ACK) {
        //Serial.println("Found a new packet in the middle of the current packet...that's bad");
        // This should not happen, and means that our packet integrity is compromised (we started reading into a new packet somehow)
        break;
      }
      
    } else if (currentchar == STX) {
      // STX received from MDR
      //Serial.println("Char is STX");
      i = 0;
      this->rcvbuf[i++] = currentchar;
      this->rcving = true;
      
    } else if (currentchar == NAK || currentchar == ACK) {
      //Serial.println("Char is either NAK or ACK");
      this->rcvbuf[0] = currentchar;
      this->rcvreadytoprocess = true;
      this->rcvlen = 1;
      this->rcving = false;
      out = true;
      break;
  
    } else {
      break;
    }
  }

  if (out) {
    return true;
  } else {
    // something inexplicable was received from MDR
    //Serial.println("I have no idea what was just received from the MDR");
    while (ser->available()) {
      // dump the buffer
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
  //Serial.println("Parsing response from MDR");
  int response = 0;
  
  if (this->rcvreadytoprocess) {
    if (rcvbuf[0] == ACK) {
      //Serial.println("Reply is ACK");
      response = -1;
    } else if (rcvbuf[0] == NAK) {
      //Serial.println("Reply is NAK");
      response = -2;
    } else if (rcvbuf[0] == STX) {
      PrestonPacket *pak = new PrestonPacket(this->rcvbuf, this->rcvlen);
      this->rcvpacket = pak;
      if (this->rcvpacket->getMode() == 0x11) {
        //Serial.println("Reply is an error packet");
        // received packet is an error message
        response = -3;
      } else {
        //Serial.println("Reply is a data packet");
        response = rcvlen;
      }
    }
    this->rcvreadytoprocess = false;
  }

  return response;
}



void PrestonDuino::sendACK() {
  byte ack[1] = {ACK};
  this->sendToMDR(ack, 1);
}



void PrestonDuino::sendNAK() {
  byte nak[1] = {NAK};
  this->sendToMDR(nak, 1);
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

  return response;

}



int PrestonDuino::sendToMDR(PrestonPacket* packet) {
  return this->sendToMDR(packet, false);
}



byte* PrestonDuino::sendCommand(PrestonPacket* pak, bool withreply) {
  // Sending a sendCommand that does expect a reply
  int8_t stat = this->sendToMDR(pak);
  if (withreply && stat == -1) {
    // Packet was acknowledged by MDR
    if (this->waitForRcv()) {
      // Response packet was received
      stat = this->parseRcv();
    }
  }
  
  delete pak;

  byte out[this->rcvpacket->getDataLen()+1];
  out[0] = stat;
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
  PrestonPacket *pak = new PrestonPacket(0x02, NULL, 0);
  return this->sendCommand(pak, true);
}

byte* PrestonDuino::who() {
  PrestonPacket *pak = new PrestonPacket(0x03, NULL, 0);
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
  PrestonPacket *pak = new PrestonPacket(0x07, NULL, 0);
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
  PrestonPacket *pak = new PrestonPacket(0x0B, NULL, 0);
  return this->sendCommand(pak, true);
}

byte* PrestonDuino::ld() {
  PrestonPacket *pak = new PrestonPacket(0x0C, NULL, 0);
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
