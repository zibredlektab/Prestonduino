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

  if(ser->available()) {
    this->mode(0,0); // tell the mdr to shut up for a second
    delay(100);
    while(ser->available()) {
      ser->read(); // dump anything the mdr said before we told it to shut up
    }
  }

  this->sendpacket = new PrestonPacket();
  this->rcvpacket = new PrestonPacket();

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
  /*for(int i = 2; i < mdrinfo.replystatus; i++){ // skip the first two characters for info(), they describe the type of info - in this case, lens name
    Serial.print((char)mdrinfo.data[i]);
  }*/
  Serial.print(this->lensname);
  Serial.println();
  

  this->mode(0x1,0x0); // start streaming mode, not controlling any channels

  this->data(0x7); // request low res (for easy debugging) focus iris and zoom data
  
}


bool PrestonDuino::readyToSend() {
  return this->connectionopen;
}

void PrestonDuino::onLoop () {
  Serial.println("loop");
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
  Serial.println("Waiting for response.");
  
  while(millis() <= this->time_now + this->timeout) { // continue checking for recieved data as long as the timeout period hasn't elapsed
    if (this->rcv()) {
      Serial.println("Got a response");
      return true;
    } else if (millis() % 1000 == 0){
      Serial.print(".");
    }
  }

  // If it gets this far, there was a timeout

  Serial.println("Timeout");
  return false;
}


bool PrestonDuino::rcv() {
  /* Check one character from serial and see if it's a control character or not. 
   * Continue until a good reply is found or timeout
   * Only put control characters and following valid data into rcvbuf
   *
   */
  
  unsigned long long timestartedrcv = millis();
  char currentchar = 0;
  
  while (timestartedrcv + this->timeout > millis()) {
    currentchar = this->waitForRead();

    if (currentchar) {
      switch (currentchar) {
        case ACK: {
          Serial.println("ACK received");
          return true;
          break;
        }
        case NAK: {
          Serial.println("NAK received");
          return true;
          break;
        }
        case STX: {
          // Start copying data to rcvbuf.
          // Start with 2 characters, look for length in 2nd
          this->rcvbuf[0] = STX;
          this->rcvbuf[1] = this->waitForRead(); // mode of message
          this->rcvbuf[2] = this->waitForRead(); // length of data in message
          int remaininglen = this->rcvbuf[2] + 3; // 2 bytes for checksum and 1 etx character
          for (int i = 0; i < remaininglen; i++) {
            this->rcvbuf[i+3] = this->waitForRead();
          }
          this->rcvbuf[remaininglen+3] = 0; // null terminator just to be safe
          return true;
          break;
        }
        default: {
          // data we do not understand, let's go around again
          break;
        }
      }
    }
  }

  return false;
}

char PrestonDuino::waitForRead() {
  unsigned long long int startedwait = millis();
  while (startedwait + this->timeout > millis()) {
    if (ser->available() > 0) {
      return ser->read();
    }
  }
  Serial.println("Timeout waiting for another serial character");
  return 0;
}

bool PrestonDuino::rcvold() {
  /* Check if there is available data, then check the first char for following:
   *    ACK/NAK: add to rcv and return
   *    STX: add all bytes to rcvbuf until ETX is found (if second control character is found, see next line)
   *    Anything else: treat as garbage data. Send NAK to MDR and toss the buffer
   * Returns true if we got usable data, false if not
   */
  
  int rcvi = 0; // index of last received character in the received message
  int chari = 0; // index of currently processing character in the received message
  char currentchar; // currently processing character
  int gotgoodreply = -1; // flag that message was a properly formatted preston reply: -1 is not checked yet, 0 is bad reply or timeout, 1 is good reply


  if (ser->available()) { //only receive if there is something to be received
    Serial.print("starting off, rcvi is ");
    Serial.print(rcvi);
    Serial.print(", and ser->available is ");
    Serial.println(ser->available());

    while (ser->available() > 0) { // move all available bytes into rcvbuf for processing
      this->rcvbuf[rcvi++] = ser->read(); // add the current byte from serial into rcvbuf
    }
    Serial.print("Serial had ");
    Serial.print(rcvi);
    Serial.println(" characters for us to process");
    
    currentchar = this->rcvbuf[chari++]; // get the first character from rcvbuf to take a look at 
    Serial.print("\nStart : 0x");
    Serial.print(currentchar, HEX);
    
    if (currentchar == STX) {
      // STX received from MDR
      // Continue reading from rcvbuf (or ser) until no more characters to process
      Serial.print(" (STX)");

      while (gotgoodreply == -1) { // iterate as long as there we have not received a full reply
        if (chari >= rcvi) {
          // if no more characters are available in rcvbuf, keep checking ser->available() for a few ms to make sure there isn't just a delay in the uart
          bool seravailable = false;
          long int timewaitstart = millis();
          Serial.println(" ...ran out of characters to process, waiting 2ms for more to arrive...");
          while (timewaitstart + 2 > millis() && !seravailable) {
            if (ser->available() > 0) {
              Serial.println("More arrived!");
              // something is available after all
              seravailable = true;
              while (ser->available() > 0) {
                this->rcvbuf[rcvi++] = ser->read();
              }


              Serial.print("Okay, now I have ");
              Serial.print(rcvi);
              Serial.println(" characters");
            }
          }
          if (!seravailable) {
            Serial.println("Nothing else ever arrived.");
            // timeout waiting for another byte in this packet, abort
            gotgoodreply = 0;
            break;
          }
        }

        currentchar = this->rcvbuf[chari++];

        Serial.print(" 0x");
        Serial.print(currentchar, HEX);

        if (currentchar == ETX) {
          // We have received ETX, stop reading
          Serial.print("chari = ");
          Serial.print(chari);
          Serial.print(", rcvi = ");
          Serial.println(rcvi);
          this->rcvlen = rcvi;
          gotgoodreply = 1;
          Serial.println(" (ETX) : End");
          Serial.print("Received a total of ");
          Serial.print(this->rcvlen);
          Serial.println(" bytes");
          
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
          Serial.print(" (");
          Serial.print(currentchar);
          Serial.print(")");
        }

      }
      
    } else if (currentchar == NAK || currentchar == ACK) {
      // Incoming character is either NAK or ACK, right now we don't care which
      Serial.println(" (ACK or NAK)");
      this->rcvlen = 1;
      gotgoodreply = 1; // NAK still counts as a good reply in this context!
  
    } else {
      // Incoming character is some kind of garbage (not a control character, but we're not in the middle of a packet yet)
      Serial.println(" (garbage data!)");
      gotgoodreply = 0;
    }
  }

  if (gotgoodreply == 1) {
    return true;
  } else {
    

    if (rcvi > 0) {
      // we received some data, but did not get a good reply
      //Serial.println("got some data, but not a full reply");
      //this->sendNAK(); // so let the MDR know we don't understand
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
  
  Serial.println("Starting to process rcvbuf");

  if (this->rcvbuf[0] == ACK) {
    // Reply is ACK
    Serial.println("Rcvbuf is ACK");
    response = -1;
    
  } else if (this->rcvbuf[0] == NAK) {
    // Reply is NAK
    Serial.println("Rcvbuf is NAK");
    response = -2;
    
  } else if (this->rcvbuf[0] == STX) {
    Serial.println("Rcvbuf is a packet");
    // Reply is a packet
    
    this->rcvpacket->packetFromBuffer(this->rcvbuf, this->rcvlen);

    Serial.println("Set up new rcvpacket.");

    // check validity of message
    
    if (!this->validatePacket()) {
      Serial.println("Packet failed validity check!");
      return -2;
    }
    
    switch (this->rcvpacket->getMode()) {
      case 0x11: {
        // Reply is an error message
        Serial.println("Rcv packet is an error");
        response = -3;
        // TODO actual error handling?
        break;
      }
      case 0x04: {
        // Reply is a data packet
        Serial.println("This is a data packet");
        response = this->rcvpacket->getDataLen();
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
        
        }
        if (datadescriptor & 4) {
          // has focal length/zoom
          this->zoom = this->rcvpacket->getData()[dataindex++] << 8;
          this->zoom += this->rcvpacket->getData()[dataindex++];  
          //Serial.print(" zoom: ");
          //Serial.print(this->zoom);          
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
        break;
      }

      case 0x0E: { // info
        Serial.println("This is an Info packet");
        response = this->rcvpacket->getDataLen();
        if (this->rcvpacket->getData()[1] == '1') {
          // lens name
          for (int i = 0; i < this->rcvpacket->getDataLen(); i++) {
            this->lensname[i] = this->rcvpacket->getData()[i];
          }
          this->lensname[this->rcvpacket->getDataLen()] = 0;

          Serial.print("lens name updated: ");
          Serial.println(this->lensname);
        }
        break;
      }

      default: {
        Serial.print("packet is of mode 0x");
        Serial.println(this->rcvpacket->getMode());
        break;
      }
    }


  } else { // This isn't a packet I recognize
    Serial.print("First byte of rcvbuf is 0x");
    Serial.print(this->rcvbuf[0], HEX);
    Serial.println(", and I don't know how to parse that.");
    Serial.print("Second byte of rcvbuf is 0x");
    Serial.println(this->rcvbuf[1], HEX);
  }

  // Done processing
  Serial.println("Done processing rcvbuf");

  return response;
}


bool PrestonDuino::validatePacket() {
  // Check validity of incoming packet, using its checksum
  //Serial.print("checksum of incoming message is 0x");
  //Serial.println(this->rcvpacket->getSum(), HEX);

  int corelen = this->rcvpacket->getPacketLen() - 4; // ignore STX, ETX, and message checksum bytes
  byte packetcore[corelen];
  memcpy(packetcore, &this->rcvpacket->getPacket()[1], corelen);

  int sum = this->rcvpacket->computeSum(packetcore, corelen);

  //Serial.print("calculated checksum is 0x");
  //Serial.println(sum, HEX);

  return sum == this->rcvpacket->getSum();
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
    Serial.print("Sending command with a mode of 0x");
    Serial.print(mode, HEX);
    Serial.print(" (attempt #");
    Serial.print(i);
    Serial.println(")");

    int stat = this->sendToMDR(pak); // MDR's receipt of the command packet
    this->reply.replystatus = stat;
    this->reply.data = NULL;
    
    Serial.print("Immediate response to command is ");
    Serial.println(stat);

    if (stat == 0 || stat < -1 || (stat == -1 && !withreply)) {
      // Either: timeout, NAK, or ACK for a command that doesn't need a reply
      return this->reply;
    } else if (stat == -1 && withreply) {
      // Packet was acknowledged by MDR, but we still need a reply
      Serial.println("Command packet acknowledged, awaiting reply");

      bool gotgoodreply = false;
      long int timestartedwaiting = millis();
      
      while (!gotgoodreply && timestartedwaiting + this->timeout > millis()) {
        if (this->waitForRcv()) {
          // Response packet was received
          Serial.println("Got a reply packet");
          stat = this->parseRcv(); // MDR's response to the command packet
          Serial.print("Status of reply packet is ");
          Serial.println(stat);

          // if stat is an error, we did not get a good reply

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
  Serial.println("issuing mode command");
  byte data[2] = {modeh, model};
  this->sendpacket->packetFromCommandWithData(0x01, data, 2);
  return this->sendCommand(this->sendpacket, false);
}

command_reply PrestonDuino::stat() {
  this->sendpacket->packetFromCommand(0x02);
  return this->sendCommand(this->sendpacket, true);
}

command_reply PrestonDuino::who() {
  this->sendpacket->packetFromCommand(0x03);
  command_reply data = this->sendCommand(this->sendpacket, true);
  return data;
}

command_reply PrestonDuino::data(byte datadescription) {
  this->sendpacket->packetFromCommandWithData(0x04, &datadescription, 1);
  return this->sendCommand(this->sendpacket, true);
}

command_reply PrestonDuino::data(byte* datadescription, int datalen) {
  this->sendpacket->packetFromCommandWithData(0x04, datadescription, datalen);
  return this->sendCommand(this->sendpacket, false);
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
  this->sendpacket->packetFromCommandWithData(0x05, data, len);
  return this->sendCommand(this->sendpacket, true);
}

command_reply PrestonDuino::setl(byte motors) {
  this->sendpacket->packetFromCommandWithData(0x06, &motors, 1);
  return this->sendCommand(this->sendpacket, false);
}

command_reply PrestonDuino::ct() {
  this->sendpacket->packetFromCommand(0x07);
  return this->sendCommand(this->sendpacket, true);
}

command_reply PrestonDuino::ct(byte cameratype) {
  this->sendpacket->packetFromCommandWithData(0x07, &cameratype, 1);
  return this->sendCommand(this->sendpacket, false);
}

command_reply PrestonDuino::mset(byte mseth, byte msetl) {
  byte data[2] = {mseth, msetl};
  this->sendpacket->packetFromCommandWithData(0x08, data, 2);
  return this->sendCommand(this->sendpacket, true);
}

command_reply PrestonDuino::mstat(byte motor) {
  this->sendpacket->packetFromCommandWithData(0x09, &motor, 1);
  return this->sendCommand(this->sendpacket, true);
}

command_reply PrestonDuino::r_s(bool rs) {
  this->sendpacket->packetFromCommandWithData(0x0A, (byte*)rs, 1);
  return this->sendCommand(this->sendpacket, false);
}

command_reply PrestonDuino::tcstat() {
  this->sendpacket->packetFromCommand(0x0B);
  return this->sendCommand(this->sendpacket, true);
}

command_reply PrestonDuino::ld() {
  this->sendpacket->packetFromCommand(0x0C);
  return this->sendCommand(this->sendpacket, true);
}

command_reply PrestonDuino::info(byte type) {
  this->sendpacket->packetFromCommandWithData(0x0E, &type, 0x01);
  return this->sendCommand(this->sendpacket, true);
}

command_reply PrestonDuino::dist(byte type, uint32_t dist) {
  //TODO properly format distance (should be 3 bytes)
  
  byte data[4] = {type, (uint8_t)dist}; // this is a stupid thing to do
  this->sendpacket->packetFromCommandWithData(0x10, data, 4);
  return this->sendCommand(this->sendpacket, false);
}



/*  Helper Commands 
 *  Note that lenses must be properly calibrated for any of this to work
 */



uint32_t PrestonDuino::getFocus() {
  return this->focus;
}



int PrestonDuino::getZoom() {
  return this->zoom;
}



int PrestonDuino::getIris() {
  return this->iris;
}


char* PrestonDuino::getLensName() {
  return this->lensname;
}


void PrestonDuino::setMDRTimeout(int newtimeout) {
  this->timeout = newtimeout;
}
