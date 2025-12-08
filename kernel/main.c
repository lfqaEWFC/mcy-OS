#include "print.h"
#include "init.h"
#include "debug.h"
#include "string.h"
#include "memory.h"
#include "thread.h"
#include "interrupt.h"
#include "console.h"
#include "keyboard.h"
#include "process.h"
#include "syscall.h"

int a=0,b=0;
void test_thread1(void* arg);
void test_thread2(void* arg);
void u_prog_a(void);
void u_prog_b(void);

int main(void) {
   put_str("I am kernel\n");
   init_all();
   process_execute(u_prog_a,"user_prog_a");
   process_execute(u_prog_b,"user_prog_b");
   intr_enable();
   
   console_put_str(" main_pid:0x");
   console_put_int(sys_getpid());
   console_put_char('\n');
   
   //thread_start("kernel_thread_a",31,test_thread1," thread_A:0x");
   //thread_start("kernel_thread_b",31,test_thread2," thread_B:0x");
   
   while(1);
   return 0;
}

void test_thread1(void* arg)
{
    console_put_str((char*)arg);
    console_put_int(getpid());
    console_put_char('\n');
    console_put_str(" u_prog_a:0x");
    console_put_int(a);
    console_put_char('\n');
    while(1);
}

void test_thread2(void* arg)
{
    console_put_str((char*)arg);
    console_put_int(getpid());
    console_put_char('\n');
    console_put_str(" u_prog_b:0x");
    console_put_int(b);
    console_put_char('\n');
    while(1);
}

void u_prog_a(void)
{
    write("u_prog_a say hello ~~\n");
    while(1);
}

void u_prog_b(void)
{
    write("u_prog_b say hello ~~\n");
    while(1);
}
