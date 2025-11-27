#include "bitmap.h"

//将位图 btmp 初始化
void bitmap_init(struct bitmap* btmp)
{
    memset(btmp->bits,0,btmp->btmp_bytes_len);
    return;
}

//判断 bit_idx 位是否为 1，若为 1 则返回 true，否则返回 false
bool bitmap_scan_test(struct bitmap* btmp,uint32_t bit_idx)
{
    uint32_t byte_idx = bit_idx/8;
    uint32_t byte_pos = bit_idx%8;
    return (btmp->bits[byte_idx] & (BITMAP_MASK << byte_pos));
}
//在位图中申请 cnt 个连续的位，成功则返回其起始位下标，失败则返回 -1
int32_t bitmap_scan(struct bitmap* btmp,uint32_t cnt)
{
    ASSERT(cnt >= 1);
    uint32_t byte_idx = 0;
    while(byte_idx < btmp->btmp_bytes_len && btmp->bits[byte_idx] == 0xff)
    	++byte_idx;
    if(byte_idx == btmp->btmp_bytes_len)	return -1;
    
    uint32_t byte_pos = 0;
    while((btmp->bits[byte_idx] & (BITMAP_MASK << byte_pos)))
    	++byte_pos;
    int32_t bit_idx_start = byte_idx*8 + byte_pos;

    if(cnt == 1)	return bit_idx_start;

    uint32_t count = 1;
    int32_t bits_left = btmp->btmp_bytes_len*8 - (bit_idx_start+1);
    uint32_t next_bit = bit_idx_start + 1;
    bit_idx_start = -1;
    while(bits_left-- > 0)
    {
        ASSERT(next_bit < btmp->btmp_bytes_len * 8);
    	if(!bitmap_scan_test(btmp,next_bit))    ++count;
    	else    count = 0;
    	if(count == cnt)
        {
            bit_idx_start = next_bit - cnt + 1;
            break;
        }
    	++next_bit;
    }
    return bit_idx_start;	
}

//将位图 btmp 的 bit_idx 位设置为 value
void bitmap_set(struct bitmap* btmp,uint32_t bit_idx,int8_t value)
{
    ASSERT(value == 1 || value == 0);
    uint32_t byte_idx = bit_idx/8;
    uint32_t byte_pos = bit_idx%8;
    if(value)	btmp->bits[byte_idx] |=  (BITMAP_MASK << byte_pos);
    else	btmp->bits[byte_idx] &= ~(BITMAP_MASK << byte_pos);
    return;
}
