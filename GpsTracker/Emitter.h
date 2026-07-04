#pragma once

class Emitter {
public:
  void operator()(const __FlashStringHelper* s) { Serial.print(s); }
  void operator()(const char* s)                { Serial.print(s); }
  void operator()(int v)                        { Serial.print(v); }
  void operator()(unsigned int v)               { Serial.print(v); }
  void operator()(uint32_t v)                   { Serial.print(v); }
  void operator()(double v, int dec = 2)        { Serial.print(v, dec); }
};
