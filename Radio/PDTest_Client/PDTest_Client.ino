#include <PDClient.h>
//#include <U8g2lib.h>


PDClient *pd;

//U8G2_SSD1306_128X64_NONAME_2_HW_I2C oled (U8G2_R0);

void setup() {
  // put your setup code here, to run once:
  
  Serial.begin(115200);
  //oled.begin();
  
  pd = new PDClient(map(analogRead(A0), 0, 1023, 0, 0xF));



}

void loop() {
  // put your main code here, to run repeatedly:
  pd->onLoop();

  /*oled.firstPage();
  do {
    
    oled.setFont(u8g2_font_logisoso18_tf);
    //command_reply lensdata = mdr->data(mode);

    oled.setCursor(11,20);
    oled.print("0x");
    oled.print(pd->getAddress(), HEX);
  } while(oled.nextPage());
  //Serial.println(pd->getFocusDistance()/305);*/
}
