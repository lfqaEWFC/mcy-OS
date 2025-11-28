#include "sync.h"

/* 初始化信号量 */
void sema_init(struct semaphore* psema, uint8_t value) {
   psema->value = value;            // 为信号量赋初值
      list_init(&psema->waiters);   //初始化信号量的等待队列
   }

/* 用于初始化锁 plock */
void lock_init(struct lock* plock) {
   plock->holder = NULL;
   plock->holder_repeat_nr = 0;
   sema_init(&plock->semaphore, 1); //将信号量初始化为1，因为此函数一般处理二元信号量
}

/* 信号量down操作 */
void sema_down(struct semaphore* psema) {
/* 关中断,保证原子操作 */
   enum intr_status old_status = intr_disable();
   while(psema->value == 0) {
      ASSERT(!elem_find(&psema->waiters, &running_thread()->general_tag));
      if (elem_find(&psema->waiters, &running_thread()->general_tag)) {
         PANIC("sema_down: thread blocked has been in waiters_list\n");
      }
      list_append(&psema->waiters, &running_thread()->general_tag); 
      thread_block(TASK_BLOCKED);   // 阻塞线程,直到被唤醒
   }
/* 若value为1或被唤醒后,会执行下面的代码,也就是获得了锁。*/
   psema->value--;
   ASSERT(psema->value == 0);	    
/* 恢复之前的中断状态 */
   intr_set_status(old_status);
}

/* 信号量up操作 */
void sema_up(struct semaphore* psema) {
/* 关中断,保证原子操作 */
   enum intr_status old_status = intr_disable();
   ASSERT(psema->value == 0);	    
   if (!list_empty(&psema->waiters)) {
      struct task_struct* thread_blocked = \
      member_to_entry(struct task_struct, general_tag, list_pop(&psema->waiters));
      thread_unblock(thread_blocked);
   }
   psema->value++;
   ASSERT(psema->value == 1);	    
/* 恢复之前的中断状态 */
   intr_set_status(old_status);
}

/* 获取锁plock */
void lock_acquire(struct lock* plock) {
   if (plock->holder != running_thread()) { 
/* 对信号量进行down操作 */
      sema_down(&plock->semaphore);
      plock->holder = running_thread();
      ASSERT(plock->holder_repeat_nr == 0);
/* 申请了一次锁 */      
      plock->holder_repeat_nr = 1;
   } else {
      plock->holder_repeat_nr++;
   }
}

/* 释放锁plock */
void lock_release(struct lock* plock) {
   ASSERT(plock->holder == running_thread());
/* 如果>1，说明自己多次申请了该锁，现在还不能立即释放锁 */
   if (plock->holder_repeat_nr > 1) {   
      plock->holder_repeat_nr--;
      return;
   }
   ASSERT(plock->holder_repeat_nr == 1);
   plock->holder = NULL;
   plock->holder_repeat_nr = 0;
/* 信号量的up操作,也是原子操作 */
   sema_up(&plock->semaphore);
}
