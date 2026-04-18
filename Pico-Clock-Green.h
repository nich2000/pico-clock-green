#ifndef PICO_CLOCK_GREEN_H
#define PICO_CLOCK_GREEN_H

#include <stdbool.h>

// const char CLOCK_VERSION[5] = "1.0.10";
#define CLOCK_VERSION "1.0.10"

void switch_on_countdown_mode(unsigned char minute, unsigned char second);
void switch_off_countdown_mode();
bool set_clock_time_from_network(unsigned char hour, unsigned char minute, unsigned char second);
void beep_start(uint8_t repeat, uint16_t duration);

#endif // PICO_CLOCK_GREEN_H
