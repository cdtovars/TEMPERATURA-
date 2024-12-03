#ifndef TM1637DISPLAY_H
#define TM1637DISPLAY_H

#include "mbed.h"

class TM1637Display {
public:
    TM1637Display(uint8_t D2, uint8_t D3);
    void init();
    void showNumberDec(int num, bool leading_zero = false, uint8_t length = 4, uint8_t pos = 0);
    void setBrightness(uint8_t brightness);
    void clear();

private:
    DigitalOut m_pinClk;
    DigitalOut m_pinDIO;
    uint8_t m_brightness;
    void showNumberDecEx(int num, uint8_t dots, bool leading_zero, uint8_t length, uint8_t pos);
    void start();
    void stop();
    bool writeByte(uint8_t b);
    void bitDelay();
    uint8_t digitToSegment(int num);
};

#endif
