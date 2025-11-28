#ifndef __THREAD_THREAD_H
#define __THREAD_THREAD_H
#define PG_SIZE 4096

#include "list.h" 
#include "stdint.h"
#include "debug.h"
#include "interrupt.h"
#include "print.h"
#include "string.h"
#include "memory.h"

typedef void thread_func(void*);

enum task_status {
   TASK_RUNNING,
   TASK_READY,
   TASK_BLOCKED,
   TASK_WAITING,
   TASK_HANGING,
   TASK_DIED
};

struct intr_stack {
    uint32_t vec_no;       // kernel.S 宏 VECTOR 中 push %1 压入的中断号
    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t esp_dummy;    // 虽然 pushad 把 esp 也压入,但 esp 是不断变化的,所以会被 popad 忽略
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;
    uint32_t gs;
    uint32_t fs;
    uint32_t es;
    uint32_t ds;
    
/*  以下由cpu从低特权级进入高特权级时压入   */
    uint32_t err_code;     // err_code 会被压入在 eip 之后
    void (*eip) (void);
    uint32_t cs;
    uint32_t eflags;
    void* esp;
    uint32_t ss;
};

struct thread_stack {
   uint32_t ebp;
   uint32_t ebx;
   uint32_t edi;
   uint32_t esi;
   void (*eip) (thread_func* func, void* func_arg);
   void (*unused_retaddr);
   thread_func* function;
   void* func_arg;
};

struct task_struct {
   uint32_t* self_kstack;
   enum task_status status;
   uint8_t priority;		           // 线程优先级
   char name[16];                  // 用于存储自己的线程的名字
   uint8_t ticks;	                 // 线程允许上处理器运行还剩下的滴答值，因为priority不能改变，所以要在其之外另行定义一个值来倒计时
   uint32_t elapsed_ticks;         // 此任务自上cpu运行后至今占用了多少cpu嘀嗒数, 也就是此任务执行了多久
   struct list_elem general_tag;	  // general_tag的作用是用于线程在一般的队列(如就绪队列或者等待队列)中的结点
   struct list_elem all_list_tag;  // all_list_tag的作用是用于线程队列thread_all_list（这个队列用于管理所有线程）中的结点
   uint32_t* pgdir;                // 进程自己页表的虚拟地址
   uint32_t stack_magic;	        // 如果线程的栈无限生长，总会覆盖pcb的信息，那么需要定义个边界数来检测是否栈已经到了PCB的边界
};

void thread_create(struct task_struct* pthread, thread_func function, void* func_arg);
void init_thread(struct task_struct* pthread, char* name, int prio);
struct task_struct* thread_start(char* name, int prio, thread_func function, void* func_arg);
struct task_struct* running_thread(void);
void schedule(void);
void thread_init(void);
void thread_block(enum task_status stat);
void thread_unblock(struct task_struct* pthread);

#endif
