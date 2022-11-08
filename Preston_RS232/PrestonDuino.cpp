/*
  PrestonDuino.cpp - Library for talking to a Preston MDR 2/3/4 over RS-232
  Created by Max Batchelder, June 2020.
*/

#include "PrestonDuino.h"
#include "wiring_private.h" // pinPeripheral() function

PrestonDuino::PrestonDuino(HardwareSerial& serial) {
  // Open a connection to the MDR on Serial port 'serial'

  ser = &serial;
  ser->begin(115200);
  delay(1000);

  Serial.print("Establishing connection to MDR...");

  // TODO get & store MDR number 
  command_reply mdrinfo = this->info(0x1);
  //Serial.print("Status of reply from info command: ");
  //Serial.println(mdrinfo.replystatus);
  
  long int time = millis();
/*
  while (mdrinfo.replystatus == 0) {
    // TODO slightly more robust way of checking for connection?
    mdrinfo = this->info(0x1);
    if (time + 100 < millis()) {
      Serial.print(".");
      time = millis();
    }
  }*/

  this->connectionopen = true;
  Serial.println("Connection open.");
  Serial.print("Current lens: ");
  for(int i = 2; i < mdrinfo.replystatus; i++){ // skip the first two characters for info(), they describe the type of info - in this case, lens name
    Serial.print((char)mdrinfo.data[i]);
  }
  Serial.println();

  this->mode(0x1,0x0); // start streaming mode

  this->data(0x13); // request high res focus and iris data
  
}


bool PrestonDuino::readyToSend() {
  return this->connectionopen;
}

void PrestonDuino::onLoop () {
  if (this->rcv()) {
    this->parseRcv();
  }
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
    response = this->parseRcv();
  }

  return response;
}




// A reply is expected, so wait for one until the timeout threshold has passed
// Return true if we got a response, false if we timed out
bool PrestonDuino::waitForRcv() {
  this->time_now = millis();
  //Serial.print("Waiting for response.");
  
  while(millis() <= this->time_now + this->timeout) { // continue checking for recieved data as long as the timeout period hasn't elapsed
    if (this->rcv()) {
      //Serial.println("Got a response");
      return true;
    } else if (millis() % 1000 == 0){
      //Serial.print(".");
    }
  }

  // If it gets this far, there was a timeout

  Serial.println("Timeout");
  return false;
}





bool PrestonDuino::rcv() {
  /* Check if there is available data, then check the first char for following:
   *    ACK/NAK: add to rcv and return
   *    STX: add all bytes to rcvbuf until ETX is found (if second control character is found, see next line)
   *    Anything else: treat as garbage data. Send NAK to MDR and toss the buffer
   * Returns true if we got usable data, false if not
   */
  
  int rcvi = 0; // index of currently processing character in the received message
  char currentchar; // currently processing character
  int gotgoodreply = -1; // flag that message was a properly formatted preston reply: -1 is not checked yet, 0 is bad reply or timeout, 1 is good reply


  if (ser->available()) { //only receive if there is something to be received and no data currently being parsed
    currentchar = ser->read();
    this->rcvbuf[rcvi] = currentchar; 
    //Serial.print("\nStart : 0x");
    //Serial.print(this->rcvbuf[rcvi], HEX);
    rcvi++;
    
    if (currentchar == STX) {
      // STX received from MDR
      //Serial.print(" (STX)");

      while (gotgoodreply == -1) { // iterate as long as there we have not received a full good or bad reply

        if (!ser->available()) {
          // if nothing is available in ser, keep checking for a few ms to make sure there isn't just a delay in the uart
          bool seravailable = false;
          long int timewaitstart = millis();
          while (timewaitstart + 2 > millis() && !seravailable) {
            if (ser->available() > 0) {
              // something is available after all
              seravailable = true;
            }
          }
          if (!seravailable) {
            // timeout waiting for another byte in this packet, abort
            gotgoodreply = 0;
            break;
          }
        }

        currentchar = ser->read();
        this->rcvbuf[rcvi] = currentchar; // Add current incoming character to rcvbuf

        //Serial.print(" 0x");
        //Serial.print(this->rcvbuf[rcvi], HEX);

        rcvi++;

        if (currentchar == ETX) {
          // We have received ETX, stop reading
          this->rcvlen = rcvi;
          gotgoodreply = 1;
          //Serial.println(" (ETX) : End");
          
          this->sendACK(); // Polite thing to do
          
          for (int i = rcvi; i < 100; i++) {
            this->rcvbuf[i] = 0; // Zero out the rest of the buffer
          }

          break;
          
        } else if (currentchar == STX || currentchar == NAK || currentchar == ACK) {
          // This should not happen, and means that our packet integrity is compromised (we somehow started reading into a new packet)
          //Serial.println(" (unexpected control character!)");
          gotgoodreply = 0;
          break;
          
        } else {
          //delay(1); // TODO seems to be necessary to stop the serial port from tripping all over itself
          //Serial.print(" (");
          //Serial.print(currentchar);
          //Serial.print(")");
        }

      }
      
    } else if (currentchar == NAK || currentchar == ACK) {
      // Incoming character is either NAK or ACK, right now we don't care which
      //Serial.println(" (ACK or NAK)");
      this->rcvbuf[0] = currentchar;
      this->rcvlen = 1;
      gotgoodreply = 1; // NAK still counts as a good reply in this context!
  
    } else {
      // Incoming character is some kind of garbage (not a control character, but we're not in the middle of a packet yet)
      //Serial.println(" (garbage data!)");
      gotgoodreply = 0;
    }
  }

  if (gotgoodreply == 1) {
    return true;
  } else {
    // Some kind of garbage was received from the MDR
    //Serial.println("Did not receive a valid message from the MDR, dumping...");
    /*
    while (ser->available()) {
      // Dump the buffer
      ser->read();
      // TODO this could result in well-structured data being tossed as well, if a good packet arrives while we are processing a bad packet.
      //Serial.print("0x");
      //Serial.print(ser->read(), HEX);
      //Serial.print(" ");
    }*/
    //Serial.println();

    if (rcvi > 0) {
      // we received some data, but did not get a good reply
      this->sendNAK(); // so let the MDR know we don't understand
    }

    this->rcvlen = 0;
  
    return false;
  }

}




int PrestonDuino::parseRcv() {
  /* Step through rcvbuf and figure out what data we got.
   * -1 result is ACK
   * -2 is error
   * -3 is NAK
   * >0 result is length of data received, and this->rcvpacket should be populated with the resulting data
   */
  int response = 0;
  
  //Serial.println("Starting to process rcvbuf");

  if (this->rcvbuf[0] == ACK) {
    // Reply is ACK
    //Serial.println("Rcvbuf is ACK");
    response = -1;
    
  } else if (this->rcvbuf[0] == NAK) {
    // Reply is NAK
    //Serial.println("Rcvbuf is NAK");
    response = -2;
    
  } else if (this->rcvbuf[0] == STX) {
    //Serial.println("Rcvbuf is a packet");
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
      // TODO actual error handling?
      
    } else {
      // Reply is a response packet
      response = this->rcvpacket->getDataLen();
      //Serial.print("Packet data is ");
      //Serial.print(response);
      //Serial.println(" bytes long");
      if (this->rcvpacket->getMode() == 0x4) {
        // Packet is a response to a request for data
        int dataindex = 0;
        byte datadescriptor = this->rcvpacket->getData()[dataindex++]; // This byte describes the kind of data being provided
        if (datadescriptor & 1) {
          // has iris

          this->iris = this->rcvpacket->getData()[dataindex++] << 8;
          this->iris += this->rcvpacket->getData()[dataindex++];
          //Serial.print("iris: ");
          //Serial.print(this->iris);
        }
        if (datadescriptor & 2) {
          // has focus

          this->focus = this->rcvpacket->getData()[dataindex++] << 8;
          this->focus += this->rcvpacket->getData()[dataindex++];

          //Serial.print(" focus: ");
          //Serial.print(this->focus);
          //Serial.print("mm");
/*          double focusfracft = this->focus / 304.8; // for displaying in ft instead of mm
          double focusin = int(focusfracft * 100) % 100;
          focusin /= 100;
          int focusft = focusfracft - focusin;

          focusin *= 12;
          
          /*double focusin = this->focus % 100; // for low-res data in fractional feet
          int focusft = this->focus - focusin;

          focusin *= 12;
          focusin /= 100;
          focusft /= 100; */
      
        
        }
        if (datadescriptor & 4) {
          // has focal length
          this->zoom = this->rcvpacket->getData()[dataindex++] << 8;
          this->zoom += this->rcvpacket->getData()[dataindex++];  
          Serial.print(" zoom: ");
          Serial.print(this->zoom);          
        }
        if (datadescriptor & 8) {
          // has AUX
          this->aux = this->rcvpacket->getData()[dataindex++] << 8;
          this->aux += this->rcvpacket->getData()[dataindex++];  
          Serial.print(" aux: ");
          Serial.print(this->aux);          
        }
        if (datadescriptor & 16) {
          // describes resolution of data (unused)
        }
        if (datadescriptor & 32) {
          // has rangefinder distance
          this->distance = this->rcvpacket->getData()[dataindex++] << 8;
          this->distance += this->rcvpacket->getData()[dataindex++];   
          Serial.print(" distance: ");
          Serial.print(this->distance);
        }
        if (datadescriptor & 64) {
          // describes datatype of data (position vs metadata) (unused)
        }
        if (datadescriptor & 128) {
          // describes status of MDR (unused, and I'm not even sure what this means tbh)
        }

        //Serial.println();
      }
    }
  } else {
    Serial.print("First byte of rcvbuf is 0x");
    Serial.print(this->rcvbuf[0], HEX);
    Serial.println(", and I don't know how to parse that.");
    Serial.print("Second byte of rcvbuf is 0x");
    Serial.println(this->rcvbuf[1], HEX);
  }

  // Done processing
  //Serial.println("Done processing rcvbuf");

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
  int mode = pak->getMode();
  for (int i = 0; i < 3; i++) { // try the whole thing 3 times, unless one of the tries is successful
    /*Serial.print("Sending command with a mode of 0x");
    Serial.print(mode, HEX);
    Serial.print(" (attempt #");
    Serial.print(i);
    Serial.println(")");*/

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

      bool gotgoodreply = false;
      long int timestartedwaiting = millis();
      
      while (!gotgoodreply && timestartedwaiting + this->timeout > millis()) {
        if (this->waitForRcv()) {
          // Response packet was received
          //Serial.println("Got a reply packet");
          stat = this->parseRcv(); // MDR's response to the command packet
          //Serial.print("Status of reply packet is ");
          //Serial.println(stat);


          if (this->rcvpacket->getMode() == mode) {
            
            //Serial.print("Received message with a mode of 0x");
            //Serial.println(this->rcvpacket->getMode(), HEX);
            // the most recently processed packet from the MDR is of the same mode as our outgoing message
            // thus, we can assume it is a reply to our message 
            this->reply.replystatus = stat;
            this->reply.data = this->rcvpacket->getData();
            /*Serial.print("got a good reply: ");

            for (int i = 0; i < this->rcvpacket->getDataLen(); i++) {
              Serial.print((char)this->rcvpacket->getData()[i]);
            }
            Serial.println();*/
            gotgoodreply = true;
            return this->reply;
          }
        }
      }

      if (!gotgoodreply) {
        Serial.println("Timed out waiting for a reply from MDR");
      }
    }
  }
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

command_reply PrestonDuino::dist(byte type, uint32_t dist) {
  //TODO properly format distance (should be 3 bytes)
  
  byte data[4] = {type, (uint8_t)dist}; // this is a stupid thing to do
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



uint32_t PrestonDuino::getFocus() {
  return this->focus;
  /*
  const byte* lensdata = this->getLensData();
  static uint32_t distint;
  byte dist[4];
  dist[3] = 0;
  
  for (int i = 2; i >= 0; i--) {
    dist[i] = lensdata[2-i];
  }

  distint = ((uint32_t*)dist)[0];

  return distint;*/
}



int PrestonDuino::getZoom() {
  return this->zoom;
  /*
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

  return ((uint16_t*)flength)[0];*/
}



int PrestonDuino::getIris() {
  return this->iris;
  /*
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
  */
}


char* PrestonDuino::getLensName() {

  
  command_reply lensinfo = this->info(0x1);

  int lensnamelen = lensinfo.replystatus-1; //first byte in the reply to info() is the type of info, so minus one

  //Serial.print("Lens name is this long: ");
  //Serial.println(lensnamelen);
  
  this->lensname[0] = lensnamelen; // first index is length of name, plus one to acccount for the length itself
  for (int i = 1; i < lensnamelen; i++) {
    this->lensname[i] = lensinfo.data[i+1]; //1+1 ensures that we skip the first two elements of data, which are the info type
  }

  this->lensname[lensnamelen+1] = '\0';


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




command_reply PrestonDuino::setLensData(uint32_t dist, uint16_t iris, uint16_t flength) {
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
  //Serial.println(iris);

  newlensdata[5] = (iris >> 0);
  newlensdata[4] = (iris >> 8);


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
