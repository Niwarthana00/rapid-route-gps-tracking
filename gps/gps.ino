#include <TinyGPS++.h>
#include <BluetoothSerial.h>

static constexpr uint32_t GPS_BAUD          = 38400;
static constexpr uint32_t SERIAL_MON_BAUD   = 115200;
static constexpr uint8_t  RXD2              = 16;
static constexpr uint8_t  TXD2              = 17;
static constexpr uint32_t DISPLAY_INTERVAL  = 2000UL;
static constexpr uint32_t GPS_TIMEOUT       = 5000UL;
static constexpr uint32_t GPS_MIN_CHARS     = 10;
static constexpr int      SL_OFFSET_HOURS   = 5;
static constexpr int      SL_OFFSET_MINUTES = 30;
#define BT_NAME "FleetTracker"

static TinyGPSPlus     gps;
static BluetoothSerial btSerial;

static unsigned long lastDisplayTime = 0;
static unsigned long gpsStartTime    = 0;
static bool          errorShown      = false;
static bool          errorCleared    = false;

struct DateTime {
  int year, month, day, hour, minute, second;
};

static inline bool btActive() { return btSerial.hasClient(); }

static void emit(const __FlashStringHelper* s) {
  Serial.print(s);
  if (btActive()) btSerial.print(s);
}
static void emitLn(const __FlashStringHelper* s) {
  Serial.println(s);
  if (btActive()) btSerial.println(s);
}
static void emit(int val) {
  Serial.print(val);
  if (btActive()) btSerial.print(val);
}
static void emitLn(int val) {
  Serial.println(val);
  if (btActive()) btSerial.println(val);
}
static void emit(uint32_t val) {
  Serial.print(val);
  if (btActive()) btSerial.print(val);
}
static void emitLn(uint32_t val) {
  Serial.println(val);
  if (btActive()) btSerial.println(val);
}
static void emit(double val, int decimals) {
  Serial.print(val, decimals);
  if (btActive()) btSerial.print(val, decimals);
}
static void emitLn(double val, int decimals) {
  Serial.println(val, decimals);
  if (btActive()) btSerial.println(val, decimals);
}
static void emitPadded(int val) {
  if (val < 10) emit(F("0"));
  emit(val);
}

static bool isLeapYear(int year) {
  return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

static int daysInMonth(int month, int year) {
  static const int TABLE[13] = { 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
  if (month == 2 && isLeapYear(year)) return 29;
  return TABLE[month];
}

static DateTime toSriLankaTime(int year, int month, int day,
                                int hour, int minute, int second) {
  DateTime dt = { year, month, day, hour, minute, second };

  dt.minute += SL_OFFSET_MINUTES;
  if (dt.minute >= 60) { dt.minute -= 60; dt.hour++; }

  dt.hour += SL_OFFSET_HOURS;
  if (dt.hour >= 24) {
    dt.hour -= 24;
    dt.day++;
    if (dt.day > daysInMonth(dt.month, dt.year)) {
      dt.day = 1;
      dt.month++;
      if (dt.month > 12) { dt.month = 1; dt.year++; }
    }
  }
  return dt;
}

static const __FlashStringHelper* hdopLabel(double h) {
  if (h <= 1.0) return F(" (Excellent)");
  if (h <= 2.0) return F(" (Good)");
  if (h <= 5.0) return F(" (Moderate)");
  return F(" (Poor)");
}

static void printLocation() {
  if (!gps.location.isValid()) {
    emitLn(F("LOCATION  : Searching for Satellites..."));
    return;
  }
  double lat = gps.location.lat();
  double lng = gps.location.lng();

  emit(F("LAT/LNG   : ")); emit(lat, 6); emit(F(" , ")); emitLn(lng, 6);
  emit(F("GOOGLE MAP: https://www.google.com/maps?q="));
  emit(lat, 6); emit(F(",")); emitLn(lng, 6);

  if (gps.hdop.isValid()) {
    double h = gps.hdop.hdop();
    emit(F("ACCURACY  : ")); emit(h, 2); emitLn(hdopLabel(h));
  }
}

static void printAltitude() {
  emit(F("ALTITUDE  : "));
  if (gps.altitude.isValid()) { emit(gps.altitude.meters(), 1); emitLn(F(" m")); }
  else                          emitLn(F("INVALID"));
}

static void printSpeed() {
  emit(F("SPEED     : "));
  if (gps.speed.isValid()) { emit(gps.speed.kmph(), 1); emitLn(F(" km/h")); }
  else                       emitLn(F("INVALID"));
}

static void printDateTime() {
  if (!gps.date.isValid() || !gps.time.isValid()) {
    emitLn(F("DATE/TIME : INVALID"));
    return;
  }
  DateTime sl = toSriLankaTime(
    gps.date.year(),   gps.date.month(),   gps.date.day(),
    gps.time.hour(),   gps.time.minute(),  gps.time.second()
  );
  emit(F("SL TIME   : "));
  emitPadded(sl.hour);   emit(F(":"));
  emitPadded(sl.minute); emit(F(":"));
  emitPadded(sl.second);
  emit(F("  DATE: "));
  emitPadded(sl.day);   emit(F("/"));
  emitPadded(sl.month); emit(F("/"));
  emitLn(sl.year);
}

static void printSatellites() {
  emit(F("SATELLITES: "));
  if (gps.satellites.isValid()) emitLn(gps.satellites.value());
  else                           emitLn(F("INVALID"));
}

static void processGpsData() {
  emitLn(F("\n--- Live Fleet Update ---"));
  printLocation();
  printAltitude();
  printSpeed();
  printDateTime();
  printSatellites();
  emitLn(F("-------------------------"));
}

static void handleGpsError() {
  bool timedOut = (millis() - gpsStartTime) > GPS_TIMEOUT
               && gps.charsProcessed() < GPS_MIN_CHARS;

  if (timedOut && !errorShown) {
    emitLn(F("[!] ERROR: No GPS data. Check TX2/RX2 wiring!"));
    errorShown   = true;
    errorCleared = false;
  } else if (!timedOut && errorShown && !errorCleared) {
    emitLn(F("[OK] GPS signal recovered."));
    errorCleared = true;
  }
}

void setup() {
  Serial.begin(SERIAL_MON_BAUD);
  Serial2.begin(GPS_BAUD, SERIAL_8N1, RXD2, TXD2);
  btSerial.begin(BT_NAME);

  lastDisplayTime = millis();
  gpsStartTime    = millis();
}

void loop() {
  while (Serial2.available() > 0)
    gps.encode(Serial2.read());

  while (btActive() && btSerial.available() > 0)
    btSerial.read();

  handleGpsError();

  if (millis() - lastDisplayTime >= DISPLAY_INTERVAL) {
    lastDisplayTime += DISPLAY_INTERVAL;
    processGpsData();
  }
}
