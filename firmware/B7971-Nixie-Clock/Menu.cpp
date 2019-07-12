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
 * @file        Menu.cpp
 * @summary     Menu for B7971-Nixie-Clock
 * @version     1.2
 * @author      nitacku
 * @data        14 August 2018
 */

#include "Menu.h"

extern StateStruct g_state;         // struct
extern Config g_config;             // struct
extern CDS3232 g_rtc;               // class
extern CDisplay g_display;          // class
extern CAudio g_audio;              // class
extern CNcoder g_encoder;           // class
extern uint8_t g_song_entries;      // integral
extern bool IsInputIncrement(void); // Function
extern bool IsInputSelect(void);    // Function
extern bool IsInputUpdate(void);    // Function


void MenuInfo(void)
{
    uint8_t function = 0;
    uint32_t timeout = Timeout::INFO;

    while (IsInputSelect() && --timeout);

    if (timeout)
    {
        do
        {
            timeout = Timeout::INFO;
            while (!IsInputSelect() && --timeout);
            g_display.SetUnitIndicator(2, false);
            
            if (!timeout)
            {
                break;
            }
            
            switch (function)
            {
            case 0:
                float f_value;
                f_value = g_rtc.GetTemperature(); // Get temperature in Celsius

                if (g_config.temperature_unit == CRTC::Unit::F)
                {
                    f_value = g_rtc.ConvertTemperature(g_rtc.GetTemperature(), CRTC::Unit::C, CRTC::Unit::F);
                }

                g_display.SetDisplayValue(f_value * 1000);
                g_display.SetUnitValue(4, '`');
                g_display.SetUnitValue(5, 'F');
                g_display.SetUnitIndicator(2, true);
                break;
            case 1:
                g_display.SetDisplayValue((100 * (ReadBatteryMillivolts() - BATTERY_MIN)) / (BATTERY_MAX - BATTERY_MIN)); // Display percentage
                g_display.SetUnitValue(0, 'B');
                g_display.SetUnitValue(1, 'a');
                g_display.SetUnitValue(2, 't');
                break;
            case 2:
                g_display.SetDisplayValue(F("Rev   "));
                g_display.SetUnitValue(4, '@' + VERSION);
                break;
            case 3:
                RestoreOutOfBox();
                break;
            }

            function++;
            timeout = Timeout::INFO;
            while (IsInputSelect() && --timeout); // Wait until release
            
            if (!timeout)
            {
                g_display.SetUnitIndicator(2, false);
                Detonate();
            }
        }
        while ((function < 4) && timeout);
    }
    else
    {
        DisplayState(State::DISABLE);
    }
}


void MenuSettings(void)
{
    // Use full brightness for Menu
    VoltageState(State::ENABLE);
    g_display.SetDisplayBrightness(CDisplay::Brightness::MAX);
    
    CDisplay::PromptSelectStruct prompt_select;
    prompt_select.item_count = MENU_ITEM_COUNT;
    prompt_select.initial_selection = MENU_ITEM_ALARM;
    prompt_select.display_mode = CDisplay::Mode::SCROLL;

    // Copy item table to local variable
    char* array[sizeof(menu_item_array) / sizeof(char*)];
    memcpy_P(array, menu_item_array, sizeof(array));
    prompt_select.item_array = reinterpret_cast<type_const_char_ptr*>(array);
    int8_t selection = g_display.PromptSelect(prompt_select, Timeout::MENU);
    
    if (selection > -1)
    {
        switch (selection)
        {
        default:
        case MENU_ITEM_ALARM:
        {
            uint8_t alarm = 0; // Track selection
            
            if (SetAlarmState(alarm))
            {
                if (SetAlarmTime(alarm))
                {
                    if (SetAlarmDays(alarm))
                    {
                        SetMusic(g_config.alarm[alarm].music);
                    }
                }
            }

            break;
        }
            
        case MENU_ITEM_BRIGHTNESS:
            if (SetBrightness() && (g_config.brightness == CDisplay::Brightness::AUTO))
            {
                if (SetGain())
                {
                    SetOffset();
                }
            }
            break;

        case MENU_ITEM_CONFIG:
            if (SetTimeFormat())
            {
                if (SetDateFormat())
                {
                    if (SetTemperatureUnit())
                    {
                        if (SetBlip())
                        {
                            if (SetBattery())
                            {
                                if (SetEffect())
                                {
                                    SetPhrase();
                                }
                            }
                        }
                    }
                }
            }
            break;

        case MENU_ITEM_BLANK:
            SetBlank();
            break;
            
        case MENU_ITEM_TIME:
            SetTime();
            break;

        case MENU_ITEM_DATE:
            SetDate();
            break;
            
        case MENU_ITEM_MUSIC:
            SetMusic(g_config.music_timer);
            break;

        case MENU_ITEM_TIMER:
            SetTimer();
            break;
        }
    }

    g_display.SetDisplayBrightness(g_config.brightness);
}


int8_t SelectCycle(const Cycle init_value)
{
    CDisplay::PromptSelectStruct prompt_select;
    prompt_select.item_count = 2;
    prompt_select.initial_selection = getValue(init_value);
    prompt_select.title = F("Cycle ");
    type_const_char_ptr item_array[] = {F("  AM  "), F("  PM  ")};
    prompt_select.item_array = item_array;
    return g_display.PromptSelect(prompt_select, Timeout::SELECT);
}


int8_t SelectState(CDisplay::PromptSelectStruct& prompt_select)
{
    prompt_select.item_count = 2;
    prompt_select.display_mode = CDisplay::Mode::STATIC;
    type_const_char_ptr item_array[] = {F("Dsable"), F("Enable")};
    prompt_select.item_array = item_array;
    return g_display.PromptSelect(prompt_select, Timeout::SELECT);
}


bool SelectRTCValue(CDisplay::PromptValueStruct& prompt_value)
{
    Cycle current_cycle = (prompt_value.item_value[0] < 12) ? Cycle::AM : Cycle::PM;

    if (g_config.time_format == FormatTime::H24)
    {
        prompt_value.item_lower_limit = (const type_const_uint8 []){0, 0, 0};
        prompt_value.item_upper_limit = (const type_const_uint8 []){23, 59, 59};
    }
    else
    {
        prompt_value.item_value[0] = FormatHour(prompt_value.item_value[0]);
        prompt_value.item_lower_limit = (const type_const_uint8 []){1, 0, 0};
        prompt_value.item_upper_limit = (const type_const_uint8 []){12, 59, 59};
    }

    if (g_display.PromptValue(prompt_value, Timeout::VALUE) > -1)
    {
        if (g_config.time_format == FormatTime::H12)
        {
            int8_t result = SelectCycle(current_cycle);

            if (result == -1)
            {
                return false;
            }

            prompt_value.item_value[0] %= 12; // Convert 12 to 0
            prompt_value.item_value[0] += (static_cast<Cycle>(result) == Cycle::PM) ? 12 : 0;
        }

        return true;
    }

    return false;
}


bool RestoreOutOfBox(void)
{
    CDisplay::PromptSelectStruct prompt_select;
    prompt_select.item_count = 2;
    prompt_select.title = F("Reset ");
    type_const_char_ptr item_array[] = {F("Cancel"), F("Reset ")};
    prompt_select.item_array = item_array;
    int8_t selection = g_display.PromptSelect(prompt_select, Timeout::SELECT);

    if (selection > 0)
    {
        Config new_config; // Use default constructor values
        SetConfig(new_config); // Write to EEPROM
        GetConfig(g_config); // Read from EEPROM
        return true;
    }

    return false;
}


bool SetBlank(void)
{
    char s[DISPLAY_COUNT + 1];
    uint8_t hour, minute;
    CDisplay::PromptValueStruct prompt_value;

    for (uint8_t index = 0; index < 2; index++)
    {
        if (index)
        {
            prompt_value.title = F(" P-On ");
            hour = g_config.blank_end / 3600;
            minute = (g_config.blank_end / 60) % 60;
        }
        else
        {
            prompt_value.title = F("P-Off ");
            hour = g_config.blank_begin / 3600;
            minute = (g_config.blank_begin / 60) % 60;
        }

        snprintf_P(s, DISPLAY_COUNT + 1, PSTR(" %02u%02u "), FormatHour(hour), minute);
        type_const_uint8 item_value[] = {hour, minute};
        prompt_value.item_count = 2;
        prompt_value.item_position = (const uint8_t []){1, 3};
        prompt_value.item_digit_count = (const uint8_t []){2, 2};
        prompt_value.item_value = item_value;
        prompt_value.initial_display = s;

        if (SelectRTCValue(prompt_value))
        {
            uint32_t value = GetSeconds(prompt_value.item_value[0], prompt_value.item_value[1], 0);
            
            if (index)
            {
                g_config.blank_end = value;
            }
            else
            {
                g_config.blank_begin = value;
            }

            SetConfig(g_config);
        }
        else
        {
            return false;
        }
    }

    return true;
}


bool SetBrightness(void)
{
    CDisplay::PromptSelectStruct prompt_select;
    prompt_select.item_count = 9;
    prompt_select.initial_selection = getValue(g_config.brightness);
    prompt_select.title = F("Bright");
    type_const_char_ptr item_array[] = {F(" Auto "), F(" set 1"), F(" set 2"),
                                        F(" set 3"), F(" set 4"), F(" set 5"),
                                        F(" set 6"), F(" set 7"), F(" set 8")};
    prompt_select.item_array = item_array;
    g_display.SetDisplayBrightness(g_config.brightness); // Set initial brightness
    
    int8_t selection = g_display.PromptSelect(prompt_select, Timeout::SELECT,
    [](CDisplay::Event event, uint8_t selection)
    {
        switch (event)
        {
        case CDisplay::Event::DECREMENT:
        case CDisplay::Event::INCREMENT:
            if (static_cast<CDisplay::Brightness>(selection) == CDisplay::Brightness::AUTO)
            {
                CDisplay::Brightness brightness;
                brightness = ReadLightIntensity();
                
                if (brightness == CDisplay::Brightness::MIN)
                {
                    brightness = CDisplay::Brightness::L1;
                }
                
                g_display.SetDisplayBrightness(brightness);
            }
            else
            {
                g_display.SetDisplayBrightness(static_cast<CDisplay::Brightness>(selection));
            }
            break;
        
        case CDisplay::Event::SELECTION:
            break;
        case CDisplay::Event::TIMEOUT:
            g_display.SetDisplayBrightness(g_config.brightness); // Restore brightness
            break;
        
        default:
            break;
        }
        
        return false;
    });

    if (selection > -1)
    {
        g_config.brightness = static_cast<decltype(g_config.brightness)>(selection);
        SetConfig(g_config);
        return true;
    }

    return false;
}


bool SetGain(void)
{
    char s[DISPLAY_COUNT + 1];
    CDisplay::PromptValueStruct prompt_value;
    snprintf_P(s, DISPLAY_COUNT + 1, PSTR("  %02u  "), g_config.gain);
    type_const_uint8 item_value[] = {g_config.gain};
    prompt_value.item_count = 1;
    prompt_value.item_position = (const uint8_t []){2};
    prompt_value.item_digit_count = (const uint8_t []){2};
    prompt_value.item_value = item_value;
    prompt_value.item_lower_limit = (const type_const_uint8 []){1};
    prompt_value.item_upper_limit = (const type_const_uint8 []){50};
    prompt_value.initial_display = s;
    prompt_value.title = F(" Gain ");

    if (g_display.PromptValue(prompt_value, Timeout::VALUE) > -1)
    {
        g_config.gain = prompt_value.item_value[0];
        SetConfig(g_config);
        return true;
    }

    return false;
}


bool SetOffset(void)
{
    char s[DISPLAY_COUNT + 1];
    CDisplay::PromptValueStruct prompt_value;
    snprintf_P(s, DISPLAY_COUNT + 1, PSTR("  %02u  "), g_config.offset);
    type_const_uint8 item_value[] = {g_config.offset};
    prompt_value.item_count = 1;
    prompt_value.item_position = (const uint8_t []){2};
    prompt_value.item_digit_count = (const uint8_t []){2};
    prompt_value.item_value = item_value;
    prompt_value.item_lower_limit = (const type_const_uint8 []){0};
    prompt_value.item_upper_limit = (const type_const_uint8 []){20};
    prompt_value.initial_display = s;
    prompt_value.title = F("Offset");

    if (g_display.PromptValue(prompt_value, Timeout::VALUE) > -1)
    {
        g_config.offset = prompt_value.item_value[0];
        SetConfig(g_config);
        return true;
    }

    return false;
}


bool SetEffect(void)
{
    CDisplay::PromptSelectStruct prompt_select;
    prompt_select.item_count = 3;
    prompt_select.initial_selection = getValue(g_config.effect);
    prompt_select.title = F("Effect");
    type_const_char_ptr item_array[] = {F(" None "), F("Spiral"), F(" Date "), F("Phrase")};
    prompt_select.item_array = item_array;
    int8_t selection = g_display.PromptSelect(prompt_select, Timeout::SELECT);

    if (selection > -1)
    {
        g_config.effect = static_cast<decltype(g_config.effect)>(selection);
        SetConfig(g_config);
        return true;
    }

    return false;
}


bool SetBlip(void)
{
    CDisplay::PromptSelectStruct prompt_select;
    prompt_select.initial_selection = getValue(g_config.noise);
    prompt_select.title = F("Noise ");
    int8_t selection = SelectState(prompt_select);

    if (selection > -1)
    {
        g_config.noise = static_cast<decltype(g_config.noise)>(selection);
        SetConfig(g_config);
        return true;
    }

    return false;
}


bool SetBattery(void)
{
    CDisplay::PromptSelectStruct prompt_select;
    prompt_select.item_count = 2;
    prompt_select.initial_selection = getValue(g_config.battery);
    prompt_select.title = F("Battry");
    type_const_char_ptr item_array[] = {F("Dsable"), F("Enable")};
    prompt_select.item_array = item_array;
    int8_t selection = g_display.PromptSelect(prompt_select, Timeout::SELECT);

    if (selection > -1)
    {
        g_config.battery = static_cast<decltype(g_config.battery)>(selection);
        SetConfig(g_config);
        return true;
    }

    return false;
}


bool SetTimeFormat(void)
{
    CDisplay::PromptSelectStruct prompt_select;
    prompt_select.item_count = 2;
    prompt_select.initial_selection = getValue(g_config.time_format);
    prompt_select.title = F(" Hour ");
    type_const_char_ptr item_array[] = {F("24 hr "), F("12 hr ")};
    prompt_select.item_array = item_array;
    int8_t selection = g_display.PromptSelect(prompt_select, Timeout::SELECT);

    if (selection > -1)
    {
        g_config.time_format = static_cast<decltype(g_config.time_format)>(selection);
        SetConfig(g_config);
        return true;
    }

    return false;
}


bool SetDateFormat(void)
{
    CDisplay::PromptSelectStruct prompt_select;
    prompt_select.item_count = 3;
    prompt_select.initial_selection = getValue(g_config.date_format);
    prompt_select.title = F(" Date ");
    type_const_char_ptr item_array[] = {F("Y-M-D "), F("M-D-Y "), F("D-M-Y ")};
    prompt_select.item_array = item_array;
    int8_t selection = g_display.PromptSelect(prompt_select, Timeout::SELECT);

    if (selection > -1)
    {
        g_config.date_format = static_cast<decltype(g_config.date_format)>(selection);
        SetConfig(g_config);
        return true;
    }

    return false;
}


bool SetTemperatureUnit(void)
{
    CDisplay::PromptSelectStruct prompt_select;
    prompt_select.item_count = 2;
    prompt_select.initial_selection = getValue(g_config.temperature_unit);
    prompt_select.title = F(" Temp ");
    type_const_char_ptr item_array[] = {F("Temp C"), F("Temp F")};
    prompt_select.item_array = item_array;
    int8_t selection = g_display.PromptSelect(prompt_select, Timeout::SELECT);

    if (selection > -1)
    {
        g_config.temperature_unit = static_cast<decltype(g_config.temperature_unit)>(selection);
        SetConfig(g_config);
        return true;
    }

    return false;
}


bool SetTime(void)
{
    char s[DISPLAY_COUNT + 1];
    CDisplay::PromptValueStruct prompt_value;
    CRTC::RTC rtc;
    g_rtc.GetRTC(rtc);
    FormatRTCString(rtc, s, RTCSelect::TIME);
    type_const_uint8 item_value[] = {rtc.hour, rtc.minute, rtc.second};
    prompt_value.item_count = 3;
    prompt_value.item_position = (const uint8_t []){0, 2, 4};
    prompt_value.item_digit_count = (const uint8_t []){2, 2, 2};
    prompt_value.item_value = item_value;
    prompt_value.initial_display = s;
    prompt_value.title = F(" Time ");

    if (SelectRTCValue(prompt_value))
    {
        g_rtc.SetTime(prompt_value.item_value[0],
                      prompt_value.item_value[1],
                      prompt_value.item_value[2]);
        return true;
    }

    return false;
}


bool SetDate(void)
{
    char s[DISPLAY_COUNT + 1];
    CDisplay::PromptValueStruct prompt_value;
    CRTC::RTC rtc;
    g_rtc.GetRTC(rtc);
    FormatRTCString(rtc, s, RTCSelect::DATE);
    uint8_t item_value_index[3]; // Each entry represents a format option
    prompt_value.item_count = 3;
    prompt_value.item_position = (const uint8_t []){0, 2, 4};
    prompt_value.item_digit_count = (const uint8_t []){2, 2, 2};
    
    // Dynamically created - Must be kept within scope
    type_const_uint8 item_valueYYMMDD[] = {rtc.year, rtc.month, rtc.day};
    type_const_uint8 item_valueMMDDYY[] = {rtc.month, rtc.day, rtc.year};
    type_const_uint8 item_valueDDMMYY[] = {rtc.day, rtc.month, rtc.year};

    switch (g_config.date_format)
    {
    default:
    case FormatDate::YYMMDD:
        prompt_value.item_upper_limit = (const type_const_uint8 []){99, 12, 31};
        prompt_value.item_lower_limit = (const type_const_uint8 []){0, 1, 1};
        prompt_value.item_value = item_valueYYMMDD;
        memcpy(item_value_index, (const type_const_uint8 []){0, 1, 2}, sizeof(item_value_index));
        break;
        
    case FormatDate::MMDDYY:
        prompt_value.item_upper_limit = (const type_const_uint8 []){12, 31, 99};
        prompt_value.item_lower_limit = (const type_const_uint8 []){1, 1, 0};
        prompt_value.item_value = item_valueMMDDYY;
        memcpy(item_value_index, (const type_const_uint8 []){2, 0, 1}, sizeof(item_value_index));
        break;
        
    case FormatDate::DDMMYY:
        prompt_value.item_upper_limit = (const type_const_uint8 []){31, 12, 99};
        prompt_value.item_lower_limit = (const type_const_uint8 []){1, 1, 0};
        prompt_value.item_value = item_valueDDMMYY;
        memcpy(item_value_index, (const type_const_uint8 []){2, 1, 0}, sizeof(item_value_index));
        break;
    }

    prompt_value.initial_display = s;
    prompt_value.title = F(" Date ");

    if (g_display.PromptValue(prompt_value, Timeout::VALUE) > -1)
    {
        g_rtc.SetDate(prompt_value.item_value[item_value_index[0]],
                      prompt_value.item_value[item_value_index[1]],
                      prompt_value.item_value[item_value_index[2]]);
        
        return true;
    }

    return false;
}


bool SetAlarmState(uint8_t& alarm)
{
    type_const_char_ptr alarm_str[] = {F("A1 OFF"), F("A1 ON "),
                                       F("A2 OFF"), F("A2 ON "),
                                       F("A3 OFF"), F("A3 ON ")};
    uint8_t alarm_index[ALARM_COUNT];
    
    for (uint8_t index = 0; index < ALARM_COUNT; index++)
    {
        bool state = (g_config.alarm[index].state == State::ENABLE);
        alarm_index[index] = (index * 2) + state;
    }
    
    CDisplay::PromptSelectStruct prompt_select;
    prompt_select.item_count = ALARM_COUNT;
    prompt_select.display_mode = CDisplay::Mode::SCROLL;
    
    type_const_char_ptr item_array[ALARM_COUNT] = {alarm_str[alarm_index[0]],
                                                   alarm_str[alarm_index[1]],
                                                   alarm_str[alarm_index[2]]};
    
    prompt_select.item_array = item_array;
    int8_t selection_alarm = g_display.PromptSelect(prompt_select, Timeout::SELECT);

    if (selection_alarm > -1)
    {
        prompt_select.initial_selection = getValue(g_config.alarm[selection_alarm].state);
        int8_t selection_state = SelectState(prompt_select);
        
        if (selection_state > -1)
        {
            // Check if alarm is enabled
            if (selection_state)
            {
                // Capture alarm selection
                alarm = (uint8_t)selection_alarm;
                // Set alarm if any days are enabled
                g_config.alarm[alarm].state = (g_config.alarm[alarm].days == 0)
                                            ? State::DISABLE : State::ENABLE;
            }
            else
            {
                // Disable alarm
                g_config.alarm[selection_alarm].state = static_cast<decltype(g_config.alarm[0].state)>(selection_state);
            }

            SetConfig(g_config);
            
            // Additional processing required if alarm enabled
            return selection_state;
        }
    }

    return false;
}


bool SetAlarmTime(const uint8_t alarm)
{
    char s[DISPLAY_COUNT + 1];
    CDisplay::PromptValueStruct prompt_value;

    uint8_t hour = g_config.alarm[alarm].time / 3600;
    uint8_t minute = ((g_config.alarm[alarm].time / 60) % 60);

    snprintf_P(s, DISPLAY_COUNT + 1, PSTR(" %02u%02u "), FormatHour(hour), minute);
    type_const_uint8 item_value[] = {hour, minute};
    prompt_value.item_count = 2;
    prompt_value.item_position = (const uint8_t []){1, 3};
    prompt_value.item_digit_count = (const uint8_t []){2, 2};
    prompt_value.item_value = item_value;
    prompt_value.initial_display = s;
    prompt_value.title = F(" Set  ");

    if (SelectRTCValue(prompt_value))
    {
        // Convert alarm to seconds
        g_config.alarm[alarm].time = GetSeconds(prompt_value.item_value[0], prompt_value.item_value[1], 0);
        SetConfig(g_config);
        return true;
    }

    return false;
}


bool SetAlarmDays(const uint8_t alarm)
{
    int8_t selection = -1;

    do
    {
        selection++;
        CDisplay::PromptSelectStruct prompt_select;
        prompt_select.item_count = 8;
        prompt_select.initial_selection = selection;
        prompt_select.display_mode = CDisplay::Mode::SCROLL;
        type_const_char_ptr item_array[] = {F("Sunday"), F("Monday"), F("Tusday"),
                                            F("Wdnsdy"), F("Thrsdy"), F("Friday"),
                                            F("Satrdy"), F("-Done-")};
        prompt_select.item_array = item_array;
        g_encoder.SetRotation(CNcoder::Rotation::CCW); // Make display scroll from left
        selection = g_display.PromptSelect(prompt_select, Timeout::SELECT);

        if ((selection < 7) && (selection > -1))
        {
            CDisplay::PromptSelectStruct prompt_select_e;
            prompt_select_e.initial_selection = ((g_config.alarm[alarm].days >> (selection + 1)) & 0x1);
            int8_t state = SelectState(prompt_select_e);

            // Check if timeout
            if (state > -1)
            {
                g_config.alarm[alarm].days ^= (-state ^ g_config.alarm[alarm].days) & (0x1 << (selection + 1));
                g_config.alarm[alarm].state = (g_config.alarm[alarm].days == 0) ? State::DISABLE : State::ENABLE;
                SetConfig(g_config);
            }
            else
            {
                return false;
            }
        }
    } while ((selection < 7) && (selection > -1));

    if (selection > -1)
    {
        return true;
    }

    return false;
}


bool SetPhrase(void)
{
    char s[DISPLAY_COUNT + 1] = {0}; // ensure null terminator
    CDisplay::PromptValueStruct prompt_value;
    type_const_uint8 item_value[DISPLAY_COUNT];
    memcpy(s, g_config.phrase, DISPLAY_COUNT);
    memcpy(item_value, g_config.phrase, DISPLAY_COUNT);
    prompt_value.alphabetic = true;
    prompt_value.item_count = DISPLAY_COUNT;
    prompt_value.item_position = (const uint8_t []){0, 1, 2, 3, 4, 5};
    prompt_value.item_digit_count = (const uint8_t []){1, 1, 1, 1, 1, 1};
    prompt_value.item_value = item_value;
    prompt_value.item_lower_limit = (const type_const_uint8 []){32, 32, 32, 32, 32, 32};
    prompt_value.item_upper_limit = (const type_const_uint8 []){127, 127, 127, 127, 127, 127};
    prompt_value.initial_display = s;
    prompt_value.title = F("Phrase");

    if (g_display.PromptValue(prompt_value, Timeout::VALUE) > -1)
    {
        memcpy(g_config.phrase, prompt_value.item_value, DISPLAY_COUNT);
        SetConfig(g_config);
        return true;
    }

    return false;
}

    
bool SetMusic(uint8_t& music)
{
    char s[DISPLAY_COUNT + 1];
    CDisplay::PromptValueStruct prompt_value;
    snprintf_P(s, DISPLAY_COUNT + 1, PSTR("set %02u"), music);
    type_const_uint8 item_value[] = {music};
    type_const_uint8 song_entries = g_song_entries - 1;
    type_const_uint8 item_upper_limit[] = {song_entries};
    prompt_value.item_count = 1;
    prompt_value.brightness_min = CDisplay::Brightness::MAX;
    prompt_value.item_position = (const uint8_t []){4};
    prompt_value.item_digit_count = (const uint8_t []){2};
    prompt_value.item_value = item_value;
    prompt_value.item_lower_limit = (const type_const_uint8 []){0};
    prompt_value.item_upper_limit = item_upper_limit;
    prompt_value.initial_display = s;
    prompt_value.title = F("Audio ");

    InterruptSpeed(INTERRUPT_SLOW);
    
    g_display.PromptValue(prompt_value, Timeout::VALUE,
    [&music](CDisplay::Event event, uint8_t selection) -> bool
    {
        switch (event)
        {
        case CDisplay::Event::DECREMENT:
        case CDisplay::Event::INCREMENT:
            g_audio.Stop(); // Mute audio
            PlayMusic(selection);
            break;
        
        case CDisplay::Event::SELECTION:
            g_audio.Stop(); // Mute audio
            music = selection;
            SetConfig(g_config);
            InterruptSpeed(INTERRUPT_FAST);
            break;

        case CDisplay::Event::TIMEOUT:
            //Don't time out during playback
            if (g_audio.IsActive())
            {
                return true;
            }

            g_audio.Stop(); // Mute audio
            break;

        default:
            break;
        }
        
        return false;
    });
    
    InterruptSpeed(INTERRUPT_FAST);

    return true;
}


void SetTimer(void)
{
    uint32_t timer = 500;
    char s[DISPLAY_COUNT + 1];
    CDisplay::PromptValueStruct prompt_value;
    uint8_t value0 = (timer / 10000);
    uint8_t value1 = (timer / 100) % 100;
    uint8_t value2 = (timer % 100);
    type_const_uint8 item_value[] = {value0, value1, value2};
    snprintf_P(s, DISPLAY_COUNT + 1, PSTR("%06lu"), timer);
    prompt_value.item_count = 3;
    prompt_value.item_position = (const uint8_t []){0, 2, 4};
    prompt_value.item_digit_count = (const uint8_t []){2, 2, 2};
    prompt_value.item_lower_limit = (const type_const_uint8 []){0, 0, 0};
    prompt_value.item_upper_limit = (const type_const_uint8 []){23, 59, 59};
    prompt_value.item_value = item_value;
    prompt_value.initial_display = s;
    prompt_value.title = F(" Set  ");

    if (g_display.PromptValue(prompt_value, Timeout::VALUE) > -1)
    {
        Timer(prompt_value.item_value[0],
              prompt_value.item_value[1],
              prompt_value.item_value[2]);
    }
}
