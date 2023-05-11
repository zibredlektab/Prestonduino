#include <PDServer.h>

#define CHCHECKDELAY 1000

PDServer *pd;

uint8_t pins[4] = {0, 1, 13, 14};

unsigned long long lastchannelcheck;

void setup() {

  Serial.begin(115200);
  while(!Serial);
  for (int i = 0; i < 4; i++) {
    pinMode(pins[i], INPUT_PULLUP);
  }

  
  uint8_t newchannel = readSwitch();
  
  pd = new PDServer(readSwitch(), Serial1);
  lastchannelcheck = millis();
}

unsigned long long ledtime = 0;
bool ledon = false;

void loop() {
  pd->onLoop();

  if (lastchannelcheck + CHCHECKDELAY < millis()) {
    uint8_t newchannel = readSwitch();
    if (pd->getChannel() != newchannel) {
      pd->setChannel(newchannel);
    }
    lastchannelcheck = millis();
  }

  if (millis() > ledtime+500) {
    ledon ? digitalWrite(13, LOW) : digitalWrite(13, HIGH);
    ledon = !ledon;
    ledtime = millis();
  }
}

uint8_t readSwitch() {
  return 0xA; // for now
  uint8_t value = 0;
  for (int i = 0; i < 4; i++) {
    if (!digitalRead(pins[i])) {
      value += 1 << i;
    }
  }
  return value;
}
