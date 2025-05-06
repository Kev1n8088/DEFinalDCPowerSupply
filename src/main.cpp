#include <Arduino.h>
#include "Adafruit_seesaw.h"
#include <AP33772S.h>
#include <Adafruit_SSD1327.h>
#include <Wire.h>
#include <Adafruit_NeoPixel.h>

#define OLED_RESET -1
Adafruit_SSD1327 display(128, 128, &Wire, OLED_RESET, 1000000);
AP33772S usbpd;
// put function declarations here:

#define VOLTAGE_STEP 0.1
#define CURRENT_STEP 0.1
#define VOLTAGE_MAX 20.0
#define CURRENT_MAX 3.0
#define VOLTAGE_MIN 0.0
#define CURRENT_MIN 0.0

#define PWR_PIN A2
#define LED_PIN A3
#define LED_COUNT 1

#define OLED_ADDR 0x3D
#define ENCD_ADDR 0x49

#define SS_SWITCH_SELECT 1
#define SS_SWITCH_UP     2
#define SS_SWITCH_LEFT   3
#define SS_SWITCH_DOWN   4
#define SS_SWITCH_RIGHT  5


Adafruit_NeoPixel pixel(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

Adafruit_seesaw ss;
int32_t encoder_position;
bool encodersReleased = true;
bool buttonReleased = true;

int mode = 0; //0 off, 1 on, 2 OCP

struct displaySelected{
  int voltage = 1; // 2 is x10, 1 is x1, 0 is x0.1
  int current = 1; // 1 is x1, 0 is x0.1
  int line = 0; //0 is voltage, 1 is current
};

struct currentAndVoltage{
  float voltage = 0.0;
  float current = 0.0;
};

currentAndVoltage primary = currentAndVoltage();
currentAndVoltage secondary = currentAndVoltage();

displaySelected displaySelect = displaySelected();

AP33772S usbpd;

void setup() {
  initSS();
  initCurrentsAndVoltages();
  initDisplay();
  pinMode(PWR_PIN, INPUT_PULLUP);
  pixel.begin();
  pixel.show();  
  pixel.setBrightness(50); 
  usbpd.begin();
  // put your setup code here, to run once:
}

void loop() {
  displaySettings();
  displayData();
  neopix();
  onOff();
  checkForEncoderUpdate();
  provideOutput();

  delay(10);
  display.clearDisplay();

  // put your main code here, to run repeatedly:
}

void initCurrentsAndVoltages(){
  primary.voltage = 0.0;
  primary.current = 0.0;
  secondary.voltage = 0.0;
  secondary.current = 0.0;
}

void swapCurrentsAndVoltages(){
  currentAndVoltage temp = primary;
  primary = secondary;
  secondary = temp;
}

void initSS(){
  
  Serial.begin(115200);
  while (!Serial) delay(10);

  Serial.println("Looking for seesaw!");
  
  if (! ss.begin(ENCD_ADDR)) {
    Serial.println("Couldn't find seesaw on default address");
    while(1) delay(10);
  }
  Serial.println("seesaw started");
  uint32_t version = ((ss.getVersion() >> 16) & 0xFFFF);
  if (version  != 5740){
    Serial.print("Wrong firmware loaded? ");
    Serial.println(version);
    while(1) delay(10);
  }
  Serial.println("Found Product 5740");

  ss.pinMode(SS_SWITCH_UP, INPUT_PULLUP);
  ss.pinMode(SS_SWITCH_DOWN, INPUT_PULLUP);
  ss.pinMode(SS_SWITCH_LEFT, INPUT_PULLUP);
  ss.pinMode(SS_SWITCH_RIGHT, INPUT_PULLUP);
  ss.pinMode(SS_SWITCH_SELECT, INPUT_PULLUP);

  // get starting position
  encoder_position = ss.getEncoderPosition();

  Serial.println("Turning on interrupts");
  ss.enableEncoderInterrupt();
  ss.setGPIOInterrupts((uint32_t)1 << SS_SWITCH_UP, 1);
}

void initDisplay(){
  if ( ! display.begin(0x3D) ) {
     Serial.println("Unable to initialize OLED");
     while (1) yield();
  }
  display.clearDisplay();
  display.display();

}

void checkForEncoderUpdate(){
  if(encodersReleased){
      if(! ss.digitalRead(SS_SWITCH_DOWN)){
        displaySelect.line += 1;
        encodersReleased = false;
      }else if(! ss.digitalRead(SS_SWITCH_UP)){
        displaySelect.line -= 1;
        encodersReleased = false;
      }else if(! ss.digitalRead(SS_SWITCH_LEFT)){
        if(displaySelect.line == 0){
          displaySelect.voltage -= 1;
        }else if(displaySelect.line == 1){
          displaySelect.current -= 1;
        }
        encodersReleased = false;
      }else if(! ss.digitalRead(SS_SWITCH_RIGHT)){
        if(displaySelect.line == 0){
          displaySelect.voltage += 1;
        }else if(displaySelect.line == 1){
          displaySelect.current += 1;
        }
        encodersReleased = false;
      }else if(! ss.digitalRead(SS_SWITCH_SELECT)){
        swapCurrentsAndVoltages();
        encodersReleased = false;
    }
  }

  if(ss.digitalRead(SS_SWITCH_SELECT) && ss.digitalRead(SS_SWITCH_UP) && ss.digitalRead(SS_SWITCH_DOWN) && ss.digitalRead(SS_SWITCH_LEFT) && ss.digitalRead(SS_SWITCH_RIGHT)){
    encodersReleased = true;
  }

  if (displaySelect.line < 0) {
    displaySelect.line = 1;
  } else if (displaySelect.line > 1) {
    displaySelect.line = 0;
  }
  if (displaySelect.voltage < 0) {
    displaySelect.voltage = 2;
  } else if (displaySelect.voltage > 2) {
    displaySelect.voltage = 0;
  }
  if (displaySelect.current < 1) {
    displaySelect.current = 2;
  } else if (displaySelect.current > 2) {
    displaySelect.current = 1;
  }

  int32_t new_position = ss.getEncoderPosition();
  // did we move around?
  if (encoder_position != new_position) {

      int32_t delta = (new_position - encoder_position) / 10;

      encoder_position = new_position;      // and save for next round
      switch(displaySelect.line){
        case (0):
          primary.voltage += delta * VOLTAGE_STEP * pow(10, displaySelect.voltage);
          break;
        case (1):
          primary.current += delta * CURRENT_STEP * pow(10, displaySelect.current);
          break;
      }
  }

}

void onOff(){
  if (buttonReleased && !digitalRead(PWR_PIN)){
    if (mode == 0){
      mode = 1;
    }else{
      mode = 0;
    }
  }

  if(digitalRead(PWR_PIN)){
    buttonReleased = true;
  }
}

void neopix(){
  switch(mode){
    case 0:
      pixel.setPixelColor(0, pixel.Color(0, 150, 150));
      break;
    case 1:
      pixel.setPixelColor(0, pixel.Color(0, 150, 0));
      break;
    case 2:
      pixel.setPixelColor(0, pixel.Color(150, 0, 0));
      break;  
  }
}

void provideOutput(){
  if (mode == 1){
    usbpd.setOutput(1);
  }else{
    usbpd.setOutput(0);
  }

  usbpd.setPPSPDO(usbpd.getPPSIndex(), primary.voltage * 1000, primary.current * 1000);
  if (usbpd.readCurrent() > primary.current * 1000){
    usbpd.setOutput(0);
    mode = 2;
  }

}

void displaySettings(){
  display.setTextSize(1);
  display.setTextColor(SSD1327_WHITE);
  display.setCursor(0, 0);
  display.setTextWrap(false);

  display.println("Primary");
  display.print(primary.voltage);
  display.println("V");
  display.print(primary.current);
  display.println("A");
  display.print(primary.voltage * primary.current);
  display.println("W");

  display.setCursor(64, 0);
  display.println("Secondary");
  display.print(secondary.voltage);
  display.println("V");
  display.print(secondary.current);
  display.println("A");
  display.print(secondary.voltage * secondary.current);
  display.println("W");
}

void displayData(){
  display.setTextSize(1);
  display.setTextColor(SSD1327_WHITE);
  display.setCursor(0, 64);
  display.setTextWrap(false);

  display.println("Output:");
  //display.setTextSize(2);
  switch(mode){
    case 0:
      display.println("Standby");
      break;
    case 1:
      display.println("Active");
      break;
    case 2:
      display.println("Overcurrent!");
      break;  
  }

  display.print(usbpd.readVoltage() / 1000.0);
  display.println("V");
  display.print(usbpd.readCurrent() / 1000.0);
  display.println("A");
  display.print(usbpd.readCurrent() / 1000.0 * usbpd.readVoltage() / 1000.0);
  display.println("W");

}