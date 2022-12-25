#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <math.h>

#define PACKET_SIZE (164)
#define PACKETS_PER_FRAME 60
#define LEPTON 0x2A

#define LENGREG  0x0006
#define CMDREG   0x0004
#define DATA0REG 0x0008
#define DATA1REG 0x000A

SPISettings settings(20000000, MSBFIRST, SPI_MODE3);
Adafruit_SH1106G display = Adafruit_SH1106G(128, 64, &Wire, -1);
byte frame_buffer[164 * 60] = {0};
byte *p;

short wireWrite16(short data) {
  return Wire.write(highByte(data)) + Wire.write(lowByte(data));
}

short getState() {
  short state;
  Wire.beginTransmission(LEPTON);
  Wire.write(byte(0x00));
  Wire.write(byte(0x02));
  Wire.endTransmission();
  
  int readBytes = Wire.requestFrom(LEPTON, 2);
  state = Wire.read();
  state = state << 8;
  state |= Wire.read();

  return state;
}

short getAGCState() {
  short state = 0;
  Wire.beginTransmission(LEPTON);
  Wire.write(byte(0x00));
  Wire.write(byte(0x04));
  Wire.write(byte(0x01));
  Wire.write(byte(0x00));
  Wire.endTransmission();
  
  Wire.requestFrom(LEPTON, 2);
  state = Wire.read();
  state = state << 8;
  state |= Wire.read();
  Wire.endTransmission();

  return state;
}

void setup(){
  display.begin();
  display.clearDisplay();
  display.setTextColor(SH110X_WHITE);
  display.setTextSize(1);
  display.print("BOOTING...");
  display.display();

  ledcAttachPin(13, 1);
  ledcSetup(1, 25000000, 1);
  ledcWrite(1, 1);
  delay(5000);  
  Serial.begin(115200);

  Wire.begin();

  uint16_t state = 0;
  
  do {
    state = getState();
    Serial.print(" state: ");
    Serial.print(state);
    delay(250);
  } while (state != 6);

  // Serial.println("\r\nLepton Ready");
  // Serial.println("");

  // Wire.beginTransmission(LEPTON);
  // wireWrite16(DATA0REG);
  // wireWrite16(0x0001);
  // Wire.endTransmission();

  // Wire.beginTransmission(LEPTON);
  // wireWrite16(DATA1REG);
  // wireWrite16(0x0000);
  // Wire.endTransmission();

  // Wire.beginTransmission(LEPTON);
  // wireWrite16(LENGREG);
  // wireWrite16(0x0002);
  // Wire.endTransmission();

  // Wire.beginTransmission(LEPTON);
  // wireWrite16(CMDREG);
  // wireWrite16(0x0101);
  // Wire.endTransmission();
  
  // do {
  //   state = getState();
  //   Serial.print(" state: ");
  //   Serial.print(state);  
  //   delay(250);
  // } while (state != 6);

  // Serial.print("\r\nAGCState: ");
  // Serial.print(getAGCState());

  pinMode(5, OUTPUT);
  pinMode(27, INPUT);
  digitalWrite(5, LOW);

  SPI.begin();

  SPI.beginTransaction(settings);
  digitalWrite(5, LOW);
  SPI.transfer16(0x0000);
  digitalWrite(5, HIGH);
  SPI.endTransaction();
  delay(185);
  SPI.beginTransaction(settings);
  digitalWrite(5, LOW);
  for(int y = 0; y < 82; y++) {
    SPI.transfer16(0x0000);
  };
  digitalWrite(5, HIGH);
  SPI.endTransaction();

  Serial.println("\r\nSetup has been finished.");
}

void loop(){
  p = frame_buffer;

  display.clearDisplay();

  SPI.beginTransaction(settings);
  for(int j = 0; j < 60; j++) {
    digitalWrite(5, LOW);
    for(int k = 0; k < 164; k++) {
      *(p+j*PACKET_SIZE+k) = SPI.transfer(0x00);
    };
    digitalWrite(5, HIGH);
  }
  SPI.endTransaction();

  bool dead = false;
  float hotspot = 0;
  float center = 0;

  for(int row=0; row<PACKETS_PER_FRAME; row++) {
    uint8_t stat1 = *(p+row*PACKET_SIZE+(2));
    uint8_t stat2 = *(p+row*PACKET_SIZE+(3));

    if((((uint16_t)stat2 << 8) | stat1) == 0xFFFF) {
      dead = true;
    }

    for(int col=4; col<(PACKET_SIZE); col+=2) {
      uint8_t byte1 = *(p+row*PACKET_SIZE+(col));
      uint8_t byte2 = *(p+row*PACKET_SIZE+(col+1));
      float temp = ((((uint16_t)byte1 << 8) | byte2)/ 100) - 273.15;
      bool centerBool = false;
      //Serial.print(temp); Serial.print(" ");

      if (temp > hotspot) {
        hotspot = temp;
      }

      if (row == 29 && col == 80) {
        center = temp;
        centerBool = true;
      }
      
      if ((temp > 27 && !centerBool) || (centerBool && temp < 27)) {
        display.drawPixel((col-3)/2, row, SH110X_WHITE);
      } else {
        display.drawPixel((col-3)/2, row, SH110X_BLACK);
      }
    }
    //Serial.println(" ");
  }
  //Serial.println(" ");
  
  display.drawLine(80, 0, 80, 64, SH110X_WHITE);
  display.fillRect(0, 60, 124, 4, SH110X_WHITE);
  display.setCursor(83, 0);
  display.println("HOT:");
  display.setCursor(83, 8);
  display.print(hotspot);
  display.setCursor(83, 24);
  display.println("CENT:");
  display.setCursor(83, 32);
  display.print(center);

  if (!dead) {
    display.display();
  }
}