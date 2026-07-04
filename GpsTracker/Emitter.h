#pragma once

#include <BluetoothSerial.h>

class Emitter {
public:
  Emitter(BluetoothSerial& bt) : _bt(bt) {}

  void operator()(const __FlashStringHelper* s) {
    Serial.print(s);
    if (_bt.hasClient()) _bt.print(s);
  }
  void operator()(const char* s) {
    Serial.print(s);
    if (_bt.hasClient()) _bt.print(s);
  }

  void operator()(int v) {
    Serial.print(v);
    if (_bt.hasClient()) _bt.print(v);
  }
  void operator()(unsigned int v) {
    Serial.print(v);
    if (_bt.hasClient()) _bt.print(v);
  }
  void operator()(uint32_t v) {
    Serial.print(v);
    if (_bt.hasClient()) _bt.print(v);
  }
  void operator()(double v, int dec = 2) {
    Serial.print(v, dec);
    if (_bt.hasClient()) _bt.print(v, dec);
  }

  bool btConnected() const { return _bt.hasClient(); }

private:
  BluetoothSerial& _bt;
};
