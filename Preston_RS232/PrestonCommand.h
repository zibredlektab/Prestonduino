#ifndef PrestonCommand_h
#define PrestonCommand_h

#include "Arduino.h"

class PrestonCommand {
  private:
    byte mode;
    byte* data;
    int checksum;
    byte* command;
    
    int computeSum();
    byte* compileCommand();

  public:
    PrestonCommand(byte cmd_mode, byte* cmd_data);
    byte getMode();
    int setMode(byte cmd_mode);
    byte* getData();
    int setData(byte* cmd_data);
    byte* getCommand();
};

#endif