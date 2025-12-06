#include "tss.h"
#include "process.h"
#include "string.h"
#include "../lib/kernel/memory.h"
#include "print.h"
#include "thread.h"
#include "interrupt.h"
#include "debug.h"
#include "console.h"

void start_process(void* filename_)
{
    void* function = filename_;
    struct task_struct* cur = running_thread();
    struct intr_stack* proc_stack = \
    (struct intr_stack*)((uint32_t)cur->self_kstack + sizeof(struct thread_stack));
    proc_stack->edi = proc_stack->esi = \
    proc_stack->ebp = proc_stack->esp_dummy = 0;
    proc_stack->ebx = proc_stack->edx = \
    proc_stack->ecx = proc_stack->eax = 0;
    proc_stack->gs = 0;
    proc_stack->ds = proc_stack->es = \
    proc_stack->fs = SELECTOR_U_DATA;	//数据段选择子
    proc_stack->eip = function; //函数地址 ip
    proc_stack->cs = SELECTOR_U_CODE; //cs ip cs选择子
    proc_stack->eflags = \
    (EFLAGS_IOPL_0 | EFLAGS_MBS | EFLAGS_IF_1); //标志寄存器
    proc_stack->esp = \
    (void*)((uint32_t)get_a_page\
    (PF_USER,USER_STACTK3_VADDR) + PG_SIZE); //用户栈顶地址
    proc_stack->ss = SELECTOR_U_DATA; //数据段选择子
    asm volatile ("movl %0,%%esp;jmp intr_exit": :"g"(proc_stack) :"memory");
}

void page_dir_activate(struct task_struct* p_thread)
{
    uint32_t pagedir_phy_addr = 0x100000; //之前设置的页目录表的物理地址
    if(p_thread->pgdir != NULL)
    {
    	pagedir_phy_addr = addr_v2p((uint32_t)p_thread->pgdir); //得到实际页目录地址
    }
    asm volatile ("movl %0,%%cr3" : : "r"(pagedir_phy_addr) : "memory");
}

void process_activate(struct task_struct* p_thread)
{
    page_dir_activate(p_thread);
    if(p_thread->pgdir)
    	update_tss_esp(p_thread);
}

uint32_t* create_page_dir(void)
{
    uint32_t* page_dir_vaddr = get_kernel_pages(1); //得到内存
    if(page_dir_vaddr == NULL)
    {
    	console_put_str("create_page_dir: get_kernel_page failed!\n");
    	return NULL;
    }
    memcpy((uint32_t*)((uint32_t)page_dir_vaddr + 0x300*4),\
    (uint32_t*)(0xfffff000+0x300*4),1024); // 256项
    uint32_t new_page_dir_phy_addr = addr_v2p((uint32_t)page_dir_vaddr);                    
    page_dir_vaddr[1023] = \
    new_page_dir_phy_addr | PG_US_U | PG_RW_W | PG_P_1;  //最后一项是页目录项自己的地址
    return page_dir_vaddr;									     
}

void create_user_vaddr_bitmap(struct task_struct* user_prog)
{
    user_prog->userprog_vaddr.vaddr_start = USER_VADDR_START; //位图开始管理的位置
    uint32_t bitmap_pg_cnt = \
    DIV_ROUND_UP((0xc0000000 - USER_VADDR_START)/ PG_SIZE / 8,PG_SIZE); //需要的页数
    user_prog->userprog_vaddr.vaddr_bitmap.bits = get_kernel_pages(bitmap_pg_cnt);
    user_prog->userprog_vaddr.vaddr_bitmap.btmp_bytes_len = 
    (0xc0000000 - USER_VADDR_START) / PG_SIZE / 8;
    bitmap_init(&user_prog->userprog_vaddr.vaddr_bitmap);
}

void process_execute(void* filename,char* name)
{
    struct task_struct* thread = get_kernel_pages(1); //分配一页空间 得到pcb
    init_thread(thread,name,default_prio);  //初始化pcb
    create_user_vaddr_bitmap(thread);   //为虚拟地址位图初始化 分配空间
    thread_create(thread,start_process,filename);   //创建线程
    thread->pgdir = create_page_dir();  //创建页目录表
    
    enum intr_status old_status = intr_disable();     
    ASSERT(!elem_find(&thread_ready_list,&thread->general_tag));
    list_append(&thread_ready_list,&thread->general_tag);   //加入就绪队列
    
    ASSERT(!elem_find(&thread_all_list,&thread->all_list_tag));
    list_append(&thread_all_list,&thread->all_list_tag);
    intr_set_status(old_status);
}
