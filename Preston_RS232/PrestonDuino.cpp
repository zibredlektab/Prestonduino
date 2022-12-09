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


  this->sendpacket = new PrestonPacket();
  this->rcvpacket = new PrestonPacket();

  //Serial.print("Establishing connection to MDR...");

  this->mode(0,0); // tell the mdr to shut up for a second
  delay(100);
  while(ser->available() > 0) {
    ser->read(); // dump anything the mdr said before we told it to shut up
  }

  ser->setTimeout(this->timeout);

  // Get & store MDR number 
  //this->info(0x1);
  ////Serial.print("Status of reply from info command: ");
  ////Serial.println(mdrinfo.replystatus);
  
  long int time = millis();
/*
  while (mdrinfo.replystatus == 0) {
    // TODO slightly more robust way of checking for connection?
    mdrinfo = this->info(0x1);
    if (time + 100 < millis()) {
      //Serial.print(".");
      time = millis();
    }
  }*/

  //Serial.print("Current lens: ");
  /*for(int i = 2; i < mdrinfo.replystatus; i++){ // skip the first two characters for info(), they describe the type of info - in this case, lens name
    //Serial.print((char)mdrinfo.data[i]);
  }*/
  //Serial.print(this->lensname);
  //Serial.println();
  

  this->mode(0x1,0x0); // start streaming mode, not controlling any channels

  this->data(0x13); // request low res (for easy debugging) focus iris and zoom data
  
}

void PrestonDuino::onLoop () {
  //Serial.println("\n---loop---");
  if (this->rcv()) {
    if (this->parseRcv() == -2) {
      // Got an error when we tried to parse the message, probably a malformed message...
      this->sendNAK();
    };
  }
}



// A reply is expected, so wait for one until the timeout threshold has passed
// Return true if we got a response, false if we timed out
bool PrestonDuino::waitForRcv() {
  unsigned long long int time_now = millis();
  //Serial.println("Waiting for response.");
  
  while(millis() <= time_now + this->timeout) { // continue checking for recieved data as long as the timeout period hasn't elapsed
    if (this->rcv()) {
      //Serial.println("Got a response");
      return true;
    } else if (millis() % 1000 == 0){
      //Serial.print(".");
    }
  }

  // If it gets this far, there was a timeout

  //Serial.println("Timeout");
  return false;
}


bool PrestonDuino::rcv() {
  /* Check one character from serial and see if it's a control character or not. 
   * Continue until a good reply is found or timeout
   * Only put control characters and following valid data into rcvbuf
   */
  
  unsigned long long int timestartedrcv = millis();
  int currentchar = 0;
  
  while (timestartedrcv + this->timeout > millis()) {
    currentchar = ser->peek();

    if (currentchar != -1) {
      switch (currentchar) {
        case ACK: {
          //Serial.println("ACK received");
          this->rcvbuf[0] = ser->read();
          this->rcvlen = 1;
          return true;
          break;
        }
        case NAK: {
          //Serial.println("NAK received");
          this->rcvbuf[0] = ser->read();
          this->rcvlen = 1;
          return true;
          break;
        }
        case STX: {
          //Serial.print("Got a packet of ");
          // Start copying data to rcvbuf, until an etx
          this->rcvlen = ser->readBytesUntil(ETX, this->rcvbuf, 100);
          //Serial.print(this->rcvlen);
          //Serial.println(" characters.");

          if (this->rcvlen < 99) {
            this->rcvbuf[rcvlen++] = ETX;
          } else {
            //Serial.println("Read a suspiciously large amount of data...");
          }
          
          return true;
          break;
        }
        default: {
          // data we do not understand, let's go around again
          char unknownchar = ser->read();
          //Serial.print("current character in serial is 0x");
          //Serial.print(unknownchar, HEX);
          //Serial.println(", which I don't understand");
          break;
        }
      }
    }

  }

  return false;
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

  //Serial.print("rcvbuf is");
  for (int i = 0; i < this->rcvlen; i++) {
    //Serial.print(" 0x");
    //Serial.print(this->rcvbuf[i], HEX);
  }
  //Serial.println();

  if (this->rcvbuf[0] == ACK) {
    // Reply is ACK
    //Serial.println("Rcvbuf is ACK");
    response = -1;
    
  } else if (this->rcvbuf[0] == NAK) {
    // Reply is NAK
    //Serial.println("Rcvbuf is NAK");
    response = -3;
    
  } else if (this->rcvbuf[0] == STX) {
    //Serial.println("Rcvbuf is a packet");
    // Reply is a packet
    
    this->rcvpacket->packetFromBuffer(this->rcvbuf, this->rcvlen);

    //Serial.println("Set up new rcvpacket.");

    // check validity of message todo
    
    if (!this->validatePacket()) {
      //Serial.println("Packet failed validity check!");
      return -2;
    }
    
    switch (this->rcvpacket->getMode()) {
      case 0x11: {
        // Reply is an error message
        //Serial.println("Rcv packet is an error");
        response = -3;
        // TODO actual error handling?
        break;
      }
      case 0x04: {
        // Reply is a data packet
        //Serial.println("This is a data packet");
        response = this->rcvpacket->getDataLen();

        /*
        //Serial.print("The data is: ");
        for (int i = 0; i <= response; i++) {
          //Serial.print(" 0x");
          //Serial.print(this->rcvpacket->getData()[i], HEX);
        }
        //Serial.println();
        */

        int dataindex = 0;
        byte datadescriptor = this->rcvpacket->getData()[dataindex++]; // This byte describes the kind of data being provided
        if (datadescriptor & 1) {
          // has iris

          this->iris = this->rcvpacket->getData()[dataindex++] << 8;
          this->iris += this->rcvpacket->getData()[dataindex++];
          ////Serial.print("iris: ");
          ////Serial.print(this->iris);
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

          ////Serial.print(" zoom: ");
          ////Serial.print(this->zoom);          
        }
        if (datadescriptor & 8) {
          // has AUX
          this->aux = this->rcvpacket->getData()[dataindex++] << 8;
          this->aux += this->rcvpacket->getData()[dataindex++];  
          //Serial.print(" aux: ");
          //Serial.print(this->aux);
        }
        if (datadescriptor & 16) {
          // describes resolution of data (unused)
        }
        if (datadescriptor & 32) {
          // has rangefinder distance
          this->distance = this->rcvpacket->getData()[dataindex++] << 8;
          this->distance += this->rcvpacket->getData()[dataindex++];   
          //Serial.print(" distance: ");
          //Serial.print(this->distance);
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
        //Serial.println("This is an Info packet");
        response = this->rcvpacket->getDataLen();
        char* arraytofill = this->fwname;
        if (this->rcvpacket->getData()[1] == '1') {
          // lens name
          arraytofill = this->lensname;
        } else if (this->rcvpacket->getData()[1] == '0') {
          // mdr firmware
          arraytofill = this->fwname;
        } else {
          //Serial.println("unknown info type received, something has gone wrong.");
          break;
        }
      
        memcpy(arraytofill, &this->rcvpacket->getData()[2], this->rcvpacket->getDataLen()-2);
        arraytofill[this->rcvpacket->getDataLen()-2] = 0;

        //Serial.print("info updated: ");
        //Serial.println(arraytofill);
        break;
      }

      default: {
        //Serial.print("packet is of mode 0x");
        //Serial.println(this->rcvpacket->getMode());
        break;
      }
    }


  } else { // This isn't a packet format I recognize
    //Serial.print("First byte of rcvbuf is 0x");
    //Serial.print(this->rcvbuf[0], HEX);
    //Serial.println(", and I don't know how to parse that.");
    //Serial.print("Second byte of rcvbuf is 0x");
    //Serial.println(this->rcvbuf[1], HEX);
  }

  // Done processing
  //Serial.println("Done processing rcvbuf");

  return response;
}


bool PrestonDuino::validatePacket() {
  // Check validity of incoming packet, using its checksum
  //Serial.print("checksum of incoming message is 0x");
  //Serial.println(this->rcvpacket->getSum(), HEX);

  int tosumlen = this->rcvpacket->getPacketLen() - 3; // ignore ETX and message checksum bytes

  int sum = this->rcvpacket->computeSum(this->rcvpacket->getPacket(), tosumlen);

  //Serial.print("calculated checksum is 0x");
  //Serial.println(sum, HEX);

  return (sum == this->rcvpacket->getSum());
}


void PrestonDuino::sendACK() {
  // Send ACK, don't wait for a reply (there shouldn't be one)
  ////Serial.println("Sending ACK to MDR");
  ser->write(ACK);
  ser->flush();
}



void PrestonDuino::sendNAK() {
  // Send NAK, don't wait for a reply (that will be handled elsewhere)
  //Serial.println("Sending NAK to MDR");
  ser->write(NAK);
  ser->flush();
}


void PrestonDuino::sendBytesToMDR(byte* tosend, int len) {
  // The most basic send function, simply writes a byte array out to ser

  for (int i = 0; i < len; i++) {
    ////Serial.print("Sending 0x");
    ////Serial.println(tosend[i], HEX);
    ser->write(tosend[i]);
    ser->flush(); // wait for byte to finish sending
  }
}

void PrestonDuino::sendPacketToMDR(PrestonPacket* packet, bool retry) {
  // Send a PrestonPacket to the MDR, byte by byte. If "retry" is true, packet will be re-sent upon timout or NAK, up to 3 times (todo)
  int packetlen = packet->getPacketLen();
  byte* packetbytes = packet->getPacket();
  
  this->sendBytesToMDR(packetbytes, packetlen);
}


command_reply PrestonDuino::sendCommand(PrestonPacket* pak, bool withreply) {
  /* Send a PrestonPacket to the MDR, optionally wait for a reply packet in response
   * Returns a command_reply object of the reply
   *
   * A reply_status of 0 indicates no valid response, meaning either a timeout or an invalid reply packet
   */


  int mode = pak->getMode();
  for (int i = 0; i < 3; i++) { // try sending the command up to 3 times
    //Serial.print("Sending command with a mode of 0x");
    //Serial.print(mode, HEX);
    //Serial.print(" (attempt #");
    //Serial.print(i);
    //Serial.println(")");

    this->sendPacketToMDR(pak); // send the packet to the mdr

    for (int j = 0; j < 10; j++) { // check for a response 3 times
      if (this->waitForRcv()){ // wait for a response
        // got a response
        this->reply.replystatus = this->parseRcv(); // figure out what the response was
        //Serial.print("reply status after parsing is ");
        //Serial.println(this->reply.replystatus);
        if (this->reply.replystatus == -1) { // command acknowledged
          //Serial.print("Command acknowledged, ");
          if (!withreply) {
            //Serial.println("no reply expected.");
            return this->reply;
          } else {
            // this command needs a reply, so start the count over and wait for another response
            //Serial.println("waiting for a reply.");
            j = -1;
            continue;
          }
        } else if (this->reply.replystatus > 0) { // response was a packet of some kind
          if (this->rcvpacket->getMode() == mode) {
            // this is a reply packet to our original command
            //Serial.println("This is a reply to the command I just sent");
            this->reply.data = this->rcvpacket->getData();
            return this->reply;
          } else {
            // this is a reply packet for another command, we should assume it's already been parsed elsewhere and ignore it here.
            //Serial.println("This is a reply to another command, ignoring");
          }
        } else if (this->reply.replystatus < 0) {
          //Serial.print("Unexpected reply status of ");
          //Serial.println(this->reply.replystatus);
          
          this->sendNAK();
          j = -1;
          continue;
        }
      }
    }
  }
  // if we get this far, we have failed to get a valid response to our sent command. :(
  //Serial.print("Never got a valid response to command of mode 0x");
  //Serial.println(mode, HEX);
  return this->reply;
}

void PrestonDuino::zoomFromLensName() {
  char *firsthalf = strchr(this->getLensName(), '|');
  char *zminname = strchr(&firsthalf[2], '|');
  this->zoom = atoi(&zminname[1]);
}


/*
 *  Preston-specified commands
 */


command_reply PrestonDuino::mode(byte modeh, byte model) {
  //Serial.println("issuing mode command");
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
  return this->sendCommand(this->sendpacket, false); // requests for data do not need to wait for a reply - MDR skips the ACK step and directly replies
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

  if (this->zoom == 0) {
    this->zoomFromLensName();
  }

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
  ser->setTimeout(this->timeout);
}
