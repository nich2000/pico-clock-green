/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 * 
 * Fixed by NIch, 2026
 */
#include "hardware/i2c.h"
#include "hardware/adc.h"

#include "define.h"
#include "Ds3231.h"
#include "ziku.h"
#include "clock_tcp.h"
//=============================================================================
#define BAUD_RATE 115200
//=============================================================================
// Determining the number of days in a month in leap years and non-leap years 
unsigned char month_date[2][12]={{31,29,31,30,31,30,31,31,30,31,30,31},
                                 {31,28,31,30,31,30,31,31,30,31,30,31}}; 

// Buffer bytes, corresponding to 24x8 points and scroll bytes 
unsigned char display_buffer[112]; 

// Current row for drawing 
unsigned char row_select = 0;

// Trigger key detection 
unsigned char UP_id=0, UP_Key_flag=0, KEY_Set_flag=0, no_operation_flag = false, no_operation_counter; 

// Set automatic brightness
uint16_t adc_light, adc_light_count = 0;  
unsigned char adc_light_flag = 0, adc_light_time_flag = 0, light_set = 0;

// Флаг необходимости отрисовать время
unsigned char update_time = 0;

unsigned char setting_id=0;

// buzzer
unsigned char beep_state = 0, beep_flag = 0, beep_on_flag = 0, beep_on_step = 0, beep_duration = 0, beep_duration_counter = 0, beep_repeat = 0, beep_repeat_counter = 0;

// scroll 
unsigned char scroll_flag = 0, scroll_state = 0, scroll_count = 0, 
    scroll_start = 0, scroll_start_count = 0, scroll_speed = 150,
    scroll_show_flag = 0,scroll_show_start = 0, 
    scroll_interval_time=61,
    scroll_manual = 0, scroll_manual_step = 0;

unsigned char alarm_id = 0, alarm_flag = 0; 

unsigned  char alarm_hour_temp = 0,alarm_min_temp = 0,alarm_hour_flag = 0,alarm_min_flag = 0,alarm_day_select_flag = 0,alarm_day_select = 1;

unsigned char Set_time_hour_flag = 0,Set_time_min_flag = 0,Set_time_year_flag = 0,Set_time_month_flag = 0,Set_time_dayofmonth_flag = 0,Set_hour_temp = 0,Set_min_temp = 0,change_time_flag = 0;

// Alarm and time 
unsigned char alarm_select_flag = 0, alarm_open_flag = 0, alarm_select_sta = 0, alarm_open_sta = 0, hour_temp, minute_temp, year_temp, month_temp, dayofmonth_temp, year_high_temp = 20; 

unsigned char seconds_counter=0, alarm_start_flag = 0;

// Время нажатия на кнопки
uint16_t set_press_cnt = 0, up_press_cnt = 0, down_press_cnt = 0;

uint16_t Flashing_count = 0, whole_year, adc_count = 0, write_flag = 0;

// 0 UP  1 DOWN  2 OFF
#define TIMING_UP   0
#define TIMING_DOWN 1
#define TIMING_OFF  2
unsigned char timing_mode_flag = 0, timing_mode_state = TIMING_OFF, 
    timing_minute_flag = 0, timing_second_flag = 0,
    timing_minute_temp = 0, timing_second_temp = 0,
    timing_UP_flag = false, timing_DOWN_flag = false, timing_DOWN_close_flag = false,
    timing_show_count = 0, timing_show_sec = 0;

// Hourly time chime
unsigned char hourly_flag = 0, hourly_state = 0;

// Time mode 
unsigned char Time_set_mode_flag = 0,Time_set_mode_sta = 0; 

// Мигалка времени 0 - выключен, 1 - включён
unsigned char indicator_state = 0; 

#define MODE_SETTINGS  0
#define MODE_CLOCK     1
#define MODE_COUNTDOUN 2
unsigned char work_mode = MODE_CLOCK;

char Time_buf[4];

unsigned char i,jr,save_buf,adc_show_flag = 0,adc_show_time = 6;

TIME_RTC Time_RTC;

unsigned char flag_Flashing[11]={0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff};

// Отображение температоры: 0 - выкл, 1 - Цельсий, 2 - Фаренгейт
unsigned char temperature_unit = 0;

unsigned char temp_high, temp_low, get_add_high = 0x11, get_add_low = 0x12;

extern app_state_t state;
//=============================================================================
// 
void get_temperature();
// Store the data that needs to be displayed 
void display_char(unsigned char x,unsigned char dis_char);
//
void send_data(unsigned char data); 
 // clear data 
void clear_display(unsigned char x);
//
void select_weekday(unsigned char x); 
// 1ms callback function 
bool repeating_timer_callback_ms(struct repeating_timer *t);
 // 1s callback function 
bool repeating_timer_callback_s(struct repeating_timer *t);
//
bool repeating_timer_callback_us(struct repeating_timer *t);
// Normal mode settings 
void dis_set_mode(); 
// Timekeeping Mode Settings
void display_timing();
// Alarm Mode Settings   
void dis_alarm(); 
// scroll 
void display_scroll();
// Время 
void display_time();
//
uint16_t get_ads1015();
// Get the month number of the current year
unsigned char get_month_date(uint16_t year,uint8_t month_cnt); 
// Kim Larson calculation formula: Weekday= (day+2*mon+3*(mon+1)/5+year+year/4-year /100+year/400) % 7
unsigned char get_weekday(uint16_t year,uint8_t month_cnt,uint8_t date_cnt); 
// Выводит напряжение на дисплей 
void display_adc_vcc();
// Отображение версии
void display_version();
//
void alarm_set(uint8_t UP_DOWN_flag);
//
void timing_set(uint8_t UP_DOWN_flag);
//
void time_set(uint8_t UP_DOWN_flag);
//
void scroll_show_judge();
//
void beep_start_judge(uint8_t repeat, uint16_t duration);
//
void beep_stop_judge();
//
void flashing_start_judge();
//
void adc_show_count();
//
void EXIT();
//
void special_exit();
//=============================================================================
struct repeating_timer timer2;
//=============================================================================
// GPIO initialization
int port_init(void) 
{
    stdio_init_all();

    gpio_init(A0);
    gpio_init(A1);
    gpio_init(A2);

    gpio_init(SDI);
    gpio_init(LE);
    gpio_init(OE);
    gpio_init(CLK);
    gpio_init(SQW);
    gpio_init(BUZZ);
    gpio_init(SET_BUTTON);
    gpio_init(UP_BUTTON);
    gpio_init(DOWN_BUTTON);

    gpio_set_dir(A0, GPIO_OUT);
    gpio_set_dir(A1, GPIO_OUT);
    gpio_set_dir(A2, GPIO_OUT);
    gpio_set_dir(SDI, GPIO_OUT);
    gpio_set_dir(OE, GPIO_OUT);
    gpio_set_dir(LE, GPIO_OUT);
    gpio_set_dir(CLK, GPIO_OUT);

    gpio_set_dir(SQW,GPIO_IN);
    gpio_set_dir(BUZZ,GPIO_OUT);

    // iic config
    i2c_init(I2C_PORT, 100000);
    gpio_set_function(SDA, GPIO_FUNC_I2C);
    gpio_set_function(SCL, GPIO_FUNC_I2C);
    gpio_pull_up(SDA);
    gpio_pull_up(SCL);
    
    // adc config
    adc_init();

    // Make sure GPIO is high-impedance, no pullups etc
    adc_gpio_init(ADC_Light);
    adc_gpio_init(ADC_VCC);
    
    // Select ADC input 0 (GPIO26)
    adc_select_input(3);
}

// main function
int main(void)
{
    port_init();

    printf("Hello, Pico!\n");

    if (cyw43_arch_init()) {
        printf("WiFi init failed\n");
        return -1;
    }

    cyw43_arch_enable_sta_mode();

    beep_state = 1;

    hourly_state = 1;
    dis_hourly_on;

    struct repeating_timer timer;
    struct repeating_timer timer1;

    add_repeating_timer_ms(1, repeating_timer_callback_ms, NULL, &timer);
    add_repeating_timer_ms(1000, repeating_timer_callback_s, NULL, &timer1);

    while(1)
    {
        if(adc_show_flag == 1)
        {
            // printf("Show version\n");
            // display_adc_vcc();
            display_version();
            adc_show_time--;
            adc_show_flag = 0;
            if(adc_show_time == 0)
            {
                update_time = 1;
                gpio_set_dir(SET_BUTTON,GPIO_IN);
                gpio_set_dir(UP_BUTTON,GPIO_IN);
                gpio_set_dir(DOWN_BUTTON,GPIO_IN);
                gpio_pull_up(SET_BUTTON);
                gpio_pull_up(UP_BUTTON);
                gpio_pull_up(DOWN_BUTTON);
            }
        }

        // Set button is clicked, enter normal setting mode
        if(KEY_Set_flag == 1) 
        {
            // printf("KEY_Set_flag\n");
            no_operation_flag = true;
            dis_set_mode();
            KEY_Set_flag = 0;
        }

        // Long press to enter the alarm setting
        if(alarm_flag == 1) 
        {
            // printf("alarm_flag\n");
            no_operation_flag = true;
            dis_alarm();
            alarm_flag = 0;
        }

        if(UP_Key_flag == 1)
        {
            // printf("UP_Key_flag\n");
            no_operation_flag = true;
            display_timing();
            UP_Key_flag = 0;
        }

        switch (work_mode)
        {
        case MODE_SETTINGS:
            break;
        case MODE_CLOCK:
            if(update_time == 1)
            {
                display_time();
                update_time = 0;
            }
            break;
        case MODE_COUNTDOUN:
            if(update_time == 1)
            {
                display_timing();
                update_time = 0;
            }
            break;
        }

        switch (state)
        {
            case WIFI_DISCONNECTED:
                if (wifi_connect()) {
                    state = WIFI_CONNECTED;
                } else {
                    sleep_ms(RECONNECT_DELAY_MS);
                }
                break;

            case WIFI_CONNECTED:
                if (tcp_client_connect()) {
                    // state -> TCP_CONNECTING внутри
                } else {
                    sleep_ms(RECONNECT_DELAY_MS);
                }
                break;

            case TCP_CONNECTED:
                // проверка линка WiFi
                if (!cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA)) {
                    printf("WiFi lost\n");
                    state = WIFI_DISCONNECTED;
                }
                break;

            case TCP_DISCONNECTED:
                printf("Reconnecting TCP...\n");
                sleep_ms(RECONNECT_DELAY_MS);
                state = WIFI_CONNECTED;
                break;

            case TCP_CONNECTING:
                // ждём callback
                break;

            default:
                break;
        }

        cyw43_arch_poll();
        sleep_ms(50);
    }

    cyw43_arch_deinit();
    return 0;
}

// Send data function
// функция реализует последовательную передачу одного байта данных по SPI-совместимому протоколу
void send_data(unsigned char data) 
{
    unsigned char i;
    for(i = 0; i < 8; i++)
    {
        CLK_LOW;

        SDI_LOW;

        if(data & 0x01)
        {
            SDI_HIGH;
        }

        data >>= 1;

        CLK_HIGH;
    }
}

// us
bool repeating_timer_callback_us(struct repeating_timer *t)
{
    if(adc_light >3600)
    {
        light_set ++;
        if(light_set == 15)
        {
            OE_OPEN;
            light_set = 0;
        }
        else
        {
            OE_CLOSE;
        }
    }
    else if(adc_light >2800)
    {
        light_set ++;
        if(light_set == 3)
        {
            OE_OPEN;
            light_set = 0;
        }
        else
        {
            OE_CLOSE;
        }
    }
    else
    {
        OE_OPEN;
    }
}

// 1ms enter once
bool repeating_timer_callback_ms(struct repeating_timer *t) {
    adc_show_count(); 
    beep_stop_judge();
    flashing_start_judge(); 
    scroll_show_judge();

    //=========================================================================
    // Кнопка нажата
    if(gpio_get(SET_BUTTON) == 0) 
    {
        set_press_cnt++;
    }
    // Кнопка отпущена
    else
    {
        // Короткое нажатие
        if(set_press_cnt > 10 && set_press_cnt < 300)
        {
            set_press_cnt = 0;

            scroll_state = !scroll_state;
            if(scroll_state != 0)
            {
                dis_move_on;
            }
            else
            {
                dis_move_off;
            }

            /*
            // Enter the alarm setting mode
            if(alarm_id == 1)
            {
                alarm_flag = 1;
            }

            // Enter timing setting mode
            else if(UP_id ==1)
            {
                UP_Key_flag = 1;
            }  

            // Enter the normal setting mode 
            else
            {
                // (external clock setting, key sound switch setting, scroll switch setting, clock display mode setting)
                KEY_Set_flag = 1;
            }

            beep_start_judge(1, 100);

            EXIT();

            setting_id++;	
            */
        }
        
        // Длинное нажатие и НЕ настройки
        else if(set_press_cnt > 300 && setting_id == 0)
        {
            set_press_cnt = 0;

            // НЕ настройки
            if(setting_id == 0)
            {
                setting_id++;
                alarm_flag = 1;
                alarm_id = 1;
            }

            beep_start_judge(1, 100);
        }
        
        else set_press_cnt = 0;
    }

    // Кнопка нажата
    if(gpio_get(UP_BUTTON) == 0) 
    {
        up_press_cnt++;
    }
    // Кнопка отпущена
    else
    {
        // Короткое нажатие
        if(up_press_cnt > 10 && up_press_cnt < 300)
        {
            up_press_cnt = 0;

            no_operation_counter = 0;

            // НЕ настройки
            if(setting_id == 0)
            {
                temperature_unit++;
                if(temperature_unit > 2) {
                    temperature_unit = 0;
                }
                switch (temperature_unit)
                {
                case 0:
                    dis_C_off;
                    dis_F_off;
                    break;
                case 1:
                    dis_C_on;
                    dis_F_off;
                    break;
                case 2:
                    dis_C_off;
                    dis_F_on;
                    break;
                }
            }

            // The key sound can set the flag bit 
            if(beep_flag == 1)
            {
                beep_state = !beep_state;
            }

            // The scroll switch can set the flag bit
            if(scroll_flag == 1)
            {
                scroll_state = !scroll_state;
                if(scroll_state != 0)
                {
                    dis_move_on;
                }
                else
                {
                    dis_move_off;
                }
            }

            if(hourly_flag == 1)
            {
                hourly_state = !hourly_state;
            }

            beep_start_judge(1, 100);

            alarm_set(UP_flag);
			timing_set(UP_flag);
			time_set(UP_flag);
        }
        
        // Длинное нажатие и НЕ настройки
        else if(up_press_cnt > 300 && setting_id == 0)
        {
            up_press_cnt = 0;

            // НЕ настройки
            if(setting_id == 0)
            {
                setting_id++;
                UP_Key_flag = 1;
                UP_id = 1;
            }

            beep_start_judge(1, 100);
        }

        else up_press_cnt = 0;
    }

    // Кнопка нажата
    if(gpio_get(DOWN_BUTTON) == 0) 
    {
       down_press_cnt++;
    }
    // Кнопка отпущена
    else
    {
        // Короткое нажатие
        if(down_press_cnt > 10 && down_press_cnt < 300)
        {
            down_press_cnt = 0;

            no_operation_counter = 0;

            // НЕ настройки
            if(setting_id == 0)
            {
                // Auto brightness switch
                // adc_light_flag = !adc_light_flag;
                // if(adc_light_flag !=0)
                // {
                //     dis_auto_light_on;
                // }
                // else
                // {
                //     dis_auto_light_off;
                // }

                if(scroll_manual)
                {
                    scroll_manual_step = 1;
                }
                else
                {
                    if(timing_mode_state == TIMING_OFF)
                    {
                        work_mode = MODE_COUNTDOUN;
                        timing_mode_state = TIMING_DOWN;
                        timing_DOWN_flag = true;
                        timing_DOWN_close_flag = false;
                        dis_count_down_on;

                        timing_minute_temp = 0;
                        timing_second_temp = 9; 
                    }
                    else
                    {
                        work_mode = MODE_CLOCK;
                        timing_mode_state = TIMING_OFF;
                        timing_DOWN_flag = false;
                        timing_DOWN_close_flag = true;
                        dis_count_down_off;
                    }
                }
            }

            // The key sound can set the flag bit
            if(beep_flag == 1)
            {
                beep_state = !beep_state;
            }
            
            // The scroll switch can set the flag bit
            if(scroll_flag == 1)
            {
                scroll_state = !scroll_state;
                if(scroll_state != 0)
                {
                    dis_move_on;
                }
                else
                {
                    dis_move_off;
                }
            }

            if(hourly_flag == 1)
            {
                hourly_state = !hourly_state;
            }

            beep_start_judge(1, 100);

			alarm_set(DOWN_flag);
			timing_set(DOWN_flag);
			time_set(DOWN_flag);
        }
        
        // Длинное нажатие и настройки
        else if(down_press_cnt > 300 && setting_id != 0)
        {
            down_press_cnt = 0;

            beep_start_judge(1, 100);

            special_exit();
            EXIT();
			setting_id = 0;
        }
        
        else  down_press_cnt = 0;
    }

    //=========================================================================
    // AutoLight
    if(adc_light_flag != 0 && adc_light_time_flag == 1)
    {
        cancel_repeating_timer(&timer2);
        OE_CLOSE;
    }
    else
    {
        if(adc_light_time_flag == 1)
        {
            adc_light_time_flag = 0;
            cancel_repeating_timer(&timer2);
        }
        OE_CLOSE;
    }

    // row select - 0 - 7
    row_select++;
    if(row_select > 7)
    {
        row_select = 0;
    }

    // Draw one row - 4 bytes - 32 bits
    unsigned char i;
    for(i = 0; i < 4; i++)
    {
        send_data(display_buffer[8 * i + row_select]);
    }

    // Do draw one row - Latch Enable
    LE_HIGH;
    LE_LOW;

    if(row_select & 0x01)A0_HIGH; else A0_LOW;

    if(row_select & 0x02)A1_HIGH; else A1_LOW;

    if(row_select & 0x04)A2_HIGH; else A2_LOW;

    // AutoLight
    if(adc_light_flag != 0)
    {
        adc_light_time_flag = 1;
        add_repeating_timer_us(50, repeating_timer_callback_us, NULL, &timer2);
    }
    else
    {
        OE_OPEN;
    }

    return true;
}

// 1s enter once
bool repeating_timer_callback_s(struct repeating_timer *t) 
{
    // Отрисовка часов
    indicator_state = !indicator_state;
    // if(no_operation_flag == false && scroll_start_count == 0 && adc_show_time == 0){
    //     update_time = 1;
    // }

    // Работа будильника
    if(alarm_start_flag == 1)
    {
        alarm_start_flag = 0;
        gpio_put(BUZZ, 0);
    }

    // Отсчёт секунд
    seconds_counter++;

    // Refresh time every minute
    if(seconds_counter == 60) 
    {
        if(alarm_open_sta != 0)
        {
            if (Time_RTC.dayofweek == alarm_day_select && Ds3231_check_alarm() == true)
            {
                alarm_start_flag = 1;
                gpio_put(BUZZ, 1);
            }
        }

        seconds_counter = 0;
    }

    if(no_operation_flag == true)
    {
        no_operation_counter++;

        if(no_operation_counter == 10 ) // Exit setting mode if there is no operation within 10 seconds
        {
            special_exit();
            EXIT();

			setting_id = 0;

            if(alarm_select_sta == 0 && alarm_open_sta !=0 && alarm_day_select_flag == 1)
            {
                Set_alarm1_clock(ALARM_MODE_HOUR_MIN_SEC_MATCHED,00,alarm_min_temp,alarm_hour_temp,alarm_day_select);
            }

            if(alarm_select_sta != 0 && alarm_open_sta !=0 && alarm_day_select_flag == 1)
            {
                Set_alarm2_clock(alarm_min_temp,alarm_hour_temp,alarm_day_select);
            }

            alarm_day_select_flag = 0;
        }
    }

    // Считаем когда показать бегущую строку
    if(scroll_state != 0)
    {
        scroll_count++;
    }
    else
    {
        scroll_count = 0;
    }

    // Настало время показать бегущую строку
    if(scroll_count == scroll_interval_time && setting_id == 0) 
    {
        scroll_show_flag = 1;
        scroll_count = 0;
        scroll_start = 1;
    }

    if(no_operation_flag == false && scroll_start == 0 && adc_show_time == 0){
        update_time = 1;
    }

    // Похоже на проверку таймера обратного отсчёта
    if(timing_DOWN_flag == true)
    {
        // Если время по нулям - выключаем таймер
        if(timing_second_temp == 0 && timing_minute_temp == 0)
        {
            work_mode = MODE_CLOCK;
            timing_mode_state = TIMING_OFF;
            timing_DOWN_flag = false;
            timing_DOWN_close_flag = true;
            dis_count_down_off;

            beep_start_judge(10, 500);
        }
        // Уменьшаем таймер
        else
        {
            timing_second_temp--;
            
            if(timing_second_temp == 255)
            {
                timing_second_temp = 0;
            }

            if(timing_second_temp == 0 && timing_minute_temp != 0)
            {
                timing_second_temp = 59;
                timing_minute_temp--;
                timing_show_sec = 0;
            }

            // timing_show_count++;
            // if(timing_show_count % 3 == 0)
            // {
            //     timing_show_count = 0;
            //     timing_show_sec = timing_second_temp;
            // }
        }
    }

    if(timing_UP_flag == true)
    {
        timing_second_temp ++;
        
        if(timing_second_temp == 60)
        {
            timing_second_temp = 0;
            timing_minute_temp++;
            if(timing_minute_temp == 60) timing_minute_temp = 0;
        }

        timing_show_count++;
        if(timing_show_count % 3 == 0)
        {
            timing_show_count = 0;
            timing_show_sec = timing_second_temp;
        }
    }

    // Пищать каждый час
    if(hourly_state == 1)
    {
        if(minute_temp == 0 && seconds_counter == 0)
        {            
            beep_start_judge(1, 100);
        }
    }

    return true;
}

// Display the day of the week
void select_weekday(unsigned char x) 
{
    switch(x)
    {
        case 6:   Monday;DisTuesday;DisWednesday;DisThursday;DisFriday;DisSaturday;DisSunday;break;
        case 0:DisMonday;   Tuesday;DisWednesday;DisThursday;DisFriday;DisSaturday;DisSunday;break;
        case 1:DisMonday;DisTuesday;   Wednesday;DisThursday;DisFriday;DisSaturday;DisSunday;break;
        case 2:DisMonday;DisTuesday;DisWednesday;   Thursday;DisFriday;DisSaturday;DisSunday;break;
        case 3:DisMonday;DisTuesday;DisWednesday;DisThursday;   Friday;DisSaturday;DisSunday;break;
        case 4:DisMonday;DisTuesday;DisWednesday;DisThursday;DisFriday;   Saturday;DisSunday;break;
        case 5:DisMonday;DisTuesday;DisWednesday; DisThursday;DisFriday;DisSaturday;  Sunday;break;
    }
}

// Clear the display content after the x position
void clear_display(unsigned char x) 
{
    do
    {
        display_char(x, ' ');
        x += 8;
    } while(x < sizeof(display_buffer));
}

// Отображение одного символа
void display_char(unsigned char x,unsigned char dis_char)
{
    unsigned char i,j,k;
    x+=disp_offset; // Add the offset of the status indicator
    j=x/8; // The number of the dot matrix to be displayed
    k=x%8; // Start to display at the first bit

    if((dis_char>='0')&&(dis_char<='9'))
        dis_char-=0x30;

    else if((dis_char>='A')&&(dis_char<='F'))
        dis_char-=0x37;

    else switch(dis_char)
        {
            case 'H':dis_char=16;break;
            case 'L':dis_char=17;break;
            case 'N':dis_char=18;break;
            case 'P':dis_char=19;break;
            case 'U':dis_char=20;break;
            case ':':dis_char=21;break;
            case ' ':dis_char=24;break;
            case 0:  dis_char=24;break;
            case 'T':dis_char=25;break;
                //  case '-':dis_char=26;break;
            case '.':dis_char=26;break;
            case '-':dis_char=27;break;
            case 'M':dis_char=28;break;
            case '/':dis_char=29;break;
                //case 'V':dis_char=28;break;
                //case 'W':dis_char=29;break;
        }

    for(i=1;i<8;i++)
    {
        if(k>0)
        {
            // reserve required data bits
            display_buffer[8*j+i]=(display_buffer[8*j+i]&(0xff>>(8-k)))|((ZIKU[dis_char*7+i-1])<<k);

            if(j<(sizeof(display_buffer)/8)-1){
                display_buffer[8*j+8+i]=(display_buffer[8*j+8+i]&(0xff<<(8-k)))|((ZIKU[dis_char*7+i-1])>>(8-k));
            }
        }

        else
        {
            display_buffer[8*j+i]=(ZIKU[dis_char*7+i-1]);
        }
    }
}

// Выводит напряжение на дисплей 
void display_adc_vcc()
{
    const float conversion_factor = 3.3f / (1 << 12); // Calculate the voltage
    uint16_t result = adc_read();
    float voltage = 3 *result * conversion_factor;
    uint8_t Single_digit = (int)voltage; // Single digit
    uint8_t Decile = (int)(voltage*10)%10; // Decile
    uint8_t Percentile = (int)(voltage*100)%10; // Percentile

    display_char(0,Single_digit+'0');
    display_char(5,'.');
    display_char(7,Decile+'0');
    display_char(12,Percentile+'0');
    display_char(17,'U');
}

// Отображение версии
void display_version()
{
    display_char(0, '0');
    display_char(5,'.');
    display_char(7, '0');
    display_char(12, '.');
    display_char(14, '1');
}

// Переключение настроек
void dis_set_mode() 
{
    if(setting_id < 3) // set hour and minute
    {
        if(setting_id == 1)
        {
            if(Set_time_hour_flag == 0)
            {
                Set_hour_temp = BCD_to_Byte(Time_RTC.hour);
            }
            Set_time_hour_flag = 1;
        }
        if(setting_id == 2)
        {
            if(Set_time_min_flag == 0)
            {
                Set_min_temp = BCD_to_Byte(Time_RTC.minutes);
            }
            Set_time_min_flag = 1;
        }
        display_char(0,(hour_temp/10+'0')&flag_Flashing[1]);
        display_char(5,(hour_temp%10+'0')&flag_Flashing[1]);
        display_char(10,':');
        display_char(13,(minute_temp/10+'0')&flag_Flashing[2]);
        display_char(18,(minute_temp%10+'0')&flag_Flashing[2]);
    }

    else if(setting_id == 3)
    {
        whole_year = year_high_temp*100 + year_temp;
        Set_time_year_flag = 1;
        display_char(0,year_high_temp/10+0x30);
        display_char(5,year_high_temp%10+0x30);
        display_char(10,((year_temp/10+0x30)&flag_Flashing[3]));
        display_char(15,((year_temp%10+0x30)&flag_Flashing[3]));
        display_char(23,' ');
    }

    else if(setting_id == 4 || setting_id == 5)
    {
        whole_year = year_high_temp*100 + year_temp;
        if(setting_id == 4)
        {
            Set_time_month_flag  = 1;
        }
        if(setting_id == 5)
        {
            Set_time_dayofmonth_flag = 1;
        }
        display_char(0,((month_temp/10+0x30)&flag_Flashing[4]));
        display_char(5,((month_temp%10+0x30)&flag_Flashing[4]));
        display_char(10,'-');
        display_char(13,((dayofmonth_temp/10+0x30)&flag_Flashing[5]));
        display_char(18,((dayofmonth_temp%10+0x30)&flag_Flashing[5]));
    }

    else if(setting_id == 6)
    {
        beep_flag  = 1;
        display_char(0,'B');
        display_char(5,'P');
        display_char(10,'.');

        display_char(13,('0'&flag_Flashing[6]));
        if(beep_state == 1)
        {
            display_char(18,('N'&flag_Flashing[6]));
        }
        else
        {
            display_char(18,('F'&flag_Flashing[6]));
        }

        clear_display(26);
    }

    else if(setting_id == 7)
    {
        scroll_flag = 1;
        display_char(0,'D');
        display_char(5,'P');
        display_char(10,'.');
        display_char(13,('0'&flag_Flashing[7]));
        if(scroll_state != 0)
        {
            display_char(18,('N'&flag_Flashing[7]));
        }
        else
        {
            display_char(18,('F'&flag_Flashing[7]));
        }
        clear_display(26);

    }
    else if(setting_id == 8)
    {
        if(Time_set_mode_flag == 0)
        {
            Set_hour_temp = BCD_to_Byte(Time_RTC.hour);
        }
        Time_set_mode_flag = 1;
        display_char(0,'M');
        display_char(6,'D');
        display_char(11,'.');
        if(Time_set_mode_sta == 0)
        {
            display_char(14,'2'&flag_Flashing[8]);
        }
        else
        {
            display_char(14,'1'&flag_Flashing[8]);
        }
        
        clear_display(23);
    }

    else if(setting_id == 9)
    {
        hourly_flag = 1;
        display_char(0,'F');
        display_char(5,'T');
        display_char(10,'.');
        display_char(13,'0'&flag_Flashing[9]);
        if(hourly_state != 0)
        {
             display_char(18,'N'&flag_Flashing[9]);
             dis_hourly_on;
        }
        else
        {
             display_char(18,'F'&flag_Flashing[9]);
             dis_hourly_off;
        }
        clear_display(26);
    }

    else
    {
        no_operation_counter = 0;
        no_operation_flag = false;
        KEY_Set_flag = 0;
        setting_id = 0;
        update_time = 1;
        beep_flag = 0;
        scroll_flag = 0;
    }
}

// Отобразить настройки будильника
void dis_alarm()
{
    if(setting_id == 1 || setting_id == 2)
    {
        if(setting_id == 1)
        {
          alarm_select_flag = 1;  
        }
        if(setting_id == 2)
        {
            alarm_open_flag = 1;
        }
        display_char(0,'A');
        if(alarm_select_sta == 0)
        {
            display_char(5,'0'&flag_Flashing[1]);
        }
        else
        {
            display_char(5,'1'&flag_Flashing[1]);
        }

        display_char(10,'.');
        display_char(13,'0'&flag_Flashing[2]);
        if(alarm_open_sta != 0)
        {
            display_char(18,'N'&flag_Flashing[2]);          
        }
        else
        {
            display_char(18,'F'&flag_Flashing[2]);
        }

        clear_display(26);
    }

    else if(setting_id == 3 || setting_id == 4)
    {
        if(setting_id == 3)
        {
            alarm_hour_flag = 1;
        }
        if(setting_id == 4)
        {
            alarm_min_flag = 1;
        }
        display_char(0,(alarm_hour_temp/10+'0') &flag_Flashing[3]);
        display_char(5,(alarm_hour_temp%10+'0')&flag_Flashing[3]);
        display_char(10,':');
        display_char(13,(alarm_min_temp/10+'0')&flag_Flashing[4]);
        display_char(18,(alarm_min_temp%10+'0')&flag_Flashing[4]);
        clear_display(26);

    }

    else if(setting_id == 5)
    {
        alarm_day_select_flag = 1;
        if(alarm_day_select == 1)
        {
            select_weekday(0);
        }
        else if(alarm_day_select == 2)
        {
            select_weekday(1);
        }
        else if(alarm_day_select == 3)
        {
            select_weekday(2);
        }
        else if(alarm_day_select == 4)
        {
            select_weekday(3);
        }
        else if(alarm_day_select == 5)
        {
            select_weekday(4);
        }
        else if(alarm_day_select == 6)
        {
            select_weekday(5);
        }
        else
        {
            select_weekday(6);
        }

    }

    else{
        if(alarm_select_sta == 0 && alarm_open_sta !=0)
        {
            Set_alarm1_clock(ALARM_MODE_HOUR_MIN_SEC_MATCHED,00,alarm_min_temp,alarm_hour_temp,alarm_day_select);
        }
        if(alarm_select_sta !=0 && alarm_open_sta !=0)
        {
            Set_alarm2_clock(alarm_min_temp,alarm_hour_temp,alarm_day_select);
        }

        no_operation_counter = 0;
        no_operation_flag = false;
        setting_id = 0;
        alarm_flag = 0;
        alarm_id = 0;
        update_time = 1;
        beep_flag = 0;
        scroll_flag = 0;
        alarm_select_flag = 0;
        alarm_open_flag = 0;
        alarm_hour_flag = 0;
        alarm_min_flag = 0;
        alarm_day_select_flag = 0;
        scroll_count = 0;
    }
}

// Отобразить настройки и работу отсчёта
void display_timing()
{
    // Настройки Timing modes
    if(setting_id == 1)
    {
        timing_mode_flag = 1;
        display_char(0,'T');
        display_char(5,'M');
        display_char(11,'.');
        if(timing_mode_state == TIMING_UP)
        {
            display_char(13,'U'&flag_Flashing[1]);
            display_char(18,'P'&flag_Flashing[1]);
            dis_count_up_on;
            dis_count_down_off;
        }
        else if(timing_mode_state == TIMING_DOWN)
        {
            display_char(13,'D'&flag_Flashing[1]);
            display_char(18,'N'&flag_Flashing[1]);
            dis_count_down_on;
            dis_count_up_off;
        }
        else if(timing_mode_state == TIMING_OFF)
        {
            display_char(13,'0'&flag_Flashing[1]);
            display_char(18,'F'&flag_Flashing[1]);
            dis_count_down_off;
            dis_count_up_off;
        }
        clear_display(26);
    }

    // Настройки Configure timing 2 Minute 3 Second
    else if(setting_id == 2 || setting_id == 3 && timing_mode_state != TIMING_OFF)
    {
        // Minute
        if(setting_id == 2 && timing_mode_state == TIMING_DOWN)
        {
            timing_minute_flag = 1;
        }
        // Minute
        else if (setting_id == 2 && timing_mode_state == TIMING_UP)
        {
            if(timing_UP_flag == false)
            {
                timing_minute_temp = 0;
                timing_second_temp = 0;
            }

            no_operation_counter = 0;
            timing_UP_flag = true;

            display_char(0,timing_minute_temp/10+'0');
            display_char(5,timing_minute_temp%10+'0');
            display_char(10,':');
            display_char(13,timing_show_sec/10+'0');
            display_char(18,timing_show_sec%10+'0');
            
            clear_display(26);
        }

        // Second
        if(setting_id == 3 && timing_mode_state == TIMING_DOWN)
        {
            timing_second_flag = 1;
            timing_DOWN_close_flag = false;
        }

        if(timing_mode_state == TIMING_DOWN)
        {
            display_char(0,timing_minute_temp/10+'0'&flag_Flashing[2]);
            display_char(5,timing_minute_temp%10+'0'&flag_Flashing[2]);
            display_char(10,':');
            display_char(13,timing_second_temp/10+'0'&flag_Flashing[3]);
            display_char(18,timing_second_temp%10+'0'&flag_Flashing[3]);

            clear_display(26);

            timing_show_sec = timing_second_temp;
        }

        // Second
        if(timing_mode_state == TIMING_UP && setting_id == 3)
        {
            display_char(0,timing_minute_temp/10+'0');
            display_char(5,timing_minute_temp%10+'0');
            display_char(10,':');
            display_char(13,timing_second_temp/10+'0');
            display_char(18,timing_second_temp%10+'0');

            clear_display(26);

            timing_show_sec = 0;
            timing_show_count = 0;
            timing_UP_flag = false;
        }
    }

    // Настройки Countdown display
    else if(setting_id == 4 && timing_mode_state == TIMING_DOWN && timing_DOWN_close_flag == false) 
    {
        no_operation_counter = 0;
        timing_DOWN_flag = true;

        display_char(0,timing_minute_temp/10+'0');
        display_char(5,timing_minute_temp%10+'0');
        display_char(10,':');
        display_char(13,timing_show_sec/10+'0');
        display_char(18,timing_show_sec%10+'0');

        clear_display(26);
    }

    // Режим таймера
    else if (work_mode == MODE_COUNTDOUN)
    {
        display_char(0,timing_minute_temp / 10 + '0');
        display_char(5,timing_minute_temp % 10 + '0');
        if(indicator_state) {
            display_char(10,':');
        } else {
            display_char(10,' ');
        }
        display_char(13,timing_second_temp / 10 + '0');
        display_char(18,timing_second_temp % 10 + '0');

        clear_display(26);
    }

    // Нихуя не понял логику
    else
    {
        if(timing_mode_state == TIMING_UP)
        {
            timing_mode_state = TIMING_OFF;
            timing_minute_temp = 0;
            timing_second_temp = 0;
            dis_count_up_off;
        }

        no_operation_counter = 0;
        no_operation_flag = false;
        setting_id = 0;
        UP_id = 0;
        UP_Key_flag = 0;
        update_time = 1;
        beep_flag = 0;
        scroll_flag = 0;
        scroll_count = 0;
        timing_second_flag = 0;
    }
}

// Display time
void display_time()
{
    // Get the value of RTC
    Time_RTC=Read_RTC(); 
    
    display_char(0,'1');

    Time_RTC.seconds = Time_RTC.seconds & 0x7F;
    Time_RTC.minutes = Time_RTC.minutes & 0x7F;
    Time_RTC.hour = Time_RTC.hour & 0x3F;
    Time_RTC.dayofweek = Time_RTC.dayofweek & 0x07;
    Time_RTC.dayofmonth = Time_RTC.dayofmonth & 0x3F;
    Time_RTC.month = Time_RTC.month & 0x1F;

    Set_hour_temp = BCD_to_Byte(Time_RTC.hour);
    minute_temp = BCD_to_Byte(Time_RTC.minutes);
    dayofmonth_temp = BCD_to_Byte(Time_RTC.dayofmonth);
    month_temp = BCD_to_Byte(Time_RTC.month);
    year_temp = BCD_to_Byte(Time_RTC.year);
    
    // When changing the time, you need to pay attention to the current time mode
    if(Time_set_mode_sta != 0)
    {
        if(Set_hour_temp > 12)
        {
            hour_temp = Set_hour_temp -12;
            dis_PM_on;
            dis_AM_off;
        }
        else if(Set_hour_temp == 12)
        {
            dis_PM_on;
            dis_AM_off;
        }
        else
        {
            hour_temp = Set_hour_temp;
            dis_AM_on;
            dis_PM_off;
        }        
    }
    else
    {
        hour_temp = Set_hour_temp;
    }

    Time_buf[0]=((hour_temp/10)+'0');
    Time_buf[1]=((hour_temp%10)+'0');
    Time_buf[2]=((Time_RTC.minutes/16)+'0');
    Time_buf[3]=((Time_RTC.minutes%16)+'0');
    
    seconds_counter=((float)Time_RTC.seconds)/1.5; // Calculate the number of seconds in the current RTC
    
    if(scroll_start == 0)
    {
        display_char(0,Time_buf[0]);
        display_char(5,Time_buf[1]);
        if(indicator_state) {
            display_char(10,':');
        } else {
            display_char(10,' ');
        }
        display_char(13,Time_buf[2]);
        display_char(18,Time_buf[3]);
    }

    if(Time_RTC.dayofweek==1){select_weekday(0);}
    else if (Time_RTC.dayofweek==2){select_weekday(1);}
    else if (Time_RTC.dayofweek==3){select_weekday(2);}
    else if (Time_RTC.dayofweek==4){select_weekday(3);}
    else if (Time_RTC.dayofweek==5){select_weekday(4);}
    else if (Time_RTC.dayofweek==6){select_weekday(5);}
    else select_weekday(6);
}

// Отобразить бегущую строку - время - дата - температура
void display_scroll()
{
    uint8_t temperature_offset = 0;

    if(scroll_show_flag == 1)
    {   
        display_char(32,'2');
        display_char(37,'0');
        display_char(42,(year_temp/10+0x30));
        display_char(47,(year_temp%10+0x30));
        display_char(52,'-');
        display_char(55,Time_RTC.month/16+'0');
        display_char(60,Time_RTC.month%16+'0');
        display_char(65,'-');
        display_char(68,Time_RTC.dayofmonth/16+'0');
        display_char(73,Time_RTC.dayofmonth%16+'0');

        if(temperature_unit > 0)
        {
            get_temperature();

            if(temperature_unit == 1)
            {
                display_char(85, temp_high/10+0x30);
                display_char(90, temp_high%10+0x30);
                display_char(95, '.');
                display_char(97, temp_low/10+0x30);
                display_char(102,'C');
            }
            else if(temperature_unit == 2)
            {
                unsigned char T = temp_high*9/5 +32;
                if(T>=100)T=99;
                display_char(85, T/10+0x30);
                display_char(90, T%10+0x30);
                display_char(95, '.');
                display_char(97, temp_low/10+0x30);
                display_char(102,'F');
            }
        }
        else
        {
            temperature_offset = 17;
        }
        
        scroll_show_flag = 0;
    }

    // Если показали всю строку, то показываем время
    if(scroll_show_start == sizeof(display_buffer)-36)
    {
        display_char(32,' ');
        display_char(40,' ');
        display_char(48,' ');

        display_char(36,hour_temp/10+0x30);
        display_char(41,hour_temp%10+0x30);
        display_char(46,':');
        display_char(49,Time_RTC.minutes/16+0x30);
        display_char(54,Time_RTC.minutes%16+0x30);
    }

    // Что тут происходит?
    for(i = 1; i < 8; i++)
    {
        save_buf = display_buffer[i] & 0x03; // Retain function bits

        for(jr = 0; jr < sizeof(display_buffer)/8; jr++)
        {
            if(jr < sizeof(display_buffer) / 8-1)
                display_buffer[8*jr+i] = display_buffer[8*jr+i] >> 1 | display_buffer[8*jr+i+8] << 7;
            else
                display_buffer[8*jr+i] = display_buffer[8*jr+i] >> 1;
        }

        display_buffer[i] = (display_buffer[i] & (~0x03)) | save_buf; // Restore function bit
    } 
}

// Determine the maximum number of days in a month
unsigned char get_month_date(uint16_t year_cnt,uint8_t month_cnt)
{
    if((year_cnt%4==0&&year_cnt%100!=0)||year_cnt%400==0)
    {
        switch(month_cnt)
        {
            case 1:return month_date[0][0];break;
            case 2:return month_date[0][1];break;
            case 3:return month_date[0][2];break;
            case 4:return month_date[0][3];break;
            case 5:return month_date[0][4];break;
            case 6:return month_date[0][5];break;
            case 7:return month_date[0][6];break;
            case 8:return month_date[0][7];break;
            case 9:return month_date[0][8];break;
            case 10:return month_date[0][9];break;
            case 11:return month_date[0][10];break;
            case 12:return month_date[0][11];break;
        }    
           
    }
    else
    {
        switch(month_cnt)
        {
            case 1:return month_date[1][0];break;
            case 2:return month_date[1][1];break;
            case 3:return month_date[1][2];break;
            case 4:return month_date[1][3];break;
            case 5:return month_date[1][4];break;
            case 6:return month_date[1][5];break;
            case 7:return month_date[1][6];break;
            case 8:return month_date[1][7];break;
            case 9:return month_date[1][8];break;
            case 10:return month_date[1][9];break;
            case 11:return month_date[1][10];break;
            case 12:return month_date[1][11];break;
        }    
    }
}

// Determine the day of the week according to the year, month and day
unsigned char get_weekday(uint16_t year_cnt,uint8_t month_cnt,uint8_t date_cnt)
{
    uint8_t weekday = 8;
    if(month_cnt == 1 || month_cnt == 2)
    {
        month_cnt += 12;
        year_cnt--;
    }
    weekday = (date_cnt+1+2*month_cnt+3*(month_cnt+1)/5+year_cnt+year_cnt/4-year_cnt/100+year_cnt/400)%7;
    switch(weekday)
    {
        case 0 : return 7; break;
        case 1 : return 1; break;
        case 2 : return 2; break;
        case 3 : return 3; break;
        case 4 : return 4; break;
        case 5 : return 5; break;                                                             
        case 6 : return 6; break;
    }
}

//
void alarm_set(uint8_t UP_DOWN_flag)
{
	if(alarm_hour_flag == 1) // The alarm clock can set the flag bit
	{
        if(UP_DOWN_flag == UP_flag)
        {
            alarm_hour_temp ++;
            if(alarm_hour_temp == 24)alarm_hour_temp = 0;
        }
		else
        {
            alarm_hour_temp --;
		    if(alarm_hour_temp == 255)alarm_hour_temp = 23;
        }   	
	}

	if(alarm_min_flag == 1)
	{
        if(UP_DOWN_flag == UP_flag)
        {
            alarm_min_temp ++;
		    if(alarm_min_temp == 60)alarm_min_temp = 0;
        }
        else
        {
            alarm_min_temp --;
		    if(alarm_min_temp == 255)alarm_min_temp = 59;
        }
	}

	if(alarm_open_flag == 1) // The alarm switch can set the flag bit
	{
		alarm_open_sta = !alarm_open_sta;
		if(alarm_open_sta != 0)
		{
			dis_alarm_on;
		}
		else
		{
			dis_alarm_off;
		}
	}

	if(alarm_select_flag == 1) // The alarm clock selection can set the flag bit
	{
		alarm_select_sta = ! alarm_select_sta;
	}

	if(alarm_day_select_flag == 1)
	{
        if(UP_DOWN_flag == UP_flag)
        {
            alarm_day_select++;
		    if(alarm_day_select == 8)alarm_day_select =1;
        }
        else
        {
            alarm_day_select --;
		    if(alarm_day_select == 0)alarm_day_select = 7;
        }
		
	}
}

//
void timing_set(uint8_t UP_DOWN_flag)
{
	if(timing_mode_flag == 1)
	{
        if(UP_DOWN_flag == UP_flag)
        {
            timing_mode_state++;
		    if(timing_mode_state == 3) timing_mode_state = 0;
        }
        else
        {
            timing_mode_state--;
            if(timing_mode_state == 255)timing_mode_state = 2;
        }
		
	}

	if(timing_mode_state == 1 && timing_minute_flag == 1)
	{
		if(UP_DOWN_flag == UP_flag)
		{
			timing_minute_temp ++;
			if(timing_minute_temp == 60)timing_minute_temp = 0;
		}
		else
		{
			timing_minute_temp --;
			if(timing_minute_temp == 255)timing_minute_temp = 59;
		}
	}

	if(timing_mode_state == 1 && timing_second_flag == 1)
	{
		if(UP_DOWN_flag == UP_flag)
		{
			timing_second_temp ++;
			if(timing_second_temp == 60)timing_second_temp = 0;
		}
		else
		{
			timing_second_temp --;
			if(timing_second_temp == 255)timing_second_temp = 59;
		}
	}

	if(Time_set_mode_flag == 1) // time mode setting
	{
		Time_set_mode_sta = !Time_set_mode_sta;
		if(Time_set_mode_sta == 0)
		{
			dis_AM_off;
			dis_PM_off;
		}
		else
		{
			if(Set_hour_temp > 12)
			{
				hour_temp = Set_hour_temp - 12;
				dis_PM_on;
				dis_AM_off;
			}
			else if(Set_hour_temp == 12)
			{
				dis_PM_on;
				dis_AM_off;
			}
			else
			{
				dis_AM_on;
				dis_PM_off;
			}
		}
	}
}

//
void time_set(uint8_t UP_DOWN_flag)
{
	if(Set_time_hour_flag == 1) // hour setting
	{
		change_time_flag = 1;
		if(UP_DOWN_flag == UP_flag)
		{
			Set_hour_temp++;
			if(Set_hour_temp == 24)Set_hour_temp = 0;
		}
		else
		{
			Set_hour_temp--;
			if(Set_hour_temp == 255)Set_hour_temp = 23;
		}
        // When changing the time, you need to pay attention to the current time mode
		if(Time_set_mode_sta != 0)
		{
			if(Set_hour_temp > 12)
			{
				hour_temp = Set_hour_temp -12;
				dis_PM_on;
				dis_AM_off;
			}
			else if(Set_hour_temp == 12)
			{
				hour_temp = Set_hour_temp;
				dis_PM_on;
				dis_AM_off;
			}
			else
			{
				hour_temp = Set_hour_temp;
				dis_AM_on;
				dis_PM_off;
			}
				
		}
		else
		{
			hour_temp = Set_hour_temp;
		}
	}

	if(Set_time_min_flag == 1) // minute design
	{
		change_time_flag = 1;
		if(UP_DOWN_flag == UP_flag)
		{
			Set_min_temp++;
			if(Set_min_temp == 60)Set_min_temp = 0;
		}
		else
		{
			Set_min_temp--;
			if(Set_min_temp == 255)Set_min_temp = 59;
		}
		minute_temp = Set_min_temp;
	}

	if(Set_time_year_flag == 1)
	{
		change_time_flag = 1;
		if(UP_DOWN_flag == UP_flag)
		{
			year_temp ++;
			if(year_temp == 100)
			{
				year_temp = 0;
				year_high_temp ++;
			}
		}
		else
		{
			year_temp --;
			if(year_temp == 255)
			{
				year_temp = 99;
				year_high_temp --;
			}
		}
		
	}

	if(Set_time_month_flag == 1)
	{
		change_time_flag = 1;
		if(UP_DOWN_flag == UP_flag)
		{
			month_temp ++ ;
			if(month_temp == 13) month_temp = 1;
		}
		else
		{
			month_temp -- ;
			if(month_temp == 0) month_temp = 12;
		}
	}

	if(Set_time_dayofmonth_flag == 1)
	{
		change_time_flag = 1;
		if(UP_DOWN_flag == UP_flag)
		{
			dayofmonth_temp++;
			if(dayofmonth_temp > get_month_date(whole_year,month_temp))
			{
				dayofmonth_temp = 0;
			}
		}
		else
		{
			dayofmonth_temp--;
			if(dayofmonth_temp == 0)
			{
				dayofmonth_temp = get_month_date(whole_year,month_temp);
			}
		}
	}
}

//
void adc_show_count()
{
	if(adc_light_flag != 0)
    {
        adc_light_count++;
        if(adc_light_count == 1000)
        {
            adc_light_count = 0;
            adc_light = get_ads1015();
        }
    }
    if(adc_show_time != 0)
    {
        adc_count ++ ;
        if(adc_count == 500)
        {
            adc_count = 0;
            adc_show_flag = 1;
        }
    }
}

// Для запуска сигнала - запускается вручную
void beep_start_judge(uint8_t repeat, uint16_t duration)
{
	if(beep_state != 1)
    {
        return;
    }
    
    beep_on_flag = 1;
    beep_repeat = repeat;
    beep_repeat_counter = 0;
    beep_duration = duration;
    beep_duration_counter = 0;

    beep_on_step = 1;
    gpio_put(BUZZ, 1);
}

// Для остановки (мигания) сигнала - запускается по таймеру
void beep_stop_judge()
{
    if(beep_on_flag == 0)
    {
        return;
    }

    beep_duration_counter++;
    if(beep_duration_counter >= beep_duration)
    {
        beep_duration_counter = 0;

        beep_on_step = !beep_on_step;
        gpio_put(BUZZ, beep_on_step);

        if(!beep_on_step)
        {
            beep_repeat_counter++;

            if(beep_repeat_counter >= beep_repeat)
            {
                beep_on_flag = 0;
                gpio_put(BUZZ, 0);
            }
        }
    }
}

//
void flashing_start_judge()
{
	if(setting_id != 0)
	{
        Flashing_count ++;
        if(Flashing_count == 600)
        {
            Flashing_count = 0;
            flag_Flashing[setting_id] = ~flag_Flashing[setting_id];

            if(alarm_id == 1)
                alarm_flag = 1;
            else if(UP_id == 1)
                UP_Key_flag = 1;
            else
                KEY_Set_flag = 1;
        }
    }
}

//
void scroll_show_judge()
{
    if(scroll_manual)
    {
        if(scroll_manual_step)
        {
            scroll_manual_step = 0;
            scroll_start_count = scroll_speed;
        }
    }
    else
    {
        if(scroll_start == 1)
        {
            scroll_start_count++;
        }
    }

    if(scroll_start_count >= scroll_speed)
    {
        if(scroll_show_start < sizeof(display_buffer))
        {
            display_scroll(); 

            scroll_show_start++;
        }
        else
        {
            display_char(24,' ');

            scroll_start = 0;
            scroll_show_start = 0;
            update_time = 1;
        }    
        
        scroll_start_count = 0;
    }
}

// Нахера?!
void EXIT()
{
	if(Set_time_hour_flag ==1 && change_time_flag == 1)
	{
		set_hour(Set_hour_temp);
	}

	if(Set_time_min_flag == 1 && change_time_flag == 1)
	{
		set_min(Set_min_temp);
	}

	if(Set_time_year_flag == 1 && change_time_flag == 1)
	{
		set_year(year_temp);
		set_dayofweekday(get_weekday(whole_year,month_temp,dayofmonth_temp));
		select_weekday(get_weekday(whole_year,month_temp,dayofmonth_temp)-1);
	}

	if(Set_time_month_flag == 1 && change_time_flag == 1)
	{
		set_month(month_temp);
		set_dayofweekday(get_weekday(whole_year,month_temp,dayofmonth_temp));
		select_weekday(get_weekday(whole_year,month_temp,dayofmonth_temp)-1);
	}

	if(Set_time_dayofmonth_flag == 1 && change_time_flag ==1)
	{
		set_dayofmouth(dayofmonth_temp);
		set_dayofweekday(get_weekday(whole_year,month_temp,dayofmonth_temp));
		select_weekday(get_weekday(whole_year,month_temp,dayofmonth_temp)-1);
	}

    flag_Flashing[setting_id] = 0xff;
    
    if(alarm_min_flag == 1) // prevent showing empty 
    {
        display_char(13,(alarm_min_temp/10+'0')&flag_Flashing[4]);
        display_char(18,(alarm_min_temp%10+'0')&flag_Flashing[4]);
    }

    if(timing_mode_flag == 1 && timing_mode_state == 2)
    {
        display_char(13,'0'&flag_Flashing[1]);
        display_char(18,'F'&flag_Flashing[1]);
    }

	no_operation_counter = 0;
    scroll_flag = 0;
	beep_flag = 0;
	alarm_select_flag = 0;
	alarm_open_flag = 0;
	alarm_hour_flag = 0;
	alarm_min_flag = 0;
	timing_mode_flag = 0;
	timing_minute_flag  = 0;
	timing_second_flag = 0;
	Time_set_mode_flag = 0;
	hourly_flag = 0;
	Set_time_hour_flag = 0;
	Set_time_min_flag = 0;
	Set_time_year_flag = 0;
	Set_time_month_flag = 0;
	Set_time_dayofmonth_flag = 0;
	change_time_flag = 0;
}

// А это тем-более нахера?!
void special_exit()
{
	no_operation_flag = false;
	KEY_Set_flag = 0;
	alarm_flag = 0;
	alarm_id = 0;
	UP_id = 0;
	UP_Key_flag = 0;
	update_time = 1;
	scroll_count = 0;
}

//
void get_temperature()
{
    unsigned char start_tran[2] = {0x0E,0x20};

    i2c_write_blocking(I2C_PORT,Address,start_tran,2,false);
    i2c_write_blocking(I2C_PORT, Address, &get_add_high, 1, true);
    i2c_read_blocking(I2C_PORT,Address, &temp_high,1,false);
    i2c_write_blocking(I2C_PORT, Address, &get_add_low, 1, true);
    i2c_read_blocking(I2C_PORT,Address, &temp_low,1,false);
    
    temp_low = (temp_low >> 6)*25; // Enlarge the resolution
}

// Get the value of the photosensitive sensor
uint16_t get_ads1015() 
{
    adc_select_input(0);
    uint16_t value = adc_read();
	return value;
}
