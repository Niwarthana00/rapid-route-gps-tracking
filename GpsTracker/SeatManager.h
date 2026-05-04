#pragma once

#include <Arduino.h>
#include "ConfigManager.h"

struct SeatState {
  bool          occupied     = false;
  bool          lastReported = false;
  unsigned long lastPollMs   = 0;
  int           lastRaw      = 0;
};

class SeatManager {
public:
  static void begin(SeatState& s, const SeatCfg& cfg) {
    pinMode(cfg.pin, INPUT);
    s.lastPollMs = millis();
    Serial.printf("[SEAT] Pressure sensor ready on GPIO%d (threshold=%d)\n",
                  cfg.pin, cfg.threshold);
  }

  static void tick(SeatState& s, const SeatCfg& cfg) {
    if (millis() - s.lastPollMs < cfg.poll_interval_ms) return;
    s.lastPollMs += cfg.poll_interval_ms;

    s.lastRaw  = analogRead(cfg.pin);
    s.occupied = s.lastRaw > cfg.threshold;

    if (s.occupied != s.lastReported) {
      Serial.printf("[SEAT] %s (raw=%d)\n",
                    s.occupied ? "OCCUPIED" : "EMPTY", s.lastRaw);
      s.lastReported = s.occupied;
    }
  }
};