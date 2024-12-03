#include "TM1637Display.h"

TM1637Display::TM1637Display(uint8_t pinClk, uint8_t pinDIO) 
    : m_pinClk(D2), m_pinDIO(D3), m_brightness(0x0f) {
    // Inicialización de pines
}

void TM1637Display::init() {
    clear();
    setBrightness(0x0f);
}

void TM1637Display::setBrightness(uint8_t brightness) {
    m_brightness = brightness;
}

void TM1637Display::clear() {
    for (int i = 0; i < 4; i++) {
        writeByte(0x00);  // Envía segmentos vacíos para limpiar
    }
}

void TM1637Display::showNumberDec(int num, bool leading_zero, uint8_t length, uint8_t pos) {
    showNumberDecEx(num, 0, leading_zero, length, pos);
}

void TM1637Display::showNumberDecEx(int num, uint8_t dots, bool leading_zero, uint8_t length, uint8_t pos) {
    uint8_t digits[4];
    for (int i = 0; i < length; i++) {
        digits[length - i - 1] = digitToSegment(num % 10);
        num /= 10;
    }

    // Aquí debes enviar los datos al display a través del protocolo
    for (int i = 0; i < length; i++) {
        writeByte(digits[i]);
    }
}

void TM1637Display::start() {
    m_pinDIO = 0;
    bitDelay();
}

void TM1637Display::stop() {
    m_pinDIO = 0;
    m_pinClk = 1;
    bitDelay();
    m_pinDIO = 1;
}

bool TM1637Display::writeByte(uint8_t b) {
    for (int i = 0; i < 8; i++) {
        m_pinClk = 0;
        m_pinDIO = (b & 0x01);
        bitDelay();
        m_pinClk = 1;
        bitDelay();
        b >>= 1;
    }

    // Lectura de confirmación del dispositivo
    m_pinClk = 0;
    m_pinDIO = 1;
    bitDelay();
    m_pinClk = 1;
    return true;
}

void TM1637Display::bitDelay() {
    wait_us(50);  // Ajustar el delay a las necesidades del protocolo
}

uint8_t TM1637Display::digitToSegment(int num) {
    // Mapeo de números a segmentos (0-9)
    static const uint8_t digitToSegmentMap[] = {
        0x3F,  // 0
        0x06,  // 1
        0x5B,  // 2
        0x4F,  // 3
        0x66,  // 4
        0x6D,  // 5
        0x7D,  // 6
        0x07,  // 7
        0x7F,  // 8
        0x6F   // 9
    };
    return digitToSegmentMap[num];
}
