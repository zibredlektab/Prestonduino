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

  //Serial.println("Starting PrestonDuino");

  this->rootmsg = new mdr_message;

  this->shutUp();

  ser->setTimeout(this->timeout);

  //Serial.println("Getting lens name");

  for (int i = 0; i < 50; i++) {
    this->lensname[i] = 0;
  }

  this->lensname[0] = '?';
  // Get & store lens name 
  this->info(0x1);
  this->sendBytesToMDR();
  delay(PERIOD);

  this->setMDRMode(0x1, 0x40, NORMALDATAMODE);
}

void PrestonDuino::shutUp() {
  // tell the mdr to shut up for a second
  uint8_t shutup[12] = {0x02, 0x30, 0x31, 0x30, 0x32, 0x30, 0x30, 0x30, 0x30, 0x38, 0x35, 0x03};
  ser->write(shutup, 12);
  delay(100);
  while(ser->available() > 0) {
    ser->read(); // dump anything the mdr said before we told it to shut up
  }
}

void PrestonDuino::setMDRMode(uint8_t newmodeh, uint8_t newmodel, uint8_t newdata) {
  //Serial.println("Setting MDR mode");

  this->mode(newmodeh, newmodel); // start streaming mode, requesting actual positions, controlling the AUX channel
  this->sendBytesToMDR();
  delay(PERIOD);

  this->data(newdata); // request high res focus iris zoom data
  this->sendBytesToMDR();
  delay(PERIOD);
}

void PrestonDuino::onLoop () {
  if (this->rcv()) {
    if (this->parseRcv() == -2) {
      // Got an error when we tried to parse the message, probably a malformed message...
      this->sendNAK();
    }
  }

  if (this->rootmsg->nextmsg != NULL && millis() >= this->lastsend + PERIOD) {
    this->sendBytesToMDR();
  }

  if (millis() >= this->lastnamecheck + NAMECHECK) {
    this->info(0x1);
    this->lastnamecheck = millis();
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
  
  uint32_t timestartedrcv = millis();
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
          // Start copying data to rcvbuf, until an etx
          this->rcvlen = ser->readBytesUntil(ETX, this->rcvbuf, 100);
          //Serial.print("Got a packet of ");
          //Serial.print(this->rcvlen);
          //Serial.println(" characters:");

          for (int i = 0; i < this->rcvlen; i++) {
            //Serial.print(" 0x");
            //Serial.print(this->rcvbuf[i], HEX);
          }
          //Serial.println();

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
  //Serial.println("Timed out waiting for a packet");
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
      /*//Serial.print("Packet failed validity check: ");
      for (int i = 0; i < this->rcvlen; i++) {
        //Serial.print(" 0x");
        //Serial.print(this->rcvbuf[i], HEX);
      }
      //Serial.println();*/
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
        for (int i = 0; i < response; i++) {
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
          
          //Serial.print("iris: ");
          //Serial.println(this->iris);
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
          //Serial.println(this->zoom);          
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
        // zeroth byte of data will always be zero
        // first byte will be identifier of type of info
        // info starts at byte 2 
        char* arraytofill;
        if (this->rcvpacket->getData()[1] == '1') {
          // lens name
          arraytofill = this->lensname;
        } else if (this->rcvpacket->getData()[1] == '0') {
          // mdr firmware
          arraytofill = this->fwname;
        } else {
          //Serial.println("unknown info type received:");
          for (int i = 1; i < this->rcvlen; i++) {
            //Serial.print(" 0x");
            //Serial.print(this->rcvpacket->getData()[i], HEX);
          }
          //Serial.println();
          break;
        }

        this->lensnamelen = this->rcvpacket->getDataLen() - 1; // we ignore those first two bytes from now on, but add one for the null terminator

        memcpy(arraytofill, &this->rcvpacket->getData()[2], this->lensnamelen - 1);
        arraytofill[this->lensnamelen] = 0; // null terminate the string
        //Serial.print("new info: ");
        for (int i = 0; i < this->rcvpacket->getDataLen() - 1; i++) {
          //Serial.print(" 0x");
          //Serial.print(arraytofill[i], HEX);
          //Serial.print("(");
          //Serial.print(arraytofill[i]);
          //Serial.print(")");
        }
        //Serial.println();
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

  int tosumlen = this->rcvpacket->getPacketLen() - 3; // ignore ETX and message checksum bytes

  int sum = this->rcvpacket->computeSum(this->rcvpacket->getPacket(), tosumlen);

  if (0 && sum != this->rcvpacket->getSum()) {
    //Serial.print("checksum of incoming message is ");
    //Serial.print(this->rcvpacket->getSum());
    //Serial.print(", calculated checksum is ");
    //Serial.println(sum);
  }

  return (sum == this->rcvpacket->getSum());
}


void PrestonDuino::sendACK() {
  // Send ACK, don't wait for a reply (there shouldn't be one)
  ////Serial.println("Sending ACK to MDR");
  ser->write(ACK);
  ser->flush();
}



void PrestonDuino::sendNAK() {
  // Resend last packet
  //this->sendPacketToMDR(this->sendpacket);
  // Send NAK, don't wait for a reply (that will be handled elsewhere)
  /*//Serial.println("Sending NAK to MDR");
  byte nak[1] = {NAK};
  this->queueForSend(nak, 1);*/
}


bool PrestonDuino::queueForSend(byte* tosend, int len) {
  // Add bytes to the send queue
  // Returns true if there is room to store another message, false otherwise.

  //Serial.println("Queuing a new message to be sent.");

  if (this->totalmessages > 10) {
    //Serial.println("PD has reached the limit on queued messages, can't store anymore.");
    return false;
  }

  mdr_message* newmsg = this->rootmsg;

  while (newmsg->nextmsg != NULL) {
    /*//Serial.print("There's already a message queued in this position:");
    for (int i = 0; i < newmsg->nextmsg->msglen; i++) {
      //Serial.print(" 0x");
      //Serial.print(newmsg->nextmsg->data[i], HEX);
    }
    //Serial.println();*/

    newmsg = newmsg->nextmsg;
  }
  newmsg->nextmsg = new mdr_message;
  newmsg = newmsg->nextmsg;
  memcpy(&newmsg->data, tosend, len);
  newmsg->msglen = len;

  this->totalmessages++;

  /*//Serial.print("Queued message:");
  for (int i = 0; i < len; i++) {
    //Serial.print(" 0x");
    //Serial.print(newmsg->data[i], HEX);
  }
  //Serial.println();*/

  return true;
}


void PrestonDuino::sendBytesToMDR() {
  // The most basic send function, simply writes a byte array out to ser

  mdr_message* msgtosend = this->rootmsg->nextmsg;

  if (msgtosend != NULL) {
    //Serial.print("Sending to MDR:");
    for (int i = 0; i < msgtosend->msglen; i++) {
      //Serial.print(" 0x");
      //Serial.print(msgtosend->data[i], HEX);

      ser->write(msgtosend->data[i]);
      ser->flush(); // wait for byte to finish sending
    }
    //Serial.println();
    this->rootmsg->nextmsg = msgtosend->nextmsg;
    free(msgtosend);
    this->totalmessages--;
  }
  this->lastsend = millis();

}

void PrestonDuino::sendPacketToMDR(PrestonPacket* packet, bool retry) {
  // Send a PrestonPacket to the MDR, byte by byte. If "retry" is true, packet will be re-sent upon timout or NAK, up to 3 times (todo)
  int packetlen = packet->getPacketLen();
  byte* packetbytes = packet->getPacket();

  this->sendpacket = packet;
  
  this->queueForSend(packetbytes, packetlen);
}


void PrestonDuino::sendCommand(PrestonPacket* pak) {
  /* Send a PrestonPacket to the MDR, optionally wait for a reply packet in response
   */
  this->sendPacketToMDR(pak); // send the packet to the mdr
}

void PrestonDuino::zoomFromLensName() {
  char *firsthalf = strchr(this->getLensName(), '|');
  char *zminname = strchr(&firsthalf[2], '|');
  this->zoom = atoi(&zminname[1]);
}


/*
 *  Preston-specified commands
 */


void PrestonDuino::mode(byte modeh, byte model) {
  //Serial.println("issuing mode command");
  byte data[2] = {modeh, model};
  this->sendpacket->packetFromCommandWithData(0x01, data, 2);
  return this->sendCommand(this->sendpacket);
}

void PrestonDuino::stat() {
  this->sendpacket->packetFromCommand(0x02);
  return this->sendCommand(this->sendpacket);
}

void PrestonDuino::who() {
  this->sendpacket->packetFromCommand(0x03);
  this->sendCommand(this->sendpacket);
}

void PrestonDuino::data(byte datadescription) {
  this->sendpacket->packetFromCommandWithData(0x04, &datadescription, 1);
  this->sendCommand(this->sendpacket); // requests for data do not need to wait for a reply - MDR skips the ACK step and directly replies
}

void PrestonDuino::data(byte* dataset, int datalen) {
  this->sendpacket->packetFromCommandWithData(0x04, dataset, datalen);
  this->sendCommand(this->sendpacket);
}

void PrestonDuino::rtc(byte select, byte* data) {
  int len = 0;
  if (select == 0x01) {
    len = 8;
  } else if (select == 0x02 || select == 0x04) {
    len = 5;
  } else {
    len = 1;
  }
  this->sendpacket->packetFromCommandWithData(0x05, data, len);
  this->sendCommand(this->sendpacket);
}

void PrestonDuino::setl(byte motors) {
  this->sendpacket->packetFromCommandWithData(0x06, &motors, 1);
  this->sendCommand(this->sendpacket);
}

void PrestonDuino::ct() {
  this->sendpacket->packetFromCommand(0x07);
  this->sendCommand(this->sendpacket);
}

void PrestonDuino::ct(byte cameratype) {
  this->sendpacket->packetFromCommandWithData(0x07, &cameratype, 1);
  this->sendCommand(this->sendpacket);
}

void PrestonDuino::mset(byte mseth, byte msetl) {
  byte data[2] = {mseth, msetl};
  this->sendpacket->packetFromCommandWithData(0x08, data, 2);
  this->sendCommand(this->sendpacket);
}

void PrestonDuino::mstat(byte motor) {
  this->sendpacket->packetFromCommandWithData(0x09, &motor, 1);
  this->sendCommand(this->sendpacket);
}

void PrestonDuino::r_s(bool rs) {
  this->sendpacket->packetFromCommandWithData(0x0A, (byte*)rs, 1);
  this->sendCommand(this->sendpacket);
}

void PrestonDuino::tcstat() {
  this->sendpacket->packetFromCommand(0x0B);
  this->sendCommand(this->sendpacket);
}

void PrestonDuino::ld() {
  this->sendpacket->packetFromCommand(0x0C);
  this->sendCommand(this->sendpacket);
}

void PrestonDuino::info(byte type) {
  this->sendpacket->packetFromCommandWithData(0x0E, &type, 0x01);
  this->sendCommand(this->sendpacket);
}

void PrestonDuino::dist(byte type, uint32_t dist) {
  //TODO properly format distance (should be 3 bytes)
  
  byte data[4] = {type, (uint8_t)dist}; // this is a stupid thing to do
  this->sendpacket->packetFromCommandWithData(0x10, data, 4);
  this->sendCommand(this->sendpacket);
}



/*  Helper Commands 
 *  Note that lenses must be properly calibrated for any of this to work
 */



uint16_t PrestonDuino::getFocus() {
  return this->focus;
}



int16_t PrestonDuino::getZoom() {

  if (this->zoom == 0) {
    this->zoomFromLensName();
  }

  return this->zoom;
}



uint16_t PrestonDuino::getIris() {
  return this->iris;
}

uint16_t PrestonDuino::getAux() {
  return this->aux;
}


char* PrestonDuino::getLensName() {
  return this->lensname;
}

uint8_t PrestonDuino::getLensNameLen() {
  return this->lensnamelen;
}

void PrestonDuino::setIris(uint16_t newiris) {
  byte irish = newiris >> 8;
  byte irisl = newiris && 0xFF;
  byte dataset[3] = {0x41, irish, irisl};
  this->data(dataset, 3);
}


void PrestonDuino::setMDRTimeout(int newtimeout) {
  this->timeout = newtimeout;
  ser->setTimeout(this->timeout);
}
