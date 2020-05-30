#include "Arduino.h"
#include "PrestonCommand.h"

PrestonCommand::PrestonCommand(byte cmd_mode, byte* cmd_data) {
  this->mode = cmd_mode;
  this->data = cmd_data;
  this->checksum = computeSum();
  this->command = compileCommand();
}


byte PrestonCommand::getMode() {
  return this->mode;
}


int PrestonCommand::setMode(byte cmd_mode) {
  this->mode = cmd_mode;
  return 1;
}


byte* PrestonCommand::getData() {
  return this->data;
}


int PrestonCommand::setData(byte* cmd_data) {
  this->data = cmd_data;
  return 1;
}


int PrestonCommand::computeSum() {
  // compile that sum
  this->checksum = 0;
}

byte* PrestonCommand::compileCommand() {
  // do the thing
}

byte* PrestonCommand::getCommand() {
  return this->command;
}
