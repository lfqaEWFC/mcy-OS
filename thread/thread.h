#ifndef __THREAD_THREAD_H
#define __THREAD_THREAD_H
#define PG_SIZE 4096

#include "list.h" 
#include "stdint.h"
#include "debug.h"
#include "interrupt.h"
#include "print.h"
#include "string.h"
#include "../lib/kernel/memory.h"
#include "stddef.h"

typedef void      thread_func(void*);
typedef int16_t   pid_t;

extern struct list thread_ready_list;
extern struct list thread_all_list;

enum task_status {
   TASK_RUNNING,
   TASK_READY,
   TASK_BLOCKED,
   TASK_WAITING,
   TASK_HANGING,
   TASK_DIED
};

struct intr_stack {
    uint32_t vec_no; //中断号
    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t esp_dummy; //被 popad 弹出时会跳过 esp
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;
    uint32_t gs;
    uint32_t fs;
    uint32_t es;
    uint32_t ds;
    
/*  以下由cpu从低特权级进入高特权级时压入   */
    uint32_t err_code;  // err_code 会被压入在 eip 之后
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
   pid_t pid;  //线程id
   enum task_status status;
   uint8_t priority; //线程优先级
   char name[16];
   uint8_t ticks; //线程时间片
   uint32_t elapsed_ticks; //线程占用cpu的总嘀嗒数
   struct list_elem general_tag; //用于线程队列中的结点
   struct list_elem all_list_tag;   //用于所有线程队列中的结点
   uint32_t* pgdir;  //线程自己页表的虚拟地址
   struct virtual_addr userprog_vaddr; //用户进程的虚拟地址池
   struct mem_block_desc u_block_desc[DESC_CNT];
   uint32_t stack_magic;
};

void thread_create(struct task_struct* pthread, thread_func function, void* func_arg);
void init_thread(struct task_struct* pthread, char* name, int prio);
struct task_struct* thread_start(char* name, int prio, thread_func function, void* func_arg);
struct task_struct* running_thread(void);
void schedule(void);
void thread_init(void);
void thread_block(enum task_status stat);
void thread_unblock(struct task_struct* pthread);
void thread_yield(void);

#endif
