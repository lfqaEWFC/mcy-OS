#ifndef __DEVICE_KEYBOARD_H
#define __DEVICE_KEYBOARD_H

#include "io.h"
#include "console.h"
#include "interrupt.h"
#include "stdint.h"
#include "stdbool.h"
#include "ioquene.h"

extern struct ioqueue kbd_buf;

/* 键盘初始化 */
void keyboard_init(void); 

#endif
