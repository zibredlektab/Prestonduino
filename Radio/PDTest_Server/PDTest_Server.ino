#include <PDServer.h>

PDServer *pd;

void setup() {
  pd = new PDServer(0xA, Serial1);
}

void loop() {
  pd->onLoop();
}
