#include "Arduino.h"
#include "PrestonPacket.h"

PrestonPacket::PrestonPacket(byte cmd_mode, byte* cmd_data) {
  this->mode = cmd_mode;
  this->data = cmd_data;
  char output[100];
  sprintf(output, "size of data is %d", sizeof(data));
  Serial.println(output);
  this->checksum = computeSum();
  this->packet = compilePacket();
}


byte PrestonPacket::getMode() {
  return this->mode;
}


int PrestonPacket::setMode(byte cmd_mode) {
  this->mode = cmd_mode;
  return 1;
}


byte* PrestonPacket::getData() {
  return this->data;
}


int PrestonPacket::setData(byte* cmd_data) {
  this->data = cmd_data;
  
  return 1;
}


int PrestonPacket::computeSum() {
  // compile that sum
  int sum = 0;
  for (int i = 0; i < sizeof(data); i++) {
    Serial.println(data[i], HEX);
    sum += data[i];
  }
  return sum;
}

int PrestonPacket::getSum() {
  return checksum;
}

byte* PrestonPacket::compilePacket() {
  // do the thing
}

byte* PrestonPacket::getPacket() {
  return this->packet;
}
