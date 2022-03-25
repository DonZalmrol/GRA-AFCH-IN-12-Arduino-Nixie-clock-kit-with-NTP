const String FirmwareVersion = "019003";
#define HardwareVersion "NCS314 for HW 2.x"
//Format                _X.XXX_
//Updated and optimized code + bug fixing by Don-Zalmrol
//1.93 21.03.2022
//NIXIE CLOCK SHIELD NCS314 v 2.x by GRA & AFCH (fominalec@gmail.com)
//1.91 15.10.2020
//Added NTP by Don-Zalmrol
//1.90 08.06.2020
//Fixed: GPS timezone issue: added breakTime(now(), tm) to adjustTime function at Time.cpp
//1.89 03.04.2020
//Dots sync with seconds
//1.88 30.03.2020
//GPS synchronization algorithm has been changed (again)
//1.86 23.02.2020
//GPS synchronization algorithm changed
//1.85.3 23.02.2020
//Added: DS3231 internal temperature sensor self test: 5 beeps if fail.
//1.85.2 21.02.2020
//GPS parser has been replaced by NEOGPS
//1.85 24.04.2019
//Fixed: Bug with time zones more than +-9
//1.84 08.04.2019
//LEDs functions moved to external file
//LEDs freezing while music (or sound) played.
//SPI Setup moved driver's file
//1.83 02.08.2018 (Driver v 1.1 is required)
//Fixed: Temp. reading speed fixed
//Fixed: Dots mixed up (driver was updated to v. 1.1)
//Fixed: RGB LEDs reading from EEPROM
//Fixed: Check for entering data from GPS in range
//1.82 18.07.2018 Dual Date Format
//1.81 18.02.2018 Temp. sensor present analyze
//1.80 06.08.2017
//Added: Date and Time GPS synchronization
//1.70 30.07.2017
//Added  IR remote control support (Sony RM-X151) ("MODE", "UP", "DOWN")
//1.60 24_07_2017
//Added: Temperature reading mode in menu and slot machine transaction
//1.0.31 27_04_2017
//Added: antipoisoning effect - slot machine
//1.021 31.01.2017
//Added: time synchronizing each 10 seconds
//Fixed: not correct time reading from RTC while start up
//1.02 17.10.2016
//Fixed: RGB color controls
//Update to Arduino IDE 1.6.12 (Time.h replaced to TimeLib.h)
//1.01
//Added RGB LEDs lock(by UP and Down Buttons)
//Added Down and Up buttons pause and resume self testing
//25.09.2016 update to HW ver 1.1
//25.05.2016

// How many tubes (excluding neons) do you got?
//#define tubes8
#define tubes6
//#define tubes4

// Serial baud explained
// Serial  = Default Serial output (e.g. COM1)
// Serial1 = GPS connection baud rate
// Serial2 = Not Used
// Serial3 = ESP8266 WIFI

// ************
// Includes
// ************
#include <SPI.h>
#include <Wire.h>
#include <ClickButton.h>
#include <TimeLib.h>
#include <Tone.h>
#include <EEPROM.h>
#include <OneWire.h>
#include <WiFiEsp.h>
#include <WiFiEspUdp.h>
#include <Timezone.h>
#include "doIndication314_HW2.x.h"
#include "arduino_secrets.h"

// ************
// NTP Settings
// ************
char ssid[] = SECRET_SSID;
char pass[] = SECRET_PASS;

unsigned long previousMillis_1 = 0, previousMillis_2 = 0;

char timeServer[] = TIMESERVER;     // NTP server
unsigned int localPort = LOCALPORT; // local port to listen for UDP packets
const int NTP_PACKET_SIZE = 48;     // NTP timestamp is in the first 48 bytes of the message
bool NTP_Package_Valid = false;     // NTP Package valid bool
const int UDP_TIMEOUT = 2000;       // timeout in miliseconds to wait for an UDP packet to arrive, original value = 3000
byte packetBuffer[NTP_PACKET_SIZE]; // buffer to hold incoming and outgoing packets

// A UDP instance to let us send and receive packets over UDP
WiFiEspUDP Udp;

// Set to false to disable NTP and enable GPS
bool NTPSyncEnabled = true;

// Enable Daylight Savings Time (DST)
bool DSTEnabled = true;

// NTP time is in UTC. Set offset in seconds
int GMTOffSet = GMTOFFSET;
//int GMTOffSet = 1;
long timeOffset = (GMTOffSet * 60UL * 60UL); // Example: (1 * 60 * 60) -> GMT +1

// NTP retry counter
int NTPRetryCount = 0;
bool NTPRetryFailed = false;
bool initialBootDone = false;

// ************
// GPS Settings
// ************
#define GPS_SYNC_INTERVAL 1800000 // 30 minutes in milliseconds
//#define GPS_SYNC_INTERVAL 60000 // 1 minute in milliseconds
//#define GPS_SYNC_INTERVAL 120000 // 2 minute in milliseconds
unsigned long Last_Time_GPS_Sync = 0;
bool GPS_Sync_Flag = 1;
bool gpsConnect = false;
//uint32_t GPS_Sync_Interval=120000; // 2 minutes
uint32_t GPS_Sync_Interval = 60000; // 1 minutes
#include <NMEAGPS.h>
static NMEAGPS  gps;
static gps_fix  fix;

int ModeButtonState = 0;
int UpButtonState = 0;
int DownButtonState = 0;

boolean UD, LD; // DOTS control;

byte data[12];
byte addr[8];
int celsius, fahrenheit;

const byte RedLedPin = 9; //MCU WDM output for red LEDs 9-g
const byte GreenLedPin = 6; //MCU WDM output for green LEDs 6-b
const byte BlueLedPin = 3; //MCU WDM output for blue LEDs 3-r
const byte pinSet = A0;
const byte pinUp = A2;
const byte pinDown = A1;
const byte pinBuzzer = 2;
//const byte pinBuzzer = 0;
const byte pinUpperDots = 12; //HIGH value light a dots
const byte pinLowerDots = 8; //HIGH value light a dots
const byte pinTemp = 7;
bool RTC_present;
#define US_DateFormat 1
#define EU_DateFormat 0
//bool DateFormat=EU_DateFormat;

OneWire ds(pinTemp);
bool TempPresent = false;
#define CELSIUS 0
#define FAHRENHEIT 1

String stringToDisplay = "000000"; // Conten of this string will be displayed on tubes (must be 6 chars length)
int menuPosition = 0;
// 0 - time
// 1 - date
// 2 - alarm
// 3 - 12/24 hours mode
// 4 - Temperature

byte blinkMask = B00000000; //bit mask for blinkin digits (1 - blink, 0 - constant light)
int blankMask = B00000000; //bit mask for digits (1 - off, 0 - on)

byte dotPattern = B00000000; //bit mask for separeting dots (1 - on, 0 - off)
//B10000000 - upper dots
//B01000000 - lower dots

#define DS1307_ADDRESS 0x68
byte zero = 0x00; //workaround for issue #527
int RTC_hours, RTC_minutes, RTC_seconds, RTC_day, RTC_month, RTC_year, RTC_day_of_week;

#define TimeIndex        0
#define DateIndex        1
#define AlarmIndex       2
#define hModeIndex       3
#define TemperatureIndex 4
#define TimeZoneIndex    5
#define TimeHoursIndex   6
#define TimeMintuesIndex 7
#define TimeSecondsIndex 8
#define DateFormatIndex  9
#define DateDayIndex     10
#define DateMonthIndex   11
#define DateYearIndex    12
#define AlarmHourIndex   13
#define AlarmMinuteIndex 14
#define AlarmSecondIndex 15
#define Alarm01          16
#define hModeValueIndex  17
#define DegreesFormatIndex 18
#define HoursOffsetIndex 19

#define FirstParent      TimeIndex
#define LastParent       TimeZoneIndex
#define SettingsCount    (HoursOffsetIndex+1)
#define NoParent         0
#define NoChild          0

//-------------------------------0--------1--------2-------3--------4--------5--------6--------7--------8--------9----------10-------11---------12---------13-------14-------15---------16---------17--------18----------19
//                     names:  Time,   Date,   Alarm,   12/24, Temperature,TimeZone,hours,   mintues, seconds, DateFormat, day,    month,   year,      hour,   minute,   second alarm01  hour_format Deg.FormIndex HoursOffset
//                               1        1        1       1        1        1        1        1        1        1          1        1          1          1        1        1        1            1         1        1
int parent[SettingsCount] = {NoParent, NoParent, NoParent, NoParent, NoParent, NoParent, 1,       1,       1,       2,         2,       2,         2,         3,       3,       3,       3,       4,           5,        6};
int firstChild[SettingsCount] = {6,       9,       13,     17,      18,      19,      0,       0,       0,    NoChild,      0,       0,         0,         0,       0,       0,       0,       0,           0,        0};
int lastChild[SettingsCount] = { 8,      12,       16,     17,      18,      19,      0,       0,       0,    NoChild,      0,       0,         0,         0,       0,       0,       0,       0,           0,        0};
int value[SettingsCount] = {     0,       0,       0,      0,       0,       0,       0,       0,       0,  EU_DateFormat,  0,       0,         0,         0,       0,       0,       0,       24,          0,        2};
int maxValue[SettingsCount] = {  0,       0,       0,      0,       0,       0,       23,      59,      59, US_DateFormat,  31,      12,        99,       23,      59,      59,       1,       24,     FAHRENHEIT,    14};
int minValue[SettingsCount] = {  0,       0,       0,      12,      0,       0,       00,      00,      00, EU_DateFormat,  1,       1,         00,       00,      00,      00,       0,       12,      CELSIUS,     -12};
int blinkPattern[SettingsCount] = {
  B00000000, //0
  B00000000, //1
  B00000000, //2
  B00000000, //3
  B00000000, //4
  B00000000, //5
  B00000011, //6
  B00001100, //7
  B00110000, //8
  B00111111, //9
  B00000011, //10
  B00001100, //11
  B00110000, //12
  B00000011, //13
  B00001100, //14
  B00110000, //15
  B00001100, //17
  B00111111, //18
  B00000011, //19
};

bool editMode = false;

long downTime = 0;
long upTime = 0;
const long settingDelay = 150;
bool BlinkUp = false;
bool BlinkDown = false;
unsigned long enteringEditModeTime = 0;
bool RGBLedsOn = true;
byte RGBLEDsEEPROMAddress = 0;
byte HourFormatEEPROMAddress = 1;
byte AlarmTimeEEPROMAddress = 2; //3,4,5
byte AlarmArmedEEPROMAddress = 6;
byte LEDsLockEEPROMAddress = 7;
byte LEDsRedValueEEPROMAddress = 8;
byte LEDsGreenValueEEPROMAddress = 9;
byte LEDsBlueValueEEPROMAddress = 10;
byte DegreesFormatEEPROMAddress = 11;
byte HoursOffsetEEPROMAddress = 12;
byte DateFormatEEPROMAddress = 13;

//buttons pins declarations
ClickButton setButton(pinSet, LOW, CLICKBTN_PULLUP);
ClickButton upButton(pinUp, LOW, CLICKBTN_PULLUP);
ClickButton downButton(pinDown, LOW, CLICKBTN_PULLUP);
///////////////////

Tone tone1;
#define isdigit(n) (n >= '0' && n <= '9')
//char *song = "MissionImp:d=16,o=6,b=95:32d,32d#,32d,32d#,32d,32d#,32d,32d#,32d,32d,32d#,32e,32f,32f#,32g,g,8p,g,8p,a#,p,c7,p,g,8p,g,8p,f,p,f#,p,g,8p,g,8p,a#,p,c7,p,g,8p,g,8p,f,p,f#,p,a#,g,2d,32p,a#,g,2c#,32p,a#,g,2c,a#5,8c,2p,32p,a#5,g5,2f#,32p,a#5,g5,2f,32p,a#5,g5,2e,d#,8d";
//char *song = "PinkPanther:d=4,o=5,b=160:8d#,8e,2p,8f#,8g,2p,8d#,8e,16p,8f#,8g,16p,8c6,8b,16p,8d#,8e,16p,8b,2a#,2p,16a,16g,16e,16d,2e";
//char *song="VanessaMae:d=4,o=6,b=70:32c7,32b,16c7,32g,32p,32g,32p,32d#,32p,32d#,32p,32c,32p,32c,32p,32c7,32b,16c7,32g#,32p,32g#,32p,32f,32p,16f,32c,32p,32c,32p,32c7,32b,16c7,32g,32p,32g,32p,32d#,32p,32d#,32p,32c,32p,32c,32p,32g,32f,32d#,32d,32c,32d,32d#,32c,32d#,32f,16g,8p,16d7,32c7,32d7,32a#,32d7,32a,32d7,32g,32d7,32d7,32p,32d7,32p,32d7,32p,16d7,32c7,32d7,32a#,32d7,32a,32d7,32g,32d7,32d7,32p,32d7,32p,32d7,32p,32g,32f,32d#,32d,32c,32d,32d#,32c,32d#,32f,16c";
char *song = "DasBoot:d=4,o=5,b=100:d#.4,8d4,8c4,8d4,8d#4,8g4,a#.4,8a4,8g4,8a4,8a#4,8d,2f.,p,f.4,8e4,8d4,8e4,8f4,8a4,c.,8b4,8a4,8b4,8c,8e,2g.,2p";
//char *song="Scatman:d=4,o=5,b=200:8b,16b,32p,8b,16b,32p,8b,2d6,16p,16c#.6,16p.,8d6,16p,16c#6,8b,16p,8f#,2p.,16c#6,8p,16d.6,16p.,16c#6,16b,8p,8f#,2p,32p,2d6,16p,16c#6,8p,16d.6,16p.,16c#6,16a.,16p.,8e,2p.,16c#6,8p,16d.6,16p.,16c#6,16b,8p,8b,16b,32p,8b,16b,32p,8b,2d6,16p,16c#.6,16p.,8d6,16p,16c#6,8b,16p,8f#,2p.,16c#6,8p,16d.6,16p.,16c#6,16b,8p,8f#,2p,32p,2d6,16p,16c#6,8p,16d.6,16p.,16c#6,16a.,16p.,8e,2p.,16c#6,8p,16d.6,16p.,16c#6,16a,8p,8e,2p,32p,16f#.6,16p.,16b.,16p.";
//char *song="Popcorn:d=4,o=5,b=160:8c6,8a#,8c6,8g,8d#,8g,c,8c6,8a#,8c6,8g,8d#,8g,c,8c6,8d6,8d#6,16c6,8d#6,16c6,8d#6,8d6,16a#,8d6,16a#,8d6,8c6,8a#,8g,8a#,c6";
//char *song="WeWishYou:d=4,o=5,b=200:d,g,8g,8a,8g,8f#,e,e,e,a,8a,8b,8a,8g,f#,d,d,b,8b,8c6,8b,8a,g,e,d,e,a,f#,2g,d,g,8g,8a,8g,8f#,e,e,e,a,8a,8b,8a,8g,f#,d,d,b,8b,8c6,8b,8a,g,e,d,e,a,f#,1g,d,g,g,g,2f#,f#,g,f#,e,2d,a,b,8a,8a,8g,8g,d6,d,d,e,a,f#,2g";
#define OCTAVE_OFFSET 0
char *p;

int notes[] = { 0,
                NOTE_C4, NOTE_CS4, NOTE_D4, NOTE_DS4, NOTE_E4, NOTE_F4, NOTE_FS4, NOTE_G4, NOTE_GS4, NOTE_A4, NOTE_AS4, NOTE_B4,
                NOTE_C5, NOTE_CS5, NOTE_D5, NOTE_DS5, NOTE_E5, NOTE_F5, NOTE_FS5, NOTE_G5, NOTE_GS5, NOTE_A5, NOTE_AS5, NOTE_B5,
                NOTE_C6, NOTE_CS6, NOTE_D6, NOTE_DS6, NOTE_E6, NOTE_F6, NOTE_FS6, NOTE_G6, NOTE_GS6, NOTE_A6, NOTE_AS6, NOTE_B6,
                NOTE_C7, NOTE_CS7, NOTE_D7, NOTE_DS7, NOTE_E7, NOTE_F7, NOTE_FS7, NOTE_G7, NOTE_GS7, NOTE_A7, NOTE_AS7, NOTE_B7
              };

int fireforks[] = {0, 0, 1, //1
                   -1, 0, 0, //2
                   0, 1, 0, //3
                   0, 0, -1, //4
                   1, 0, 0, //5
                   0, -1, 0
                  }; //array with RGB rules (0 - do nothing, -1 - decrese, +1 - increse

void setRTCDateTime(byte h, byte m, byte s, byte d, byte mon, byte y, byte w = 1);

int functionDownButton = 0;
int functionUpButton = 0;
bool LEDsLock = false;

//antipoisoning transaction
bool modeChangedByUser = false;
bool transactionInProgress = false; //antipoisoning transaction
#define timeModePeriod 60000
#define dateModePeriod 5000
long modesChangePeriod = timeModePeriod;
//end of antipoisoning transaction

bool GPS_sync_flag = false;

extern const int LEDsDelay;

// ************
// Setup
// ************
void setup()
{
  Serial.begin(115200);
  Serial1.begin(9600);
  
  Wire.begin();

  // Fix for Brussels/Europe time.
  if (GMTOffSet == 1)
  {
    GMTOffSet = 0;
  }

  if (NTPSyncEnabled)
  {
    WiFiSetup();
  }

  if (EEPROM.read(HourFormatEEPROMAddress) != 12) value[hModeValueIndex] = 24; else value[hModeValueIndex] = 12;
  if (EEPROM.read(RGBLEDsEEPROMAddress) != 0) RGBLedsOn = true; else RGBLedsOn = false;
  if (EEPROM.read(AlarmTimeEEPROMAddress) == 255) value[AlarmHourIndex] = 0; else value[AlarmHourIndex] = EEPROM.read(AlarmTimeEEPROMAddress);
  if (EEPROM.read(AlarmTimeEEPROMAddress + 1) == 255) value[AlarmMinuteIndex] = 0; else value[AlarmMinuteIndex] = EEPROM.read(AlarmTimeEEPROMAddress + 1);
  if (EEPROM.read(AlarmTimeEEPROMAddress + 2) == 255) value[AlarmSecondIndex] = 0; else value[AlarmSecondIndex] = EEPROM.read(AlarmTimeEEPROMAddress + 2);
  if (EEPROM.read(AlarmArmedEEPROMAddress) == 255) value[Alarm01] = 0; else value[Alarm01] = EEPROM.read(AlarmArmedEEPROMAddress);
  if (EEPROM.read(LEDsLockEEPROMAddress) == 255) LEDsLock = false; else LEDsLock = EEPROM.read(LEDsLockEEPROMAddress);
  if (EEPROM.read(DegreesFormatEEPROMAddress) == 255) value[DegreesFormatIndex] = CELSIUS; else value[DegreesFormatIndex] = EEPROM.read(DegreesFormatEEPROMAddress);
  if (EEPROM.read(HoursOffsetEEPROMAddress) == 255) value[HoursOffsetIndex] = value[HoursOffsetIndex]; else value[HoursOffsetIndex] = EEPROM.read(HoursOffsetEEPROMAddress) + minValue[HoursOffsetIndex];
  if (EEPROM.read(DateFormatEEPROMAddress) == 255) value[DateFormatIndex] = value[DateFormatIndex]; else value[DateFormatIndex] = EEPROM.read(DateFormatEEPROMAddress);

  Serial.print(F("led lock = "));
  Serial.println(LEDsLock);

  pinMode(RedLedPin, OUTPUT);
  pinMode(GreenLedPin, OUTPUT);
  pinMode(BlueLedPin, OUTPUT);

  tone1.begin(pinBuzzer);
  song = parseSong(song);

  pinMode(LEpin, OUTPUT);

  // SPI setup
  SPISetup();

  // LED setup
  LEDsSetup();
  
  // Buttons pins initialization 
  pinMode(pinSet,  INPUT_PULLUP);
  pinMode(pinUp,  INPUT_PULLUP);
  pinMode(pinDown,  INPUT_PULLUP);

  // Buzzer pin initialization
  pinMode(pinBuzzer, OUTPUT);

  // buttons objects inits
  setButton.debounceTime   = 20UL;    // Debounce timer in ms
  setButton.multiclickTime = 30UL;    // Time limit for multi clicks
  setButton.longClickTime  = 2000UL;  // time until "held-down clicks" register

  upButton.debounceTime   = 20UL;     // Debounce timer in ms
  upButton.multiclickTime = 30UL;     // Time limit for multi clicks
  upButton.longClickTime  = 2000UL;   // time until "held-down clicks" register

  downButton.debounceTime   = 20UL;   // Debounce timer in ms
  downButton.multiclickTime = 30UL;   // Time limit for multi clicks
  downButton.longClickTime  = 2000UL; // time until "held-down clicks" register

  // Run first boot steps.
  //!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
  doTest();
  //!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

  if (LEDsLock == 1)
  {
    setLEDsFromEEPROM();
  }

  getRTCTime();
  byte prevSeconds = RTC_seconds;
  unsigned long RTC_ReadingStartTime = millis();
  RTC_present = true;
  while (prevSeconds == RTC_seconds)
  {
    getRTCTime();
    //Serial.println(RTC_seconds);
    if ((millis() - RTC_ReadingStartTime) > 3000)
    {
      Serial.println(F("Warning! RTC NOT RESPONDING!"));
      RTC_present = false;
      break;
    }
  }
  
  // Get clock time that was fetched from the RTC clock
  Serial.println((String)"Fetched stored RTC timedate = " + RTC_hours + ":" + RTC_minutes + ":" + RTC_seconds + "@" + RTC_day + "-" + RTC_month + "-" + RTC_year);
  
  // Set clock time that was fetched from the RTC clock
  setTime(RTC_hours, RTC_minutes, RTC_seconds, RTC_day, RTC_month, RTC_year);
}

int rotator = 0; //index in array with RGB "rules" (increse by one on each 255 cycles)
int cycle = 0; //cycles counter
int RedLight = 255;
int GreenLight = 0;
int BlueLight = 0;
unsigned long prevTime = 0; // time of lase tube was lit
unsigned long prevTime4FireWorks = 0; //time of last RGB changed

/***************************************************************************************************************
  MAIN Programm
***************************************************************************************************************/
void loop()
{
  // synchronize RTC with nixies(clock) every 60 seconds (60UL x 1000UL = 60 seconds)
  if(RTC_present)
  {
    if ((millis() - previousMillis_1) >= (60UL * 1000UL))
    {
      // Reset previousMillis to current millis
      previousMillis_1 = millis();
      
      getRTCTime();
      setTime(RTC_hours, RTC_minutes, RTC_seconds, RTC_day, RTC_month, RTC_year);
      Serial.println(F("Synced current time from RTC"));
    }
  }

  // If NTPSyncEnabled is enabled (true) and no GPS was detected
  if (NTPSyncEnabled == true && !NTPRetryFailed)
  {
    // Update NTP upon boot
    if (!initialBootDone)
    {
      initialBootDone = true;
      
      Serial.println(F("\n"));
      Serial.println(F("Attempting to sync with NTP after boot"));
      setNTPTime();
    }
    
    //synchronize with NTP every 5 minutes (300UL * 1000UL)
    else if ((millis() - previousMillis_2) >= (300UL * 1000UL))
    {
      // Reset previousMillis to current millis
      previousMillis_2 = millis();

      Serial.println(F("\n"));
      Serial.println(F("Attempting to sync with NTP"));
      setNTPTime();
    }
  }
  else if (NTPRetryFailed)
  {
    if (gps.available(Serial1))
    {
      fix = gps.read();
      gpsConnect = true;
    }
    else
    {
      fix.valid.time = false;
      gpsConnect = false;
    }
    
    if ((((millis()) - Last_Time_GPS_Sync) > GPS_Sync_Interval) && gpsConnect)
    {
      GPS_Sync_Interval = GPS_SYNC_INTERVAL;
      GPS_Sync_Flag = 0;
      Serial.println(F("Attempting to sync with GPS"));
    }
    
    if (GPS_Sync_Flag == 0)
    {
      Serial.println(F("Checking GPS datetime validity"));
      GPSCheckValidity();
    }
  }

  else
  {
    Serial.println(F("No time sync (NTP or GPS) options available."));
  }
  
  p = playmusic(p);

  if ((millis() - prevTime4FireWorks) > LEDsDelay)
  {
    rotateFireWorks(); //change color (by 1 step)
    prevTime4FireWorks = millis();
  }

  if ((menuPosition == TimeIndex) || (modeChangedByUser == false))
  {
    modesChanger();
  }
  doIndication();

  setButton.Update();
  upButton.Update();
  downButton.Update();
  if (editMode == false)
  {
    blinkMask = B00000000;

  }
  else if ((millis() - enteringEditModeTime) > 60000UL)
  {
    editMode = false;
    menuPosition = firstChild[menuPosition];
    blinkMask = blinkPattern[menuPosition];
  }
  if ((setButton.clicks > 0) || (ModeButtonState == 1)) //short click
  {
    modeChangedByUser = true;
    p = 0; //shut off music )))
    tone1.play(1000, 100);
    enteringEditModeTime = millis();
    /*if (value[DateFormatIndex] == US_DateFormat)
      {
      //if (menuPosition == )
      } else */
    menuPosition = menuPosition + 1;

    if (menuPosition == LastParent + 1)
    {
      menuPosition = TimeIndex;
    }
    Serial.print(F("menuPosition="));
    Serial.println(menuPosition);
    Serial.print(F("value="));
    Serial.println(value[menuPosition]);

    blinkMask = blinkPattern[menuPosition];
    if ((parent[menuPosition - 1] != 0) and (lastChild[parent[menuPosition - 1] - 1] == (menuPosition - 1))) //exit from edit mode
    {
      if ((parent[menuPosition - 1] - 1 == 1) && (!isValidDate()))
      {
        menuPosition = DateDayIndex;
        return;
      }
      editMode = false;
      menuPosition = parent[menuPosition - 1] - 1;
      if (menuPosition == TimeIndex)
      {
        setTime(value[TimeHoursIndex], value[TimeMintuesIndex], value[TimeSecondsIndex], day(), month(), year());
      }
      else if (menuPosition == DateIndex)
      {
        Serial.print("Day:");
        Serial.println(value[DateDayIndex]);
        Serial.print("Month:");
        Serial.println(value[DateMonthIndex]);
        setTime(hour(), minute(), second(), value[DateDayIndex], value[DateMonthIndex], 2000 + value[DateYearIndex]);
        EEPROM.write(DateFormatEEPROMAddress, value[DateFormatIndex]);
      }
      else if (menuPosition == AlarmIndex)
      {
        EEPROM.write(AlarmTimeEEPROMAddress, value[AlarmHourIndex]);
        EEPROM.write(AlarmTimeEEPROMAddress + 1, value[AlarmMinuteIndex]);
        EEPROM.write(AlarmTimeEEPROMAddress + 2, value[AlarmSecondIndex]);
        EEPROM.write(AlarmArmedEEPROMAddress, value[Alarm01]);
      }
      else if (menuPosition == hModeIndex)
      {
        EEPROM.write(HourFormatEEPROMAddress, value[hModeValueIndex]);
      }
      else if (menuPosition == TemperatureIndex)
      {
        EEPROM.write(DegreesFormatEEPROMAddress, value[DegreesFormatIndex]);
      }
      else if (menuPosition == TimeZoneIndex)
      {
        EEPROM.write(HoursOffsetEEPROMAddress, value[HoursOffsetIndex] - minValue[HoursOffsetIndex]);
      }
      
      // Set time
      setRTCDateTime(hour(), minute(), second(), day(), month(), year() % 1000, 1);
      
      // End and exit from edit mode
      return;
    }
    
    Serial.print((String)"menu position = " + menuPosition);
    Serial.print((String)"DateFormat = " + value[DateFormatIndex]);

    if ((menuPosition != HoursOffsetIndex) && (menuPosition != DateFormatIndex) && (menuPosition != DateDayIndex))
    {
      value[menuPosition] = extractDigits(blinkMask);
    }
  }
  if ((setButton.clicks < 0) || (ModeButtonState == -1)) //long click
  {
    // Beep every time a button is pressed
    tone1.play(1000, 100);
    
    if (!editMode)
    {
      enteringEditModeTime = millis();
      if (menuPosition == TimeIndex)
      {
        stringToDisplay = PreZero(hour()) + PreZero(minute()) + PreZero(second()); //temporary enabled 24 hour format while settings
      }
    }
    if (menuPosition == DateIndex)
    {
      Serial.println(F("DateEdit"));
      value[DateDayIndex] =  day();
      value[DateMonthIndex] = month();
      value[DateYearIndex] = year() % 1000;
      if (value[DateFormatIndex] == EU_DateFormat)
      {
        stringToDisplay = PreZero(value[DateDayIndex]) + PreZero(value[DateMonthIndex]) + PreZero(value[DateYearIndex]);
      }
      else
      {
        stringToDisplay = PreZero(value[DateMonthIndex]) + PreZero(value[DateDayIndex]) + PreZero(value[DateYearIndex]);
      }
      Serial.println((String)"String to display = " + stringToDisplay);
    }
    menuPosition = firstChild[menuPosition];
    if (menuPosition == AlarmHourIndex)
    {
      value[Alarm01] = 1; /*digitalWrite(pinUpperDots, HIGH);*/dotPattern = B10000000;
    }
    editMode = !editMode;
    blinkMask = blinkPattern[menuPosition];
    if ((menuPosition != DegreesFormatIndex) && (menuPosition != HoursOffsetIndex) && (menuPosition != DateFormatIndex))
    {
      value[menuPosition] = extractDigits(blinkMask);
    }
    Serial.println((String)"Menu Position = " + menuPosition);
    Serial.println((String)"Value = " + value[menuPosition]);
  }

  if (upButton.clicks != 0)
  {
    functionUpButton = upButton.clicks;
  }

  if ((upButton.clicks > 0) || (UpButtonState == 1))
  {
    modeChangedByUser = true;
    p = 0; //shut off music )))
    tone1.play(1000, 100);
    incrementValue();
    if (!editMode)
    {
      LEDsLock = false;
      EEPROM.write(LEDsLockEEPROMAddress, 0);
    }
  }

  if (functionUpButton == -1 && upButton.depressed == true)
  {
    BlinkUp = false;
    if (editMode == true)
    {
      if ( (millis() - upTime) > settingDelay)
      {
        upTime = millis();// + settingDelay;
        incrementValue();
      }
    }
  }
  else
  {
    BlinkUp = true;
  }

  if (downButton.clicks != 0)
  {
    functionDownButton = downButton.clicks;
  }

  if ((downButton.clicks > 0) || (DownButtonState == 1))
  {
    modeChangedByUser = true;
    p = 0; //shut off music )))
    tone1.play(1000, 100);
    dicrementValue();
    if (!editMode)
    {
      LEDsLock = true;
      EEPROM.write(LEDsLockEEPROMAddress, 1);
      EEPROM.write(LEDsRedValueEEPROMAddress, RedLight);
      EEPROM.write(LEDsGreenValueEEPROMAddress, GreenLight);
      EEPROM.write(LEDsBlueValueEEPROMAddress, BlueLight);
      Serial.println(F("Store to EEPROM:"));
      Serial.print(F("RED="));
      Serial.println(RedLight);
      Serial.print(F("GREEN="));
      Serial.println(GreenLight);
      Serial.print(F("Blue="));
      Serial.println(BlueLight);
    }
  }

  if (functionDownButton == -1 && downButton.depressed == true)
  {
    BlinkDown = false;
    if (editMode == true)
    {
      if ( (millis() - downTime) > settingDelay)
      {
        downTime = millis();// + settingDelay;
        dicrementValue();
      }
    }
  }
  else
  {
    BlinkDown = true;
  }

  if (!editMode)
  {
    if ((upButton.clicks < 0) || (UpButtonState == -1))
    {
      tone1.play(1000, 100);
      RGBLedsOn = true;
      EEPROM.write(RGBLEDsEEPROMAddress, 1);
      Serial.println(F("RGB=on"));
      setLEDsFromEEPROM();
    }
    if ((downButton.clicks < 0) || (DownButtonState == -1))
    {
      tone1.play(1000, 100);
      RGBLedsOn = false;
      EEPROM.write(RGBLEDsEEPROMAddress, 0);
      Serial.println(F("RGB=off"));
    }
  }

  static bool updateDateTime = false;
  switch (menuPosition)
  {
    case TimeIndex: //time mode
      if (!transactionInProgress) stringToDisplay = updateDisplayString();
      doDotBlink();
      checkAlarmTime();
      blankMask = B00000000;
      break;
      
    case DateIndex: //date mode
      if (!transactionInProgress) stringToDisplay = updateDateString();
      dotPattern = B01000000; //turn on lower dots
      checkAlarmTime();
      blankMask = B00000000;
      break;
      
    case AlarmIndex: //alarm mode
      stringToDisplay = PreZero(value[AlarmHourIndex]) + PreZero(value[AlarmMinuteIndex]) + PreZero(value[AlarmSecondIndex]);
      blankMask = B00000000;
      if (value[Alarm01] == 1)
      {
        /*digitalWrite(pinUpperDots, HIGH);*/ 
        dotPattern = B10000000; //turn on upper dots
      }
      else
      {
        /*digitalWrite(pinUpperDots, LOW);
          digitalWrite(pinLowerDots, LOW);*/
        dotPattern = B00000000; //turn off upper dots
      }
      checkAlarmTime();
      break;
      
    case hModeIndex: //12/24 hours mode
      stringToDisplay = "00" + String(value[hModeValueIndex]) + "00";
      blankMask = B00110011;
      dotPattern = B00000000; //turn off all dots
      checkAlarmTime();
      break;
      
    case TemperatureIndex: //missed break
    
    case DegreesFormatIndex:
      if (!transactionInProgress)
      {
        stringToDisplay = updateTemperatureString(getTemperature(value[DegreesFormatIndex]));
        if (value[DegreesFormatIndex] == CELSIUS)
        {
          blankMask = B00110001;
          dotPattern = B01000000;
        }
        else
        {
          blankMask = B00100011;
          dotPattern = B00000000;
        }
      }

      if (getTemperature(value[DegreesFormatIndex]) < 0)
      {
        dotPattern |= B10000000;
      }
      else
      {
        dotPattern &= B01111111;
      }
      break;
      
    case TimeZoneIndex: //missed break
    
    case HoursOffsetIndex:
      stringToDisplay = String(PreZero(value[HoursOffsetIndex])) + "0000";
      blankMask = B00001111;
      if (value[HoursOffsetIndex] >= 0)
      {
        dotPattern = B00000000; //turn off all dots
      }
      else
      {
        dotPattern = B10000000; //turn on upper dots
      }
      break;
      
    case DateFormatIndex:
      // If EU_DateFormat
      if (value[DateFormatIndex] == EU_DateFormat)
      {
        stringToDisplay = "311299";
        blinkPattern[DateDayIndex] = B00000011;
        blinkPattern[DateMonthIndex] = B00001100;
      }
      // Else US_DateFormat
      else
      {
        stringToDisplay = "123199";
        blinkPattern[DateDayIndex] = B00001100;
        blinkPattern[DateMonthIndex] = B00000011;
      }
      break;
      
    case DateDayIndex: //missed break
    
    case DateMonthIndex: //missed break
    
    case DateYearIndex:
      if (value[DateFormatIndex] == EU_DateFormat)
      {
        stringToDisplay = PreZero(value[DateDayIndex]) + PreZero(value[DateMonthIndex]) + PreZero(value[DateYearIndex]);
      }
      else
      {
        stringToDisplay = PreZero(value[DateMonthIndex]) + PreZero(value[DateDayIndex]) + PreZero(value[DateYearIndex]);
      }
      break;
  }
}

String PreZero(int digit)
{
  digit = abs(digit);
  if (digit < 10)
  {
    return String("0") + String(digit);
  }
  else
  {
    return String(digit);
  }
}

String updateDisplayString()
{
  static int prevS = -1;

  if (second() != prevS)
  {
    prevS = second();
    return getTimeNow();
  }
  else
  {
    return stringToDisplay;
  }
}

String getTimeNow()
{
  if (value[hModeValueIndex] == 24)
  {
    return PreZero(hour()) + PreZero(minute()) + PreZero(second());
  }
  else
  {
    return PreZero(hourFormat12()) + PreZero(minute()) + PreZero(second());
  }
}

void doTest()
{
  Serial.print(F("Firmware version: "));
  Serial.println(FirmwareVersion.substring(1, 2) + "." + FirmwareVersion.substring(2, 5));
  Serial.print(F("Hardware version: "));
  Serial.println(HardwareVersion);
  Serial.println(F("Start Test"));

  // comment below to stop music during testing...
  Serial.println(F("Music Test"));
  p = song;
  parseSong(p);

  Serial.println(F("LED Test"));
  LEDsTest();

  Serial.println(F("Nixies Test"));
#ifdef tubes8
  String testStringArray[11] = {"00000000", "11111111", "22222222", "33333333", "44444444", "55555555", "66666666", "77777777", "88888888", "99999999", ""};
  testStringArray[10] = FirmwareVersion + "00";
#endif
#ifdef tubes6
  String testStringArray[11] = {"000000", "111111", "222222", "333333", "444444", "555555", "666666", "777777", "888888", "999999", ""};
  testStringArray[10] = FirmwareVersion;
#endif

  int dlay = 500;
  bool test = 1;
  byte strIndex = -1;
  unsigned long startOfTest = millis() + 1000; //disable delaying in first iteration
  bool digitsLock = false;
  while (test)
  {
    if (digitalRead(pinDown) == 0)
    {
      digitsLock = true;
    }
    if (digitalRead(pinUp) == 0)
    {
      digitsLock = false;
    }

    if ((millis() - startOfTest) > dlay)
    {
      startOfTest = millis();
      if (!digitsLock)
      {
        strIndex = strIndex + 1;
      }
      if (strIndex == 10)
      {
        dlay = 2000;
      }
      if (strIndex > 10)
      {
        test = false;
        strIndex = 10;
      }

      stringToDisplay = testStringArray[strIndex];
      Serial.println(stringToDisplay);
      doIndication();
    }
    delayMicroseconds(2000);
  };

  Serial.println(F("Temp sensor Test"));
  if (!ds.search(addr))
  {
    Serial.println(F("Temp. sensor not found."));
    TempPresent = false;
  }
  else
  {
    TempPresent = true;
    testDS3231TempSensor();
  }

  Serial.println(F("Stop Test"));
  // while(1);
}

void doDotBlink()
{
  //dotPattern = B11000000; return; //always on
  //dotPattern = B00000000; return; //always off
  if (second() % 2 == 0)
  {
    dotPattern = B11000000;
  }
  else
  {
    dotPattern = B00000000;
  }
}

void setRTCDateTime(byte h, byte m, byte s, byte d, byte mon, byte y, byte w)
{
  Wire.beginTransmission(DS1307_ADDRESS);
  Wire.write(zero); //stop Oscillator

  Wire.write(decToBcd(s));
  Wire.write(decToBcd(m));
  Wire.write(decToBcd(h));
  Wire.write(decToBcd(w));
  Wire.write(decToBcd(d));
  Wire.write(decToBcd(mon));
  Wire.write(decToBcd(y));

  Wire.write(zero); //start Oscillator

  Wire.endTransmission();
}

byte decToBcd(byte val)
{
  // Convert normal decimal numbers to binary coded decimal
  return ( (val / 10 * 16) + (val % 10) );
}

byte bcdToDec(byte val)
{
  // Convert binary coded decimal to normal decimal numbers
  return ( (val / 16 * 10) + (val % 16) );
}

void getRTCTime()
{
  Wire.beginTransmission(DS1307_ADDRESS);
  Wire.write(zero);
  Wire.endTransmission();

  Wire.requestFrom(DS1307_ADDRESS, 7);

  RTC_seconds = bcdToDec(Wire.read());
  RTC_minutes = bcdToDec(Wire.read());
  RTC_hours = bcdToDec(Wire.read() & 0b111111); //24 hour time
  RTC_day_of_week = bcdToDec(Wire.read()); //0-6 -> Sunday - Saturday, 1 = Monday
  RTC_day = bcdToDec(Wire.read());
  RTC_month = bcdToDec(Wire.read());
  RTC_year = bcdToDec(Wire.read());
}

int extractDigits(byte b)
{
  String tmp = "1";

  if (b == B00000011)
  {
    tmp = stringToDisplay.substring(0, 2);
  }
  else if (b == B00001100)
  {
    tmp = stringToDisplay.substring(2, 4);
  }
  else if (b == B00110000)
  {
    tmp = stringToDisplay.substring(4);
  }
  return tmp.toInt();
}

void injectDigits(byte b, int value)
{
  if (b == B00000011)
  {
    stringToDisplay = PreZero(value) + stringToDisplay.substring(2);
  }
  else if (b == B00001100)
  {
    stringToDisplay = stringToDisplay.substring(0, 2) + PreZero(value) + stringToDisplay.substring(4);
  }
  else if (b == B00110000)
  {
    stringToDisplay = stringToDisplay.substring(0, 4) + PreZero(value);
  }
}

bool isValidDate()
{
  int days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (value[DateYearIndex] % 4 == 0)
  {
    days[1] = 29;
  }
  if (value[DateDayIndex] > days[value[DateMonthIndex] - 1])
  {
    return false;
  }
  else
  {
    return true;
  }
}

byte default_dur = 4;
byte default_oct = 6;
int bpm = 63;
int num;
long wholenote;
long duration;
byte note;
byte scale;
char* parseSong(char *p)
{
  // Absolutely no error checking in here
  // format: d=N,o=N,b=NNN:
  // find the start (skip name, etc)

  while (*p != ':') p++;   // ignore name
  p++;                     // skip ':'

  // get default duration
  if (*p == 'd')
  {
    p++; p++;              // skip "d="
    num = 0;
    while (isdigit(*p))
    {
      num = (num * 10) + (*p++ - '0');
    }
    if (num > 0)
    {
      default_dur = num;
    }
    p++;                   // skip comma
  }

  // get default octave
  if (*p == 'o')
  {
    p++; p++;              // skip "o="
    num = *p++ - '0';
    if (num >= 3 && num <= 7)
    {
      default_oct = num;
    }
    p++;                   // skip comma
  }

  // get BPM
  if (*p == 'b')
  {
    p++; p++;              // skip "b="
    num = 0;
    while (isdigit(*p))
    {
      num = (num * 10) + (*p++ - '0');
    }
    bpm = num;
    p++;                   // skip colon
  }

  // BPM usually expresses the number of quarter notes per minute
  wholenote = (60 * 1000L / bpm) * 4;  // this is the time for whole note (in milliseconds)
  return p;
}

// now begin note loop
static unsigned long lastTimeNotePlaying = 0;
char* playmusic(char *p)
{
  if (*p == 0)
  {
    return p;
  }
  if (millis() - lastTimeNotePlaying > duration)
  {
    lastTimeNotePlaying = millis();
  }
  else
  {
    return p;
  }
  // first, get note duration, if available
  num = 0;
  while (isdigit(*p))
  {
    num = (num * 10) + (*p++ - '0');
  }

  if (num)
  {
    duration = wholenote / num;
  }
  else
  {
    duration = wholenote / default_dur;  // we will need to check if we are a dotted note after
  }

  // now get the note
  note = 0;

  switch (*p)
  {
    case 'c':
      note = 1;
      break;
    case 'd':
      note = 3;
      break;
    case 'e':
      note = 5;
      break;
    case 'f':
      note = 6;
      break;
    case 'g':
      note = 8;
      break;
    case 'a':
      note = 10;
      break;
    case 'b':
      note = 12;
      break;
    case 'p':
    default:
      note = 0;
  }
  p++;

  // now, get optional '#' sharp
  if (*p == '#')
  {
    note++;
    p++;
  }

  // now, get optional '.' dotted note
  if (*p == '.')
  {
    duration += duration / 2;
    p++;
  }

  // now, get scale
  if (isdigit(*p))
  {
    scale = *p - '0';
    p++;
  }
  else
  {
    scale = default_oct;
  }
  scale += OCTAVE_OFFSET;

  if (*p == ',')
  {
    p++;       // skip comma for next note (or we may be at the end)
  }

  // now play the note
  if (note)
  {
    tone1.play(notes[(scale - 4) * 12 + note], duration);
    if (millis() - lastTimeNotePlaying > duration)
    {
      lastTimeNotePlaying = millis();
    }
    else
    {
      return p;
    }
    tone1.stop();
  }
  else
  {
    return p;
  }
  
  Serial.println(F("Incorrect Song Format!"));
  return 0; //error
}


void incrementValue()
{
  enteringEditModeTime = millis();
  if (editMode == true)
  {
    // 12/24 hour mode menu position
    if (menuPosition != hModeValueIndex)
    {
      value[menuPosition] = value[menuPosition] + 1;
    }
    else
    {
      value[menuPosition] = value[menuPosition] + 12;
    }
    if (value[menuPosition] > maxValue[menuPosition])
    {
      value[menuPosition] = minValue[menuPosition];
    }
    if (menuPosition == Alarm01)
    {
      if (value[menuPosition] == 1)
      {
        /*digitalWrite(pinUpperDots, HIGH);*/
        dotPattern = B10000000; //turn on upper dots
      }
      else
      {
        /*digitalWrite(pinUpperDots, LOW); */
        dotPattern = B00000000; //turn off all dots
      }
    }
    if (menuPosition != DateFormatIndex)
    {
      injectDigits(blinkMask, value[menuPosition]);
    }
    Serial.print((String)"value = " + value[menuPosition]);
  }
}

void dicrementValue()
{
  enteringEditModeTime = millis();
  if (editMode == true)
  {
    if (menuPosition != hModeValueIndex)
    {
      value[menuPosition] = value[menuPosition] - 1;
    }
    else
    {
      value[menuPosition] = value[menuPosition] - 12;
    }
    if (value[menuPosition] < minValue[menuPosition])
    {
      value[menuPosition] = maxValue[menuPosition];
    }
    if (menuPosition == Alarm01)
    {
      if (value[menuPosition] == 1)
      {
        /*digitalWrite(pinUpperDots, HIGH);*/
        dotPattern = B10000000; //turn on upper dots
      }
      else
      {
        /*digitalWrite(pinUpperDots, LOW);*/
        dotPattern = B00000000; //turn off all dots
      }
    }
    if (menuPosition != DateFormatIndex)
    {
      injectDigits(blinkMask, value[menuPosition]);
    }
    Serial.print(F("value="));
    Serial.println(value[menuPosition]);
  }
}

bool Alarm1SecondBlock = false;
unsigned long lastTimeAlarmTriggired = 0;
void checkAlarmTime()
{
  if (value[Alarm01] == 0)
  {
    return;
  }
  if ((Alarm1SecondBlock == true) && ((millis() - lastTimeAlarmTriggired) > 1000))
  {
    Alarm1SecondBlock = false;
  }
  if (Alarm1SecondBlock == true)
  {
    return;
  }
  if ((hour() == value[AlarmHourIndex]) && (minute() == value[AlarmMinuteIndex]) && (second() == value[AlarmSecondIndex]))
  {
    lastTimeAlarmTriggired = millis();
    Alarm1SecondBlock = true;
    Serial.println(F("Wake up, Neo!"));
    p = song;
  }
}

void modesChanger()
{
  if (editMode == true)
  {
    return;
  }
  static unsigned long lastTimeModeChanged = millis();
  static unsigned long lastTimeAntiPoisoningIterate = millis();
  static int transnumber = 0;
  if ((millis() - lastTimeModeChanged) > modesChangePeriod)
  {
    lastTimeModeChanged = millis();
    if (transnumber == 0)
    {
      menuPosition = DateIndex;
      modesChangePeriod = dateModePeriod;
    }
    else if (transnumber == 1)
    {
      menuPosition = TemperatureIndex;
      modesChangePeriod = dateModePeriod;
      if (!TempPresent)
      {
        transnumber = 2;
      }
    }
    else if (transnumber == 2)
    {
      menuPosition = TimeIndex;
      modesChangePeriod = timeModePeriod;
    }
    transnumber++;
    if (transnumber > 2)
    {
      transnumber = 0;
    }

    if (modeChangedByUser == true)
    {
      menuPosition = TimeIndex;
    }
    modeChangedByUser = false;
  }
  if ((millis() - lastTimeModeChanged) < 2000)
  {
    if ((millis() - lastTimeAntiPoisoningIterate) > 100)
    {
      lastTimeAntiPoisoningIterate = millis();
      if (TempPresent)
      {
        //Serial.println("TempPresent == YES");
        if (menuPosition == TimeIndex)
        {
          stringToDisplay = antiPoisoning2(updateTemperatureString(getTemperature(value[DegreesFormatIndex])), getTimeNow());
          //Serial.println("menuPosition == TimeIndex");
        }
        else if (menuPosition == DateIndex)
        {
          stringToDisplay = antiPoisoning2(getTimeNow(), PreZero(day()) + PreZero(month()) + PreZero(year() % 1000) );
          //Serial.println("menuPosition == DateIndex");
        }
        else if (menuPosition == TemperatureIndex)
        {
          stringToDisplay = antiPoisoning2(PreZero(day()) + PreZero(month()) + PreZero(year() % 1000), updateTemperatureString(getTemperature(value[DegreesFormatIndex])));
          //Serial.println("menuPosition == TemperatureIndex");
        }
      }
      else
      {
        //Serial.println("TempPresent == NO");
        if (menuPosition == TimeIndex)
        {
          stringToDisplay = antiPoisoning2(PreZero(day()) + PreZero(month()) + PreZero(year() % 1000), getTimeNow());
          //Serial.println("menuPosition == TimeIndex");
        }
        else if (menuPosition == DateIndex)
        {
          stringToDisplay = antiPoisoning2(getTimeNow(), PreZero(day()) + PreZero(month()) + PreZero(year() % 1000) );
          //Serial.println("menuPosition == DateIndex");
        }
      }
      // For testing
      //Serial.println((String)"StrTDInToModeChng= " + stringToDisplay);
    }
  }
  else
  {
    transactionInProgress = false;
  }
}

String antiPoisoning2(String fromStr, String toStr)
{
  //static bool transactionInProgress=false;
  //byte fromDigits[6];
  static byte toDigits[6];
  static byte currentDigits[6];
  static byte iterationCounter = 0;
  if (!transactionInProgress)
  {
    transactionInProgress = true;
    blankMask = B00000000;
    for (int i = 0; i < 6; i++)
    {
      currentDigits[i] = fromStr.substring(i, i + 1).toInt();
      toDigits[i] = toStr.substring(i, i + 1).toInt();
    }
  }
  for (int i = 0; i < 6; i++)
  {
    if (iterationCounter < 10)
    {
      currentDigits[i]++;
    }
    else if (currentDigits[i] != toDigits[i])
    {
      currentDigits[i]++;
    }
    if (currentDigits[i] == 10)
    {
      currentDigits[i] = 0;
    }
  }
  iterationCounter++;
  if (iterationCounter == 20)
  {
    iterationCounter = 0;
    transactionInProgress = false;
  }
  String tmpStr;
  for (int i = 0; i < 6; i++)
  {
    tmpStr += currentDigits[i];
  }
  
  return tmpStr;
}

String updateDateString()
{
  static unsigned long lastTimeDateUpdate = millis() + 1001;
  static String DateString = PreZero(day()) + PreZero(month()) + PreZero(year() % 1000);
  static byte prevoiusDateFormatWas = value[DateFormatIndex];
  if (((millis() - lastTimeDateUpdate) > 1000) || (prevoiusDateFormatWas != value[DateFormatIndex]))
  {
    lastTimeDateUpdate = millis();
    if (value[DateFormatIndex] == EU_DateFormat)
    {
      DateString = PreZero(day()) + PreZero(month()) + PreZero(year() % 1000);
    }
    else
    {
      DateString = PreZero(month()) + PreZero(day()) + PreZero(year() % 1000);
    }
  }
  return DateString;
}

float getTemperature (boolean bTempFormat)
{
  byte TempRawData[2];
  ds.reset();
  ds.write(0xCC); //skip ROM command
  ds.write(0x44); //send make convert to all devices
  ds.reset();
  ds.write(0xCC); //skip ROM command
  ds.write(0xBE); //send request to all devices

  TempRawData[0] = ds.read();
  TempRawData[1] = ds.read();
  int16_t raw = (TempRawData[1] << 8) | TempRawData[0];
  if (raw == -1)
  {
    raw = 0;
  }
  float celsius = (float)raw / 16.0;
  float fDegrees;
  if (!bTempFormat)
  {
    fDegrees = celsius * 10;
  }
  else
  {
    fDegrees = (celsius * 1.8 + 32.0) * 10;
  }
  //Serial.println((String)"fDegrees = " + fDegrees);
  return fDegrees;
}

void SyncWithGPS()
{
  // Play a tone when updating with GPS since this is the backup timesync
  tone1.play(2000, 100);
  Serial.println(F("Updating time GPS..."));
  Serial.print(F("UTC Hour(s) = "));
  Serial.println(fix.dateTime.hours);
  Serial.print(F("UTC Minute(s) = "));
  Serial.println(fix.dateTime.minutes);
  Serial.print(F("Second(s) = "));
  Serial.println(fix.dateTime.seconds);

  //setTime(GPS_Date_Time.GPS_hours, GPS_Date_Time.GPS_minutes, GPS_Date_Time.GPS_seconds, GPS_Date_Time.GPS_day, GPS_Date_Time.GPS_mounth, GPS_Date_Time.GPS_year % 1000);
  setTime(fix.dateTime.hours, fix.dateTime.minutes, fix.dateTime.seconds, fix.dateTime.date, fix.dateTime.month, fix.dateTime.year);
  
  // Adjust time with 1 if drift is >= 500 to compensate for GPS drifting
  if (gps.UTCms() >= 500)
  {
    adjustTime(1);
  }
  
  adjustTime((long)value[HoursOffsetIndex] * 3600);
  setRTCDateTime(hour(), minute(), second(), day(), month(), year() % 1000, 1);
  GPS_Sync_Flag = 1;
  Last_Time_GPS_Sync = millis();

  // Reset the NTP bools
  NTPRetryFailed = false;
  NTPSyncEnabled = true;

  //Serial.println(F("Out of SyncWithGPS"));
}

void GPSCheckValidity()
{
  if ((fix.dateTime.hours < 0) || (fix.dateTime.hours > 23))
  {
    return;
  }

  else if ((fix.dateTime.minutes < 0) || (fix.dateTime.minutes > 59))
  {
    return;
  }

  else if ((fix.dateTime.seconds < 0) || (fix.dateTime.seconds > 59)){
    return;
  }

  else if ((fix.dateTime.date < 0) || (fix.dateTime.date > 31)){
    return;
  }

  else if ((fix.dateTime.month < 0) || (fix.dateTime.month > 12)){
    return;
  }

  else if ((fix.dateTime.full_year() < 2020) || (fix.dateTime.full_year() > 2030)){
    return;
  }

  else
  {
    fix.valid.time = false;
    SyncWithGPS();
  }
}

String updateTemperatureString(float fDegrees)
{
  static  unsigned long lastTimeTemperatureString = millis() + 1100;
  static String strTemp = "000000";
  if ((millis() - lastTimeTemperatureString) > 1000)
  {
    //Serial.println(F("Updating temp. str."));
    lastTimeTemperatureString = millis();
    int iDegrees = round(fDegrees);
    if (value[DegreesFormatIndex] == CELSIUS)
    {
      strTemp = "0" + String(abs(iDegrees)) + "0";
      if (abs(iDegrees) < 1000)
      {
        strTemp = "00" + String(abs(iDegrees)) + "0";
      }
      else if (abs(iDegrees) < 100)
      {
        strTemp = "000" + String(abs(iDegrees)) + "0";
      }
      else if (abs(iDegrees) < 10)
      {
        strTemp = "0000" + String(abs(iDegrees)) + "0";
      }
    }
    else
    {
      strTemp = "0" + String(abs(iDegrees)) + "0";
      if (abs(iDegrees) < 1000)
      {
        strTemp = "00" + String(abs(iDegrees) / 10) + "00";
      }
      else if (abs(iDegrees) < 100)
      {
        strTemp = "000" + String(abs(iDegrees) / 10) + "00";
      }
      else if (abs(iDegrees) < 10)
      {
        strTemp = "0000" + String(abs(iDegrees) / 10) + "00";
      }
    }

#ifdef tubes8
    strTemp = "" + strTemp + "00";
#endif
    return strTemp;
  }
    return strTemp;
}

void testDS3231TempSensor()
{
  int8_t DS3231InternalTemperature = 0;
  Wire.beginTransmission(DS1307_ADDRESS);
  Wire.write(0x11);
  Wire.endTransmission();

  Wire.requestFrom(DS1307_ADDRESS, 2);
  DS3231InternalTemperature = Wire.read();
  Serial.print(F("DS3231_T="));
  Serial.println(DS3231InternalTemperature);
  if ((DS3231InternalTemperature < 5) || (DS3231InternalTemperature > 60))
  {
    Serial.println(F("Faulty DS3231!"));
    for (int i = 0; i < 5; i++)
    {
      tone1.play(1000, 1000);
      delay(2000);
    }
  }
}

// ************
// WIFI
// ************
void WiFiSetup()
{
  Serial3.begin(115200);
  WiFi.init(&Serial3);    // initialize ESP module
    
  Serial.println(F("\n"));
  Serial.println((String)"Connecting to = " + ssid);
  
  WiFi.begin(ssid, pass);
  
  // Set up Wifi connection
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  // Wifi is connected
  if(WiFi.status() == WL_CONNECTED)
  {
    // Start local UDP port for NTP
    Udp.begin(localPort);
    
    Serial.println(F("\n"));

    IPAddress ip = WiFi.localIP();
    Serial.print(F("IP Address: "));
    Serial.println(ip);
  
    Serial.println((String)"Signal strength (RSSI) = " + WiFi.RSSI() + " dBm");
    Serial.println((String)"ESP8266 Firmware = " + WiFi.firmwareVersion());

    Serial.println(F("\n"));
  }

  // Wifi is not connected
  else
  {
      Serial.println(F("WiFi NOT connected"));
      Serial.println(F("\n"));
  }
}

// ************
// NTP
// ************
time_t getNTPTime()
{
  // Reset NTP package validity
  NTP_Package_Valid = false;

  // Reset timeOffset
  timeOffset = 0;

  // Request UDP NTP package
  sendNTPpacket(timeServer);

  // Wait for package retrieval
  Serial.println(F("Retrieving UDP packages"));
  unsigned long startMs = millis();
  while (!Udp.available() && (millis() - startMs) < UDP_TIMEOUT) {}
  Serial.println(F("Done etrieving UDP packages"));
  
  Serial.println((String)"UDP package size = " + Udp.parsePacket());

  //if (Udp.parsePacket() <- original
  // Only change time if the received UDP package has the same value as the expected package size (48)
  if (Udp.parsePacket() == NTP_PACKET_SIZE)
  {
    // UDP package has the correct size
    NTP_Package_Valid = true;

    // If DST is enabled
    if (DSTEnabled && NTP_Package_Valid)
    {
      Serial.print(F("Current month is = "));
      Serial.println(RTC_month);

      // No summertime in January, February, November and December
      if ((RTC_month <= 2) || (RTC_month >= 11))
      {
        Serial.println(F("Wintertime, GMT set to +1"));
        timeOffset = ((GMTOffSet + 1) * 60UL * 60UL);
      }

////////// Disabled on 2022-03-21 else-if below to fix offset stuck in month bug.
//      // Summertime in April, May, June, July, August and September
//      //if ((RTC_month >= 2) && (RTC_month <= 9))
//      else if ((RTC_month >= 2) && (RTC_month <= 9))
//      {
//        Serial.println(F("Summertime, GMT set to +2"));
//        timeOffset = ((GMTOffSet + 2) * 60UL * 60UL);
//      }
//////////

      // Summertime starts on the 21st of March and ends on the 21st of September
      //if (((RTC_month == 3) && ((RTC_hours + 24 * RTC_day) >= (3 +  24 * (31 - (5 * RTC_year / 4 + 4) % 7)))) || ((RTC_month == 10) && ((RTC_hours + 24 * RTC_day) < (3 +  24 * (31 - (5 * RTC_year / 4 + 1) % 7)))))
      else if (((RTC_month == 3) && ((RTC_hours + 24 * RTC_day) >= (3 +  24 * (31 - (5 * RTC_year / 4 + 4) % 7)))) || ((RTC_month == 10) && ((RTC_hours + 24 * RTC_day) < (3 +  24 * (31 - (5 * RTC_year / 4 + 1) % 7)))))
      {
        Serial.println(F("Summertime, GMT set to +2"));
        timeOffset = ((GMTOffSet + 2) * 60UL * 60UL);
      }

      else
      {
        Serial.println(F("Wintertime, GMT set to +1"));
        timeOffset = ((GMTOffSet + 1) * 60UL * 60UL);
      }
    }

    // DST is NOT enabled
    else
    {
      Serial.println(F("GMT set to 0, DST not enabled"));
      timeOffset = (GMTOffSet * 60UL * 60UL);
    }

    // Reset retry count
    NTPRetryCount = 0;
    NTPRetryFailed = false;
    NTPSyncEnabled = true;

    // Process the epoch
    Udp.read(packetBuffer, NTP_PACKET_SIZE);
    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
    unsigned long secsSince1900 = highWord << 16 | lowWord;
    const unsigned long seventyYears = 2208988800UL;
    unsigned long epoch = secsSince1900 - seventyYears + timeOffset;

    Serial.print((String)"Epoch updated = " + epoch);
    Serial.println(F("\n"));

    return epoch;
  }

  // Try again getting new epoch
  else
  {
    NTP_Package_Valid = false;

    NTPRetryCount++;
    Serial.print(F("NTP Retry count = "));
    Serial.println(NTPRetryCount);

    // Retry 3 times before returning previous epoch
    if (NTPRetryCount < 3 )
    {
      Serial.println(F("Epoch NOT updated, retrying..."));
      NTPRetryFailed = false;
      getNTPTime();
    }

    // Reset retrycount and return current RTC time
    else
    {
      Serial.println(F("Retry failed"));
      NTPRetryCount = 0;
      NTPRetryFailed = true;
      return;
    }
  }
}

void setNTPTime()
{
  // Extra WIFI connection check
  if (WiFi.status() != WL_CONNECTED)
  {
      Serial.println(F("Reconnecting to WiFi..."));
      WiFi.disconnect();

      // Restart WIFI setup
      WiFiSetup();
  }

  else
  {
    // Try and set the time with the updated NTP epoch to the nixies
    setTime(getNTPTime());
    Serial.println(F("Current time updated with NTP"));

    // If no errors occured write the time into the RTC
    if (NTPRetryFailed == false)
    {
      NTPRetryCount = 0;

      String tempRTCTime = (String)RTC_hours + ":" + RTC_minutes + ":" + RTC_seconds;
      String tempNTPTime = (String)hour() + ":" + minute() + ":" + second();

      Serial.println("Fetched RTC time = " + tempRTCTime);
      Serial.println("Fetched NTP time = " + tempNTPTime);

      // Compare drift between RTC and NTP and update RTC or not (prevents excessive writes)
      if(tempRTCTime != tempNTPTime)
      {
        setRTCDateTime(hour(), minute(), second(), day(), month(), year() % 1000, 1);
        Serial.println(F("Synced current time to RTC"));
      }

      else
      {
        Serial.println(F("Time not synced with RTC, RTC and NTP time are the same"));
      }
    }

    // Else if an error occured catch it
    else if (NTPRetryFailed == true)
    {
      NTP_Package_Valid = false;
      NTPRetryFailed = true;

      Serial.println(F("Time failed updating with NTP!"));
    }

    Serial.println(F("\n"));
  }
}

// send an NTP request to the time server at the given address
void sendNTPpacket(char *ntpSrv)
{
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  packetBuffer[0] = 0b11100011; // LI, Version, Mode
  packetBuffer[1] = 0; // Stratum, or type of clock, 0 = directly connected device (e.g. GPS)
  packetBuffer[2] = 6; // Polling Interval
  packetBuffer[3] = 0xEC; // Peer Clock Precision
  // packetBuffer[4] -> packetBuffer[11] : 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;

  // All NTP fields have been given values
  // Now you can send a packet requesting a timestamp:
  Udp.beginPacket(ntpSrv, 123); // NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}
