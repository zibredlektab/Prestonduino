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

  if (this->waitForData()) {
    // response received
    response = this->parseData();
  }

  return response;
}



bool PrestonDuino::waitForData() {
  this->time_now = millis();
  
  while(millis() <= this->time_now + this->timeout) {
    if (ser->available()) {
      return this->rcvData();
    }
  }

  return false;
}




bool PrestonDuino::rcvData() {
  // check if available data, if so check the first char for following:
  //  ACK/NAK: add to rcvdata and return
  //  STX: add all bytes to rcvdata until ETX is found (if second STX is found, see next line)
  //  Anything else: treat as garbage data. Send NAK to MDR and toss the buffer
  // return true if we got usable data, false if not
  
  static int i = 0;
  char currentchar;
  
  while (ser->available() > 0 && !this->rcvreadytoprocess) { //only receive if there is something to be received and no data in our buffer
    currentchar = ser->read();
    if (this->rcving) {
      this->rcvdata[i++] = currentchar;
      if (currentchar == ETX) { // We have received etx, stop reading
        this->rcving = false;
        this->rcvdatalen = i;
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
      this->rcvdata[i++] = currentchar;
      this->rcving = true;
      
    } else if (currentchar == NAK || currentchar == ACK) {
      this->rcvdata[0] = currentchar;
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
  this->rcvdatalen = 0;
  this->rcvreadytoprocess = false;
  
  return false; // :(

}




int PrestonDuino::parseData() {
  // >0 result is length of data received, -1 if ACK, -2 if error, -3 if NAK
  
  int response = 0;
  
  if (this->rcvreadytoprocess) {
    if (rcvdata[0] == ACK) {
      response = -1;
    } else if (rcvdata[0] == NAK) {
      response = -2;
    } else if (rcvdata[0] == STX) {
      PrestonPacket *pak = new PrestonPacket(this->rcvdata, this->rcvdatalen);
      this->rcvpacket = pak;
      if (this->rcvpacket->getMode() == 0x11) {
        // received packet is an error message
        response = -3;
      } else {
        response = rcvdatalen;
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
  int packetlen = packet->getPacketLength();
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
    if (this->waitForData()) {
      // Response packet was received
      if (this->parseData() > 0) {
        // Response was successfully 
        return this->rcvpacket->getData();
      }
    }
  }

  delete pak;
}



void PrestonDuino::mode(byte modeh, byte model) {
  byte data[2];
  data[0] = modeh;
  data[1] = model;
  PrestonPacket *pak = new PrestonPacket(0x01, data, 2);
  this->command(pak);
}



byte* PrestonDuino::stat() {
  PrestonPacket *pak = new PrestonPacket(0x02, NULL, 0);
  return this->commandWithReply(pak);

}
