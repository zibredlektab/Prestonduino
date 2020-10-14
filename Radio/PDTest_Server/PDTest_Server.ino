#include <PDServer.h>

PDServer *pd;

void setup() {
  pd = new PDServer();
}

void loop() {
  pd->onLoop();
}
