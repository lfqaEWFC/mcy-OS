#ifndef __DEVICE_TIME_H
#define __DEVICE_TIME_H

#include "stdint.h"
#include "io.h"
#include "print.h"
#include "interrupt.h"
#include "thread.h"
#include "debug.h"

void timer_init(void);
void frequency_set(uint8_t counter_port ,uint8_t counter_no,uint8_t rwl, \
uint8_t counter_mode,uint16_t counter_value);
void mtime_sleep(uint32_t m_seconds);
void ticks_to_sleep(uint32_t sleep_ticks);

#endif
