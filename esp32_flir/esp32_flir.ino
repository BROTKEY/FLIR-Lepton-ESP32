#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <math.h>

#define PACKET_SIZE 80
#define PACKETS_PER_FRAME 60
#define LEPTON 0x2A

#define LENGREG  0x0006
#define CMDREG   0x0004
#define DATA0REG 0x0008
#define DATA1REG 0x000A

short wireWrite16(short target, short data, bool end = true, bool begin = true) {
  if (begin) {
    Wire.beginTransmission(LEPTON);
  }
  Wire.write(highByte(target));
  Wire.write(lowByte(target));
  Wire.write(highByte(data));
  Wire.write(lowByte(data));
  if (end) {
    Wire.endTransmission();
  }
  return 0;
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

SPISettings settings(20000000, MSBFIRST, SPI_MODE3);
SPIClass test = SPIClass(HSPI);
Adafruit_ST7789 tft = Adafruit_ST7789(&test, 4, 2, 15);
uint16_t frame_buffer[PACKETS_PER_FRAME * PACKET_SIZE] = {0};
uint16_t *p;

void setup(){
  tft.setSPISpeed(20000000);
  delay(250);
  tft.init(240,320);
  tft.setRotation(1);
  tft.fillScreen(0x0000);
  tft.setTextColor(0xFFFF);
  tft.setTextSize(1);
  tft.print("BOOTING...");

  ledcAttachPin(25, 1);
  ledcSetup(1, 25000000, 1);
  ledcWrite(1, 1);
  delay(5000);
  Serial.begin(115200);

  Wire.begin();

  wireWrite16(LENGREG, 0x2);
  wireWrite16(DATA0REG, 0x0, false, true);
  wireWrite16(DATA1REG, 0x0, true, false);
  wireWrite16(CMDREG, 0x101);
  delay(100); 

  short state = 0;

  do {
    state = getState();
    Serial.print(" state: ");
    Serial.print(state,HEX);  
    delay(250);
  } while (state != 6);

  state = getAGCState();
  Serial.print(" AGC state: ");
  Serial.print(state,HEX);  
  delay(250);

  Serial.println("");
  Serial.println("\r\nLepton Ready");

  Serial.println("\r\nsync...");

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
  tft.fillScreen(0x0000);
  // tft.setCursor(83, 0);
  // tft.println("HOT:");
  // tft.setCursor(83, 16);
  // tft.println("COLD:");
}

float pixelToTemp(uint16_t pixel) {
  return (pixel/ 100) - 273.15;
}

uint16_t tempToColor(float temp, float hotspot, float coldspot) {
  uint16_t norm = ((temp - coldspot) / (hotspot - coldspot)) * 255;

  uint16_t b = (norm >> 3) & 0x1f;
  uint16_t g = ((norm >> 2) & 0x3f) << 5;
  uint16_t r = ((norm >> 3) & 0x1f) << 11;

  return (uint16_t) (r | g | b);
}


void loop(){
  p = frame_buffer;
  bool dead = false;
  float hotspot = -690;
  float coldspot = 690;
  long start = millis();

  SPI.beginTransaction(settings);
  for(int row = 0; row < PACKETS_PER_FRAME; row++) {
    digitalWrite(5, LOW);
    SPI.transfer(0x00);
    SPI.transfer(0x00);
    uint8_t byte1 = SPI.transfer(0x00);
    uint8_t byte2 = SPI.transfer(0x00);
    uint16_t pixel = ((uint16_t)byte1 << 8) | byte2;
    if(pixel == 0xFFFF) {
      dead = true;
    };
    for(int col = 0; col < PACKET_SIZE; col++) {
      byte1 = SPI.transfer(0x00);
      byte2 = SPI.transfer(0x00);
      pixel = ((uint16_t)byte1 << 8) | byte2;
      *(p+row*PACKET_SIZE+col) = pixel;
    };
    digitalWrite(5, HIGH);
  };
  SPI.endTransaction();

  for(int row = 0; row < PACKETS_PER_FRAME; row++) {
    for(int col = 0; col < PACKET_SIZE; col++) {
      float temp = pixelToTemp(*(p+row*PACKET_SIZE+col));

      if (temp > hotspot) {
        hotspot = temp;
      }

      if (temp < coldspot) {
        coldspot = temp;
      }
    };
  };

  std::for_each(IIter, IIter, Funct)
 
  for(int row = 0; row < PACKETS_PER_FRAME; row++) {
    for(int col = 0; col < PACKET_SIZE; col++) {
      *(p+row*PACKET_SIZE+col) = tempToColor(pixelToTemp(*(p+row*PACKET_SIZE+col)), hotspot, coldspot);
    };
  };

  if (!dead) {
    tft.startWrite();
    tft.setAddrWindow(0, 0, 80, 60);
    tft.writePixels(frame_buffer, PACKETS_PER_FRAME * PACKET_SIZE);
    tft.endWrite();
    // tft.setCursor(83, 8);
    // tft.print(hotspot);
    // tft.setCursor(83, 24);
    // tft.print(coldspot);
    Serial.print(millis() - start);
    Serial.println(" : screen end");
  }
}