#pragma once

#include <TinyGPS++.h>
#include "ConfigManager.h"

namespace GpsUtil {
  template<typename Emitter>
  static void padded(Emitter& em, int v) {
    if (v < 10) em(F("0"));
    em(v);
  }

  static bool isLeapYear(int y) {
    return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
  }
  static int daysInMonth(int m, int y) {
    static const int T[13] = { 0,31,28,31,30,31,30,31,31,30,31,30,31 };
    return (m == 2 && isLeapYear(y)) ? 29 : T[m];
  }
}

struct DateTime { int year, month, day, hour, minute, second; };

struct GpsState {
  TinyGPSPlus      gps;
  unsigned long    lastDisplayMs  = 0;
  unsigned long    startMs        = 0;
  bool             errorShown     = false;
  bool             errorCleared   = false;
};

class GpsManager {
public:
  static void begin(GpsState& s, HardwareSerial& gpsSerial,
                    const GpsCfg& cfg)
  {
    gpsSerial.begin(cfg.baud, SERIAL_8N1, cfg.rxd, cfg.txd);
    s.startMs       = millis();
    s.lastDisplayMs = millis();
  }

  static uint32_t drain(GpsState& s, HardwareSerial& gpsSerial) {
    uint32_t n = 0;
    while (gpsSerial.available() > 0) {
      s.gps.encode(static_cast<char>(gpsSerial.read()));
      ++n;
    }
    return n;
  }

  template<typename Emitter>
  static void checkError(GpsState& s, const GpsCfg& cfg, Emitter& em) {
    bool timedOut = (millis() - s.startMs) > cfg.timeout_ms
                 && s.gps.charsProcessed() < cfg.min_chars;

    if (timedOut && !s.errorShown) {
      em(F("[!] ERROR: No GPS data. Check TX2/RX2 wiring!\n"));
      s.errorShown   = true;
      s.errorCleared = false;
    } else if (!timedOut && s.errorShown && !s.errorCleared) {
      em(F("[OK] GPS signal recovered.\n"));
      s.errorCleared = true;
    }
  }

  template<typename Emitter>
  static void tick(GpsState& s, const GpsCfg& cfg,
                   const TimezoneCfg& tz, Emitter& em)
  {
    if (millis() - s.lastDisplayMs < cfg.display_interval_ms) return;
    s.lastDisplayMs += cfg.display_interval_ms;
    printReport(s.gps, tz, em);
  }

private:
  static DateTime toLocalTime(int yr, int mo, int dy,
                               int hr, int mn, int sc,
                               const TimezoneCfg& tz)
  {
    DateTime dt = { yr, mo, dy, hr, mn, sc };
    dt.minute += tz.offset_minutes;
    if (dt.minute >= 60) { dt.minute -= 60; dt.hour++; }
    dt.hour += tz.offset_hours;
    if (dt.hour >= 24) {
      dt.hour -= 24;
      dt.day++;
      if (dt.day > GpsUtil::daysInMonth(dt.month, dt.year)) {
        dt.day = 1; dt.month++;
        if (dt.month > 12) { dt.month = 1; dt.year++; }
      }
    }
    return dt;
  }

  template<typename Emitter>
  static void printReport(TinyGPSPlus& gps, const TimezoneCfg& tz, Emitter& em) {
    em(F("\n--- Live Fleet Update ---\n"));
    printLocation(gps, em);
    printAltitude(gps, em);
    printSpeed(gps, em);
    printDateTime(gps, tz, em);
    printSatellites(gps, em);
    em(F("-------------------------\n"));
  }

  template<typename Emitter>
  static void printLocation(TinyGPSPlus& gps, Emitter& em) {
    if (!gps.location.isValid()) {
      em(F("LOCATION  : Searching for Satellites...\n"));
      return;
    }
    double lat = gps.location.lat();
    double lng = gps.location.lng();
    em(F("LAT/LNG   : ")); em(lat, 6); em(F(" , ")); em(lng, 6); em(F("\n"));
    em(F("GOOGLE MAP: https://www.google.com/maps?q="));
    em(lat, 6); em(F(",")); em(lng, 6); em(F("\n"));
    if (gps.hdop.isValid()) {
      double h = gps.hdop.hdop();
      em(F("ACCURACY  : ")); em(h, 2); em(hdopLabel(h)); em(F("\n"));
    }
  }

  template<typename Emitter>
  static void printAltitude(TinyGPSPlus& gps, Emitter& em) {
    em(F("ALTITUDE  : "));
    if (gps.altitude.isValid()) { em(gps.altitude.meters(), 1); em(F(" m\n")); }
    else                          em(F("INVALID\n"));
  }

  template<typename Emitter>
  static void printSpeed(TinyGPSPlus& gps, Emitter& em) {
    em(F("SPEED     : "));
    if (gps.speed.isValid()) { em(gps.speed.kmph(), 1); em(F(" km/h\n")); }
    else                       em(F("INVALID\n"));
  }

  template<typename Emitter>
  static void printDateTime(TinyGPSPlus& gps, const TimezoneCfg& tz, Emitter& em) {
    if (!gps.date.isValid() || !gps.time.isValid()) {
      em(F("DATE/TIME : INVALID\n")); return;
    }
    DateTime sl = toLocalTime(
      gps.date.year(),  gps.date.month(),  gps.date.day(),
      gps.time.hour(),  gps.time.minute(), gps.time.second(), tz);
    em(F("SL TIME   : "));
    GpsUtil::padded(em, sl.hour);   em(F(":"));
    GpsUtil::padded(em, sl.minute); em(F(":"));
    GpsUtil::padded(em, sl.second);
    em(F("  DATE: "));
    GpsUtil::padded(em, sl.day);   em(F("/"));
    GpsUtil::padded(em, sl.month); em(F("/"));
    em(sl.year); em(F("\n"));
  }

  template<typename Emitter>
  static void printSatellites(TinyGPSPlus& gps, Emitter& em) {
    em(F("SATELLITES: "));
    if (gps.satellites.isValid()) { em((int)gps.satellites.value()); em(F("\n")); }
    else                            em(F("INVALID\n"));
  }

  static const __FlashStringHelper* hdopLabel(double h) {
    if (h <= 1.0) return F(" (Excellent)");
    if (h <= 2.0) return F(" (Good)");
    if (h <= 5.0) return F(" (Moderate)");
    return F(" (Poor)");
  }
};
