#ifndef __LIB_USER_SCSCALL_H
#define __LIB_USER_SCSCALL_H

#include "stdint.h"

enum SYSCALL_NR
{
    SYS_GETPID
};

uint32_t getpid(void);

#endif
