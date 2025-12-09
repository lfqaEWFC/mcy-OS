#include "memory.h"
#include "thread.h"
#include "sync.h"
#include "stddef.h"
#include "stdbool.h"

#define MEM_BITMAP_BASE 0Xc009a000  //位图开始存放的位置
#define K_HEAP_START    0xc0100000  //内核堆起始位置
#define PDE_IDX(addr) ((addr & 0xffc00000) >> 22)
#define PTE_IDX(addr) ((addr & 0x003ff000) >> 12)

/* 物理内存池结构体 */
struct pool
{
    struct bitmap pool_bitmap;  //位图来管理内存使用
    uint32_t phy_addr_start;    //内存池管理的内存的起始地址
    struct lock lock;           //申请内存时互斥 
    uint32_t pool_size;	        //池容量
};

/* 内存仓库 arena */
struct arena
{   
    uint32_t cnt;
    bool large; 
    struct mem_block_desc* desc;
};

struct mem_block_desc k_block_descs[DESC_CNT];  //内核内存块描述符数组

struct pool kernel_pool ,user_pool; //生成内核物理内存池 和 用户物理内存池
struct virtual_addr kernel_vaddr;   //生成内核虚拟内存池

//获取 pg_cnt 页连续虚拟内存的起始地址
void* vaddr_get(enum pool_flags pf,uint32_t pg_cnt)
{
    int vaddr_start = 0,bit_idx_start = -1;
    uint32_t cnt = 0;
    if(pf == PF_KERNEL) //内核虚拟内存池
    {
    	bit_idx_start = bitmap_scan(&kernel_vaddr.vaddr_bitmap,pg_cnt);
    	if(bit_idx_start == -1)	return NULL;
    	while(cnt < pg_cnt)
    	    bitmap_set(&kernel_vaddr.vaddr_bitmap,\
                bit_idx_start + (cnt++),1);
    	vaddr_start = kernel_vaddr.vaddr_start + bit_idx_start * PG_SIZE;
    }
    else //用户虚拟内存池
    {
        struct task_struct* cur = running_thread();
    	bit_idx_start = bitmap_scan(&cur->userprog_vaddr.vaddr_bitmap,pg_cnt);
    	if(bit_idx_start == -1) return NULL;
    	while(cnt < pg_cnt)
    	    bitmap_set(&cur->userprog_vaddr.vaddr_bitmap,\
                bit_idx_start + (cnt++),1);
    	vaddr_start = cur->userprog_vaddr.vaddr_start + bit_idx_start * PG_SIZE;        
    }
    return (void*)vaddr_start;
}

//获取虚拟地址映射的页表项地址
uint32_t* pte_ptr(uint32_t vaddr)
{
    uint32_t* pte = (uint32_t*)(0xffc00000 + \
        ((vaddr & 0xffc00000) >> 10) + PTE_IDX(vaddr) * 4);
    return pte;         
}

//获取虚拟地址映射的页目录项地址
uint32_t* pde_ptr(uint32_t vaddr)
{
    uint32_t* pde = (uint32_t*) ((0xfffff000) + PDE_IDX(vaddr) * 4);
    return pde;         
}

//在 m_pool 物理内存池中分配 1 页物理内存，成功则返回其物理地址，失败则返回 NULL
void* palloc(struct pool* m_pool)
{
    int bit_idx = bitmap_scan(&m_pool->pool_bitmap,1);
    if(bit_idx == -1)	return NULL;
    bitmap_set(&m_pool->pool_bitmap,bit_idx,1);
    uint32_t page_phyaddr = ((bit_idx * PG_SIZE) + m_pool->phy_addr_start);
    return (void*)page_phyaddr;
}

//在虚拟地址 vaddr 与物理地址 page_phyaddr 之间建立映射
void page_table_add(void* _vaddr,void* _page_phyaddr)
{
    uint32_t vaddr = (uint32_t)_vaddr,page_phyaddr = (uint32_t)_page_phyaddr;
    uint32_t* pde = pde_ptr(vaddr);
    uint32_t* pte = pte_ptr(vaddr);
    
//0x00000001 是 P 位，返回值为 0 表示不存在 1 表示存在
    if(*pde & 0x00000001)
    {
    	ASSERT(!(*pte & 0x00000001));
    	if(!(*pte & 0x00000001))
    	    *pte = (page_phyaddr | PG_US_U | PG_RW_W | PG_P_1);
    	else
    	{
    	    PANIC("pte repeat");
    	    *pte = (page_phyaddr | PG_US_U | PG_RW_W | PG_P_1);
    	}
    } 
    else
    {
    	uint32_t pde_phyaddr = (uint32_t)palloc(&kernel_pool);
    	*pde = (pde_phyaddr | PG_US_U | PG_RW_W | PG_P_1);
    	memset((void*)((int)pte & 0xfffff000),0,PG_SIZE);
    	ASSERT(!(*pte & 0x00000001));
    	*pte = (page_phyaddr | PG_US_U | PG_RW_W | PG_P_1);
    }
    return;
}

//分配 pg_cnt 页连续的虚拟内存，成功则返回起始地址，失败则返回 NULL
void* malloc_page(enum pool_flags pf,uint32_t pg_cnt)
{
    ASSERT(pg_cnt > 0 && pg_cnt < 3840);
    
    void* vaddr_start = vaddr_get(pf,pg_cnt);
    if(vaddr_start == NULL)	return NULL;
    
    
    uint32_t vaddr = (uint32_t)vaddr_start,cnt = pg_cnt;
    struct pool* mem_pool = pf & PF_KERNEL ? &kernel_pool : &user_pool;
    
    while(cnt-- > 0)
    {
    	void* page_phyaddr = palloc(mem_pool);
    	if(page_phyaddr == NULL)	return NULL;
    	page_table_add((void*)vaddr,page_phyaddr);
    	vaddr += PG_SIZE;
    }
    return vaddr_start;
}

//分配 pg_cnt 页的内核虚拟地址，成功则返回其起始地址，失败则返回 NULL
void* get_kernel_pages(uint32_t pg_cnt)
{
    lock_acquire(&kernel_pool.lock);
    void* vaddr = malloc_page(PF_KERNEL,pg_cnt);
    if(vaddr != NULL)	memset(vaddr,0,pg_cnt*PG_SIZE);
    lock_release(&kernel_pool.lock);
    return vaddr;
}

void* get_user_pages(uint32_t pg_cnt)
{
    lock_acquire(&user_pool.lock);
    void* vaddr = malloc_page(PF_USER,pg_cnt);
    if(vaddr != NULL)	memset(vaddr,0,pg_cnt*PG_SIZE);
    lock_release(&user_pool.lock);
    return vaddr;
}

void* get_a_page(enum pool_flags pf,uint32_t vaddr)
{
    struct pool* mem_pool = (pf == PF_KERNEL) ? &kernel_pool : &user_pool;
    lock_acquire(&mem_pool->lock);
    
    struct task_struct* cur = running_thread();
    int32_t bit_idx = -1;
    
    if(cur->pgdir != NULL && pf == PF_USER)
    {
    	bit_idx = (vaddr - cur->userprog_vaddr.vaddr_start) / PG_SIZE;
    	ASSERT(bit_idx > 0);
    	bitmap_set(&cur->userprog_vaddr.vaddr_bitmap,bit_idx,1);
    }
    else if(cur->pgdir == NULL && pf == PF_KERNEL) 
    {
    	bit_idx = (vaddr - kernel_vaddr.vaddr_start) / PG_SIZE;
    	ASSERT(bit_idx > 0);
    	bitmap_set(&kernel_vaddr.vaddr_bitmap,bit_idx,1);
    }
    else
    	PANIC("get_a_page:not allow kernel alloc userspace or \
        user alloc kernelspace by get_a_page");
    	
    void* page_phyaddr = palloc(mem_pool);
    if(page_phyaddr == NULL)
    	return NULL;
    page_table_add((void*)vaddr,page_phyaddr);
    lock_release(&mem_pool->lock);
    return (void*)vaddr;
}

//虚拟地址映射的物理地址
uint32_t addr_v2p(uint32_t vaddr)
{
    uint32_t* pte = pte_ptr(vaddr);
    return ((*pte & 0xfffff000) + (vaddr & 0x00000fff));
}

//内存池初始化
void mem_pool_init(uint32_t all_mem)
{
    put_str("    mem_pool_init start!\n");
    uint32_t page_table_size = PG_SIZE * 256;
    uint32_t used_mem = page_table_size + 0x100000;
    uint32_t free_mem = all_mem - used_mem;
    
    uint16_t all_free_pages = free_mem / PG_SIZE;
    
    uint16_t kernel_free_pages = all_free_pages / 2;
    uint16_t user_free_pages = all_free_pages - kernel_free_pages;
    
    //计算位图所需的字节数
    uint32_t kbm_length = kernel_free_pages / 8;
    uint32_t ubm_length = user_free_pages / 8;
    
    //内存池大小对齐到页
    uint32_t kp_start = used_mem;
    uint32_t up_start = kp_start + kernel_free_pages * PG_SIZE;
    
    kernel_pool.phy_addr_start = kp_start;
    user_pool.phy_addr_start = up_start;
    
    kernel_pool.pool_size = kernel_free_pages * PG_SIZE;
    user_pool.pool_size = user_free_pages * PG_SIZE;
    
    kernel_pool.pool_bitmap.bits = (void*)MEM_BITMAP_BASE;
    user_pool.pool_bitmap.bits = (void*)(MEM_BITMAP_BASE + kbm_length);
    
    kernel_pool.pool_bitmap.btmp_bytes_len = kbm_length;
    user_pool.pool_bitmap.btmp_bytes_len = ubm_length; 
    
    put_str("        kernel_pool_bitmap_start:");
    put_int((int)kernel_pool.pool_bitmap.bits);
    put_str(" kernel_pool_phy_addr_start:");
    put_int(kernel_pool.phy_addr_start);    put_char('\n');

    put_str("        user_pool_bitmap_start:");
    put_int((int)user_pool.pool_bitmap.bits);
    put_str(" user_pool_phy_addr_start:");
    put_int(user_pool.phy_addr_start);    put_char('\n');
    
    bitmap_init(&kernel_pool.pool_bitmap);
    bitmap_init(&user_pool.pool_bitmap);
    
    kernel_vaddr.vaddr_bitmap.bits = \
    (void*)(MEM_BITMAP_BASE + kbm_length + ubm_length);
    kernel_vaddr.vaddr_bitmap.btmp_bytes_len = kbm_length;
    
    kernel_vaddr.vaddr_start = K_HEAP_START;
    bitmap_init(&kernel_vaddr.vaddr_bitmap);
    lock_init(&kernel_pool.lock);
    lock_init(&user_pool.lock);
    put_str("    mem_pool_init done\n");
    return;
}

//初始化内存块描述符数组
void block_desc_init(struct mem_block_desc* desc_array)
{
    uint16_t desc_idx,block_size = 16;
    for(desc_idx = 0;desc_idx < DESC_CNT;desc_idx++)
    {
    	desc_array[desc_idx].block_size = block_size;
    	desc_array[desc_idx].block_per_arena = (PG_SIZE - sizeof(struct arena)) / block_size;
    	list_init(&desc_array[desc_idx].free_list);
    	block_size *= 2;
    }   
}

//返回idx个内存块的起始地址
static struct mem_block* arena2block(struct arena* a,uint32_t idx)
{
    return (struct mem_block*)((uint32_t)a + sizeof(struct arena) + idx * a->desc->block_size);
}

//返回内存块所在的arena的起始地址
static struct arena* block2arena(struct mem_block* b)
{
    return (struct arena*)((uint32_t)b & 0xfffff000);
}

//在堆中申请 size 字节的内存
void* sys_malloc(uint32_t size)
{
    enum pool_flags PF;
    struct pool* mem_pool;
    uint32_t pool_size;
    struct mem_block_desc* descs;
    struct task_struct* cur_thread = running_thread();
    
    if(cur_thread->pgdir == NULL)
    {
    	PF = PF_KERNEL;
    	pool_size = kernel_pool.pool_size;
    	mem_pool = &kernel_pool;
    	descs = k_block_descs;
    }
    else
    {
    	PF = PF_USER;
    	pool_size = user_pool.pool_size;
    	mem_pool = &user_pool;
    	descs = cur_thread->u_block_desc;
    }
    if(!(size > 0 && size < pool_size))
    {
    	return NULL;
    }
    
    struct arena* a;
    struct mem_block* b;
    lock_acquire(&mem_pool->lock);
    
    if(size > 1024)
    {
    	uint32_t page_cnt = DIV_ROUND_UP(size + sizeof(struct arena),PG_SIZE);
    	a = malloc_page(PF,page_cnt);
    	if(a != NULL)
    	{
    	    memset(a,0,page_cnt * PG_SIZE);
	    a->desc = NULL;
	    a->cnt  = page_cnt;
	    a->large = true;    
	    lock_release(&mem_pool->lock);
	    return (void*)(a+1);
	}
	else
	{
	    lock_release(&mem_pool->lock);
	    return NULL;
	}
    }
    else
    {
    	uint8_t desc_idx;
    	for(desc_idx = 0;desc_idx < DESC_CNT;desc_idx++)
    	{
    	    if(size <= descs[desc_idx].block_size)
    	    {
    	    	break;
    	    }
    	}
    	
    	if(list_empty(&descs[desc_idx].free_list))
    	{
    	    a = malloc_page(PF,1);
    	    if(a == NULL)
    	    {
    	    	lock_release(&mem_pool->lock);
    	    	return NULL;
    	    }
    	    memset(a,0,PG_SIZE);
    	    
    	    a->desc = &descs[desc_idx];
    	    a->large = false;
    	    a->cnt = descs[desc_idx].block_per_arena;
    	    uint32_t block_idx;
    	    
    	    enum intr_status old_status = intr_disable();
    	    
    	    for(block_idx = 0;block_idx < descs[desc_idx].block_per_arena;++block_idx)
    	    {
    	    	b = arena2block(a,block_idx);
    	    	ASSERT(!elem_find(&a->desc->free_list,&b->free_elem));
    	    	list_append(&a->desc->free_list,&b->free_elem);
    	    }
    	    intr_set_status(old_status);
    	}
    	
    	b = (struct mem_block*)list_pop(&(descs[desc_idx].free_list));
    	memset(b,0,descs[desc_idx].block_size);
    	
    	a = block2arena(b);
    	a->cnt--;
    	lock_release(&mem_pool->lock);
    	return (void*)b;
    }
}

//内存管理初始化
void mem_init()
{
    put_str("mem_init start!\n");
    uint32_t mem_bytes_total = (*(uint32_t*)(0xb03));
    mem_pool_init(mem_bytes_total);
    block_desc_init(k_block_descs);
    put_str("mem_init done!\n");
    return;
}
