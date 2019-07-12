/*
 * Copyright (c) 2018 nitacku
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * @file        B7971-Nixie-Clock.h
 * @summary     Digital Clock for B7971 Nixie display tubes
 * @version     3.2
 * @author      nitacku
 * @data        14 August 2018
 */

#ifndef _B7971CLOCK_H
#define _B7971CLOCK_H
 
#include <avr/pgmspace.h>
#include <avr/eeprom.h>
#include <avr/wdt.h>
#include <nCoder.h>
#include <DS323x.h>
#include <nDisplay.h>
#include <nAudio.h>
#include <nI2C.h>
#include "Music.h"

#ifdef USE_FASTLED
#define FASTLED_INTERNAL
#include <FastLED.h>
#endif

const uint8_t VERSION       = 5;
const uint8_t DISPLAY_COUNT = 6;
const char CONFIG_KEY       = '$';
const uint8_t ALARM_COUNT   = 3;

// Macros to simplify port manipulation without additional overhead
#define getPort(pin)    ((pin < 8) ? PORTD : ((pin < A0) ? PORTB : PORTC))
#define getMask(pin)    _BV((pin < 8) ? pin : ((pin < A0) ? pin - 8 : pin - A0))
#define setPinHigh(pin) (getPort(pin) |= getMask(pin))
#define setPinLow(pin)  (getPort(pin) &= ~getMask(pin))

/* === Digit Representation ===

 07 07 07  07 07 07
 12 13   00   03 04
 12  13  00  03  04
 12   13 00 03   04
 12      00      04
 10 10 10  06 06 06
 11      09      05
 11   14 09 02   05
 11  14  09  02  05
 11 14   09   02 05
 08 08 08  08 08 08

 01 01 01  01 01 01

 Where bit == (1 << #)
=============================*/

static const uint16_t BITMAP[] PROGMEM =
{
    0x0001, 0x0008, 0x0040, 0x0004, 0x0200, 0x4000, 0x0400, 0x2000, // spin
    0x0000, 0x0203, 0x0011, 0x06F1, 0x17E1, 0x746C, 0x2DAC, 0x0008, //  !"#$%&'
    0x000C, 0x6000, 0x664D, 0x0641, 0x4000, 0x0440, 0x0800, 0x4008, // ()*+,-./
    0x59B8, 0x0038, 0x41D0, 0x01E8, 0x2070, 0x1584, 0x1D84, 0x4088, // 01234567
    0x55E8, 0x20F0, 0x0600, 0x0404, 0x0048, 0x04C0, 0x2400, 0x02D2, // 89:;<=>?
    0x18F4, 0x4078, 0x1DE8, 0x1980, 0x3904, 0x1D80, 0x1C80, 0x1984, // @ABCDEFG
    0x1C70, 0x0381, 0x41B0, 0x1C0C, 0x1900, 0x3838, 0x3834, 0x19B0, // HIJKLMNO
    0x1CD0, 0x19B4, 0x1C8C, 0x21E0, 0x0281, 0x1930, 0x5808, 0x5834, // PQRSTUVW
    0x600C, 0x2208, 0x4188, 0x6180, 0x2004, 0x018C, 0x4004, 0x0002, // XYZ[\]^_
    0x3400, 0x4560, 0x1D04, 0x4140, 0x4170, 0x4D00, 0x26C0, 0x21F0, // `abcdefg
    0x1C04, 0x0200, 0x0160, 0x020D, 0x0005, 0x0E60, 0x0C04, 0x0D04, // hijklmno
    0x1C88, 0x5588, 0x4040, 0x0144, 0x1D00, 0x4120, 0x4800, 0x0B20, // pqrstuvw
    0x600C, 0x2170, 0x4500, 0x6580, 0x0201, 0x01CC, 0x0650, 0xFFFF, // xyz{|}~█
};

static const char word_00[] PROGMEM = "Nixie ";
static const char word_01[] PROGMEM = " Neon ";
static const char word_02[] PROGMEM = "Orange";
static const char word_03[] PROGMEM = "Photon";
static const char word_04[] PROGMEM = "Helium";
static const char word_05[] PROGMEM = "Sodium";
static const char word_06[] PROGMEM = "Oxygen";
static const char word_07[] PROGMEM = "Plasma";
static const char word_08[] PROGMEM = "Vacuum";
static const char word_09[] PROGMEM = "Quartz";

// Item entries must use order above
static PGM_P const word_array[] PROGMEM =
{
    word_00,
    word_01,
    word_02,
    word_03,
    word_04,
    word_05,
    word_06,
    word_07,
    word_08,
    word_09,
};

static const char sentence_00[] PROGMEM = "High-Voltage Ionization    ";
static const char sentence_01[] PROGMEM = "Don't resist the electrons ";
static const char sentence_02[] PROGMEM = "Plasma is a state of matter";
static const char sentence_03[] PROGMEM = "Element atomic number 10   ";
static const char sentence_04[] PROGMEM = "Fusion of Helium and Oxygen";
static const char sentence_05[] PROGMEM = "PhotonicFusion Nixie Clock ";

// Item entries must use order above
static PGM_P const sentence_array[] PROGMEM =
{
    sentence_00,
    sentence_01,
    sentence_02,
    sentence_03,
    sentence_04,
    sentence_05,
};

enum digital_pin_t : uint8_t
{
    DIGITAL_PIN_ENCODER_0 = 2,
    DIGITAL_PIN_ENCODER_1 = 3,
    DIGITAL_PIN_BUTTON = 4,
    DIGITAL_PIN_CLOCK = 9,
    DIGITAL_PIN_SDATA = 11,
    DIGITAL_PIN_LATCH = 12,
    DIGITAL_PIN_BLANK = 10,
    DIGITAL_PIN_HV_ENABLE = 8,
    DIGITAL_PIN_TRANSDUCER_0 = 5,
    DIGITAL_PIN_TRANSDUCER_1 = 6,
    DIGITAL_PIN_TRANSDUCER_2 = 7,
};

enum analog_pin_t : uint8_t
{
    ANALOG_PIN_PHOTODIODE = A3,
    ANALOG_PIN_BATTERY = A2,
};

enum battery_t : uint16_t
{
    BATTERY_MIN = 2400,
    BATTERY_MAX = 3100,
};

enum interrupt_speed_t : uint8_t
{
    INTERRUPT_FAST = 32, // 16MHz / (60Hz * 8 levels * 1024 prescaler)
    INTERRUPT_SLOW = 255,
};

enum class FormatDate : uint8_t
{
    YYMMDD,
    MMDDYY,
    DDMMYY,
};

enum class FormatTime : uint8_t
{
    H24,
    H12,
};

enum class Cycle : uint8_t
{
    AM,
    PM,
};

enum class RTCSelect : uint8_t
{
    TIME,
    DATE,
};

enum class Effect : uint8_t
{
    NONE,
    SPIRAL,
    DATE,
    PHRASE,
};

enum class State : bool
{
    DISABLE,
    ENABLE,
};

struct StateStruct
{
    StateStruct()
    : voltage(State::DISABLE)
    , display(State::DISABLE)
    , alarm(State::DISABLE)
    {
        // empty
    }

    State voltage;
    State display;
    State alarm;
};

struct AlarmStruct
{
    AlarmStruct()
    : state(State::DISABLE)
    , music(0)
    , days(0)
    , time(0)
    {
        // empty
    }
    
    State       state;
    uint8_t     music;
    uint8_t     days;
    uint32_t    time;
};

struct Config
{
    Config()
    : validate(CONFIG_KEY)
    , noise(State::ENABLE)
    , battery(State::ENABLE)
    , alarm_state(0x00)
    , brightness(CDisplay::Brightness::AUTO)
    , gain(10)
    , offset(10)
    , date_format(FormatDate::DDMMYY)
    , time_format(FormatTime::H24)
    , temperature_unit(CRTC::Unit::F)
    , effect(Effect::NONE)
    , blank_begin(0)
    , blank_end(0)
    , music_timer(0)
    , alarm() // Default initialization
    {
        memcpy_P(phrase, PSTR("Photon"), DISPLAY_COUNT + 1);
    }

    char                    validate;
    State                   noise;
    State                   battery;
    State                   detonate;
    uint8_t                 alarm_state;
    CDisplay::Brightness    brightness;
    uint8_t                 gain;
    uint8_t                 offset;
    FormatDate              date_format;
    FormatTime              time_format;
    CRTC::Unit              temperature_unit;
    Effect                  effect;
    uint32_t                blank_begin;
    uint32_t                blank_end;
    uint8_t                 music_alarm;
    uint8_t                 music_timer;
    AlarmStruct             alarm[ALARM_COUNT];
    char                    phrase[DISPLAY_COUNT + 1];
};

// Return integral value of Enumeration
template<typename T> constexpr uint8_t getValue(const T e)
{
    return static_cast<uint8_t>(e);
}

//---------------------------------------------------------------------
// Implicit Function Prototypes
//---------------------------------------------------------------------

// delay            B7971
// digitalWrite     Wire
// analogRead       B7971
// pinMode          B7971
// attachInterrupt  CEncoder
// strlen           CDisplay
// strlen_P         CDisplay
// strncpy          CDisplay
// strcpy_P         CDisplay
// snprintf_P       B7971
// memcpy           B7971
// memset           B7971
// memcpy_P         B7971

//---------------------------------------------------------------------
// Function Prototypes
//---------------------------------------------------------------------

// Mode functions
void Timer(const uint8_t hour, const uint8_t minute, const uint8_t second);
void Detonate(void);
void PlayAlarm(const uint8_t song_index, const char* phrase);

// Automatic functions
void AutoBrightness(void);
void AutoBlanking(void);
void AutoAlarm(void);

// Update functions
void UpdateAlarmIndicator(void);

// Format functions
uint8_t FormatHour(const uint8_t hour);
void FormatRTCString(const CRTC::RTC& rtc, char* s, const RTCSelect type);
uint32_t GetSeconds(const uint8_t hour, const uint8_t minute, const uint8_t second);

// Analog functions
CDisplay::Brightness ReadLightIntensity(void);
uint32_t ReadBatteryMillivolts(void);

// EEPROM functions
void GetConfig(Config& g_config);
void SetConfig(const Config& g_config);

// State functions
void VoltageState(const State state);
void DisplayState(const State state);
bool GetBatteryState(void);

// Interrupt functions
void InterruptSpeed(const uint8_t speed);

// Callback functions
void EncoderCallback(void);
bool IsInputIncrement(void);
bool IsInputSelect(void);
bool IsInputUpdate(void);

#endif
