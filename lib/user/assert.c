#include "assert.h"
#include "stdio.h"

void user_spin(char *filename, int line, const char *func, const char *condition)
{
    printf("\n\n\n========= Error =========\n");
    printf("filename: %s\n", filename);
    printf("line: 0x%x\n", line);
    printf("function: %s\n", (char*)func);
    printf("condition:  %s\n", (char*)condition);
    printf("========= Error =========\n");
    while (1);
}
