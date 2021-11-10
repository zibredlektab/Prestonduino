#include <PDServer.h>

#define CHCHECKDELAY 1000

PDServer *pd;

uint8_t pins[4] = {0, 1, 13, 14};

unsigned long long lastchannelcheck;

void setup() {

  Serial.begin(115200);
  for (int i = 0; i < 4; i++) {
    pinMode(pins[i], INPUT_PULLUP);
  }

  
  uint8_t newchannel = readSwitch();
  
  pd = new PDServer(readSwitch(), Serial1);
  lastchannelcheck = millis();
}

void loop() {
  pd->onLoop();

  if (lastchannelcheck + CHCHECKDELAY < millis()) {
    uint8_t newchannel = readSwitch();
    if (pd->getChannel() != newchannel) {
      pd->setChannel(newchannel);
    }
    lastchannelcheck = millis();
  }
}

uint8_t readSwitch() {
  return 0xA;
  uint8_t value = 0;
  for (int i = 0; i < 4; i++) {
    if (!digitalRead(pins[i])) {
      value += 1 << i;
    }
  }
  return value;
}
