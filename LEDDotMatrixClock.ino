#include "DS3231.h"
#include "Adafruit_GFX.h"
#include "PS2Keyboard.h"
#include "Max72xxPanel.h"
#include "math.h"

DS3231 rtc(SDA, SCL);

const byte LDR_PIN = A0; // LDR Sensor pin
const byte CS_PIN = 10; // Attach CS to this pin, DIN to MOSI and CLK to SCK (cf http://arduino.cc/en/Reference/SPI )
const byte LM_PIN = A1;
const byte H_DISPLAYS = 4; // Horizontal displays number
const byte V_DISPLAYS = 1; // Vertical displays number
const int DataPin = 2; // d-
const int IRQpin = 3; // d+

Max72xxPanel matrix = Max72xxPanel(CS_PIN, H_DISPLAYS, V_DISPLAYS);

PS2Keyboard keyboard;

const byte WAIT = 60;
const byte SPACER = 1;
const byte FONT_WIDTH = 5 + SPACER; // The font width is 5 pixels

int waktu = 0; 
float suhu;

unsigned long intensityThrottle = 0;
byte ledIntensity = 0;

enum class STATE{WAKTU, SUHU, MENU, SET_WAKTU, SELECT_ALARM, SET_ALARM, SET_DUR, ALARM_ACTIVE};
STATE program_state;

void adjustClock(String data) {
	// byte _day = data.substring(0,2).toInt();
	// byte _month = data.substring(3,5).toInt();
	// int _year = data.substring(6,10).toInt();
	byte _hour = data.substring(0,2).toInt();
	byte _min = data.substring(3,5).toInt();
	byte _sec = data.substring(6,8).toInt();
	rtc.setTime(_hour, _min, _sec);
	// rtc.setDate(_day, _month, _year);
	Serial.println(F(">> Time successfully set!"));
}

String outputStrClock() {
	String _output;
	_output.concat(rtc.getTimeStr());
	return _output;
}

String outputStrTemp() {
	String _outputtemp;
	int waktu2 = millis();
	if (waktu == 0 || (waktu2 - waktu) >= 10000 ) {
		suhu = (float)analogRead(LM_PIN) / (2.0479 * 6); 
		waktu = waktu2;
	}
	_outputtemp.concat("  ");
	_outputtemp.concat(suhu);    
	_outputtemp.concat((char)247);
	_outputtemp.concat("C");
	return _outputtemp;
}

byte getLedIntensity(int& light) {
    int a = light * 16;
    a /= 1023;
    --a;
    return abs(a);
}

void ledIntensitySelect(const uint8_t& ldrPin) {
	unsigned long timeNow = millis();

	if (timeNow - intensityThrottle >= 1000) {
		int light = analogRead(ldrPin);
		ledIntensity = getLedIntensity(light);
		intensityThrottle = timeNow;
	}
}


void setup() {
	pinMode(LDR_PIN, INPUT_PULLUP);
  keyboard.begin(DataPin, IRQpin);
	Serial.begin(9600);
	Serial.println(F(">> Arduino 32x8 LED Dot Matrix Clock!"));
	Serial.println(F(">> Use <hh:mm:ss> format to set clock's and hour!"));
  Serial.println("Keyboard Test:");
	rtc.begin();
	matrix.setPosition(0, 0, 0);
	matrix.setPosition(1, 1, 0);
	matrix.setPosition(2, 2, 0);
	matrix.setPosition(3, 3, 0);
	matrix.setRotation(0, 1);    
	matrix.setRotation(1, 1);
	matrix.setRotation(2, 1);
	matrix.setRotation(3, 1);
}

void printMatrix(String output, int i) {
    i = i == -1 ? (FONT_WIDTH * output.length() + matrix.width() - 1 - SPACER) / 2 : i; // kalo mau print center i nya kasih -1
    int letter = i / FONT_WIDTH;
    int x = (matrix.width() - 1) - i % FONT_WIDTH;
    int y = (matrix.height() - 8) / 2; // center the text vertically

    while ( x + FONT_WIDTH - SPACER >= 0 && letter >= 0 ) {
      if ( letter < output.length() ) {
        matrix.drawChar(x, y, output[letter], HIGH, LOW, 1);
      }
      letter--;
      x -= FONT_WIDTH;
    }
    matrix.write();
}

void runningMatrix() {
  STATE prevState = program_state;
  byte detik = rtc.getTime().sec;
  String output;
  switch (program_state) {
    case STATE::WAKTU:
      output = outputStrClock();
      break;
    case STATE::SUHU:
      output = outputStrTemp();
      break;
  }
  for ( int i = 0 ; i < FONT_WIDTH * output.length() + matrix.width() - 1 - SPACER; i++ ) {
		ledIntensitySelect(LDR_PIN);
  	matrix.setIntensity(ledIntensity); // value between 0 and 15 for brightness
    matrix.fillScreen(LOW);
    int letter = i / FONT_WIDTH;
    int x = (matrix.width() - 1) - i % FONT_WIDTH;
    int y = (matrix.height() - 8) / 2; // center the text vertically
    printMatrix(output, i);
    detik = rtc.getTime().sec;
    if ((detik >= 10 && detik < 15) || (detik >= 40 && detik < 45)) {
      program_state = STATE::SUHU;
    }
    if (prevState != program_state) {
      return;
    }
    if (program_state == STATE::WAKTU) {
      output = outputStrClock();
    }
    delay(WAIT);
  }
}

void loop() {
  String output = outputStrClock();
  String outputtemp = outputStrTemp();

  // for ( int i = 0 ; i < FONT_WIDTH * output.length() + matrix.width() - 1 - SPACER; i++ ) {
	// 	ledIntensitySelect(LDR_PIN);
  // 	matrix.setIntensity(ledIntensity); // value between 0 and 15 for brightness
  //   matrix.fillScreen(LOW);
  //   output = outputStrClock();
  //   outputtemp = outputStrTemp();
  //   if (program_state == STATE::SUHU) {
  //       output = outputtemp;
  //   }
  //   int letter = i / FONT_WIDTH;
  //   int x = (matrix.width() - 1) - i % FONT_WIDTH;
  //   int y = (matrix.height() - 8) / 2; // center the text vertically

  //   while ( x + FONT_WIDTH - SPACER >= 0 && letter >= 0 ) {
  //     if ( letter < output.length() ) {
  //       matrix.drawChar(x, y, output[letter], HIGH, LOW, 1);
  //     }
  //     letter--;
  //     x -= FONT_WIDTH;

  //     detik = rtc.getTime().sec;
  //     if ((detik >= 10 && detik < 15) || (detik >= 40 && detik < 45)) {
  //       if (program_state == STATE::WAKTU) {
  //           done = true;
  //           break;
  //       }
  //     } else {
  //       if (program_state == STATE::SUHU) {
  //           done = true;
  //           break;
  //       }
  //     }
  //   }
  //   matrix.write();
  //   if (done) {
  //       break;
  //   }
  //   delay(WAIT);
  // }
  runningMatrix();

  if (keyboard.available()) {
    
    // read the next key
    char c = keyboard.read();
    
    // check for some of the special keys
    if (c == PS2_ENTER) {
      Serial.println();
    } else if (c == PS2_TAB) {
      Serial.print("[Tab]");
    } else if (c == PS2_ESC) {
      Serial.print("[ESC]");
    } else if (c == PS2_PAGEDOWN) {
      Serial.print("[PgDn]");
    } else if (c == PS2_PAGEUP) {
      Serial.print("[PgUp]");
    } else if (c == PS2_LEFTARROW) {
      Serial.print("[Left]");
    } else if (c == PS2_RIGHTARROW) {
      Serial.print("[Right]");
    } else if (c == PS2_UPARROW) {
      Serial.print("[Up]");
    } else if (c == PS2_DOWNARROW) {
      Serial.print("[Down]");
    } else if (c == PS2_DELETE) {
      Serial.print("[Del]");
    } else {
      
      // otherwise, just print all normal characters
      Serial.print(c);
    }
  }

  if (Serial.available() > 0) {
    adjustClock(Serial.readString());
  }
  Serial.println(output);  
  Serial.println(suhu);
  Serial.println(analogRead(LDR_PIN));
}