#include "inode.h"
#include "ide.h"
#include "debug.h"
#include "thread.h"
#include "memory.h"
#include "string.h"
#include "list.h"
#include "file.h"
#include "interrupt.h"

/* 得到inode 所在位置,给inode_pos赋值 */
void inode_locate(struct partition *part, uint32_t inode_no, struct inode_position *inode_pos)
{
    ASSERT(inode_no < 4096);
    uint32_t inode_table_lba = part->sb->inode_table_lba;

    uint32_t inode_size = sizeof(struct inode);
    uint32_t off_size = inode_no * inode_size;
    uint32_t off_sec = off_size / 512;
    uint32_t off_size_in_sec = off_size % 512;

    uint32_t left_in_sec = 512 - off_size_in_sec;
    if (left_in_sec < inode_size)
        inode_pos->two_sec = true;
    else
        inode_pos->two_sec = false;

    inode_pos->sec_lba = inode_table_lba + off_sec;
    inode_pos->off_size = off_size_in_sec;
}

/* 将内存中的inode同步到硬盘分区中 */
void inode_sync(struct partition *part, struct inode *inode, void *io_buf)
{
    uint8_t inode_no = inode->i_no;
    struct inode_position inode_pos; // inode_position
    inode_locate(part, inode_no, &inode_pos);

    ASSERT(inode_pos.sec_lba <= (part->start_lba + part->sec_cnt));

    struct inode pure_inode;
    memcpy(&pure_inode, inode, sizeof(struct inode));

    // 这些都只在内存中有用
    pure_inode.i_open_cnts = 0;
    pure_inode.write_deny = false;
    pure_inode.inode_tag.prev = pure_inode.inode_tag.next = NULL;

    char *inode_buf = (char *)io_buf;
    // 如果跨分区
    if (inode_pos.two_sec)
    {
        // 一次性读两个扇区的内容 复制粘贴完了回读回去
        ide_read(part->my_disk, inode_pos.sec_lba, inode_buf, 2);
        memcpy((inode_buf + inode_pos.off_size), &pure_inode, sizeof(struct inode));
        ide_write(part->my_disk, inode_pos.sec_lba, inode_buf, 2);
    }
    else
    {
        ide_read(part->my_disk, inode_pos.sec_lba, inode_buf, 1);
        memcpy((inode_buf + inode_pos.off_size), &pure_inode, sizeof(struct inode));
        ide_write(part->my_disk, inode_pos.sec_lba, inode_buf, 1);
    }
}

/* 打开编号为inode_no的inode,返回其指针 */
struct inode *inode_open(struct partition *part, uint32_t inode_no)
{
    // 先从inode链表找到inode 为了提速而创建的
    struct list_elem *elem = part->open_inodes.head.next; // 第一个结点开始
    struct inode *inode_found;                            // 返回
    while (elem != &part->open_inodes.tail)
    {
        inode_found = member_to_entry(struct inode, inode_tag, elem);
        if (inode_found->i_no == inode_no)
        {
            inode_found->i_open_cnts++;
            return inode_found;
        }
        elem = elem->next;
    }

    // 缓冲链表没找到 则需要在硬盘区找
    struct inode_position inode_pos;
    inode_locate(part, inode_no, &inode_pos);

    // 由于我们的sys_malloc 看当前线程处于用户还是内核是根据 pgdir是否有独立页表
    // 而不管是我们处在用户还是内核 为了让所有进程都能看到这个inode 我们必须把内存分配在内核
    struct task_struct *cur = running_thread();
    uint32_t *cur_pagedir_back = cur->pgdir;
    cur->pgdir = NULL;
    inode_found = (struct inode *)sys_malloc(sizeof(struct inode));
    cur->pgdir = cur_pagedir_back;

    char *inode_buf;
    if (inode_pos.two_sec)
    {
        inode_buf = (char *)sys_malloc(1024);
        ide_read(part->my_disk, inode_pos.sec_lba, inode_buf, 2);
    }
    else
    {
        inode_buf = (char *)sys_malloc(512);
        ide_read(part->my_disk, inode_pos.sec_lba, inode_buf, 1);
    }
    memcpy(inode_found, inode_buf + inode_pos.off_size, sizeof(struct inode));
    // 放在队首,因为很有可能接下来要访问
    list_push(&part->open_inodes, &inode_found->inode_tag);
    inode_found->i_open_cnts = 1;

    sys_free(inode_buf);
    return inode_found;
}

/* 关闭inode,如果i_open_cnts为0,则释放内存 */
void inode_close(struct inode *inode)
{
    enum intr_status old_status = intr_disable();
    // 减少次数后 当发现为0了 直接释放即可
    if (--inode->i_open_cnts == 0)
    {
        // 从 open_inode 链表中移除
        list_remove(&inode->inode_tag);
        struct task_struct *cur = running_thread();
        uint32_t *cur_pagedir_back = cur->pgdir;
        cur->pgdir = NULL;
        sys_free(inode);
        cur->pgdir = cur_pagedir_back;
    }
    intr_set_status(old_status);
}

/* 初始化inode,inode第一个编号是0 */
void inode_init(uint32_t inode_no, struct inode *new_inode)
{
    new_inode->i_no = inode_no;
    new_inode->i_size = 0;
    new_inode->i_open_cnts = 0;
    new_inode->write_deny = false;

    uint8_t sec_idx = 0;
    while (sec_idx < 13)
    {
        /* i_sectors[12]为一级间接块地址 */
        new_inode->i_sectors[sec_idx] = 0;
        sec_idx++;
    }
}

/* 将硬盘分区part上的inode清空 */
void inode_delete(struct partition *part, uint32_t inode_no, void *io_buf)
{
    ASSERT(inode_no < 4096);
    struct inode_position inode_pos;
    inode_locate(part, inode_no, &inode_pos); // inode位置信息会存入inode_pos
    ASSERT(inode_pos.sec_lba <= (part->start_lba + part->sec_cnt));

    char *inode_buf = (char *)io_buf;
    if (inode_pos.two_sec)
    { // inode跨扇区,读入2个扇区
        /* 将原硬盘上的内容先读出来 */
        ide_read(part->my_disk, inode_pos.sec_lba, inode_buf, 2);
        /* 将inode_buf清0 */
        memset((inode_buf + inode_pos.off_size), 0, sizeof(struct inode));
        /* 用清0的内存数据覆盖磁盘 */
        ide_write(part->my_disk, inode_pos.sec_lba, inode_buf, 2);
    }
    else
    { // 未跨扇区,只读入1个扇区就好
        /* 将原硬盘上的内容先读出来 */
        ide_read(part->my_disk, inode_pos.sec_lba, inode_buf, 1);
        /* 将inode_buf清0 */
        memset((inode_buf + inode_pos.off_size), 0, sizeof(struct inode));
        /* 用清0的内存数据覆盖磁盘 */
        ide_write(part->my_disk, inode_pos.sec_lba, inode_buf, 1);
    }
}

/* 回收inode的数据块和inode本身 */
void inode_release(struct partition *part, uint32_t inode_no)
{
    struct inode *inode_to_del = inode_open(part, inode_no);
    ASSERT(inode_to_del->i_no == inode_no);

    /* 1 回收inode占用的所有块 */
    uint8_t block_idx = 0, block_cnt = 12;
    uint32_t block_bitmap_idx;
    uint32_t all_blocks[140] = {0}; // 12个直接块+128个间接块

    /* a 先将前12个直接块存入all_blocks */
    while (block_idx < 12)
    {
        all_blocks[block_idx] = inode_to_del->i_sectors[block_idx];
        block_idx++;
    }

    /* b 如果一级间接块表存在,将其128个间接块读到all_blocks[12~], 并释放一级间接块表所占的扇区 */
    if (inode_to_del->i_sectors[12] != 0)
    {
        ide_read(part->my_disk, inode_to_del->i_sectors[12], all_blocks + 12, 1);
        block_cnt = 140;

        /* 回收一级间接块表占用的扇区 */
        block_bitmap_idx = inode_to_del->i_sectors[12] - part->sb->data_start_lba;
        ASSERT(block_bitmap_idx > 0);
        bitmap_set(&part->block_bitmap, block_bitmap_idx, 0);
        bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);
    }

    /* c inode所有的块地址已经收集到all_blocks中,下面逐个回收 */
    block_idx = 0;
    while (block_idx < block_cnt)
    {
        if (all_blocks[block_idx] != 0)
        {
            block_bitmap_idx = 0;
            block_bitmap_idx = all_blocks[block_idx] - part->sb->data_start_lba;
            ASSERT(block_bitmap_idx > 0);
            bitmap_set(&part->block_bitmap, block_bitmap_idx, 0);
            bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);
        }
        block_idx++;
    }

    /*2 回收该inode所占用的inode */
    bitmap_set(&part->inode_bitmap, inode_no, 0);
    bitmap_sync(cur_part, inode_no, INODE_BITMAP);

    /*******     以下inode_delete是调试用的    *******
     * 此函数会在inode_table中将此inode清0,
     * 但实际上是不需要的,inode分配是由inode位图控制的,
     * 硬盘上的数据不需要清0,可以直接覆盖*/
    void *io_buf = sys_malloc(1024);
    inode_delete(part, inode_no, io_buf);
    sys_free(io_buf);
    /***********************************************/

    inode_close(inode_to_del);
}

