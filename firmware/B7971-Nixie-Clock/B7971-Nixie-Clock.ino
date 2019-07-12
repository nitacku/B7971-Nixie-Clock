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
 * @file        B7971-Nixie-Clock.ino
 * @summary     Digital Clock for B7971 Nixie display tubes
 * @version     3.2
 * @author      nitacku
 * @data        14 August 2018
 */

#include "B7971-Nixie-Clock.h"
#include "Menu.h"
 
//---------------------------------------------------------------------
// Global Variables
//---------------------------------------------------------------------

// Object variables
StateStruct     g_state;
Config          g_config;
CDS3232         g_rtc;
CAudio          g_audio{DIGITAL_PIN_TRANSDUCER_0, DIGITAL_PIN_TRANSDUCER_1, DIGITAL_PIN_TRANSDUCER_2};
CDisplay        g_display{DISPLAY_COUNT};
CNcoder         g_encoder{DIGITAL_PIN_BUTTON, CNcoder::ButtonMode::NORMAL, CNcoder::RotationMode::NORMAL};

// Container variables
CRTC::RTC*      g_rtc_struct;

// Integral variables
uint8_t         g_encoder_timeout = 0;
uint8_t         g_song_entries = INBUILT_SONG_COUNT;

//---------------------------------------------------------------------
// Functions
//---------------------------------------------------------------------

void loop(void)
{
    CRTC::RTC rtc; // struct
    uint8_t previous_second;
    char s[DISPLAY_COUNT + 1];
    
    g_rtc_struct = &rtc; // Assign global pointer
    
    GetConfig(g_config);

    if (g_config.validate != CONFIG_KEY)
    {
        Config new_config; // Use default constructor values
        SetConfig(new_config); // Write to EEPROM
        GetConfig(g_config); // Read from EEPROM
    }
    
    // Initialize Display
    g_display.SetCallbackIsIncrement(IsInputIncrement);
    g_display.SetCallbackIsSelect(IsInputSelect);
    g_display.SetCallbackIsUpdate(IsInputUpdate);
    g_display.SetDisplayBrightness(g_config.brightness);
    InterruptSpeed(INTERRUPT_FAST);
    delay(1); // Wait for interrupt to occur
    DisplayState(State::ENABLE); // Enable voltage after update
    
    // Initialize RTC
    g_rtc.Initialize();
    UpdateAlarmIndicator();
    
    // Initialize Encoder
    g_encoder.SetCallback(EncoderCallback); // Register callback function

    while (true)
    {
        AutoBrightness();
        previous_second = rtc.second;
        g_rtc.GetRTC(rtc);
        
        if (rtc.second != previous_second)
        {
            switch (rtc.second)
            {
            case 15:
            case 45:
            {
                uint8_t word_count = (sizeof(word_array) / sizeof(char*));
                uint8_t sentence_count = (sizeof(sentence_array) / sizeof(char*));
                uint8_t max = word_count + sentence_count;
                
                #ifdef USE_FASTLED
                    uint8_t index = random8(max); // FastLED implementation
                #else
                    uint8_t index = random(max); // Arduino implementation
                #endif
                
                if (index < word_count)
                {
                    // Copy item table to local variable
                    char* array[word_count];
                    memcpy_P(array, word_array, sizeof(array));
                    
                    g_display.SetDisplayValue(reinterpret_cast<const __FlashStringHelper*>(array[index]));
                    g_display.EffectSlotMachine(44);
                    delay(3000);
                }
                else
                {
                    // Copy item table to local variable
                    char* array[sentence_count];
                    memcpy_P(array, sentence_array, sizeof(array));
                    
                    g_display.EffectScroll(F("      "), CDisplay::Direction::LEFT, 150);
                    g_display.EffectScroll(reinterpret_cast<const __FlashStringHelper*>(array[index - word_count]), CDisplay::Direction::LEFT, 150);
                    g_display.EffectScroll(F("      "), CDisplay::Direction::LEFT, 150);
                }
                
                break;
            }
            case 0:
                AutoBlanking();
                AutoAlarm(); // Do after AutoBlanking - Alarm will enable display

                if (g_config.effect == Effect::SPIRAL)
                {
                    g_display.SetDisplayIndicator(false);
                    g_display.EffectScroll(F("\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F"
                                             "\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F"
                                             "\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F"
                                             "\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F"),
                                             CDisplay::Direction::LEFT, 54);
                    g_rtc.GetRTC(rtc); // Refresh RTC
                    rtc.second++;
                    FormatRTCString(rtc, s, RTCSelect::TIME);
                    g_display.EffectScroll(s, CDisplay::Direction::LEFT, 54);
                    break;
                }
                [[gnu::fallthrough]]; // Fall-through
            case 30:
                if (rtc.second && ((g_config.effect == Effect::DATE) || (g_config.effect == Effect::PHRASE)))
                {
                    if (g_config.effect == Effect::PHRASE)
                    {
                        // Display phrase
                        g_display.SetDisplayValue(g_config.phrase);
                    }
                    else
                    {
                        // Display date
                        FormatRTCString(rtc, s, RTCSelect::DATE);
                        g_display.SetDisplayValue(s);
                    }

                    g_display.SetDisplayIndicator(false);
                    g_display.EffectSlotMachine(20);
                    delay(3050);
                    break;
                }
                [[gnu::fallthrough]]; // Fall-through
            default:
                // Show indicators for AM/PM and alarm
                bool pip = (!rtc.am && (g_config.time_format == FormatTime::H12));
                g_display.SetUnitIndicator(0, getValue(g_state.alarm)); // Alarm
                g_display.SetUnitIndicator(5, pip); // AM/PM
                
                if ((g_config.battery == State::ENABLE) && !GetBatteryState() && (rtc.second % 2)) // Check battery voltage
                {
                    snprintf(s, DISPLAY_COUNT + 1, "Battry");
                }
                else
                {
                    FormatRTCString(rtc, s, RTCSelect::TIME);
                }
                
                g_display.SetDisplayValue(s);
                break;
            }
        }

        if (IsInputUpdate() || IsInputSelect())
        {
            // Check if time threshold elapsed
            if (g_encoder_timeout == 0)
            {
                // Check if display is disabled
                if (g_state.display == State::DISABLE)
                {
                    DisplayState(State::ENABLE);
                }
                else
                {
                    g_display.SetDisplayIndicator(false);
                    
                    // Check if button was pressed
                    if (IsInputSelect())
                    {                        
                        FormatRTCString(rtc, s, RTCSelect::DATE);
                        g_display.SetDisplayValue(s);
                        MenuInfo();
                    }
                    else
                    {
                        MenuSettings();
                    }

                    UpdateAlarmIndicator();
                }
            }
            
            g_encoder_timeout = 5;
        }

        if (g_encoder_timeout)
        {
            g_encoder_timeout--;
        }
        
        delay(50); // Idle
    }
}


void Timer(const uint8_t hour, const uint8_t minute, const uint8_t second)
{
    uint8_t previous_second;
    uint32_t countdown = (10000 * (uint32_t)hour)
                       + (100 * (uint32_t)minute)
                       + (uint32_t)second;

    CRTC::RTC rtc;
    g_rtc.GetRTC(rtc);
    previous_second = rtc.second;

    while (countdown)
    {
        if (previous_second != rtc.second)
        {
            if ((countdown % 100) == 0)
            {
                if ((countdown % 10000) == 0)
                {
                    countdown -= 4000; // Subtract hours
                }

                countdown -= 40; // Subtract minutes
            }

            countdown--; // Subtract seconds
            previous_second = rtc.second;
            g_display.SetDisplayValue(countdown);
        }

        delay(50);
        g_rtc.GetRTC(rtc);

        if (IsInputSelect())
        {
            return;
        }
    }

    PlayAlarm(g_config.music_timer, "Count!");
}


void Detonate(void)
{
    uint32_t countdown = 99999;
    uint16_t value = 2500;
    uint16_t timer = 0;
    uint8_t count = 0;

    g_encoder.SetCallback(nullptr); // Disable callback function
    g_display.SetDisplayValue(countdown);
    g_display.SetDisplayBrightness(CDisplay::Brightness::MAX);
    InterruptSpeed(INTERRUPT_SLOW);
    
    delay(500);
    g_audio.Play(CAudio::Functions::PGMStream, music_detonate_begin, music_detonate_begin, music_detonate_begin);
    delay(500);

    do
    {
        g_display.SetDisplayValue(countdown);

        if (++timer >= value)
        {
            count++;
            value -= 50;
            value += count / 2;
            timer = 0;
            countdown -= 150;
            g_audio.Play(CAudio::Functions::PGMStream, music_detonate_beep_A, music_detonate_beep_B, music_detonate_beep_C);
        }
    } while (--countdown > 150);

    g_display.SetDisplayValue(F("000000"));
    g_audio.Play(CAudio::Functions::PGMStream, music_detonate_fuse_A, music_detonate_fuse_A, music_detonate_fuse_A);
    delay(500);

    for (uint8_t i = 0; i < 3; i++)
    {
        g_audio.Play(CAudio::Functions::PGMStream, music_detonate_fuse_A, music_detonate_fuse_B, music_detonate_fuse_B);
        
        for (uint8_t j = 0; j < 25; j++)
        {
            for (uint8_t k = 0; k < DISPLAY_COUNT / 2; k++)
            {
                uint8_t digit = random(0, DISPLAY_COUNT);
                g_display.SetUnitValue(digit, random(128, 255));
            }
            
            delay(40);
            g_display.SetDisplayValue(F("ERROR#")); // Restore
        }
    }
    
    g_display.SetDisplayValue(F("\xFF\xFF\xFF\xFF\xFF\xFF")); // Connect all anodes
    g_audio.Play(CAudio::Functions::PGMStream, music_detonate_end_A, music_detonate_end_B, music_detonate_end_B);
    delay(1000);
    g_display.SetDisplayValue(F("      "));
    delay(3000);
    g_encoder.SetCallback(EncoderCallback); // Enable callback function
    InterruptSpeed(INTERRUPT_FAST);
    g_display.SetDisplayBrightness(g_config.brightness);
}


void PlayAlarm(const uint8_t song_index, const char* phrase)
{
    uint8_t elapsed_seconds = 0;
    bool toggle_state = false;
    bool audio_active = false;

    DisplayState(State::ENABLE);
    g_display.SetDisplayIndicator(false);
    g_display.SetDisplayBrightness(CDisplay::Brightness::MAX);
    InterruptSpeed(INTERRUPT_SLOW);
    
    // Alarm for at least 120 seconds until music ends or until user interrupt
    do
    {
        if (!audio_active)
        {
            PlayMusic(song_index);
        }
        
        if (g_rtc.GetTimeSeconds() & 0x1)
        {
            if (toggle_state == true)
            {
                toggle_state = false;
                elapsed_seconds++;
                g_display.SetDisplayValue(phrase);
            }
        }
        else
        {
            if (toggle_state == false)
            {
                toggle_state = true;
                elapsed_seconds++;
                g_display.SetDisplayValue(F("      "));
            }
        }

        delay(50);
        audio_active = g_audio.IsActive();
        
    } while (((elapsed_seconds < 120) || audio_active) &&
             !(IsInputUpdate() || IsInputSelect()));

    g_audio.Stop(); // Ensure music is stopped
    
    g_encoder_timeout = 5; // Prevent encoder interaction
    InterruptSpeed(INTERRUPT_FAST);
    g_display.SetDisplayBrightness(g_config.brightness);
}


void AutoBrightness(void)
{
    if (g_config.brightness == CDisplay::Brightness::AUTO)
    {
        CDisplay::Brightness brightness;
        brightness = ReadLightIntensity();
        g_display.SetDisplayBrightness(brightness);
    }
}


void AutoBlanking(void)
{
    if (g_config.blank_begin != g_config.blank_end)
    {
        uint32_t seconds = g_rtc.GetTimeSeconds();

        if (seconds == g_config.blank_end)
        {
            DisplayState(State::ENABLE);
        }
        else if (seconds == g_config.blank_begin)
        {
            DisplayState(State::DISABLE);
        }
    }
}


void AutoAlarm(void)
{
    uint32_t current_time = GetSeconds(g_rtc_struct->hour, g_rtc_struct->minute, 0);
    
    for (uint8_t index = 0; index < ALARM_COUNT; index++)
    {
        // Check if alarm is enabled
        if (g_config.alarm[index].state == State::ENABLE)
        {
            // Check if alarm day matches current day
            if ((g_config.alarm[index].days >> g_rtc_struct->week_day) & 0x1)
            {
                // Check if alarm time matches current time
                if (g_config.alarm[index].time == current_time)
                {
                    PlayAlarm(g_config.alarm[index].music, g_config.phrase);
                    break; // No need to process remaining alarms
                }
            }
        }
    }
    
    UpdateAlarmIndicator();
}


void UpdateAlarmIndicator(void)
{
    CRTC::RTC rtc;
    uint8_t day;
    State next_state = State::DISABLE; // Assume disabled
    
    g_rtc.GetRTC(rtc);
    uint32_t current_time = GetSeconds(rtc.hour, rtc.minute, 0);
    
    for (uint8_t index = 0; index < ALARM_COUNT; index++)
    {
        // Check if alarm is enabled
        if (g_config.alarm[index].state == State::ENABLE)
        {
            // Determine which day to check
            if (g_config.alarm[index].time > current_time)
            {
                day = rtc.week_day; // Today
            }
            else
            {
                day = (rtc.week_day > 6) ? 1 : (rtc.week_day + 1); // Tomorrow
            }

            // Check if alarm is active for selected day
            if ((g_config.alarm[index].days >> day) & 0x1)
            {
                next_state = State::ENABLE;
                break; // Alarm will be indicated - no need to continue
            }
        }
    }

    // Update alarm indicator
    g_state.alarm = static_cast<decltype(g_state.alarm)>(next_state);
}


uint8_t FormatHour(const uint8_t hour)
{
    if (g_config.time_format == FormatTime::H24)
    {
        return hour;
    }
    else
    {
        uint8_t conversion = hour % 12;
        conversion += (conversion == 0) ? 12 : 0;
        return conversion;
    }
}


void FormatRTCString(const CRTC::RTC& rtc, char* s, const RTCSelect type)
{
    const char* format_string = PSTR("%02u%02u%02u");

    switch (type)
    {
    case RTCSelect::TIME:
        snprintf_P(s, DISPLAY_COUNT + 1, format_string, FormatHour(rtc.hour), rtc.minute, rtc.second);
        break;

    case RTCSelect::DATE:
        switch (g_config.date_format)
        {
        default:
        case FormatDate::YYMMDD:
            snprintf_P(s, DISPLAY_COUNT + 1, format_string, rtc.year, rtc.month, rtc.day);
            break;

        case FormatDate::MMDDYY:
            snprintf_P(s, DISPLAY_COUNT + 1, format_string, rtc.month, rtc.day, rtc.year);
            break;

        case FormatDate::DDMMYY:
            snprintf_P(s, DISPLAY_COUNT + 1, format_string, rtc.day, rtc.month, rtc.year);
            break;
        }

        break;
    }
}


uint32_t GetSeconds(const uint8_t hour, const uint8_t minute, const uint8_t second)
{
    return ((3600 * (uint32_t)hour) + (60 * (uint32_t)minute) + (uint32_t)second);
}


CDisplay::Brightness ReadLightIntensity(void)
{
    const uint8_t SAMPLES = 32;
    static uint16_t history[SAMPLES];
    static uint8_t result = 0;
    static uint32_t previous_average = 0;
    uint32_t average = 0;
    int32_t difference;

    for (uint8_t index = 0; index < (SAMPLES - 1); index++)
    {
        average += (history[index] = history[index + 1]);
    }

    average += (history[SAMPLES - 1] = (analogRead(ANALOG_PIN_PHOTODIODE - A0) + g_config.offset));
    average /= SAMPLES;
    average *= g_config.gain;
    average /= 10; // pseudo float
    difference = (average - previous_average);
    difference = (difference > 0) ? difference : -difference;

    if (difference > 3)
    {
        uint8_t value;
        
        if (average < 100)
        {
            value = 1 + (average / 25);
        }
        else
        {
            value = 2 + (average / 100);
        }

        uint8_t max = getValue(CDisplay::Brightness::MAX);

        if (value > max)
        {
            value = max;
        }
        
        result = value;
        previous_average = average;
    }

    return static_cast<CDisplay::Brightness>(result);
}


uint32_t ReadBatteryMillivolts(void)
{
    const uint8_t samples = 16;
    static uint16_t history[samples] = {510u, 510u, 510u, 510u,
                                        510u, 510u, 510u, 510u,
                                        510u, 510u, 510u, 510u,
                                        510u, 510u, 510u, 510u};
    uint8_t index;
    uint32_t average = 0;

    for (index = 0; index < (samples - 1); index++)
    {
        average += (history[index] = history[index+1]);
    }

    average += (history[samples-1] = analogRead(ANALOG_PIN_BATTERY));
    average /= samples;

    return ((average * 49) / 10); // Convert to millivolts
}


bool GetBatteryState(void)
{
    static uint8_t hysteresis = 0;

    if (ReadBatteryMillivolts() < (BATTERY_MIN + hysteresis))
    {
        hysteresis = 25; // millivolts
        return false; // voltage low, replace battery
    }
    else
    {
        hysteresis = 0;
    }

    return true; // voltage good
}


void GetConfig(Config& config)
{
    while (!eeprom_is_ready());
    cli();
    eeprom_read_block((void*)&config, (void*)0, sizeof(Config));
    sei();
}


void SetConfig(const Config& config)
{
    while (!eeprom_is_ready());
    cli();
    eeprom_update_block((const void*)&config, (void*)0, sizeof(Config));
    sei();
}


void VoltageState(State state)
{
    g_state.voltage = state;
    digitalWrite(DIGITAL_PIN_HV_ENABLE, getValue(state));
}


void DisplayState(State state)
{
    g_state.display = state;
    digitalWrite(DIGITAL_PIN_BLANK, getValue(state));
    VoltageState(state);
}


void InterruptSpeed(const uint8_t speed)
{
    // set compare match register for xHz increments
    OCR2A = speed; // = (16MHz) / (x*prescale) - 1 (must be <255)
}


__attribute__((optimize("-O3")))
void EncoderCallback(void)
{
    if (g_config.noise == State::ENABLE)
    {
        g_audio.Play(CAudio::Functions::MemStream, music_blip, music_blip, music_blip);
    }
}


bool IsInputIncrement(void)
{
    return (g_encoder.GetRotation() == CNcoder::Rotation::CW);
}


bool IsInputSelect(void)
{
    return (g_encoder.GetButtonState() == CNcoder::Button::DOWN);
}


bool IsInputUpdate(void)
{
    return (g_encoder.IsUpdateAvailable());
}


// Interrupt is called every millisecond
ISR(TIMER0_COMPA_vect) 
{
    wdt_reset(); // Reset watchdog timer
}


ISR(TIMER2_COMPA_vect)
{
    static uint8_t pwm_cycle = 0;
    static const uint8_t toggle[] = {0xFF, 0x01, 0x11, 0x25, 0x55, 0x5B, 0x77, 0x7F, 0xFF};
    
    sei(); // Enable interrupts for audio processing
    
    // No need to update display if disabled
    if (g_state.display == State::DISABLE)
    {
        return;
    }

    pwm_cycle++;

    if (pwm_cycle > 7)
    {
        pwm_cycle = 0;
    }
    
    // Update at 1/8 speed during audio playback
    if (g_audio.IsActive() && (pwm_cycle > 0))
    {
        return;
    }

    setPinLow(DIGITAL_PIN_LATCH); // latch

    for (uint8_t tube = DISPLAY_COUNT - 1; tube < 255; tube--)
    {
        uint16_t digit_bitmap = 0;

        if ((toggle[getValue(g_display.GetUnitBrightness(tube))] >> pwm_cycle) & 0x1)
        {
            uint8_t unit = g_display.GetUnitValue(tube);
            uint8_t indicator = g_display.GetUnitIndicator(tube);
        
            digit_bitmap = (pgm_read_word_near(BITMAP + unit - 24) | (indicator << 1));
        }

        for (uint16_t index = 0; index < 16; index++)
        {
            setPinHigh(DIGITAL_PIN_CLOCK); // clock

            uint8_t segment = ((digit_bitmap >> (15 - index)) & 0x1);
            
            if (segment)
            {
                setPinHigh(DIGITAL_PIN_SDATA); // sdata
            }
            else
            {
                setPinLow(DIGITAL_PIN_SDATA); // sdata
            }

            setPinLow(DIGITAL_PIN_CLOCK); // clock
        }
    }

    setPinHigh(DIGITAL_PIN_LATCH); // latch
}


void setup(void)
{
    pinMode(DIGITAL_PIN_CLOCK, OUTPUT);         // Clock
    pinMode(DIGITAL_PIN_SDATA, OUTPUT);         // Serial Data
    pinMode(DIGITAL_PIN_LATCH, OUTPUT);         // Latch Enable
    pinMode(DIGITAL_PIN_BLANK, OUTPUT);         // Blank
    pinMode(DIGITAL_PIN_HV_ENABLE, OUTPUT);     // High Voltage Enable
    pinMode(ANALOG_PIN_PHOTODIODE, INPUT);      // Photodiode Voltage
    pinMode(ANALOG_PIN_BATTERY, INPUT);         // Battery Voltage
    pinMode(DIGITAL_PIN_TRANSDUCER_0, OUTPUT);  // Transducer A
    pinMode(DIGITAL_PIN_TRANSDUCER_1, OUTPUT);  // Transducer B
    pinMode(DIGITAL_PIN_TRANSDUCER_2, OUTPUT);  // Transducer B
    
    // Watchdog timer
    wdt_enable(WDTO_1S); // Set for 1 second
    
    // Millisecond timer
    OCR0A = 0x7D;
    TIMSK0 |= _BV(OCIE0A);
    
    // Configure Timer2 (Display)
    TCCR2A = 0; // Reset register
    TCCR2B = 0; // Reset register
    TCNT2  = 0; // Initialize counter value to 0
    //OCR2A set by InterruptSpeed() function - 8 bit register
    TCCR2A |= _BV(WGM21); // Enable CTC mode
    TCCR2B |= (1 << CS22) | (1 << CS21) | (1 << CS20); // Set for 1024 prescaler
    TIMSK2 |= _BV(OCIE2A); // Enable timer compare interrupt
}
