#include <PDClient.h>

PDClient *pd;

void setup() {
  // put your setup code here, to run once:
  pd = new PDClient(1);
  Serial.begin(115200);

  
  
}

void loop() {
  // put your main code here, to run repeatedly:
  
  Serial.println(pd->getFocusDistance()/305);
}
