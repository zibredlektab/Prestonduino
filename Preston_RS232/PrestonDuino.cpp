/*
  PrestonDuino.cpp - Library for talking to a Preston MDR 2/3/4 over RS-232
  Created by Max Batchelder, June 2020.
*/

#include "PrestonDuino.h"


PrestonDuino::PrestonDuino(HardwareSerial& serial) {
  // Open a connection to the MDR on Serial port 'serial'
  ser = &serial;
  ser->begin(115200);
  while (!ser) {
    ;
  }
}



int PrestonDuino::sendToMDR(byte* tosend, int len) {
  // >0 result is length of data received, 0 if timeout, -1 if ACK, -2 if NAK, -3 if error message
  
  int response = 0;
  
  for (int i = 0; i < len; i++) {
    ser->write(tosend[i]);
    ser->flush(); // wait for byte to finish sending
  }

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
      return this->rcv();
    }
  }

  return false;
}




bool PrestonDuino::rcv() {
  // check if available data, if so check the first char for following:
  //  ACK/NAK: add to rcv and return
  //  STX: add all bytes to rcv until ETX is found (if second STX is found, see next line)
  //  Anything else: treat as garbage data. Send NAK to MDR and toss the buffer
  // return true if we got usable data, false if not
  
  static int i = 0;
  char currentchar;
  
  while (ser->available() > 0 && !this->rcvreadytoprocess) { //only receive if there is something to be received and no data in our buffer
    currentchar = ser->read();
    if (this->rcving) {
      this->rcvbuf[i++] = currentchar;
      if (currentchar == ETX) { // We have received etx, stop reading
        this->rcving = false;
        this->rcvlen = i;
        this->rcvreadytoprocess = true;
        this->sendACK(); // Polite thing to do
        return true;
      } else if (currentchar == STX || currentchar == NAK || currentchar == ACK) {
        // This should not happen, and means that our packet integrity is compromised (we started reading into a new packet somehow)
        break;
      }
      
    } else if (currentchar == STX) {
      // STX received from MDR
      i = 0;
      this->rcvbuf[i++] = currentchar;
      this->rcving = true;
      
    } else if (currentchar == NAK || currentchar == ACK) {
      this->rcvbuf[0] = currentchar;
      return true;
  
    } else {
      break;
    }
  }
  
  // something inexplicable was received from MDR
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




int PrestonDuino::parseRcv() {
  // >0 result is length of data received, -1 if ACK, -2 if error, -3 if NAK
  
  int response = 0;
  
  if (this->rcvreadytoprocess) {
    if (rcvbuf[0] == ACK) {
      response = -1;
    } else if (rcvbuf[0] == NAK) {
      response = -2;
    } else if (rcvbuf[0] == STX) {
      PrestonPacket *pak = new PrestonPacket(this->rcvbuf, this->rcvlen);
      this->rcvpacket = pak;
      if (this->rcvpacket->getMode() == 0x11) {
        // received packet is an error message
        response = -3;
      } else {
        response = rcvlen;
      }
    }
    this->rcvreadytoprocess = false;
  }

  return response;
}



void PrestonDuino::sendACK() {
  this->sendToMDR(ACK, 1);
}



void PrestonDuino::sendNAK() {
  this->sendToMDR(NAK, 1);
}



int PrestonDuino::sendToMDR(PrestonPacket* packet, bool retry) {
  // Send a PrestonPacket to the MDR, byte by byte. If "retry" is true, packet will be re-sent upon timout or NAK, up to 3 times
  int packetlen = packet->getPacketLen();
  byte* packetbytes = packet->getPacket();
  int response = this->sendToMDR(packetbytes, packetlen);

  if (response = -2) {
    // TODO retry
  }

  return response;

}



int PrestonDuino::sendToMDR(PrestonPacket* packet) {
  return this->sendToMDR(packet, false);
}



bool PrestonDuino::command(PrestonPacket* pak) {
  if (this->sendToMDR(pak) == ACK) {
    delete pak;
    return true;
  }
  
  return false;
}



byte* PrestonDuino::commandWithReply(PrestonPacket* pak) {
  if (this->sendToMDR(pak) == ACK) {
    // Packet was acknowledged by MDR
    if (this->waitForRcv()) {
      // Response packet was received
      if (this->parseRcv() > 0) {
        // Response was successfully 
        return this->rcvpacket->getData();
      }
    }
  }

  delete pak;
}



void PrestonDuino::mode(byte modeh, byte model) {
  byte data[2] = {modeh, model};
  PrestonPacket *pak = new PrestonPacket(0x01, data, 2);
  this->command(pak);
}



byte* PrestonDuino::stat() {
  PrestonPacket *pak = new PrestonPacket(0x02, NULL, 0);
  return this->commandWithReply(pak);

}

byte PrestonDuino::who() {
  PrestonPacket *pak = new PrestonPacket(0x03, NULL, 0);
  byte* data = this->commandWithReply(pak);
  return data[0];
}

byte* PrestonDuino::data(byte datadescription) {
  PrestonPacket *pak = new PrestonPacket(0x04, datadescription, 1);
  return this->commandWithReply(pak);
}

void PrestonDuino::data(byte* datadescription, int datalen) {
  PrestonPacket *pak = new PrestonPacket(0x04, datadescription, datalen);
  this->command(pak);
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
  return this->commandWithReply(pak);
}

void PrestonDuino::setl(byte motors) {
  PrestonPacket *pak = new PrestonPacket(0x06, motors, 1);
  this->command(pak);
}

byte PrestonDuino::ct() {
  PrestonPacket *pak = new PrestonPacket(0x07, NULL, 0);
  return this->commandWithReply(pak);
}

void PrestonDuino::ct(byte cameratype) {
  PrestonPacket *pak = new PrestonPacket(0x07, cameratype, 1);
  this->command(pak);
}

byte* PrestonDuino::mset(byte mseth, byte msetl) {
  byte data[2] = {mseth, msetl};
  PrestonPacket *pak = new PrestonPacket(0x08, data, 2);
  return this->commandWithReply(pak);
}

byte* PrestonDuino::mstat(byte motor) {
  PrestonPacket *pak = new PrestonPacket(0x09, motor, 1);
  return this->commandWithReply(pak);
}

void PrestonDuino::r_s(bool rs) {
  PrestonPacket *pak = new PrestonPacket(0x0A, byte(rs), 1);
  this->command(pak);
}

byte PrestonDuino::tcstat() {
  PrestonPacket *pak = new PrestonPacket(0x0B, NULL, 0);
  return this->commandWithReply(pak);
}

byte* PrestonDuino::ld() {
  PrestonPacket *pak = new PrestonPacket(0x0C, NULL, 0);
  return this->commandWithReply(pak);
}

byte* PrestonDuino::info(byte type) {
  PrestonPacket *pak = new PrestonPacket(0x0E, type, 1);
  byte* reply = this->commandWithReply(pak);
  byte len = this->rcvpacket->getDataLen();
  byte out[len+1];
  out[0] = len;
  
  for (int i = 0; i < len; i++) {
    out[i+1] = reply[i];
  }
  
  return reply;
}

void PrestonDuino::dist(byte type, int dist) {
  //TODO properly format distance (should be 3 bytes)
  
  byte data[4] = {type, dist};
  PrestonPacket *pak = new PrestonPacket(0x10, data, 4);
  this->command(pak);
}
