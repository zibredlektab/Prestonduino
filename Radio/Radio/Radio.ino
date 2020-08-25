#include <RH_RF95.h>
//#include <RHReliableDatagram.h>


#include <U8g2lib.h>

#define SERVER_ADDRESS 0
#define CLIENT_ADDRESS 1


RH_RF95 driver;
//RHReliableDatagram manager(driver, SERVER_ADDRESS);

U8G2_SSD1306_128X64_NONAME_2_HW_I2C oled (U8G2_R0);

bool ledison = true;

void setup() {

  
  oled.begin();
  
  Serial.begin(9600);  
  if (driver.init()) {
    driver.setFrequency(915.0);
    //driver.setModemConfig(RH_RF95::Bw31_25Cr48Sf512);
  }
  
 /* if (!manager.init()) {
    Serial.println("init failed");
    
  } else {
    
    Serial.println("init complete");
  }*/

  
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
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

  double distdouble = (double)dist/305;
  double distft;
  double distin = modf(distdouble, &distft);
  distin *= 12;

  distft += 0.001;
  double distftrounded = floor(distft*10)/10;
  
  oled.firstPage();
  do {
    
    oled.setFont(u8g2_font_logisoso18_tf);
    //command_reply lensdata = mdr->data(mode);

    oled.setCursor(11,20);
    oled.print("Focus:");
    
    oled.setCursor(11, 60);

    if (distftrounded < 4000) {
      oled.print((uint32_t)distftrounded);
      oled.print("'");
      oled.print(distin);
      oled.print("\"");
    } else {
      oled.print("INF");
    }



  } while (oled.nextPage());


}
