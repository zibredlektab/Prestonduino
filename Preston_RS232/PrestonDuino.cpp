/*
  PrestonDuino.cpp - Library for talking to a Preston MDR 2/3/4 over RS-232
  Created by Max Batchelder, June 2020.
*/

#include "PrestonDuino.h"


PrestonDuino::PrestonDuino(HardwareSerial& serial) {
  // Open a connection to the MDR on Serial port 'serial'
  ser = &serial;
  ser->begin(115200);
  while (!ser) {
    ;
  }
}

int PrestonDuino::sendToMDR(PrestonPacket packet) {
  // Send a PrestonPacket to the MDR, byte by byte
  int packetlen = packet.getPacketLength();
  byte* packetbytes = packet.getPacket();
  for (int i = 0; i < packetlen; i++) {
    ser->write(packetbytes[i]);
    ser->flush();
  }
}
