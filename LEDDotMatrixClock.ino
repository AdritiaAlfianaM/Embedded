#include "DS3231.h" // untuk akses rtc
#include "Adafruit_GFX.h" // nampilin di dot matrix
#include "PS2Keyboard.h" // buat inputan keyboard
#include "Max72xxPanel.h" // buat kasi ic font" ke adafruit
#include "math.h"

DS3231 rtc(SDA, SCL); // inisalisasi objek rtc

const byte LDR_PIN = A0; // LDR Sensor pin
const byte CS_PIN = 10; // Attach CS to this pin, DIN to MOSI and CLK to SCK (cf http://arduino.cc/en/Reference/SPI )
const byte LM_PIN = A1;
const byte H_DISPLAYS = 4; // Horizontal displays number
const byte V_DISPLAYS = 1; // Vertical displays number
const byte DataPin = 2; // d-
const byte IRQpin = 3; // d+

Max72xxPanel matrix = Max72xxPanel(CS_PIN, H_DISPLAYS, V_DISPLAYS);

PS2Keyboard keyboard; // objek keyboard

const byte WAIT = 60; // untuk delay
const byte SPACER = 1; // jarak antar font
const byte FONT_WIDTH = 5 + SPACER; // The font width is 5 pixels

int waktu = 0; 
float suhu;

struct Alarm {
  bool active;
  byte hours;
  byte minutes;
  byte duration; // duration in seconds
};

Alarm alarms[] = {
  { false },
  { false },
  { false },
  { false },
  { false }
}; // nyimpen alarm dalam array

unsigned long intensityThrottle = 0; // biar tidak mengdisko
byte ledIntensity = 0;

enum class STATE{WAKTU, SUHU, MENU, SET_TIMER, SET_WAKTU, SELECT_ALARM, SET_ALARM, SET_DUR, ALARM_ACTIVE, TIMER_ACTIVE, TIMER_DONE, SET_ALARM5};
STATE program_state; // ngatur state

enum class M_STATE{JAM, ALARM, TIMER};
M_STATE menu_state; // ngatur state menu

enum class A_STATE{A1, A2, A3, A4, A5};
A_STATE alarm_state; // ngatur state alarm

String inputAlarmHours = "__";
String inputAlarmMinutes = "__";
byte inputAlarmDuration = 0;
byte inputtedAlarm = 0; // ngitung berapa yg udah diinput

String alarm5Input = ""; 

byte activeAlarm = 0; // alarm berapa yg aktif
unsigned long alarmStartTime = 0;
unsigned long timerStartTime = 0;
unsigned long timerDoneStartTime; // mulainya string done untuk ditampilkan 

String inputTimerMinutes = "__";
String inputTimerSeconds = "__";
byte inputtedTimer = 0;

String inputClockHours = "__";
String inputClockMinutes = "__";
byte inputtedClock = 0;

String NRP = "07211940000017";
String NAMA = "Adritia Alfiana Merdila";

void adjustClock(String data) {
	byte _hour = data.substring(0,2).toInt();
	byte _min = data.substring(3,5).toInt();
	byte _sec = data.substring(6,8).toInt();
	rtc.setTime(_hour, _min, _sec);
	Serial.println(F(">> Time successfully set!"));
}

String outputStrClock() {
	String _output;
	_output.concat(rtc.getTimeStr());
	return _output;
}

String outputStrTemp() {
	String _outputtemp;
  float suhurtc = rtc.getTemp();
	int waktu2 = millis();
	if (waktu == 0 || (waktu2 - waktu) >= 1000 ) {
    do {
      suhu = abs(((float)analogRead(LM_PIN) / (2.0479))); 
      waktu = waktu2;
    } while (abs(suhurtc - suhu) > 2);
	} // agar temp mengtidak mengdisko
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
    return max(0, a);
}

void ledIntensitySelect(const uint8_t& ldrPin) {
	unsigned long timeNow = millis();

	//if (timeNow - intensityThrottle >= 1000) {
		int light = analogRead(ldrPin);
		ledIntensity = getLedIntensity(light);
		intensityThrottle = timeNow;
	//}
}

void resetAlarmInput() {
  inputtedAlarm = 0;
  inputAlarmHours = "__";
  inputAlarmMinutes = "__";
  inputAlarmDuration = 0;
} // mereset input alarm, kalo tekan esc terus masuk ke alarmnya jadi __ : __

void setup() {
	pinMode(LDR_PIN, INPUT_PULLUP);
  keyboard.begin(DataPin, IRQpin); // mulai keyboardnya
	Serial.begin(9600);
	Serial.println(F(">> Arduino 32x8 LED Dot Matrix Clock!"));
	Serial.println(F(">> Use <hh:mm:ss> format to set clock's and hour!"));
	rtc.begin();
	matrix.setPosition(0, 0, 0);
	matrix.setPosition(1, 1, 0);
	matrix.setPosition(2, 2, 0);
	matrix.setPosition(3, 3, 0);
	matrix.setRotation(0, 1);    
	matrix.setRotation(1, 1);
	matrix.setRotation(2, 1);
	matrix.setRotation(3, 1); 

  attachInterrupt(digitalPinToInterrupt(DataPin), keyboardHandler, FALLING); // memasang interrupt
}

void printMatrix(String output, int i) {
  if (i == -1) { // kalo mau print center i nya kasih -1
    i = (FONT_WIDTH * output.length() + matrix.width() - 1 - SPACER) / 2;
  } else if (i == -2) { // kalo mau print rata kanan i nya kasih -2
    i = FONT_WIDTH * alarm5Input.length() - SPACER;
  }
  int letter = i / FONT_WIDTH;
  int x = (matrix.width() - 1) - i % FONT_WIDTH;
  int y = (matrix.height() - 8) / 2; // center the text vertically

  while ( x + FONT_WIDTH - SPACER >= 0 && letter >= 0 ) {
    if ( letter < output.length() ) {
      matrix.drawChar(x, y, output[letter], HIGH, LOW, 1);
    }
    --letter;
    x -= FONT_WIDTH;
  }
  matrix.write();
} // fungsi buat print matrix

void runningMatrix() {
  STATE prevState = program_state; // nyimpan state sebelumnya
  byte detik = rtc.getTime().sec;
  String output;
  switch (program_state) { // buat ngecek state dan menentukan outputnya
    case STATE::WAKTU:
      output = outputStrClock();
      break;
    case STATE::SUHU:
      output = outputStrTemp();
      break;
    case STATE::ALARM_ACTIVE:
      if (millis() - alarmStartTime > alarms[activeAlarm].duration * 1000) { // ngecek alarmnya sudah melebihi durasi atau tidak
        alarms[activeAlarm].active = false;
        program_state = STATE::WAKTU;
        return;
      } else { // ngecek alarm yg aktif yg mana
        switch (activeAlarm) {
          case 0:
            output = NRP;
            break;
          case 1:
            output = NAMA;
            break;
          case 2:
            output = NRP + " ";
            output += NAMA;
            break;
          case 3:
            output = String(alarms[activeAlarm].duration - ((millis() - alarmStartTime) / 1000)) + " s"; // output durasi terus countdown
            break;
          case 4:
            output = alarm5Input;
            break;
        }
      }
      break;
    default: // ketika ngga ada di switch masuk return
      return;
  }
  for ( int i = 0 ; i < FONT_WIDTH * output.length() + matrix.width() - 1 - SPACER; i++ ) { // ngatur posisi print
		ledIntensitySelect(LDR_PIN);
  	matrix.setIntensity(ledIntensity); // value between 0 and 15 for brightness
    matrix.fillScreen(LOW);
    int letter = i / FONT_WIDTH;
    int x = (matrix.width() - 1) - i % FONT_WIDTH;
    int y = (matrix.height() - 8) / 2; // center the text vertically
    printMatrix(output, i);
    detik = rtc.getTime().sec;
    byte menit = rtc.getTime().min;
    byte jam = rtc.getTime().hour;

    if (prevState != program_state) { // kalo state berubah karena interrupt do
      matrix.fillScreen(LOW);
      return;
    }

    if (program_state != STATE::ALARM_ACTIVE) {
      if ((detik >= 10 && detik < 15) || (detik >= 40 && detik < 45)) {
        program_state = STATE::SUHU;
      } else {
        program_state = STATE::WAKTU;
      }

      for (byte j = 0; j < 5; ++j) { // mengecek apa ada alarm aktif di waktu itu
        if (alarms[j].hours == jam && alarms[j].minutes == menit && alarms[j].active) {
          program_state = STATE::ALARM_ACTIVE;
          activeAlarm = j;
          alarmStartTime = millis();
        }
      }
    } else {
      if (millis() - alarmStartTime > alarms[activeAlarm].duration * 1000) { // ngecek alarmnya sudah melebihi durasi atau tidak
        alarms[activeAlarm].active = false;
        program_state = STATE::WAKTU;
        return;
      }
    }

    if (prevState != program_state) { // kalo state berubah karena interrupt do
      matrix.fillScreen(LOW);
      return;
    }
    if (program_state == STATE::WAKTU) {
      output = outputStrClock();
    }
    if (program_state == STATE::ALARM_ACTIVE && activeAlarm == 3) { // supaya detiknya keupdate
      output = String(alarms[activeAlarm].duration - ((millis() - alarmStartTime) / 1000)) + " s";
    }
    delay(WAIT);
  }
} // fungsi running text

void loop() {
  matrix.fillScreen(LOW); // clear matrix
  ledIntensitySelect(LDR_PIN); // menentukan
  matrix.setIntensity(ledIntensity); // di apply ke matrix

  if (program_state == STATE::ALARM_ACTIVE && millis() - alarmStartTime > alarms[activeAlarm].duration * 1000) { // ngecek alarmnya sudah melebihi durasi atau tidak
    alarms[activeAlarm].active = false;
    program_state = STATE::WAKTU;
  }
  switch (program_state) { // kalo di loop buat cek state
    case STATE::WAKTU:
    case STATE::SUHU:
      runningMatrix();
      break;
    case STATE::ALARM_ACTIVE: {
      if (activeAlarm == 3) {
        printMatrix(String(alarms[activeAlarm].duration - ((millis() - alarmStartTime) / 1000)) + " s", -1);
      } else {
        runningMatrix();
      }
      break;
    }
    case STATE::MENU: {
      switch (menu_state) {
        case M_STATE::JAM:
          printMatrix("JAM", -1);
          break;
        case M_STATE::ALARM:
          printMatrix("ALARM", -1);
          break;
        case M_STATE::TIMER:
          printMatrix("TIMER", -1);
          break;
      }
      break;
    }
    case STATE::SET_TIMER: {
      printMatrix(inputTimerMinutes + ":" + inputTimerSeconds, -1);
      break;      
    }
    case STATE::SET_WAKTU: {
      printMatrix(inputClockHours + ":" + inputClockMinutes, -1);
      break;   
    }
    case STATE::SELECT_ALARM: {
      switch (alarm_state) {
        case A_STATE::A1:
          printMatrix("1", -1);
          break;
        case A_STATE::A2:
          printMatrix("2", -1);
          break;
        case A_STATE::A3:
          printMatrix("3", -1);
          break;
        case A_STATE::A4:
          printMatrix("4", -1);
          break;
        case A_STATE::A5:
          printMatrix("5", -1);
          break;
      }
      break;
    }
    case STATE::SET_ALARM5: {
      printMatrix(alarm5Input, -2);
      break;
    }
    case STATE::SET_ALARM: {
      printMatrix(inputAlarmHours + ":" + inputAlarmMinutes, -1);
      break;
    }
    case STATE::SET_DUR: {
      printMatrix(String(inputAlarmDuration) + " s", -1);
      break;
    }
    case STATE::TIMER_ACTIVE: {
      unsigned long seconds = inputTimerMinutes.toInt() * 60 + inputTimerSeconds.toInt();
      unsigned long timeDiff = millis() - timerStartTime; // waktu sekarang - waktu mulai timer (durasi)
      if (timeDiff > (seconds * 1000)) { // buat ngecek timer selesai
        program_state = STATE::TIMER_DONE;
        timerDoneStartTime = millis();
      }
      unsigned int secsLeft = seconds - (timeDiff / 1000); //sisa detik
      unsigned int minsLeft = secsLeft / 60; // sisa menit tapi detiknya masi kaya awal
      secsLeft -= minsLeft * 60; // penyesuaian detik 
      String minsOutput = String(minsLeft);
      String secsOutput = String(secsLeft);
      if (minsOutput.length() < 2) {
        minsOutput = "0" + minsOutput;
      }
      if (secsOutput.length() < 2) {
        secsOutput = "0" + secsOutput;
      }
      printMatrix(minsOutput + ":" + secsOutput, -1);
      break;
    }
    case STATE::TIMER_DONE: {
      unsigned long timePassed = millis() - timerDoneStartTime;
      if (timePassed > 3000) {
        program_state = STATE::WAKTU;
      }

      String output = (timePassed / 300) & 1 ? "" : "SUDAH"; // delay blink sudah
      printMatrix(output, -1);
      break;
    }
  }

  if (Serial.available() > 0) {
    adjustClock(Serial.readString());
  }
  delay(60);
}

void keyboardHandler() { // untuk interrupt keyboard
  if (!keyboard.available()) {
    return;    
  }
  char key = keyboard.read(); 
  switch(program_state) {
    case STATE::WAKTU:
    case STATE::SUHU:    
      if (key == PS2_ENTER) {
        program_state = STATE::MENU;
      }
      break;
    case STATE::MENU:
      if (key == PS2_LEFTARROW) {
        switch (menu_state) {
          case M_STATE::JAM:
            menu_state = M_STATE::TIMER;
            break;
          case M_STATE::ALARM:
            menu_state = M_STATE::JAM;
            break;
          case M_STATE::TIMER:
            menu_state = M_STATE::ALARM;
            break;
        }
      } else if (key == PS2_RIGHTARROW) {
        switch (menu_state) {
          case M_STATE::JAM:
            menu_state = M_STATE::ALARM;
            break;
          case M_STATE::ALARM:
            menu_state = M_STATE::TIMER;
            break;
          case M_STATE::TIMER:
            menu_state = M_STATE::JAM;
            break;
        }
      } else if (key == PS2_ESC) {
        menu_state = M_STATE::JAM;
        program_state = STATE::WAKTU;
      } else if (key == PS2_ENTER) {
        if (menu_state == M_STATE::JAM) {
          program_state = STATE::SET_WAKTU;
        } else if (menu_state == M_STATE::ALARM) {
          program_state = STATE::SELECT_ALARM;
        } else {
          program_state = STATE::SET_TIMER;
        }
      }
      break;
    case STATE::SET_WAKTU:
      if (key == PS2_ESC) {
        program_state = STATE::MENU;
      } else if (key >= '0' && key <= '9') {
        switch (inputtedClock) {
          case 0:
            if (key > '2') { // biar jam awal tidak lebih dari 2
              break;
            }// _ _ -> 1 _
            inputClockHours = String(key) + String(inputClockHours[1]);
            ++inputtedClock;
            break;
          case 1:
            if (inputClockHours[0] > '1' && key > '3') {
              break;
            }
            // 1 _ -> 1 9
            inputClockHours = String(inputClockHours[0]) + String(key);
            ++inputtedClock;
            break;
          case 2:
            if (key > '5') {
              break;
            }
            inputClockMinutes = String(key) + String(inputClockMinutes[1]);
            ++inputtedClock;
            break;
          case 3:
            inputClockMinutes = String(inputClockMinutes[0]) + String(key);
            ++inputtedClock;
            break;
          default:
            break;
        }
      } else if (key == PS2_BACKSPACE) {
        // 19 23 -> inputtedClock = 4        
        switch (inputtedClock) {
          case 1:
            inputClockHours = "__";
            --inputtedClock;
            break;
          case 2:
            inputClockHours = String(inputClockHours[0]) + "_";
            --inputtedClock;
            break;
          case 3:
            inputClockMinutes = "__";
            --inputtedClock;
            break;
          case 4:
            inputClockMinutes = String(inputClockMinutes[0]) + "_";
            --inputtedClock;
            break;
          default:
            break;
        }
      } else if (key == PS2_ENTER) {
        if (inputtedClock >= 4) {
          byte sec = rtc.getTime().sec;
          byte hour = inputClockHours.toInt();
          byte min = inputClockMinutes.toInt();
          rtc.setTime(hour, min, sec);
          program_state = STATE::WAKTU;
        }
      }
      break;
    case STATE::SELECT_ALARM:
      if (key == PS2_ESC) {
        program_state = STATE::MENU;
      } else if (key == PS2_LEFTARROW) {
        switch (alarm_state) {
          case A_STATE::A1:
            alarm_state = A_STATE::A5;
            break;
          case A_STATE::A2:
            alarm_state = A_STATE::A1;
            break;
          case A_STATE::A3:
            alarm_state = A_STATE::A2;
            break;
          case A_STATE::A4:
            alarm_state = A_STATE::A3;
            break;
          case A_STATE::A5:
            alarm_state = A_STATE::A4;
            break;           
        }
      } else if (key == PS2_RIGHTARROW) {
        switch (alarm_state) {
          case A_STATE::A1:
            alarm_state = A_STATE::A2;
            break;
          case A_STATE::A2:
            alarm_state = A_STATE::A3;
            break;
          case A_STATE::A3:
            alarm_state = A_STATE::A4;
            break;
          case A_STATE::A4:
            alarm_state = A_STATE::A5;
            break;
          case A_STATE::A5:
            alarm_state = A_STATE::A1;
            break;           
        }
      } else if (key == PS2_ENTER) {
        if (alarm_state == A_STATE::A5) {
          program_state = STATE::SET_ALARM5;
        } else {
          program_state = STATE::SET_ALARM;          
        }
      }
      break;
    case STATE::SET_ALARM5:
      if (key == PS2_ENTER && alarm5Input.length() > 0) {
        program_state = STATE::SET_ALARM;        
      } else if (key == PS2_ESC) {
        program_state = STATE::SELECT_ALARM;
      } else if (key == PS2_BACKSPACE && alarm5Input.length() > 0) {
        alarm5Input.remove(alarm5Input.length() - 1, 1);
      } else {
        alarm5Input += String(key);
      }
      break;
    case STATE::SET_ALARM:
      if (key == PS2_ESC) {
        program_state = STATE::SELECT_ALARM;
        resetAlarmInput();
      } else if (key >= '0' && key <= '9') {
        switch (inputtedAlarm) {
          case 0:
            if (key > '2') {
              break;
            }
            inputAlarmHours = String(key) + String(inputAlarmHours[1]);
            ++inputtedAlarm;
            break;
          case 1:
            if (inputAlarmHours[0] > '1' && key > '3') {
              break;
            }
            inputAlarmHours = String(inputAlarmHours[0]) + String(key);
            ++inputtedAlarm;
            break;
          case 2:
            if (key > '5') {
              break;
            }
            inputAlarmMinutes = String(key) + String(inputAlarmMinutes[1]);
            ++inputtedAlarm;
            break;
          case 3:
            inputAlarmMinutes = String(inputAlarmMinutes[0]) + String(key);
            ++inputtedAlarm;
            break;
          default:
            break;
        }
      } else if (key == PS2_BACKSPACE) {
        switch (inputtedAlarm) {
          case 1:
            inputAlarmHours = "__";
            --inputtedAlarm;
            break;
          case 2:
            inputAlarmHours = String(inputAlarmHours[0]) + "_";
            --inputtedAlarm;
            break;
          case 3:
            inputAlarmMinutes = "__";
            --inputtedAlarm;
            break;
          case 4:
            inputAlarmMinutes = String(inputAlarmMinutes[0]) + "_";
            --inputtedAlarm;
            break;
          default:
            break;
        }
      } else if (key == PS2_ENTER) {
        if (inputtedAlarm >= 4) {
          program_state = STATE::SET_DUR;
        }
      } else if (key == PS2_BACKSPACE) {
        byte index;
        switch (alarm_state) {
          case A_STATE::A1: {
            index = 0;
            break;
          }
          case A_STATE::A2: {
            index = 1;
            break;
          }
          case A_STATE::A3: {
            index = 2;
            break;
          }
          case A_STATE::A4: {
            index = 3;
            break;
          }
          case A_STATE::A5: {
            index = 4;
            break;
          }
        }
        alarms[index].active = false;
      }
      break;
    case STATE::SET_DUR:
      if (key == PS2_ESC) {
        program_state = STATE::SET_ALARM;
      } else if (key == PS2_UPARROW) {
        ++inputAlarmDuration;
      } else if (key == PS2_DOWNARROW) {
        --inputAlarmDuration;
      } else if (key == PS2_ENTER && inputAlarmDuration) {
        if (inputtedAlarm >= 4) {
          program_state = STATE::SET_DUR;
          byte index = 0;
          switch (alarm_state) { // buat tau index mana alarm yg aktif 
            case A_STATE::A1:
              index = 0;
              break;
            case A_STATE::A2:
              index = 1;
              break;
            case A_STATE::A3:
              index = 2;
              break;
            case A_STATE::A4:
              index = 3;
              break;
            case A_STATE::A5:
              index = 4;
              break;
          }
          alarms[index].active = true;
          alarms[index].hours = inputAlarmHours.toInt();
          alarms[index].minutes = inputAlarmMinutes.toInt();
          alarms[index].duration = inputAlarmDuration;
          resetAlarmInput();
          program_state = STATE::WAKTU;
        }
      }
      break;
    case STATE::ALARM_ACTIVE:
      if (key == PS2_ESC) {
        program_state = STATE::WAKTU;
        alarms[activeAlarm].active = false;
      }
      break;   
    case STATE::SET_TIMER:
      if (key == PS2_ESC) {
        program_state = STATE::MENU;
      } else if (key >= '0' && key <= '9') {
        switch (inputtedTimer) {
          case 0:
            inputTimerMinutes = String(key) + String(inputTimerMinutes[1]);
            ++inputtedTimer;
            break;
          case 1:
            inputTimerMinutes = String(inputTimerMinutes[0]) + String(key);
            ++inputtedTimer;
            break;
          case 2:
            if (inputTimerMinutes == String("99") && key > '5') {
              break;
            }
            inputTimerSeconds = String(key) + String(inputTimerSeconds[1]);
            ++inputtedTimer;
            break;
          case 3:
            inputTimerSeconds = String(inputTimerSeconds[0]) + String(key);
            ++inputtedTimer;
            break;
          default:
            break;
        }
      } else if (key == PS2_BACKSPACE) {
        switch (inputtedTimer) {
          case 1:
            inputTimerMinutes = "__";
            --inputtedTimer;
            break;
          case 2:
            inputTimerMinutes = String(inputTimerMinutes[0]) + "_";
            --inputtedTimer;
            break;
          case 3:
            inputTimerSeconds = "__";
            --inputtedTimer;
            break;
          case 4:
            inputTimerSeconds = String(inputTimerSeconds[0]) + "_";
            --inputtedTimer;
            break;
          default:
            break;
        }
      } else if (key == PS2_ENTER) {
        if (inputtedTimer >= 4) {
          program_state = STATE::TIMER_ACTIVE;
          timerStartTime = millis();       
        }
      }
      break;
    case STATE::TIMER_ACTIVE:
      if (key == PS2_ESC) {
        program_state = STATE::WAKTU;
      }
      break;
  }
}