#include <RH_RF95.h>
//#include <RHReliableDatagram.h>


#define SERVER_ADDRESS 0
#define CLIENT_ADDRESS 1


RH_RF95 driver;
//RHReliableDatagram manager(driver, SERVER_ADDRESS);


bool ledison = true;

void setup() {
  Serial.begin(9600);  
  if (driver.init()) {
    driver.setModemConfig(RH_RF95::Bw500Cr45Sf128);
  }
  
 /* if (!manager.init()) {
    Serial.println("init failed");
    
  } else {
    
    Serial.println("init complete");
  }*/

  
  pinMode(LED_BUILTIN, OUTPUT);

}

uint8_t buf[11];
uint32_t dist;

void loop() {
  
  if (driver.available()) {
    uint8_t len = sizeof(buf);
    if (driver.recv(buf, &len)){
      dist = strtoul(buf, NULL, 10);
      Serial.print("Distance: ");
      Serial.println(dist);
    }
  }


}
