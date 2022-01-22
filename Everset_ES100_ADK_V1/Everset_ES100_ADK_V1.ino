/*
This is the demonstration code for the UNIVERSAL-SOLDER / Everset ES100
Application Development Kit. It reads the decoded time stamp from
the ES100MOD receiver module and shows several information on a 4x20
character display. There are no function assignments for unused GPIO,
analog inputs and the 3 push buttons included in this sketch.

Version: 1 (10/04/2020)

PLEASE FEEL FREE TO CONTRIBUTE TO THE DEVELOPMENT. CORRECTIONS AND
ADDITIONS ARE HIGHLY APPRECIATED. SEND YOUR COMMENTS OR CODE TO:
support@universal-solder.ca

Please support us by purchasing products from UNIVERSAL-SOLDER.ca store!

Copyright (c) 2020 UNIVERSAL-SOLDER Electronics Ltd. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

- Redistributions of source code must retain the above copyright notice,
  this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
 */


// include the library code:
#include <LiquidCrystal.h>
#include <DS3231.h>
#include <ES100.h>
#include <Wire.h>


#define VERSION     (0)
#define ISSUE       (3)
#define ISSUE_DATE  "2022-01-20"


#define CONTINUOUS_MODE         (false)

#define MAX_STRING_SIZE         (60)
#define MAX_ISODATE_STRING_SIZE (sizeof("yyyy-mm-dd hh:mm:ssZ"))

// Define the 4 line x 20 chars/line LCD peripheral.
#define lcdRS                   (4)
#define lcdEN                   (5)
#define lcdD4                   (8)
#define lcdD5                   (9)
#define lcdD6                   (10)
#define lcdD7                   (11)
LiquidCrystal lcd(lcdRS, lcdEN, lcdD4, lcdD5, lcdD6, lcdD7);

// Define the DS3231 RTC peripheral.
DS3231 rtc(SDA, SCL);

// Define the ES100 WWVB receiver peripheral - uses I2C lines common with RTC.
#define es100Int                (2)
#define es100En                 (13)
ES100 es100;

unsigned long           LastMillis = 0;
volatile unsigned long  AtomicMillis = 0;
unsigned long           LastSyncMillis = 0;

volatile unsigned int InterruptCount = 0;
unsigned int          LastInterruptCount = 0;


boolean       InReceiveMode = false;            // variable to determine if we are in receive mode
boolean       TriggerReception = true;          // variable to trigger the reception
boolean       ContinuousMode = CONTINUOUS_MODE; // variable to tell the system to continuously receive atomic time, if not it will happen every night at midnight
boolean       ValidDecode = false;              // variable to rapidly know if the system had a valid decode done lately

ES100DateTime SavedDateTime;
ES100Status0  SavedStatus0;
ES100NextDst  SavedNextDst;


void atomic() {
  // Called procedure when we receive an interrupt from the ES100
  // Got a interrupt and store the currect millis for future use if we have a valid decode
  AtomicMillis = millis();
  InterruptCount++;
}

char * getISODateStr() {
  static char  ReturnValue[MAX_ISODATE_STRING_SIZE+10];
  static char result[19];
  Time TimeValue;


  TimeValue=rtc.getTime();
  snprintf(ReturnValue, MAX_ISODATE_STRING_SIZE, "%4.4d-%2.2d-%2.2d %2.2d:%2.2d:%2.2d",
          TimeValue.year, TimeValue.mon, TimeValue.date, TimeValue.hour, TimeValue.min, TimeValue.sec);

  return ReturnValue;
}

void displayDST() {
  char  StringBuffer[MAX_STRING_SIZE+10];
  char *DstThisMonth;


  switch (SavedStatus0.dstState) {
    case B00:
      DstThisMonth = "NOT in Effect";
      break;
    case B10:
      DstThisMonth = "BEGINS Today";
      break;
    case B11:
      DstThisMonth = "YES in Effect";
      break;
    case B01:
      DstThisMonth = "ENDS Today";
      break;
  }

  //                                                11111111112
  //                                       12345678901234567890
  //                                       DST NOT in Effect
  snprintf(StringBuffer, MAX_STRING_SIZE, "DST %s", DstThisMonth);
  lcd.print(StringBuffer);
}

void displayNDST() {
  char  StringBuffer[MAX_STRING_SIZE+10];

  //                                                11111111112
  //                                       12345678901234567890
  //                                       DSTChg mm-dd @ hh:00
  snprintf(StringBuffer, MAX_STRING_SIZE, "DSTChg %2.2d-%2.2d @ %2.2dh00",
            SavedNextDst.month, SavedNextDst.day, SavedNextDst.hour);
  lcd.print(StringBuffer);
}

void displayLeapSecond() {
  char  StringBuffer[MAX_STRING_SIZE+10];
  char *TypeThisMonth;


  switch (SavedStatus0.leapSecond) {
    case B00:
      TypeThisMonth = "NoLeapSec";
      break;
    case B10:
      TypeThisMonth = "LeapSec- ";
      break;
    case B11:
      TypeThisMonth = "LeapSec+ ";
      break;
  }

  //                                                11111111112
  //                                       12345678901234567890
  //                                       NoLeapSec this month
  snprintf(StringBuffer, MAX_STRING_SIZE, "%s this month", TypeThisMonth);
  lcd.print(StringBuffer);
}

void displayLastSync() {
  char  StringBuffer[MAX_STRING_SIZE+10];


  if (LastSyncMillis > 0) {
    int days =    (millis() - LastSyncMillis) / 86400000;
    int hours =   ((millis() - LastSyncMillis) % 86400000) / 3600000;
    int minutes = (((millis() - LastSyncMillis) % 86400000) % 3600000) / 60000;
    int seconds = ((((millis() - LastSyncMillis) % 86400000) % 3600000) % 60000) / 1000;

    //                                                11111111112
    //                                       12345678901234567890
    //                                       LastSync DdHHhMMmSSs
    snprintf(StringBuffer, MAX_STRING_SIZE, "LastSync%2dd%2.2dh%2.2dm%2.2ds",
            days, hours, minutes, seconds);
  } else {
    //                                                11111111112
    //                                       12345678901234567890
    //                                       LastSync no sync yet
    snprintf(StringBuffer, MAX_STRING_SIZE, "LastSync no sync yet");
  }

  lcd.print(StringBuffer);
}

void displayInterrupt() {
  char  StringBuffer[MAX_STRING_SIZE+10];

  //                                                11111111112
  //                                       12345678901234567890
  //                                       IRQ count nnnnn
  snprintf(StringBuffer, MAX_STRING_SIZE, "IRQ Count %5d", InterruptCount);
  lcd.print(StringBuffer);
}

void displayAntenna() {
  char  StringBuffer[MAX_STRING_SIZE+10];
  char *AntennaUsed;


  switch (SavedStatus0.antenna) {
    case 0:
      AntennaUsed = "1";
      break;
    case 1:
      AntennaUsed = "2";
      break;
    default:
      AntennaUsed = "?";
      break;
  }

  //                                                11111111112
  //                                       12345678901234567890
  //                                       Antenna ? used
  snprintf(StringBuffer, MAX_STRING_SIZE, "Antenna %s used", AntennaUsed);
  lcd.print(StringBuffer);
}

void clearLine(unsigned int n) {
  while (n-- > 0)
    lcd.print(" ");
}

void showlcd() {
  lcd.setCursor(0,0);
  lcd.print(getISODateStr());
  lcd.print("Z");

  if (ValidDecode) {
    // Scroll lines every 5 seconds.
    int lcdLine = (millis() / 5000 % 6) + 1;

    lcd.setCursor(0,1);
    clearLine(20);
    lcd.setCursor(0,1);
    switch (lcdLine) {
      case 1:
        displayInterrupt();
        break;
      case 2:
        displayLastSync();
        break;
      case 3:
        displayDST();
        break;
      case 4:
        displayNDST();
        break;
      case 5:
        displayLeapSecond();
        break;
      case 6:
        displayAntenna();
        break;
    }

    lcd.setCursor(0,2);
    clearLine(20);
    lcd.setCursor(0,2);
    switch (lcdLine) {
      case 6:
        displayInterrupt();
        break;
      case 1:
        displayLastSync();
        break;
      case 2:
        displayDST();
        break;
      case 3:
        displayNDST();
        break;
      case 4:
        displayLeapSecond();
        break;
      case 5:
        displayAntenna();
        break;
    }

    lcd.setCursor(0,3);
    clearLine(20);
    lcd.setCursor(0,3);
    switch (lcdLine) {
      case 5:
        displayInterrupt();
        break;
      case 6:
        displayLastSync();
        break;
      case 1:
        displayDST();
        break;
      case 2:
        displayNDST();
        break;
      case 3:
        displayLeapSecond();
        break;
      case 4:
        displayAntenna();
        break;
    }
  }
  else {
    lcd.setCursor(0,1);
    displayInterrupt();
    lcd.setCursor(0,2);
    clearLine(20);
    lcd.setCursor(0,3);
    clearLine(20);

  }
}

void setup() {
  char  StringBuffer[MAX_STRING_SIZE+10];


  Wire.begin();
  Serial.begin(9600);
  Serial.println();
  Serial.println();
  Serial.println("-------------------------");

  snprintf(StringBuffer, MAX_STRING_SIZE, "Everset ES100 ADK [v%d.%d %s] startup", VERSION, ISSUE, ISSUE_DATE);
  Serial.println(StringBuffer);

  Serial.println("-------------------------");
  Serial.println();

  snprintf(StringBuffer, MAX_STRING_SIZE, "Continuous mode %s.", ContinuousMode ? "enabled" : "disabled");
  Serial.println(StringBuffer);
  Serial.println();

  lcd.begin(20, 4);
  lcd.clear();
  //                  11111111112
  //         12345678901234567890
  //          Everset ES100 ADK
  //           vn.m yyyy-mm-dd
  // If VERSION or ISSUE is 2 digits,
  // will spill a bit to the right, boo hoo.
  lcd.setCursor(0,1);
  lcd.print(" Everset ES100 ADK  ");

  lcd.setCursor(0,2);
  snprintf(StringBuffer, MAX_STRING_SIZE, " [v%d.%d %s]", VERSION, ISSUE, ISSUE_DATE);
  lcd.print(StringBuffer);

  delay(5000);

  lcd.begin(20, 4);
  lcd.clear();

  es100.begin(es100Int, es100En);

  rtc.begin();

  snprintf(StringBuffer, MAX_STRING_SIZE, "Starting at %s UTC.", getISODateStr());
  Serial.print(StringBuffer);

  attachInterrupt(digitalPinToInterrupt(es100Int), atomic, FALLING);
}

void loop() {
  char  StringBuffer[MAX_STRING_SIZE+10];
  Time  TimeValue;


  if (!InReceiveMode && (TriggerReception || ContinuousMode)) {
    InterruptCount = 0;

    Serial.println();

    es100.enable();
    es100.startRx();

    InReceiveMode = true;
    TriggerReception = false;

    /* Important to set the interrupt counter AFTER the startRx because the es100
     * confirm that the rx has started by triggering the interrupt.
     * We can't disable interrupts because the wire library will stop working
     * so we initialize the counters after we start so we can ignore the first false
     * trigger
     */
    LastInterruptCount = 0;
    InterruptCount = 0;
  }

  if (LastInterruptCount < InterruptCount) {
    Serial.println();

    snprintf(StringBuffer, MAX_STRING_SIZE, "ES100 IRQ %5d: ", InterruptCount);

    if (es100.getIRQStatus() == 0x01 && es100.getRxOk() == 0x01) {
      ValidDecode = true;

      strncat(StringBuffer, "has data at ", MAX_STRING_SIZE);
      strncat(StringBuffer, getISODateStr(), MAX_STRING_SIZE);
      strncat(StringBuffer, " UTC.", MAX_STRING_SIZE);
      Serial.println(StringBuffer);

      // Update LastSyncMillis for lcd display
      LastSyncMillis = millis();
      // We received a valid decode
      SavedDateTime = es100.getDateTime();
      // Updating the RTC
      rtc.setDate(SavedDateTime.day,  SavedDateTime.month,  2000+SavedDateTime.year);
      rtc.setTime(SavedDateTime.hour, SavedDateTime.minute, SavedDateTime.second + ((millis() - AtomicMillis)/1000));

      // Get everything before disabling the chip.
      SavedStatus0 = es100.getStatus0();
      SavedNextDst = es100.getNextDst();

/* DEBUG */
      snprintf(StringBuffer, MAX_STRING_SIZE, "status: rxOk    0x%2.2X, antenna  0x%2.2X, leapSecond 0x%2.2X",
                SavedStatus0.rxOk, SavedStatus0.antenna, SavedStatus0.leapSecond, SavedStatus0.dstState, SavedStatus0.tracking);
      Serial.println(StringBuffer);

      snprintf(StringBuffer, MAX_STRING_SIZE, "        dstState 0x%2.2X, tracking 0x%2.2X",
                SavedStatus0.rxOk, SavedStatus0.antenna, SavedStatus0.leapSecond, SavedStatus0.dstState, SavedStatus0.tracking);
      Serial.println(StringBuffer);
/* END DENUG */

      es100.stopRx();
      es100.disable();
      InReceiveMode = false;
    }
    else {
      strncat(StringBuffer, "no data at ", MAX_STRING_SIZE);
      strncat(StringBuffer, getISODateStr(), MAX_STRING_SIZE);
      strncat(StringBuffer, " UTC.", MAX_STRING_SIZE);
      Serial.println(StringBuffer);

    }
    LastInterruptCount = InterruptCount;
  }

  if (LastMillis + 100 < millis()) {
    showlcd();

    // set the trigger to start reception at midnight (UTC-4) if we are not in continuous mode.
    // 4am UTC is midnight for me, adjust to your need
    TimeValue=rtc.getTime();
    TriggerReception = (!ContinuousMode && !InReceiveMode && TimeValue.hour == 4 && TimeValue.min == 0);

    LastMillis = millis();
  }
}
