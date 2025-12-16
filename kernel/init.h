#ifndef __KERNEL_INIT_H
#define __KERNEL_INIT_H

#include "tss.h"
#include "print.h"
#include "interrupt.h"
#include "timer.h"
#include "memory.h"
#include "thread.h"
#include "console.h"
#include "keyboard.h"
#include "syscall-init.h"
#include "ide.h"

void init_all(void);

#endif
