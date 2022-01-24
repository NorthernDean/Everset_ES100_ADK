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


#define VERSION                 (0)
#define ISSUE                   (6)
#define ISSUE_DATE              "2022-01-22"

#define CONTINUOUS_MODE         (true)

// Hours to retry, if we claim to be in continuous mode.
// ( continuous mode won't truly be continuous until I
//   figure out how to make it work properly )
#define CONTINUOUS_RETRY_HOURS  (1)
#define MILLIS_PER_SECOND       (1000)
#define SECONDS_PER_MINUTE      (60)
#define MINUTES_PER_HOUR        (60)
#define MILLIS_PER_MINUTE       ((unsigned long) (MILLIS_PER_SECOND) * (unsigned long) (SECONDS_PER_MINUTE))
#define MILLIS_PER_HOUR         ((unsigned long) (MILLIS_PER_SECOND) * (unsigned long) (SECONDS_PER_MINUTE) * (unsigned long) (MINUTES_PER_HOUR))
#define CONTINUOUS_RETRY_MILLIS ((unsigned long) (CONTINUOUS_RETRY_HOURS) * (unsigned long) (MILLIS_PER_HOUR))

// Define these up by one character to ensure space for NUL.
#define MAX_STRING_SIZE         (61)
#define MAX_LCD_STRING_SIZE     (21)

// Define the 4 line x 20 chars/line LCD peripheral.
#define lcdRS                   (4)
#define lcdEN                   (5)
#define lcdD4                   (8)
#define lcdD5                   (9)
#define lcdD6                   (10)
#define lcdD7                   (11)
LiquidCrystal lcd(lcdRS, lcdEN, lcdD4, lcdD5, lcdD6, lcdD7);

/*
  Characters are specified in strings as \nnn (octal) or \xnn (hex)
  We avoid using 0x00 because it conflicts with the NUL character which
  terminates a string.  We could work around it, but then we couldn't
  include it in saved, copied & printed strings.

      0x00        0x01        0x02        0x03        0x04        0x05
     unused     receive1    receive2    receive3    receive4    antennna
    ........    ........    ........    ........    ...*....    ........
    ........    ........    ........    ...*....    .....*..    ........
    ........    ........    ...*....    .....*..    ......*.    ...*****
    ........    ...*....    .....*..    ......*.    .......*    ...*.*.*
    ........    .....*..    ......*.    .......*    .......*    ....*.*.
    ........    ......*.    .......*    .......*    ........    .....*..
    ........    .......*    .......*    ........    ........    .....*..
    ........    ...*...*    ........    ........    ........    .....*..

      0x06        0x07
      check        no
    ........    ...*..*.    ........    ........    ........    ........
    ........    ...**.*.    ........    ........    ........    ........
    ........    ...*.**.    ........    ........    ........    ........
    ........    ...*..*.    ........    ........    ........    ........
    .......*    .....**.    ........    ........    ........    ........
    ......*.    ....*..*    ........    ........    ........    ........
    ...*.*..    ....*..*    ........    ........    ........    ........
    ....*...    .....**.    ........    ........    ........    ........

*/

#define CG_LINES_IN_CHAR  (8)

#define CG_RECEIVE1       (1)
byte    cg_receive1[CG_LINES_IN_CHAR] = {
  0x00,
  0x00,
  0x00,
  0x10,
  0x04,
  0x02,
  0x01,
  0x11
};

#define CG_RECEIVE2       (2)
byte    cg_receive2[CG_LINES_IN_CHAR] = {
  0x00,
  0x00,
  0x10,
  0x04,
  0x02,
  0x01,
  0x01,
  0x00
};

#define CG_RECEIVE3       (3)
byte    cg_receive3[CG_LINES_IN_CHAR] = {

  0x00,
  0x10,
  0x04,
  0x02,
  0x01,
  0x01,
  0x00,
  0x00
};

#define CG_RECEIVE4       (4)
byte    cg_receive4[CG_LINES_IN_CHAR] = {
  0x10,
  0x04,
  0x02,
  0x01,
  0x01,
  0x00,
  0x00,
  0x00
};

#define NUM_RECEIVE_ICONS (CG_RECEIVE4)

#define CG_ANTENNA        (5)
byte    cg_antenna[CG_LINES_IN_CHAR] = {
  0x00,
  0x00,
  0x1F,
  0x15,
  0x0A,
  0x04,
  0x04,
  0x04
};

#define CG_CHECK          (6)
byte    cg_check[CG_LINES_IN_CHAR] = {
  0x00,
  0x00,
  0x00,
  0x00,
  0x01,
  0x02,
  0x14,
  0x08
};

#define CG_NO             (6)
byte    cg_no[CG_LINES_IN_CHAR] = {
  0x12,
  0x1A,
  0x16,
  0x12,
  0x06,
  0x09,
  0x09,
  0x06
};


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
boolean       TriggerReceiveMode = true;        // variable to trigger (turn on) receive mode
boolean       LastTriggerValue = false;         // Track false -> true trigger transition
boolean       ValidDecode = false;              // variable to rapidly know if the system had a valid decode done lately

ES100DateTime SavedDateTime;
ES100Status0  SavedStatus0;
ES100NextDst  SavedNextDst;

char  StringBuffer[MAX_STRING_SIZE+10];

char  ReturnValue[MAX_LCD_STRING_SIZE+10];


void
atomic() {
  // Called procedure when we receive an interrupt from the ES100
  // Got a interrupt and store the currect millis for future use if we have a valid decode
  AtomicMillis = millis();

  // Wrap interrupt count at 9999.
  if  (InterruptCount<9999)
    InterruptCount++;
  else
    InterruptCount=0;
}

// ******************

char *
getISODateStr() {
  Time TimeValue;


  TimeValue=rtc.getTime();
  snprintf(ReturnValue, MAX_LCD_STRING_SIZE, "%4.4d-%2.2d-%2.2d %2.2d:%2.2d:%2.2d",
          TimeValue.year, TimeValue.mon, TimeValue.date, TimeValue.hour, TimeValue.min, TimeValue.sec);

  return ReturnValue;
}

// ******************

char *
getDST() {
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

  //                                                   11111111112
  //                                          12345678901234567890
  //                                          DST NOT in Effect
  snprintf(ReturnValue, MAX_LCD_STRING_SIZE, "DST %s", DstThisMonth);

  return ReturnValue;
}

void
displayDST() {
  lcd.print(getDST());
}

// ******************

char *
getNDST () {

  //                                                   11111111112
  //                                          12345678901234567890
  //                                          DSTChg mm-dd @ hh:00
  snprintf(ReturnValue, MAX_LCD_STRING_SIZE, "DSTChg %2.2d-%2.2d @ %2.2dh00",
            SavedNextDst.month, SavedNextDst.day, SavedNextDst.hour);

  return ReturnValue;
}

void
displayNDST() {
  lcd.print(getNDST());
}

// ******************

char *
getLeapSecond() {
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

  //                                                   11111111112
  //                                          12345678901234567890
  //                                          NoLeapSec this month
  snprintf(ReturnValue, MAX_LCD_STRING_SIZE, "%s this month", TypeThisMonth);

  return ReturnValue;
}

void
displayLeapSecond() {
  lcd.print(getLeapSecond());
}

// ******************

char *
getLastSync () {
  if (LastSyncMillis > 0) {
    int days =    (millis() - LastSyncMillis) / 86400000;
    int hours =   ((millis() - LastSyncMillis) % 86400000) / 3600000;
    int minutes = (((millis() - LastSyncMillis) % 86400000) % 3600000) / 60000;
    int seconds = ((((millis() - LastSyncMillis) % 86400000) % 3600000) % 60000) / 1000;

    //                                                   11111111112
    //                                          12345678901234567890
    //                                          LastSync DdHHhMMmSSs
    snprintf(ReturnValue, MAX_LCD_STRING_SIZE, "LastSync%2dd%2.2dh%2.2dm%2.2ds",
            days, hours, minutes, seconds);
  } else {
    //                                                   11111111112
    //                                          12345678901234567890
    //                                          LastSync no sync yet
    snprintf(ReturnValue, MAX_LCD_STRING_SIZE, "LastSync no sync yet");
  }

  return ReturnValue;
}

void
displayLastSync() {
  lcd.print(getLastSync());
}

// ******************

char *
getInterrupt () {
  //                                                   11111111112
  //                                          12345678901234567890
  //                                          IRQ count nnnnn
  snprintf(ReturnValue, MAX_LCD_STRING_SIZE, "IRQ Count %5d", InterruptCount);

  return ReturnValue;
}

void
displayInterrupt() {
  lcd.print(getInterrupt());
}

// ******************

char *
getAntenna () {
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

  //                                                   11111111112
  //                                          12345678901234567890
  //                                          Antenna ? used
  snprintf(ReturnValue, MAX_LCD_STRING_SIZE, "Antenna %s used", AntennaUsed);

  return ReturnValue;
}

void
displayAntenna() {
  lcd.print(getAntenna());
}

// ******************

void
clearLine(unsigned int n) {
  while (n-- > 0)
    lcd.print(" ");
}

// ******************

void
showlcd() {
  char        StringBuffer[MAX_LCD_STRING_SIZE+10];
  char        ReceiveIconChar;
  static int  ReceiveIconCounter;

  if  (InReceiveMode) {
    switch (ReceiveIconCounter) {
      case 0:
        ReceiveIconChar = CG_RECEIVE1;
        break;

      case 1:
        ReceiveIconChar = CG_RECEIVE2;
        break;

      case 2:
        ReceiveIconChar = CG_RECEIVE3;
        break;

      case 3:
        ReceiveIconChar = CG_RECEIVE4;
        break;

      case 4:
        ReceiveIconChar = ' ';
        break;

      default:
        ReceiveIconChar = '?';
        break;
    }
    ReceiveIconCounter++;
    if  (ReceiveIconCounter > NUM_RECEIVE_ICONS)
      ReceiveIconCounter = 0;
  } else {
    ReceiveIconChar = ' ';
    ReceiveIconCounter = 0;
  }

  lcd.setCursor(0,0);
  snprintf(StringBuffer, MAX_STRING_SIZE, "%s%c", getISODateStr(), ReceiveIconChar);
  lcd.print(StringBuffer);

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

// ******************

void
setup() {


  Wire.begin();
  Serial.begin(9600);
  Serial.println();
  Serial.println();
  Serial.println("-------------------------");

  snprintf(StringBuffer, MAX_STRING_SIZE, "Everset ES100 ADK [v%d.%d %s] startup", VERSION, ISSUE, ISSUE_DATE);
  Serial.println(StringBuffer);

  Serial.println("-------------------------");
  Serial.println();

  if  (CONTINUOUS_MODE) {
    //                                                11111111112222222222333333333344444444445555555555
    //                                       12345678901234567890123456789012345678901234567890123456789
    //                                       Continuous mode enabled, interval approx. nn hour(s).
    snprintf(StringBuffer, MAX_STRING_SIZE, "Continuous mode enabled, interval approx. %2d hour(s).", CONTINUOUS_RETRY_HOURS);
  } else {
    snprintf(StringBuffer, MAX_STRING_SIZE, "Continuous mode disabled.");
  }
  Serial.println(StringBuffer);
  Serial.println();

  lcd.createChar(CG_RECEIVE1, cg_receive1);
  lcd.createChar(CG_RECEIVE2, cg_receive2);
  lcd.createChar(CG_RECEIVE3, cg_receive3);
  lcd.createChar(CG_RECEIVE4, cg_receive4);
  lcd.createChar(CG_ANTENNA,  cg_antenna);
  lcd.createChar(CG_CHECK,    cg_check);
  lcd.createChar(CG_NO,       cg_no);
  lcd.begin(20, 4);
  lcd.clear();
  //                  11111111112
  //         12345678901234567890
  //          Everset ES100 ADK
  //          [vn.m yyyy-mm-dd]
  // If VERSION or ISSUE is 2 digits,
  // will spill a bit to the right, boo hoo.
  lcd.setCursor(0,1);
  lcd.print(" Everset ES100 ADK  ");

  lcd.setCursor(0,2);
  snprintf(StringBuffer, MAX_STRING_SIZE, " [v%d.%d %s] %c", VERSION, ISSUE, ISSUE_DATE, CG_ANTENNA);
  lcd.print(StringBuffer);

//  lcd.setCursor(0,3);
//  snprintf(StringBuffer, MAX_STRING_SIZE, " %c %c %c %c %c %c %c", 
//            CG_RECEIVE1, CG_RECEIVE2, CG_RECEIVE3, CG_RECEIVE4, CG_ANTENNA, CG_CHECK, CG_NO);
//  lcd.print(StringBuffer);

  delay(5000);

  lcd.begin(20, 4);
  lcd.clear();

  es100.begin(es100Int, es100En);

  rtc.begin();

  snprintf(StringBuffer, MAX_STRING_SIZE, "Starting at %s UTC.", getISODateStr());
  Serial.print(StringBuffer);

  attachInterrupt(digitalPinToInterrupt(es100Int), atomic, FALLING);
}

// ******************

void
loop() {
  Time  TimeValue;
  unsigned long NowMillis;
  uint8_t       IRQStatus;
  uint8_t       RxOk;
  uint8_t       Control0;


  if (!InReceiveMode && TriggerReceiveMode) {
    InterruptCount = 0;

    Serial.println();
    TimeValue=rtc.getTime();
    snprintf(StringBuffer, MAX_STRING_SIZE, "Activate receive mode at %s UTC.", getISODateStr());
    Serial.println(StringBuffer);

    es100.enable();
    es100.startRx();

    InReceiveMode = true;
    TriggerReceiveMode = false;
    LastTriggerValue = false;

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

    Control0   = es100.getControl0();
    IRQStatus = es100.getIRQStatus();
    RxOk = es100.getRxOk();

    snprintf(StringBuffer, MAX_STRING_SIZE, "Control0=0x%2.2X, IRQStatus=0x%2.2X, RxOk=0x%2.2X", Control0, IRQStatus, RxOk);
    Serial.println(StringBuffer);

    snprintf(StringBuffer, MAX_STRING_SIZE, "ES100 IRQ %5d: ", InterruptCount);
    // Add more on the end of that line.

    if (IRQStatus == 0x01 && RxOk == 0x01) {
      ValidDecode = true;

      strncat(StringBuffer, "has data at ", MAX_STRING_SIZE);
      strncat(StringBuffer, getISODateStr(), MAX_STRING_SIZE);
      strncat(StringBuffer, " UTC.", MAX_STRING_SIZE);
      Serial.println(StringBuffer);

      // We received a valid decode
      SavedDateTime = es100.getDateTime();
      // Updating the RTC
      rtc.setDate(SavedDateTime.day,  SavedDateTime.month,  2000+SavedDateTime.year);
      rtc.setTime(SavedDateTime.hour, SavedDateTime.minute, SavedDateTime.second + ((millis() - AtomicMillis)/1000));

      // Get everything before disabling the chip.
      SavedStatus0 = es100.getStatus0();
      SavedNextDst = es100.getNextDst();

/* DEBUG */
      snprintf(StringBuffer, MAX_STRING_SIZE, "status: rxOk     0x%2.2X, antenna  0x%2.2X, leapSecond 0x%2.2X",
                SavedStatus0.rxOk, SavedStatus0.antenna, SavedStatus0.leapSecond, SavedStatus0.dstState, SavedStatus0.tracking);
      Serial.println(StringBuffer);

      snprintf(StringBuffer, MAX_STRING_SIZE, "        dstState 0x%2.2X, tracking 0x%2.2X",
                SavedStatus0.rxOk, SavedStatus0.antenna, SavedStatus0.leapSecond, SavedStatus0.dstState, SavedStatus0.tracking);
      Serial.println(StringBuffer);
/* END DEBUG */

      Serial.println();
      Serial.println(getDST());
      Serial.println(getNDST());
      Serial.println(getLeapSecond());
      Serial.println(getLastSync());
      Serial.println(getInterrupt());
      Serial.println(getAntenna());
      Serial.println();

      // Update LastSyncMillis for lcd display
      LastSyncMillis = millis();

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

  NowMillis = millis();
  if (LastMillis + 500 < millis()) {
    showlcd();

    // set the trigger to start reception at midnight (UTC-4) if we are not in continuous mode.
    // 4am UTC is midnight for me, adjust to your need
    TimeValue=rtc.getTime();
    TriggerReceiveMode = (!CONTINUOUS_MODE && (!InReceiveMode && TimeValue.hour == 4 && TimeValue.min == 0)) ||
                          (CONTINUOUS_MODE && (!InReceiveMode && (LastSyncMillis+CONTINUOUS_RETRY_MILLIS) < NowMillis));
    if  (!LastTriggerValue && TriggerReceiveMode) {
      snprintf(StringBuffer, MAX_STRING_SIZE, "Triggering receive mode at %s UTC.", getISODateStr());
      Serial.println(StringBuffer);
      Serial.println();
    }
    LastTriggerValue = TriggerReceiveMode;

    LastMillis = NowMillis;
  }
}
