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

  Serial.println("PD: Starting PrestonDuino");

  // Zero out lens name and fw name
  for (int i = 0; i < 50; i++) {
    this->lensname[i] = 0;
    if (i < 5) this->mdrtype[i] = 0;
  }

  this->shutUp();

  ser->setTimeout(this->timeout);

  delay(PDPERIOD);

  Serial.print("PD: rcvbuf is from 0x");
  Serial.println((int)this->rcvbuf, HEX);
}

bool PrestonDuino::isMDRReady() {
  // Check for stored MDR firmware number
  if (this->mdrtype[0] != 0) {
    // If we have that, return true
    Serial.println("PD: MDR is ready");
    return true;
  } else {
    // If not, send an "info" command to the MDR, requesting firmware number
    Serial.println("PD: MDR is not ready...");
    this->info(0x0);
    delay(PDPERIOD);
    return false;
  }
}

void PrestonDuino::shutUp() {
  // tell the mdr to shut up for a second
  Serial.println("PD: Asking MDR to stop streaming data (sending as hard coded bytes)");
  uint8_t shutup[12] = {0x02, 0x30, 0x31, 0x30, 0x32, 0x30, 0x30, 0x30, 0x30, 0x38, 0x35, 0x03};
  ser->write(shutup, 12);
  delay(100);
  while(ser->available() > 0) {
    ser->read(); // dump anything the mdr said before we told it to shut up
  }
}

bool PrestonDuino::waitForAck(int timeout = 1000) {
  Serial.println("PD: Waiting for ACK...");

  uint32_t timestartedwaiting = millis();

  while (millis() < timestartedwaiting + timeout) {
    if (waitForRcv() && this->rcvbuf[0] == ACK) {
      Serial.println("PD: got ACK while waiting.");
      return true;
    }
  }

  Serial.println("PD: timed out waiting for ACK.");
  return false;

}

void PrestonDuino::onLoop () {
  if (this->rcv()) {
    if (this->parseRcv() == -2) {
      // Got an error when we tried to parse the message, probably a malformed message...
      this->sendNAK();
    }
  }

}



// A reply is expected, so wait for one until the timeout threshold has passed
// Return true if we got a response, false if we timed out
bool PrestonDuino::waitForRcv() {
  unsigned long long int time_now = millis();
  //Serial.println("PD: Waiting for response.");
  
  while(millis() <= time_now + this->timeout) { // continue checking for recieved data as long as the timeout period hasn't elapsed
    if (this->rcv()) {
      //Serial.println("PD: Got a response");
      return true;
    } else if (millis() % 1000 == 0){
      //Serial.print(".");
    }
  }

  // If it gets this far, there was a timeout

  //Serial.println("PD: Timeout");
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
          Serial.println("PD: ACK received");
          this->rcvbuf[0] = ser->read();
          this->rcvlen = 1;
          return true;
          break;
        }
        case NAK: {
          Serial.println("PD: NAK received");
          this->rcvbuf[0] = ser->read();
          this->rcvlen = 1;
          return true;
          break;
        }
        case STX: {
          // Start copying data to rcvbuf, until an etx
          this->rcvlen = ser->readBytesUntil(ETX, this->rcvbuf, 100);
          Serial.print("PD: Got a packet of ");
          Serial.print(this->rcvlen);
          Serial.println(" characters:");

          this->rcvbuf[this->rcvlen] = 0x0; // null terminate that thang

          for (int i = 0; i < this->rcvlen; i++) {
            Serial.print(" 0x");
            Serial.print(this->rcvbuf[i], HEX);
          }
          Serial.println();

          // -- Packet collision checking --
          Serial.print("PD: Checking for packet collision...");
          for (int i = this->rcvlen - 1; i > 0; i--) {
            // iterate from the end of the buffer, because we only care about the latest packet (the older packet is garbage anyway)
            if (this->rcvbuf[i] == 0x6) {
              char* errantack = (char*)&this->rcvbuf[i];
              Serial.print("found ACK, at position ");
              Serial.print(errantack - (char*)this->rcvbuf);
              Serial.println(". Shuffling packet up by one to get rid of it.");
              memmove(errantack, errantack + 1, this->rcvlen - (errantack - (char*)this->rcvbuf)); // move the rest packet up by one, starting from the errant ack position 
              this->rcvlen--;
              
              Serial.print("PD: buffer is now ");
              Serial.print(this->rcvlen);
              Serial.println(" characters long");

              Serial.print("New packet:");
              for (int j = 0; j < this->rcvlen; j++) {
                Serial.print(" 0x");
                Serial.print(this->rcvbuf[j], HEX);
              }
              Serial.println();
              i = this->rcvlen - 1; // restart loop
            } else if (this->rcvbuf[i] == 0x2) {
              char* errantstx = (char*)&this->rcvbuf[i];
              Serial.print("found STX, at position ");
              Serial.print(errantstx - (char*)this->rcvbuf);
              Serial.println(". Moving packet up from that index.");
              this->rcvlen -= errantstx - (char*)this->rcvbuf; // new packet length is reduced by length of string leading up to the errant stx

              Serial.print("PD: buffer is now ");
              Serial.print(this->rcvlen);
              Serial.println(" characters long");

              memmove((char*)this->rcvbuf, errantstx, this->rcvlen);

              Serial.print("New packet:");
              for (int j = 0; j < this->rcvlen; j++) {
                Serial.print(" 0x");
                Serial.print(this->rcvbuf[j], HEX);
              }
              Serial.println();
              i = this->rcvlen - 1; // restart loop
            }
          }
          Serial.println("PD: packet collision detection complete");
          

          if (this->rcvlen < 99) {
            this->rcvbuf[rcvlen++] = ETX;
          } else {
            Serial.println("PD: Read a suspiciously large amount of data...");
          }
          
          return true;
          break;
        }
        default: {
          // data we do not understand, let's go around again
          char unknownchar = ser->read();
          Serial.print("PD: current character in serial is 0x");
          Serial.print(unknownchar, HEX);
          Serial.println(", which I don't understand");
          break;
        }
      }
    }

  }
  Serial.println("PD: Timed out waiting for a packet");
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
  
  //Serial.println("PD: Starting to process rcvbuf");

  Serial.print("PD: rcvbuf is");
  for (int i = 0; i < this->rcvlen; i++) {
    Serial.print(" 0x");
    Serial.print(this->rcvbuf[i], HEX);
  }
  Serial.println();

  if (this->rcvbuf[0] == ACK) {
    // Reply is ACK
    Serial.println("PD: Rcvbuf is ACK");
    response = -1;
    
  } else if (this->rcvbuf[0] == NAK) {
    // Reply is NAK
    Serial.println("PD: Rcvbuf is NAK");
    response = -3;
    
  } else if (this->rcvbuf[0] == STX) {
    Serial.println("PD: Rcvbuf is a packet");
    // Reply is a packet
    
    if (!this->rcvpacket->packetFromBuffer(this->rcvbuf, this->rcvlen)) {
      Serial.println("PD: PP failed to make a new packet from this buffer");  
      return -2;
    }

    
    if (!this->validatePacket()) {
      Serial.print("PD: Packet failed validity check: ");
      for (int i = 0; i < this->rcvlen; i++) {
        Serial.print(" 0x");
        Serial.print(this->rcvbuf[i], HEX);
      }
      Serial.println();
      return -2;
    }
    
    switch (this->rcvpacket->getCommand()) {
      case 0x02: {
        // Reply is a stat message
        Serial.println("PD: This is a stat packet");

        byte statdescriptor = this->rcvpacket->getData()[1]; // for now, I don't care about the high byte

        if (statdescriptor & 1) {
          // iris motor is connected & calibrated
        }
        if (statdescriptor & 2) {
          // focus motor is connected & calibrated
        }
        if (statdescriptor & 4) {
          // zoom motor is connected & calibrated
        }
        if (statdescriptor & 8) {
          // camera is set (not sure what this means)
        }
        if (statdescriptor & 16) {
          // camera is running
          this->running = true;
          Serial.println("PD: Camera is running.");
        } else {
          // camera is not running
          this->running = false;
          Serial.println("PD: Camera is stopped.");
        }
        if (statdescriptor & 32) {
          // autofocus is on
        }
        if (statdescriptor & 64) {
          // DXL camera is in use
        }
        if (statdescriptor & 128) {
          // focus map in use
        }
        // there are more but I'm not using them yet (in the high byte)
        break;
      }
      case 0x11: {
        // Reply is an error message
        Serial.println("PD: Rcv packet is an error");
        response = -3;
        // TODO actual error handling?
        break;
      }
      case 0x04: {
        // Reply is a data packet
        Serial.println("PD: This is a data packet");
        response = this->rcvpacket->getDataLen();

        
        Serial.print("PD: The data is: ");
        for (int i = 0; i < response; i++) {
          Serial.print(" 0x");
          Serial.print(this->rcvpacket->getData()[i], HEX);
        }
        Serial.println();
        

        int dataindex = 0;
        byte datadescriptor = this->rcvpacket->getData()[dataindex++]; // This byte describes the kind of data being provided
        if (datadescriptor & 1) {
          // has iris

          this->iris = this->rcvpacket->getData()[dataindex++] << 8;
          this->iris += this->rcvpacket->getData()[dataindex++];
          
          Serial.print("iris: ");
          Serial.println(this->iris);
        }
        if (datadescriptor & 2) {
          // has focus

          this->focus = this->rcvpacket->getData()[dataindex++] << 8;
          this->focus += this->rcvpacket->getData()[dataindex++];

          Serial.print(" focus: ");
          Serial.print(this->focus);
          Serial.print("mm");
        
        }
        if (datadescriptor & 4) {
          // has focal length/zoom
          this->zoom = this->rcvpacket->getData()[dataindex++] << 8;
          this->zoom += this->rcvpacket->getData()[dataindex++];

          Serial.print(" zoom: ");
          Serial.println(this->zoom);          
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

        Serial.println();
        break;
      }

      case 0x1F: { // MDR type
        for (int i = 0; i < 4; i++) {
          this->mdrtype[i] = toupper(this->rcvpacket->getData()[i+2]);
        }
        this->mdrtype[4] = 0;

        Serial.print("PD: new MDR type: ");
        for (int i = 0; i < 5; i++) {
          Serial.print(" 0x");
          Serial.print(this->mdrtype[i], HEX);
          Serial.print("(");
          Serial.print(this->mdrtype[i]);
          Serial.print(")");
        }
        Serial.println();

        break;
      }

      case 0x0E: { // info
        // zeroth byte of data will always be zero
        // first byte will be identifier of type of info
        // info starts at byte 2 

        int len = this->rcvpacket->getDataLen() - 1; // ignore those first two bytes from now on, but add one for the null terminator

        if (this->rcvpacket->getData()[1] == '1') {
          // lens name
          memcpy(this->lensname, &this->rcvpacket->getData()[2], len - 1);
          this->lensnamelen = len;
          this->lensname[len] = 0; // null terminate the string

        } else if (this->rcvpacket->getData()[1] == '0') {
          // mdr firmware
          memcpy(this->mdrtype, &this->rcvpacket->getData()[2], len - 1);
          this->mdrtype[4] = 0;

        } else {
          Serial.println("PD: unknown info type received:");
          for (int i = 1; i < this->rcvlen; i++) {
            Serial.print(" 0x");
            Serial.print(this->rcvpacket->getData()[i], HEX);
          }
          Serial.println();
          break;

        }

        Serial.print("PD: new info: ");
        for (int i = 0; i < this->rcvpacket->getDataLen() - 1; i++) {
          Serial.print(" 0x");
          Serial.print(this->rcvpacket->getData()[i+2], HEX);
          Serial.print("(");
          Serial.print((char)this->rcvpacket->getData()[i+2]);
          Serial.print(")");
        }

        Serial.println();
        break;
      }

      default: {
        Serial.print("PD: packet is of command 0x");
        Serial.println(this->rcvpacket->getCommand());
        break;
      }
    }


  } else { // This isn't a packet format I recognize
    Serial.print("PD: First byte of rcvbuf is 0x");
    Serial.print(this->rcvbuf[0], HEX);
    Serial.println(", and I don't know how to parse that.");
    Serial.print("PD: Second byte of rcvbuf is 0x");
    Serial.println(this->rcvbuf[1], HEX);
  }

  // Done processing
  Serial.println("PD: Done processing rcvbuf");

  return response;
}


bool PrestonDuino::validatePacket() {
  // Check validity of incoming packet, using its checksum

  if (this->rcvpacket->getPacketLen() < 9) {
    // stx, 2x command, 2x len, 1x data, 2x checksum, etx
    Serial.println("PD: this packet is too short to be valid.");
    return false;
  }

  if (this->rcvpacket->getCommand() == 0x1F) return true; // "info" messages with MDR version number are always malformed...

  int tosumlen = this->rcvpacket->getPacketLen() - 3; // ignore ETX and message checksum bytes

  int sum = this->rcvpacket->computeSum(this->rcvpacket->getPacket(), tosumlen);

  if (sum != this->rcvpacket->getSum()) {
    Serial.print("PD: checksum of incoming message is ");
    Serial.print(this->rcvpacket->getSum());
    Serial.print(", calculated checksum is ");
    Serial.println(sum);
  }

  return (sum == this->rcvpacket->getSum());
}


void PrestonDuino::sendACK() {
  // Send ACK, don't wait for a reply (there shouldn't be one)
  //Serial.println("PD: Sending ACK to MDR");
  ser->write(ACK);
  ser->flush();
}



void PrestonDuino::sendNAK() {
  // Only send NAK for non-data commands
  // (This is a hack to allow for NAKing while streaming data...data streams often send broken packets, and I don't want to NAK every one of them)
  if (this->rcvpacket->getCommand() != 0x04) {
    Serial.println("PD: Sending NAK...");
  } else {
    Serial.println("PD: Not sending NAK for data commands");
  }
  //ser->write(NAK);
  //this->lastsend = millis();
}


void PrestonDuino::sendBytesToMDR(byte* bytestosend, int sendlen) {
  // The most basic send function, simply writes a byte array out to ser

  // Check if down period has passed yet
  if (millis() < this->lastsend + PDPERIOD) {
    Serial.println("PD: Warning: messages sending too fast for MDR to keep up");
  }

  Serial.print("PD: Sending to MDR:");
  for (int i = 0; i < sendlen; i++) {
    Serial.print(" 0x");
    Serial.print(bytestosend[i], HEX);

    ser->write(bytestosend[i]);
    ser->flush(); // wait for byte to finish sending
  }
  Serial.println();
  this->lastsend = millis();

}

void PrestonDuino::sendPacketToMDR(PrestonPacket* packet, bool retry) {
  // Send a PrestonPacket to the MDR, byte by byte. If "retry" is true, packet will be re-sent upon timout or NAK, up to 3 times (todo)
  int packetlen = packet->getPacketLen();
  byte* packetbytes = packet->getPacket();

  this->sendBytesToMDR(packetbytes, packetlen);
  
}

void PrestonDuino::zoomFromLensName() {
  char *firsthalf = strchr(this->getLensName(), '|');
  char *zminname = strchr(&firsthalf[2], '|');
  this->zoom = atoi(&zminname[1]);
}

void PrestonDuino::sendCommand(byte cmd) {
  this->sendpacket->packetFromCommand(cmd);
  return this->sendPacketToMDR(this->sendpacket);
}

void PrestonDuino::sendCommandWithData(byte cmd, byte* data, int datalen) {
  this->sendpacket->packetFromCommandWithData(cmd, data, datalen);
  return this->sendPacketToMDR(this->sendpacket);
}


/*
 *  Preston-specified commands
 */


void PrestonDuino::mode(byte modeh, byte model) {
  byte data[2] = {modeh, model};
  return this->sendCommandWithData(0x01, data, 2);
}

void PrestonDuino::stat() {
  return this->sendCommand(0x02);
}

void PrestonDuino::who() {
  return this->sendCommand(0x03);
}

void PrestonDuino::data(byte datadescription) {
  return this->sendCommandWithData(0x04, &datadescription, 1);
}

void PrestonDuino::data(byte* dataset, int datalen) {
  return this->sendCommandWithData(0x04, dataset, datalen);
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

  return this->sendCommandWithData(0x05, data, len);
}

void PrestonDuino::setl(byte motors) {
  return this->sendCommandWithData(0x06, &motors, 1);
}

void PrestonDuino::ct() {
  return this->sendCommand(0x07);
}

void PrestonDuino::ct(byte cameratype) {
  return this->sendCommandWithData(0x07, &cameratype, 1);
}

void PrestonDuino::mset(byte mseth, byte msetl) {
  byte data[2] = {mseth, msetl};
  return this->sendCommandWithData(0x08, data, 2);
}

void PrestonDuino::mstat(byte motor) {
  return this->sendCommandWithData(0x09, &motor, 1);
}

void PrestonDuino::r_s(bool rs) {
  byte data = rs;
  return this->sendCommandWithData(0x0A, &data, 1);
}

void PrestonDuino::tcstat() {
  return this->sendCommand(0x0B);
}

void PrestonDuino::ld() {
  return this->sendCommand(0x0C);
}

void PrestonDuino::info(byte type) {
  return this->sendCommandWithData(0x0E, &type, 1);
}

void PrestonDuino::dist(byte type, uint32_t dist) {
  //TODO properly format distance (should be 3 bytes)
  
  byte data[4] = {type, (uint8_t)dist}; // this is a stupid thing to do
  return this->sendCommandWithData(0x10, data, 4);
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

char* PrestonDuino::getMDRType() {
  return this->mdrtype;
}

void PrestonDuino::setIris(uint16_t newiris) {
  byte irish = newiris >> 8;
  byte irisl = newiris && 0xFF;
  byte dataset[3] = {0x41, irish, irisl};
  this->data(dataset, 3);
}


void PrestonDuino::setMDRTimeout(int newtimeout) {
  this->timeout = newtimeout;
  this->ser->setTimeout(this->timeout);
}

bool PrestonDuino::getRunning() {
  return this->running;
}

