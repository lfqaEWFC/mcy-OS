#include "timer.h"

#define IRQ0_FREQUENCY 	    100
#define INPUT_FREQUENCY     1193180
#define COUNTER0_VALUE		 INPUT_FREQUENCY / IRQ0_FREQUENCY
#define COUNTER0_PORT		 0x40
#define COUNTER0_NO 		0
#define COUNTER_MODE		2
#define READ_WRITE_LATCH	3
#define PIT_COUNTROL_PORT	0x43
#define mil_second_per_init   1000 / IRQ0_FREQUENCY

void frequency_set(uint8_t counter_port ,uint8_t counter_no,uint8_t rwl, \
   uint8_t counter_mode,uint16_t counter_value)
{
   outb(PIT_COUNTROL_PORT,(uint8_t) (counter_no << 6 | rwl << 4 | counter_mode << 1));
   outb(counter_port,(uint8_t)counter_value);
   outb(counter_port,(uint8_t)counter_value >> 8);
   return;
} 

uint32_t ticks;     // ticks是内核自中断开启以来总共的嘀嗒数

/* 时钟的中断处理函数 */
static void intr_timer_handler(uint8_t vec_nr) {
   (void)vec_nr;
   struct task_struct* cur_thread = running_thread();

   ASSERT(cur_thread->stack_magic == 0x19870916);   // 检查栈是否溢出

   cur_thread->elapsed_ticks++;     // 记录此线程占用的cpu时间
   ticks++;	  // 从内核第一次处理时间中断后开始至今的滴哒数,内核态和用户态总共的嘀哒数

   if (cur_thread->ticks == 0) {    // 若进程时间片用完就开始调度新的进程上cpu
      schedule(); 
   } 
   else {     // 将当前进程的时间片-1
      cur_thread->ticks--;
   }
}

/* 初始化PIT8253 */
void timer_init(void) {
   put_str("timer_init start\n");
/* 设置8253的定时周期,也就是发中断的周期 */
   frequency_set(COUNTER0_PORT, COUNTER0_NO, READ_WRITE_LATCH, COUNTER_MODE, COUNTER0_VALUE);
   register_handler(0x20, intr_timer_handler);
   put_str("timer_init done\n");
}

/* 休息n个时钟中断 */
void ticks_to_sleep(uint32_t sleep_ticks)
{
   uint32_t start_tick = ticks;
   while(ticks - start_tick < sleep_ticks)
   thread_yield();
}

/* 通过毫秒的中断数来调用ticks_to_sleep达到休息毫秒的作用 */
void mtime_sleep(uint32_t m_seconds)
{
   uint32_t sleep_ticks = DIV_ROUND_UP(m_seconds,mil_second_per_init);
   ASSERT(sleep_ticks > 0);
   ticks_to_sleep(sleep_ticks);
}
