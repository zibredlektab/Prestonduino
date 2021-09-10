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

  //Serial.println("MDR connection is open");
  // TODO slightly more robust way of checking for connection?
  this->connectionopen = true;

  // TODO get & store MDR number 
  /*byte* mdrinfo = this->info(0x0);

  int infolen = mdrinfo[0];

  this->mdrtype = mdrinfo[5];
  ////Serial.println(this->mdrtype);
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
    //Serial.print("Sending 0x");
    //Serial.println(tosend[i], HEX);
    ser->write(tosend[i]);
    ser->flush(); // wait for byte to finish sending
  }

  
  int response = 0;
  
  if (this->waitForRcv()) {
    // response received
    //Serial.println("Got a response");
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
  //Serial.println("Timeout");
  return false;
}





bool PrestonDuino::rcv() {
  /* Check if there is available data, then check the first char for following:
   *    ACK/NAK: add to rcv and return
   *    STX: add all bytes to rcv until ETX is found (if second STX etc is found, see next line)
   *    Anything else: treat as garbage data. Send NAK to MDR and toss the buffer
   * Returns true if we got usable data, false if not
   */
  
  static int rcvi = 0;
  static char currentchar;
  bool gotgoodreply = false;

  while (ser->available() > 0 && !this->rcvreadytoprocess) { //only receive if there is something to be received and no data in our buffer
    
    currentchar = ser->read();
    delay(1);
    //Serial.print("Received ");
    //Serial.print(currentchar, HEX);
    //Serial.println(" from MDR");
    
    if (this->rcving) {
      // Currently in the process of receiving an incoming packet
      this->rcvbuf[rcvi++] = currentchar; // Add current incoming character to rcvbuf
      if (currentchar == ETX) {
        // We have received ETX, stop reading
        this->rcving = false;
        this->rcvlen = rcvi;
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
      rcvi = 0;
      this->rcvbuf[rcvi++] = currentchar;
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
  
  //if (this->rcvreadytoprocess) {
    //Serial.println("Starting to process rcv");
    // TODO This should always be true, parseRcv() shouldn't be called unless we're ready to process...
    if (rcvbuf[0] == ACK) {
      // Reply is ACK
      //Serial.println("Rcv is ACK");
      response = -1;
      
    } else if (rcvbuf[0] == NAK) {
      // Reply is NAK
      //Serial.println("Rcv is NAK");
      response = -2;
      
    } else if (rcvbuf[0] == STX) {
      //Serial.println("Rcv is a packet");
      // Reply is a packet
      
      if (!this->firstpacket) {
        // Delete the previously-received packet before making a new one
        delete this->rcvpacket;
      } else {
        this->firstpacket = false;
      }
      PrestonPacket *pak = new PrestonPacket(this->rcvbuf, this->rcvlen);
      this->rcvpacket = pak;
      
      if (this->rcvpacket->getMode() == 0x11) {
        // Reply is an error message
        //Serial.println("Rcv packet is an error");
        response = -3;
        // TODO actual error handling
        
      } else {
        // Reply is a response packet
        response = this->rcvpacket->getDataLen();
        //Serial.print("Packet data is ");
        //Serial.print(response);
        //Serial.println(" bytes long");
      }
    }

    // Done processing
    //Serial.println("Done processing rcv");
    this->rcvreadytoprocess = false;
  //}

  return response;
}



void PrestonDuino::sendACK() {
  // Send ACK, don't wait for a reply (there shouldn't be one)
  //Serial.println("Sending ACK to MDR");
  ser->write(ACK);
  ser->flush();
}



void PrestonDuino::sendNAK() {
  // Send NAK, don't wait for a reply (that will be handled elsewhere)
  //Serial.println("Sending NAK to MDR");
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


  //Serial.print("Response status is ");
  //Serial.println(response);
  delete packet;
  return response;

}



int PrestonDuino::sendToMDR(PrestonPacket* packet) {
  return this->sendToMDR(packet, false);
}



command_reply PrestonDuino::sendCommand(PrestonPacket* pak, bool withreply) {
  // Send a PrestonPacket to the MDR, optionally wait for a reply packet in response
  // Return an array of the reply, first element of which is an indicator of the reply type
  int stat = this->sendToMDR(pak); // MDR's receipt of the command packet
  this->reply.replystatus = stat;
  this->reply.data = NULL;
  
  //Serial.print("Immediate response to command is ");
  //Serial.println(stat);

  if (stat == 0 || stat < -1 || (stat == -1 && !withreply)) {
    // Either: timeout, NAK, or ACK for a command that doesn't need a reply
    return this->reply;
  } else if (stat == -1 && withreply) {
    // Packet was acknowledged by MDR, but we still need a reply
  
    //Serial.println("Command packet acknowledged, awaiting reply");
    if (this->waitForRcv()) {
      // Response packet was received
      //Serial.println("Got a reply packet");
      stat = this->parseRcv(); // MDR's response to the command packet
      //Serial.print("Status of reply packet is ");
      //Serial.println(stat);
      this->reply.replystatus = stat;
    } else {
      //Serial.println("Reply packet never came");
    }
  }


  //Serial.println("Reply data is as follows");
  for (int i = 0; i < this->rcvpacket->getDataLen(); i++) {
    //Serial.println(this->rcvpacket->getData()[i], HEX);
  }
  //Serial.println("End of reply array");

  this->reply.data = this->rcvpacket->getData();

  return this->reply;
}


command_reply PrestonDuino::getReply() {
  return this->reply;
}


/*
 *  Preston-specified commands
 */


command_reply PrestonDuino::mode(byte modeh, byte model) {
  byte data[2] = {modeh, model};
  PrestonPacket *pak = new PrestonPacket(0x01, data, 2);
  return this->sendCommand(pak, false);
}

command_reply PrestonDuino::stat() {
  PrestonPacket *pak = new PrestonPacket(0x02);
  return this->sendCommand(pak, true);
}

command_reply PrestonDuino::who() {
  PrestonPacket *pak = new PrestonPacket(0x03);
  command_reply data = this->sendCommand(pak, true);
  return data;
}

command_reply PrestonDuino::data(byte datadescription) {
  PrestonPacket *pak = new PrestonPacket(0x04, &datadescription, 1);
  return this->sendCommand(pak, true);
}

command_reply PrestonDuino::data(byte* datadescription, int datalen) {
  PrestonPacket *pak = new PrestonPacket(0x04, datadescription, datalen);
  return this->sendCommand(pak, false);
}

command_reply PrestonDuino::rtc(byte select, byte* data) {
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

command_reply PrestonDuino::setl(byte motors) {
  PrestonPacket *pak = new PrestonPacket(0x06, &motors, 1);
  return this->sendCommand(pak, false);
}

command_reply PrestonDuino::ct() {
  PrestonPacket *pak = new PrestonPacket(0x07);
  return this->sendCommand(pak, true);
}

command_reply PrestonDuino::ct(byte cameratype) {
  PrestonPacket *pak = new PrestonPacket(0x07, &cameratype, 1);
  return this->sendCommand(pak, false);
}

command_reply PrestonDuino::mset(byte mseth, byte msetl) {
  byte data[2] = {mseth, msetl};
  PrestonPacket *pak = new PrestonPacket(0x08, data, 2);
  return this->sendCommand(pak, true);
}

command_reply PrestonDuino::mstat(byte motor) {
  PrestonPacket *pak = new PrestonPacket(0x09, &motor, 1);
  return this->sendCommand(pak, true);
}

command_reply PrestonDuino::r_s(bool rs) {
  PrestonPacket *pak = new PrestonPacket(0x0A, (byte*)rs, 1);
  return this->sendCommand(pak, false);
}

command_reply PrestonDuino::tcstat() {
  PrestonPacket *pak = new PrestonPacket(0x0B);
  return this->sendCommand(pak, true);
}

command_reply PrestonDuino::ld() {
  PrestonPacket *pak = new PrestonPacket(0x0C);
  return this->sendCommand(pak, true);
}

command_reply PrestonDuino::info(byte type) {
  PrestonPacket *pak = new PrestonPacket(0x0E, &type, 0x01);
  return this->sendCommand(pak, true);
}

command_reply PrestonDuino::dist(byte type, int dist) {
  //TODO properly format distance (should be 3 bytes)
  
  byte data[4] = {type, dist};
  PrestonPacket *pak = new PrestonPacket(0x10, data, 4);
  return this->sendCommand(pak, false);
}



/*  Helper Commands 
 *  Note that lenses must be properly calibrated for any of this to work
 */


const byte* PrestonDuino::getLensData() {
  command_reply reply = this->ld();

  if (reply.replystatus > 0) {
    //Serial.print(F("Got valid lens data: "));
    for (int i = 0; i < reply.replystatus; i++) {
      //Serial.print(reply.data[i], HEX);
      //Serial.print(" ");
    }
    //Serial.println();
    return reply.data;
  } else {
    //Serial.println(F("Didn't get valid lens data this time either"));
    return this->dummydata;
  }
}



uint32_t PrestonDuino::getFocusDistance() {
  const byte* lensdata = this->getLensData();
  static uint32_t distint;
  byte dist[4];
  dist[3] = 0;
  
  for (int i = 2; i >= 0; i--) {
    dist[i] = lensdata[2-i];
  }

  distint = ((uint32_t*)dist)[0];

  return distint;
}



int PrestonDuino::getFocalLength() {
  const byte* lensdata = this->getLensData();
  byte flength[2];


  //Serial.println("Focal length data:");
  for (int i = 0; i < 2; i++) {
    //Serial.println(lensdata[5+i], HEX);
  }
  //Serial.println("----");
  
  for (int i = 1; i >= 0; i--) {
    flength[i] = lensdata[6-i];
  }

  //Serial.println("Focal Length data isolated:");
  for (int i = 0; i < 2; i++) {
    //Serial.println(flength[i], HEX);
  }

  //Serial.println("----");
  //Serial.print("Focal length data as int: ");
  //Serial.println(((uint16_t*)flength)[0], DEC);

  return ((uint16_t*)flength)[0];
}



int PrestonDuino::getAperture() {
  const byte* lensdata = this->getLensData();
  byte iris[2];
  //Serial.print(F("Lens data as reported: "));
  for (int i = 0; i < 7; i++) {
    //Serial.print(lensdata[i]);
  }
  //Serial.println();


  //Serial.println("Iris data:");
  for (int i = 0; i < 2; i++) {
    //Serial.println(lensdata[3+i], HEX);
  }
  //Serial.println("----");
  
  for (int i = 1; i >= 0; i--) {
    iris[i] = lensdata[4-i];
  }

  //Serial.println("Iris data isolated:");
  for (int i = 0; i < 2; i++) {
    //Serial.println(iris[i], HEX);
  }

  //Serial.println("----");
  //Serial.print("Iris data as int: ");
  //Serial.println(((uint16_t*)iris)[0], DEC);

  return ((uint16_t*)iris)[0];
}


char* PrestonDuino::getLensName() {
  command_reply lensinfo = this->info(0x1);

  int lensnamelen = lensinfo.replystatus-1; //first two bytes in the reply to info() are the type of info

  //Serial.print("Lens name is this long: ");
  //Serial.println(lensnamelen);
  
  this->lensname[0] = lensnamelen+1; // first index is length of name
  for (int i = 1; i < lensnamelen; i++) {
    this->lensname[i] = lensinfo.data[i+1]; //1+1 ensures that we skip the first two elements of data, which are the info type
  }



  //Serial.print("Lens name: ");
  //Serial.print("[0x");
  //Serial.print(this->lensname[0], HEX);
  //Serial.print("]");
  for (int i = 1; i < lensnamelen; i++) {
    //Serial.print(this->lensname[i]);
  }
  //Serial.println();
  
  return this->lensname;
}




command_reply PrestonDuino::setLensData(uint32_t dist, uint16_t aperture, uint16_t flength) {
  // dist is actually a 0-padded little-endian 24-bit int

  byte newlensdata[8];
  newlensdata[0] = 0x7; //FIZ data


  // Focus

  //Serial.print("Input dist is 0x");
  //Serial.println(dist, HEX);

  newlensdata[3] = (dist >> 0);
  newlensdata[2] = (dist >> 8);
  newlensdata[1] = (dist >> 16);


  // Iris

  //Serial.print("Input iris is ");
  //Serial.println(aperture);

  newlensdata[5] = (aperture >> 0);
  newlensdata[4] = (aperture >> 8);


  // Zoom

  //Serial.print("Input zoom is ");
  //Serial.println(flength);

  newlensdata[7] = (flength >> 0);
  newlensdata[6] = (flength >> 8);


  //Serial.println("New lens data packet: ");
  for (int i = 0; i < 8; i++){
    //Serial.println(newlensdata[i], HEX);
  }
  
  return this->data(newlensdata, 8);
  
}




void PrestonDuino::setMDRTimeout(int newtimeout) {
  this->timeout = newtimeout;
}
