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

#define CONTINUOUS_MODE (false)


#define MAX_STRING_SIZE         (60)
#define MAX_ISODATE_STRING_SIZE (sizeof("yyyy-mm-dd hh:mm:ssZ"))
#define lcdRS                   (4)
#define lcdEN                   (5)
#define lcdD4                   (8)
#define lcdD5                   (9)
#define lcdD6                   (10)
#define lcdD7                   (11)
LiquidCrystal lcd(lcdRS, lcdEN, lcdD4, lcdD5, lcdD6, lcdD7);

DS3231 rtc(SDA, SCL);

#define es100Int                (2)
#define es100En                 (13)
ES100 es100;

unsigned long lastMillis = 0;
volatile unsigned long atomicMillis = 0;
unsigned long lastSyncMillis = 0;

volatile unsigned int interruptCnt = 0;
unsigned int lastinterruptCnt = 0;


boolean receiving = false;        // variable to determine if we are in receiving mode
boolean trigger = true;           // variable to trigger the reception
boolean continuous = CONTINUOUS_MODE;  // variable to tell the system to continuously receive atomic time, if not it will happen every night at midnight
boolean validdecode = false;      // variable to rapidly know if the system had a valid decode done lately


Time t;
ES100DateTime d;
ES100Status0  status0;
ES100NextDst  nextDst;

void atomic() {
  // Called procedure when we receive an interrupt from the ES100
  // Got a interrupt and store the currect millis for future use if we have a valid decode
  atomicMillis = millis();
  interruptCnt++;
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


  if (lastSyncMillis > 0) {
    int days =    (millis() - lastSyncMillis) / 86400000;
    int hours =   ((millis() - lastSyncMillis) % 86400000) / 3600000;
    int minutes = (((millis() - lastSyncMillis) % 86400000) % 3600000) / 60000;
    int seconds = ((((millis() - lastSyncMillis) % 86400000) % 3600000) % 60000) / 1000;

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


  switch (status0.antenna) {
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

  if (validdecode) {
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


  if (!receiving && (trigger || continuous)) {
    interruptCnt = 0;

    Serial.println();

    es100.enable();
    es100.startRx();

    receiving = true;
    trigger = false;

    /* Important to set the interrupt counter AFTER the startRx because the es100
     * confirm that the rx has started by triggering the interrupt.
     * We can't disable interrupts because the wire library will stop working
     * so we initialize the counters after we start so we can ignore the first false
     * trigger
     */
    lastinterruptCnt = 0;
    interruptCnt = 0;
  }

  if (lastinterruptCnt < interruptCnt) {
    Serial.println();

    snprintf(StringBuffer, MAX_STRING_SIZE, "ES100 IRQ %5d: ", interruptCnt);

    if (es100.getIRQStatus() == 0x01 && es100.getRxOk() == 0x01) {
      validdecode = true;

      strncat(StringBuffer, "has data at ", MAX_STRING_SIZE);
      strncat(StringBuffer, getISODateStr(), MAX_STRING_SIZE);
      Serial.println(StringBuffer);

      // Update lastSyncMillis for lcd display
      lastSyncMillis = millis();
      // We received a valid decode
      d = es100.getDateTime();
      // Updating the RTC
      rtc.setDate(d.day, d.month, 2000+d.year);
      rtc.setTime(d.hour, d.minute, d.second + ((millis() - atomicMillis)/1000));

      // Get everything before disabling the chip.
      status0 = es100.getStatus0();
      nextDst = es100.getNextDst();

/* DEBUG */
      snprintf(StringBuffer, MAX_STRING_SIZE, "status: rxOk    0x%2.2X, antenna  0x%2.2X, leapSecond 0x%2.2X",
                status0.rxOk, status0.antenna, status0.leapSecond, status0.dstState, status0.tracking);
      Serial.println(StringBuffer);

      snprintf(StringBuffer, MAX_STRING_SIZE, "        dstState 0x%2.2X, tracking 0x%2.2X",
                status0.rxOk, status0.antenna, status0.leapSecond, status0.dstState, status0.tracking);
      Serial.println(StringBuffer);
/* END DENUG */

      es100.stopRx();
      es100.disable();
      receiving = false;
    }
    else {
      strncat(StringBuffer, "no data at ", MAX_STRING_SIZE);
      strncat(StringBuffer, getISODateStr(), MAX_STRING_SIZE);
      Serial.println(StringBuffer);

    }
    lastinterruptCnt = interruptCnt;
  }

  if (lastMillis + 100 < millis()) {
    showlcd();

    // set the trigger to start reception at midnight (UTC-4) if we are not in continuous mode.
    // 4am UTC is midnight for me, adjust to your need
    trigger = (!continuous && !receiving && t.hour == 4 && t.min == 0);

    lastMillis = millis();
  }
}
