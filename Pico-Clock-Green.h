#ifndef PICO_CLOCK_GREEN_H
#define PICO_CLOCK_GREEN_H

// const char CLOCK_VERSION[5] = "1.0.10";
#define CLOCK_VERSION "1.0.10"

void switch_on_countdown_mode(unsigned char minute, unsigned char second);
void switch_off_countdown_mode();

#endif // PICO_CLOCK_GREEN_H