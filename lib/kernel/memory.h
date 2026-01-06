#ifndef __KERNEL_MEMORY_H
#define __KERNEL_MEMORY_H

#include "list.h"
#include "stdint.h"
#include "bitmap.h"
#include "stddef.h"
#include "print.h"
#include "debug.h"
#include "string.h"

/* 内存池标记 */
enum pool_flags
{
    PF_KERNEL = 1,
    PF_USER = 2
};

/* 虚拟内存池结构体 */
struct virtual_addr
{
    struct bitmap vaddr_bitmap; //位图来管理内存使用
    uint32_t vaddr_start;       //内存池管理的内存的起始地址
};

/* 内存块节点 */
struct mem_block {
    struct list_elem free_elem;
};

/* 内存块描述符 */
struct mem_block_desc
{
    uint32_t block_size;
    uint32_t block_per_arena;
    struct list free_list;	
};

#define DESC_CNT    7   //内存块描述符个数

#define PG_P_1      1
#define PG_P_0      0
#define PG_RW_R     0
#define PG_RW_W     2
#define PG_US_S     0
#define PG_US_U     4
#define PG_SIZE     4096

extern struct pool kernel_pool,user_pool;
void mem_init(void);
void* vaddr_get(enum pool_flags pf,uint32_t pg_cnt);
uint32_t* pte_ptr(uint32_t vaddr);
uint32_t* pde_ptr(uint32_t vaddr);
void* palloc(struct pool* m_pool);
void page_table_add(void* _vaddr,void* _page_phyaddr);
void* malloc_page(enum pool_flags pf,uint32_t pg_cnt);
void* get_kernel_pages(uint32_t pg_cnt);
void* get_user_pages(uint32_t pg_cnt);
void* get_a_page(enum pool_flags pf,uint32_t vaddr);
uint32_t addr_v2p(uint32_t vaddr);
void mem_pool_init(uint32_t all_mem);
void block_desc_init(struct mem_block_desc* desc_array);
void* sys_malloc(uint32_t size);
void mfree_page(enum pool_flags pf, void* _vaddr, uint32_t pg_cnt);
void pfree(uint32_t pg_phy_addr);
void sys_free(void* ptr);
void *get_a_page_without_opvaddrbitmap(enum pool_flags pf, uint32_t vaddr);

#endif
