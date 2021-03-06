/*
This is the demonstration code for the UNIVERSAL-SOLDER / Everset ES100
Application Development Kit. It reads the decoded time stamp from
the ES100MOD receiver module and shows several information on a 4x20
character display. There are no function assignments for unused GPIO,
analog inputs and the 3 push buttons included in this sketch.

v0i11 of Dean Weiten revision 2022-01-25

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
#define ISSUE                   (11)
#define ISSUE_DATE              "2022-01-25"

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
#define MAX_STRING_SIZE         (41)
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
    ........    ........    ........    ...*....    .....*..    ...*****
    ........    ........    ...*....    .....*..    ......*.    ...*.*.*
    ........    ...*....    .....*..    ......*.    .......*    ....*.*.
    ........    .....*..    ......*.    .......*    .......*    .....*..
    ........    ......*.    .......*    .......*    ........    .....*..
    ........    .......*    .......*    ........    ........    .....*..
    ........    ...*...*    ........    ........    ........    ........



    ........    ........    ........    ........
    ........    ........    ........    ........
    ........    ........    ........    ........
    ........    ........    ........    ........
    ........    ........    ........    ........
    ........    ........    ........    ........
    ........    ........    ........    ........
    ........    ........    ........    ........

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
  0x1F,
  0x15,
  0x0A,
  0x04,
  0x04,
  0x04,
  0x00
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

  // Hold interrupt count at 10000.
  if  (InterruptCount<10000)
    InterruptCount++;
  else
    InterruptCount=0;
}

// ******************

char *
getISODateStr() {
  Time TimeValue;


  //                                                   11111111112
  //                                          12345678901234567890
  //                                          YYYY-MM-DD HH:MM:SS
  TimeValue=rtc.getTime();
  snprintf(ReturnValue, MAX_LCD_STRING_SIZE, "%4.4d-%2.2d-%2.2d %2.2d:%2.2d:%2.2d",
          TimeValue.year, TimeValue.mon, TimeValue.date, TimeValue.hour, TimeValue.min, TimeValue.sec);

  return ReturnValue;
}

// ******************
//          11111111112
// 12345678901234567890
// YYYY-MM-DD HH:MM:SSX   Line 1 - Current Date & Time, Receive Active Indicator
// Last DdHHhMMmSSs  X?   Line 2 - Time since Last Sync, Antenna symbol & number
// DSTstart Chgmm-dd@HH   Line 3 - DST status (start, end, yes, no), Date & Hour of next change
// LSPx        IRQnnnnn   Line 4 - Leap Second Pending (x=+/-/NO), IRQs
//
void
showlcd() {
  char        ReceiveIconChar;
  static int  ReceiveIconCounter;
  char        *DstThisMonth;
  char        *TypeThisMonth;
  char        AntennaUsed;


//          11111111112
// 12345678901234567890
// YYYY-MM-DD HH:MM:SSX   Line 1 - Current Date & Time, Receive Active Indicator
  lcd.setCursor(0,0);
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

  snprintf(StringBuffer, MAX_STRING_SIZE, "%s%c", getISODateStr(), ReceiveIconChar);
  lcd.print(StringBuffer);

  //          11111111112
  // 12345678901234567890
  // Last DdHHhMMmSSs  X?   Line 2 - Time since Last Sync, Antenna symbol & number
  lcd.setCursor(0,1);
  if (ValidDecode) {
    switch (SavedStatus0.antenna) {
      case 0:
        AntennaUsed = '1';
        break;
      case 1:
        AntennaUsed = '2';
        break;
      default:
        AntennaUsed = '?';
        break;
    }
  } else {
    AntennaUsed = '-';
  }

  if (LastSyncMillis > 0) {
    int days =    (millis() - LastSyncMillis) / 86400000;
    int hours =   ((millis() - LastSyncMillis) % 86400000) / 3600000;
    int minutes = (((millis() - LastSyncMillis) % 86400000) % 3600000) / 60000;
    int seconds = ((((millis() - LastSyncMillis) % 86400000) % 3600000) % 60000) / 1000;

    //                                                    11111111112
    //                                           12345678901234567890
    //                                           Last DdHHhMMmSSs  X?
    snprintf(StringBuffer, MAX_LCD_STRING_SIZE, "Last%2dd%2.2dh%2.2dm%2.2ds  %c%c",
            days, hours, minutes, seconds, CG_ANTENNA, AntennaUsed);
  } else {
    //                                                    11111111112
    //                                           12345678901234567890
    //                                           Last -d--h--m--s  X?
    snprintf(StringBuffer, MAX_LCD_STRING_SIZE, "Last -d--h--m--s  %c%c",
            CG_ANTENNA, AntennaUsed);
  }
  lcd.print(StringBuffer);

  //          11111111112
  // 12345678901234567890
  // DSTstartChgmm-dd@HHh   Line 3 - DST status (start, end, yes, no), Date & Hour of next change
  lcd.setCursor(0,2);
  if  (ValidDecode) {
    switch (SavedStatus0.dstState) {
      case B00:
        DstThisMonth = "no   ";
        break;
      case B10:
        DstThisMonth = "start";
        break;
      case B11:
        DstThisMonth = "yes  ";
        break;
      case B01:
        DstThisMonth = "end  ";
        break;
      default:
        DstThisMonth = "?    ";
        break;
    }
    snprintf(StringBuffer, MAX_LCD_STRING_SIZE, "DST%sChg%2.2d-%2.2d@%2.2dh",
              DstThisMonth, SavedNextDst.month, SavedNextDst.day, SavedNextDst.hour);
  } else {
    snprintf(StringBuffer, MAX_LCD_STRING_SIZE, "DST-----Chg-----@--h",
              DstThisMonth, SavedNextDst.month, SavedNextDst.day, SavedNextDst.hour);
  }
  lcd.print(StringBuffer);

  //          11111111112
  // 12345678901234567890
  // LSPxx       IRQ>nnnn   Line 4 - Leap Second Pending (x=+/-/NO), Failed Rxs
  lcd.setCursor(0,3);
  if  (ValidDecode) {
    switch (SavedStatus0.leapSecond) {
      case B00:
        TypeThisMonth = "no";
        break;
      case B10:
        TypeThisMonth = "- ";
        break;
      case B11:
        TypeThisMonth = "+ ";
        break;
      default:
        TypeThisMonth = "?";
        break;
    }
  } else {
    TypeThisMonth = "--";
  }
  if (InterruptCount<10000) {
    snprintf(StringBuffer, MAX_LCD_STRING_SIZE, "LSP%s       IRQ %4d",
              TypeThisMonth, InterruptCount);
  } else {
    snprintf(StringBuffer, MAX_LCD_STRING_SIZE, "LSP%s       IRQ>9999",
              TypeThisMonth, InterruptCount);
  }
  lcd.print(StringBuffer);
}

// ******************

void
setup() {


  Wire.begin();
  Serial.begin(9600);
  Serial.println();
  Serial.println();
  Serial.println("-------------------------");

  //                                                1111111111222222222233333333334
  //                                       1234567890123456789012345678901234567890
  //                                       Everset ES100 ADK [vnn.mm YYYY-MM-DD]
  snprintf(StringBuffer, MAX_STRING_SIZE, "Everset ES100 ADK [v%d.%d %s]", VERSION, ISSUE, ISSUE_DATE);
  Serial.println(StringBuffer);

  Serial.println("-------------------------");
  Serial.println();

  if  (CONTINUOUS_MODE) {
    //                                                1111111111222222222233333333334
    //                                       1234567890123456789012345678901234567890
    //                                       Continuous enabled, intrvl nn hour(s).
    snprintf(StringBuffer, MAX_STRING_SIZE, "Continuous enabled, intrvl %2d hour(s).", CONTINUOUS_RETRY_HOURS);
  } else {
    snprintf(StringBuffer, MAX_STRING_SIZE, "Continuous disabled.");
  }
  Serial.println(StringBuffer);
  Serial.println();

  lcd.createChar(CG_RECEIVE1, cg_receive1);
  lcd.createChar(CG_RECEIVE2, cg_receive2);
  lcd.createChar(CG_RECEIVE3, cg_receive3);
  lcd.createChar(CG_RECEIVE4, cg_receive4);
  lcd.createChar(CG_ANTENNA,  cg_antenna);
  lcd.begin(20, 4);
  lcd.clear();
  //                  11111111112
  //         12345678901234567890
  //          Everset ES100 ADK
  //          [vn.m yyyy-mm-dd]
  // If VERSION or ISSUE is 2 digits,
  // will spill a bit to the right, boo hoo.
  lcd.setCursor(0,0);
  lcd.print(" Everset ES100 ADK");

  lcd.setCursor(0,1);
  snprintf(StringBuffer, MAX_STRING_SIZE, " [v%d.%d %s]", VERSION, ISSUE, ISSUE_DATE);
  lcd.print(StringBuffer);

  lcd.setCursor(0,3);
  snprintf(StringBuffer, MAX_STRING_SIZE, "     %c %c %c %c %c",
            CG_RECEIVE1, CG_RECEIVE2, CG_RECEIVE3, CG_RECEIVE4, CG_ANTENNA);
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

// ******************

void
loop() {
  Time  TimeValue;
  unsigned long NowMillis;
  uint8_t       IRQStatus;
  uint8_t       RxOk;
  // uint8_t       Control0;


  if (!InReceiveMode && TriggerReceiveMode) {
    InterruptCount = 0;

    Serial.println();
    TimeValue=rtc.getTime();
    //                                                1111111111222222222233333333334
    //                                       1234567890123456789012345678901234567890
    //                                       Start receive @ YYYY-MM-DD HH:MM:SS UTC
    snprintf(StringBuffer, MAX_STRING_SIZE, "Start receive @ %s UTC.", getISODateStr());
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

    // Control0   = es100.getControl0();
    IRQStatus = es100.getIRQStatus();
    RxOk = es100.getRxOk();

    //                                                1111111111222222222233333333334
    //                                       1234567890123456789012345678901234567890
    //                                       Control0=0xHH IRQStatus=0xHH RxOk=0xHH
    // snprintf(StringBuffer, MAX_STRING_SIZE, "Control0=0x%2.2X IRQStatus=0x%2.2X RxOk=0x%2.2X", Control0, IRQStatus, RxOk);
    //                                       IRQStatus=0xHH RxOk=0xHH
    snprintf(StringBuffer, MAX_STRING_SIZE, "IRQStatus=0x%2.2X RxOk=0x%2.2X", IRQStatus, RxOk);
    Serial.println(StringBuffer);

    //                                                1111111111222222222233333333334
    //                                       1234567890123456789012345678901234567890
    //                                       IRQ nnnnn: data@YYYY-MM-DD HH:MM:SS UTC.
    //                                       IRQ nnnnn: none@YYYY-MM-DD HH:MM:SS UTC.
    snprintf(StringBuffer, MAX_STRING_SIZE, "IRQ %5d: ", InterruptCount);
    // Add more on the end of that line.

    if (IRQStatus == 0x01 && RxOk == 0x01) {
      ValidDecode = true;

      strncat(StringBuffer, "data@", MAX_STRING_SIZE);
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
      //                                                1111111111222222222233333333334
      //                                       1234567890123456789012345678901234567890
      //                                       status: rxOk=0xHH ant=0xHH leap=0xHH
      snprintf(StringBuffer, MAX_STRING_SIZE, "status: rxOk=0x%2.2X ant=0x%2.2X leap=0x%2.2X",
                SavedStatus0.rxOk, SavedStatus0.antenna, SavedStatus0.leapSecond, SavedStatus0.dstState, SavedStatus0.tracking);
      Serial.println(StringBuffer);

      //                                                1111111111222222222233333333334
      //                                       1234567890123456789012345678901234567890
      //                                               dstState=0xHH track=0xHH
      snprintf(StringBuffer, MAX_STRING_SIZE, "        dstState=0x%2.2X, track=0x%2.2X",
                SavedStatus0.rxOk, SavedStatus0.antenna, SavedStatus0.leapSecond, SavedStatus0.dstState, SavedStatus0.tracking);
      Serial.println(StringBuffer);
/* END DEBUG */

      // Update LastSyncMillis for lcd display
      LastSyncMillis = millis();

      es100.stopRx();
      es100.disable();
      InReceiveMode = false;
    }
    else {
      strncat(StringBuffer, "none@", MAX_STRING_SIZE);
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
      //                                                1111111111222222222233333333334
      //                                       1234567890123456789012345678901234567890
      //                                       Trigger receive@YYYY-MM-DD HH:MM:SS UTC.
      snprintf(StringBuffer, MAX_STRING_SIZE, "Trigger receive@%s UTC.", getISODateStr());
      Serial.println(StringBuffer);
      Serial.println();
    }
    LastTriggerValue = TriggerReceiveMode;

    LastMillis = NowMillis;
  }
}
