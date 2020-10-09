/***********************************************************************
 * Alarm clock
 * 
 * Uses the following peripherals:
 * 
 * HT16K33 4-digit 7-segment display with I2C interface
 * DS32321 Real-time clock module with I2C interface
 * 1 passive buzzer
 * 3 pushbuttons connected to digital inputs (active LOW)
 * 1 latching switch (active LOW)
 * 
 * Features:
 * - keeps time, even when powered off thanks to battery-backed RTC
 * - 24-hour time display
 * - alarm that plays a tune
 ***********************************************************************/

#include <PlayRtttl.h>
#include <RTClib.h>
#include <HT16K33.h>

// pushbuttons for hour, minute and temperature display
const byte btnHour = 2;
const byte btnMinute = 3;
const byte btnTemp = 4;
// alarm on/off switch
const byte btnAlarm = 5;
// pin connected to the buzzer
const byte pinTone = 6;

RTC_DS3231 rtc;
HT16K33 display(0x70);

// encodings of the digits on the 7-segment display
const uint8_t charmap[] = {
  0x3F,   // 0
  0x06,   // 1
  0x5B,   // 2
  0x4F,   // 3
  0x66,   // 4
  0x6D,   // 5
  0x7D,   // 6
  0x07,   // 7
  0x7F,   // 8
  0x6F,   // 9
};

char buf[32];
int showAlarmTime = -1;
int alarmHour;
int alarmMin;
const char *weekdays[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
char StarWarsInRam[] = "StarWars:d=32,o=5,b=45,l=2,s=N:p,f#,f#,f#,8b.,8f#.6,e6,d#6,c#6,8b.6,16f#.6,e6,d#6,c#6,8b.6,16f#.6,e6,d#6,e6,8c#6";

void setup() {
  Serial.begin(9600);

  display.begin();
  Wire.setClock(100000);
  display.displayClear();
  display.suppressLeadingZeroPlaces(1);
  display.displayOn();

  pinMode(btnHour, INPUT_PULLUP);
  pinMode(btnMinute, INPUT_PULLUP);
  pinMode(btnTemp, INPUT_PULLUP);
  pinMode(btnAlarm, INPUT_PULLUP);
  pinMode(pinTone, OUTPUT);  //buzzer
  pinMode(13, OUTPUT);       //led indicator when singing a note

  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    Serial.flush();
    abort();
  }
  if (rtc.lostPower()) {
    Serial.println("RTC lost power, let's set the time!");
    // When time needs to be set on a new device, or after a power loss, the
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:
    // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
  }

  //we don't need the 32K Pin, so disable it
  rtc.disable32K();
  rtc.clearAlarm(1);
  rtc.clearAlarm(2);

  // stop oscillating signals at SQW Pin
  // otherwise setAlarm1 will fail
  rtc.writeSqwPinMode(DS3231_OFF);

  // turn off alarm 2 (in case it isn't off already)
  // again, this isn't done at reboot, so a previously set alarm could easily go overlooked
  rtc.disableAlarm(2);
}

void loop() {
  DateTime now = rtc.now();
  int hour = now.hour();
  int minute = now.minute();

  Serial.print(" ");
  sprintf(buf, "%s %04d-%02d-%02d %02d:%02d:%02d ",
          weekdays[now.dayOfTheWeek()],
          now.year(), now.month(), now.day(),
          hour, minute, now.second());
  Serial.print(buf);

  // Display the temperature
  int temp = rtc.getTemperature();
  sprintf(buf, "T=%2dC\n", temp);
  Serial.print(buf);

  int isHourBtn = digitalRead(btnHour) == LOW;
  int isMinBtn = digitalRead(btnMinute) == LOW;
  int isAlarmBtn = digitalRead(btnAlarm) == LOW;
  if (isAlarmBtn) {
    if (showAlarmTime == -1) {
      // show alarm time for a while after Alarm is activated
      showAlarmTime = 10;
    }
    if (isHourBtn || isMinBtn) {
      if (isHourBtn) {
        alarmHour = (alarmHour + 1) % 24;
      }
      if (isMinBtn) {
        alarmMin = (alarmMin + 1) % 60;
      }
      setAlarm();
      showAlarmTime = 10;
    }
  } else {
    if (isHourBtn) {
      now = now + TimeSpan(3600);
      rtc.adjust(now);
    }
    if (isMinBtn) {
      now = now + TimeSpan(60);
      rtc.adjust(now);
    }
    showAlarmTime = -1;
  }
  
  if (digitalRead(btnTemp) == LOW) {
    displayTemperature(temp);
  } else if (showAlarmTime > 0) {
    displayTime(alarmHour, alarmMin, false);
    --showAlarmTime;
  } else {
    displayTime(hour, minute, isAlarmBtn);
  }
  
  if (rtc.alarmFired(1)) {
    rtc.clearAlarm(1);
    Serial.println("Alarm 1 fired!");
    if (isAlarmBtn) {
      playRtttlBlocking(pinTone, StarWarsInRam);
    }
  }

  delay(500);
}

void displayTime(int hour, int minute, bool armed) {
  display.suppressLeadingZeroPlaces(0);
  uint8_t x[4];
  x[0] = charmap[hour / 10];
  x[1] = charmap[hour % 10];
  x[2] = charmap[minute / 10];
  x[3] = charmap[minute % 10];
  // if armed, display a dot in the last digit
  if (armed) {
    x[3] |= 0x80;
  }
  display.displayRaw(x);
  display.displayColon(true);
}

void displayTemperature(int temp) {
  uint8_t x[4];
  temp = min(temp, 99);
  x[0] = charmap[temp / 10];
  x[1] = charmap[temp % 10];
  x[2] = 0;
  x[3] = 0x39;  // 'C'
  display.displayColon(false);
  display.displayRaw(x);
}

void setAlarm() {
  if (!rtc.setAlarm1(DateTime(0, 1, 1, alarmHour, alarmMin, 0), DS3231_A1_Hour)) {
    Serial.println("Error setting alarm!");
  }
}
