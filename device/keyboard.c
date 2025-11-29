#include "keyboard.h"

#define KBD_BUF_PORT 0x60	   // 键盘buffer寄存器端口号为0x60

/* 键盘中断处理程序 */
static void intr_keyboard_handler(uint8_t vec_no) {
   (void)vec_no;
// 每次必须要从8042读走键盘8048传递过来的数据，否则8042不会接收后续8048传递过来的数据
   uint8_t scan_code = inb(KBD_BUF_PORT);  //从键盘控制器的端口0x60读出扫描码
   console_put_int(scan_code);
   return;
}

/* 键盘初始化 */
void keyboard_init(void) {
   put_str("keyboard init start\n");
   register_handler(0x21, intr_keyboard_handler);       //注册键盘中断处理函数
   put_str("keyboard init done\n");
}
