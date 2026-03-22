//
// Created by yufu on 2021/1/23.
//

#ifndef DEFINE_H
#define DEFINE_H

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"

//-----define IO------------------------------

#define	OE	         13
#define	OE_OPEN		 gpio_put(OE, 0)
#define	OE_CLOSE	 gpio_put(OE, 1)

#define	SDI	         11
#define	SDI_LOW		 gpio_put(SDI, 0)
#define	SDI_HIGH	 gpio_put(SDI, 1)

// Clock
#define	CLK	         10
#define	CLK_LOW		 gpio_put(CLK, 0)
#define	CLK_HIGH	 gpio_put(CLK, 1)

// Latch Enable
#define	LE	         12
#define	LE_LOW		 gpio_put(LE, 0)
#define	LE_HIGH		 gpio_put(LE, 1)

#define	A0	         16
#define	A0_LOW		 gpio_put(A0, 0)
#define	A0_HIGH		 gpio_put(A0, 1)

#define	A1	         18
#define	A1_LOW		 gpio_put(A1,0)
#define	A1_HIGH		 gpio_put(A1, 1)

#define	A2	         22
#define	A2_LOW		 gpio_put(A2, 0)
#define	A2_HIGH		 gpio_put(A2, 1)

#define SDA          6
#define SCL          7

#define SET_BUTTON   2
#define UP_BUTTON    17
#define DOWN_BUTTON  15

#define SQW          3
#define BUZZ         14

// АЦП Аналого-цифровой преобразователь Analog-to-digital converter
#define ADC_Light    26
#define ADC_VCC      29

#define UP_flag      1
#define DOWN_flag    0

//------------Define the number used by the left status indicator---------
#define	disp_offset		2

//definition IIC
#define I2C_PORT        i2c1
#define Address         0x68
#define Address_ADS     0x48

//----------------
// Back Light
#define back_light_on           disp_buf[0]|=(1<<2)|(1<<5)
#define back_light_off          disp_buf[0]&=~((1<<2)|(1<<5))

//----------------Day of the week LED indicator definition -------------------------
#define Monday                  {disp_buf[0]|=(1<<3)|(1<<4);}
#define DisMonday               {disp_buf[0] &= ~((1<<3)|(1<<4));}

#define Tuesday                 {disp_buf[0]|=(1<<6)|(1<<7);}
#define DisTuesday              {disp_buf[0] &= ~((1<<6)|(1<<7));}

#define Wednesday               {disp_buf[8]|=(1<<1)|(1<<2);}
#define DisWednesday            {disp_buf[8] &= ~((1<<1)|(1<<2));}

#define Thursday                {disp_buf[8]|=(1<<4)|(1<<5);}
#define DisThursday             {disp_buf[8] &= ~((1<<4)|(1<<5));}

#define Friday                  {disp_buf[8]|=(1<<7);disp_buf[16]|=(1<<0);}
#define DisFriday               {disp_buf[8] &= ~(1<<7);disp_buf[16] &= ~(1<<0);}

#define Saturday                {disp_buf[16]|=(1<<2)|(1<<3);}
#define DisSaturday             {disp_buf[16]&= ~((1<<2)|(1<<3));}

#define Sunday                  {disp_buf[16]|=(1<<5)|(1<<6);}
#define DisSunday               {disp_buf[16] &= ~((1<<5)|(1<<6));}

//----------------Status LED Indicator Definitions -------------------------
// Move On - scroll
#define dis_move_on             disp_buf[0]|= 0X03
#define dis_move_off            disp_buf[0] &= ~0X03
// Alarm On
#define dis_alarm_on            disp_buf[1]|= 0X03
#define dis_alarm_off           disp_buf[1] &= ~0x03
// CountDown
#define dis_count_down_on       disp_buf[2]|= 0X03
#define dis_count_down_off      disp_buf[2] &= ~0x03
// F
#define dis_F_on                disp_buf[3]|= (1<<0)
#define dis_F_off               disp_buf[3] &= ~(1<<0)
// C
#define dis_C_on                disp_buf[3]|= (1<<1)
#define dis_C_off               disp_buf[3] &= ~(1<<1)
// AM
#define dis_AM_on               disp_buf[4]|=(1<<0)
#define dis_AM_off              disp_buf[4] &= ~(1<<0)
// PM
#define dis_PM_on               disp_buf[4]|= (1<<1)
#define dis_PM_off              disp_buf[4] &= ~(1<<1)
// CountUp
#define dis_count_up_on         disp_buf[5]|=0X03
#define dis_count_up_off        disp_buf[5] &= ~0x03
// Hourly
#define dis_hourly_on           disp_buf[6]|= 0X03
#define dis_hourly_off          disp_buf[6] &= ~0X03
// AutoLight
#define dis_auto_light_on       disp_buf[7]|= 0X03
#define dis_auto_light_off      disp_buf[7] &= ~0X03

#endif
