#ifndef __LIB_USER_SCSCALL_H
#define __LIB_USER_SCSCALL_H

#include "stdint.h"

enum SYSCALL_NR
{
    SYS_GETPID,
    SYS_WRITE
};

uint32_t getpid(void);
uint32_t write(char* str);
uint32_t sys_write(char* str);

#endif
