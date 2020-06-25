#ifndef tools_h
#define tools_h

#include "Arduino.h"

#define DEBUG true

class MaxTools {
  public:
    MaxTools();
    void debugPrint(byte* toprint);
    void debugPrintln(byte* toprint);
    void debugPrint(byte* toprint, bool hex);
    void debugPrintln(byte* toprint, bool hex);
  
};



#endif
