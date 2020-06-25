#include "MaxTools.h"

MaxTools::MaxTools() {}

void MaxTools::debugPrint(byte* toprint, bool hex) {
  if (DEBUG) {
    for (int i = 0; i < 20; i++) {
      if (toprint[i] != "\0") {
        Serial.print(toprint[i], hex?HEX:DEC);
      } else {
        break;
      }
    }
  }
}

void MaxTools::debugPrintln(byte* toprint, bool hex) {
  debugPrint(toprint, hex);
  
}

void MaxTools::debugPrint(byte* toprint) {
  debugPrint(toprint, false);
}

void MaxTools::debugPrintln(byte* toprint) {
  debugPrint(toprint);
  debugPrint("/r/n");
}
