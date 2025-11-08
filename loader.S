%include "boot.inc"

section loader vstart=LOADER_BASE_ADDR

; ================================================================
;                       loader 启动段
; ================================================================
LOADER_STACK_TOP    equ LOADER_BASE_ADDR
jmp loader_start                                  ; 跳过 GDT 描述符区，直接跳转到 loader 入口执行


; ================================================================
;                       GDT 表定义区
; ================================================================
GDT_BASE:
    dd 0x00000000                                 ; 第 0 项：空描述符（必须全 0）
    dd 0x00000000

; ------------------------ 代码段描述符 ------------------------
CODE_DESC:
    dd 0x0000FFFF                                 ; 段界限低 16 位 + 段基址低 16 位
    dd DESC_CODE_HIGH4                            ; 段基址高字节 + 段属性（高 4 字节）

; ---------------------- 数据段/栈段描述符 ----------------------
DATA_STACK_DESC:
    dd 0x0000FFFF
    dd DESC_DATA_HIGH4

; ------------------------ 显存段描述符 ------------------------
VIDEO_DESC:
    dd 0x80000007                                 ; 显存段基址示意值（文本显存）
    dd DESC_VIDEO_HIGH4                           ; 视频段描述符（可写、DPL=0）

; ---------------------- GDT 长度与选择子 ----------------------
GDT_SIZE        equ $ - GDT_BASE
GDT_LIMIT       equ GDT_SIZE - 1

times 60 dq 0                                     ; 预留描述符槽（供后续添加系统段/TSS 等）

SELECTOR_CODE   equ (0x0001 << 3) + TI_GDT + RPL0 ; 代码段选择子
SELECTOR_DATA   equ (0x0002 << 3) + TI_GDT + RPL0 ; 数据/栈 段选择子
SELECTOR_VIDEO  equ (0x0003 << 3) + TI_GDT + RPL0 ; 显存 段选择子

; ----------------------- GDTR 指针结构 -----------------------
total_mem_bytes dd 0				                    
; total_mem_bytes：保存计算出的物理内存字节数，后续内核可引用
; 注：这里的 total_mem_bytes 位于内存的 0xb03 处
; 后续内核应该从 0xb03 处引用

gdt_ptr:
    dw GDT_LIMIT                                  ; GDTR.limit（2 字节）
    dd GDT_BASE                                   ; GDTR.base  （4 字节）

ards_buf times 241 db 0                                 ; ARDS 缓冲区（对齐用，合计 256 字节区块）
ards_nr dw 0		                                    ; 记录 ARDS（内存描述条目）数量

; ================================================================
;                       启动提示信息
; ================================================================
loadermsg db '2 loader in real.'                      ; 屏显提示字符串（长度在调用处指定）


; ================================================================
;                       loader 主体
; ================================================================
loader_start:

; ----------------------------------------------------------------
; BIOS 中断 0x10 - 功能号 0x13：显示字符串（写入显存）
; ----------------------------------------------------------------
; 输入：
;   AH = 13h    - 显示字符串功能
;   AL = 01h    - 显示后光标前移
;   BH = 页面号
;   BL = 字符属性（例如 1Fh）
;   CX = 字符串长度
;   (DH,DL) = 起始行/列
;   ES:BP = 字符串地址
; ----------------------------------------------------------------
    mov sp, LOADER_BASE_ADDR
    mov bp, loadermsg
    mov cx, 17
    mov ax, 0x1301
    mov bx, 0x001f
    mov dx, 0x1800
    int 0x10                                      ; BIOS 调用：在屏幕输出提示文本

; ---------- 使用 INT 15h (E820) 获取系统内存布局（SMAP） ------------

    xor ebx, ebx		                                ; 初次调用需 EBX=0（用于遍历返回条目）
    mov edx, 0x534d4150	                                ; 'SMAP' 标识（EDX 固定）
    mov di, ards_buf	                                ; ARDS 缓冲区指针
.e820_mem_get_loop:	                                    ; 循环读取每个内存描述条目
    mov eax, 0x0000e820	                                ; EAX = 子功能号（E820）
    mov ecx, 20		                                    ; 结构体大小为 20 字节
    int 0x15
    add di, cx		                                    ; DI 指向下一个缓冲区位置
    inc word [ards_nr]	                                ; 统计返回的条目数量
    cmp ebx, 0		                                    ; EBX=0 且 CF=0 表示全部返回
    jnz .e820_mem_get_loop
; 在返回的 ARDS 条目中，找到 (base + length) 的最大值 -> 总内存大小
    mov cx, [ards_nr]	                                ; 遍历次数 = ARDS 条目数
    mov ebx, ards_buf 
    xor edx, edx		                                ; 清零，用于记录最大值
.find_max_mem_area:	                                    ; 遍历所有条目（不区分 type）
    mov eax, [ebx]	                                    ; base_low（低 32 位）
    add eax, [ebx+8]	                                ; length_low（低 32 位）
    add ebx, 20		                                    ; 指向下一条 ARDS
    cmp edx, eax		                                ; 对比并保留最大值
    jge .next_ards
    mov edx, eax		                                ; 更新最大内存值
.next_ards:
    loop .find_max_mem_area

    mov [total_mem_bytes], edx	                        ; 保存物理内存字节数

; ----------------------------------------------------------------
;                   进入保护模式前的准备工作
; ----------------------------------------------------------------
; 三步：
;   1.开启 A20 地址线（允许访问 1MB 以上）
;   2.加载 GDT 到 GDTR
;   3.设置 CR0.PE = 1（切换到保护模式）
; ----------------------------------------------------------------

; ------------------- Step 1: 开启 A20 地址线 ----------------------
; 端口 0x92 的位 1 控制 A20；置位后允许访问线性地址 > 0xFFFFF
    in al, 0x92
    or al, 0000_0010B
    out 0x92, al

; ---------------------- Step 2: 加载 GDT -------------------------
; 将 gdt_ptr 指向的界限和基址加载到 GDTR 寄存器
    lgdt [gdt_ptr]

; ------------------- Step 3: 设置 CR0.PE = 1 ---------------------
; 打开保护模式使能位（CR0.[0] = 1）
    mov eax, cr0
    or eax, 0x00000001
    mov cr0, eax

; ----------------- 远跳以刷新预取队列并进入保护模式 -------------------
; 需要执行 far jmp（选择子:偏移）以切换到保护模式下的代码段
    jmp dword SELECTOR_CODE:p_mode_start


; ----------------------------------------------------------------
;                     保护模式正式开始 (32-bit)
; ----------------------------------------------------------------
[bits 32]
p_mode_start:

; ----------------------- 加载数据段寄存器 --------------------------
    mov ax, SELECTOR_DATA
    mov ds, ax
    mov es, ax
    mov ss, ax

; ----------- 将 GS 指向显存段（便于用 GS: 偏移直接访问显存）-----------
    mov ax, SELECTOR_VIDEO
    mov gs, ax

; ------------- 测试输出：在屏幕第二行第一个位置写入 'P' ---------------
; 文本模式下每字符占 2 字节（字符 + 属性），160 为第二行首列偏移
    mov byte [gs:160], 'P'
; ----------------------------------------------------------------
    call setup_page                                     ; 建立页目录和页表（分页相关设置）

    mov ebx, [gdt_ptr + 2]                              ; EBX = GDT_BASE（原始线性地址）
    or dword [ebx + 0x18 + 4], 0xc0000000               ; 给视频段描述符高位加上虚拟偏移
                                           
    add dword [gdt_ptr + 2], 0xc0000000                 ; GDTR.base 加上内核高地址偏移

    add esp, 0xc0000000                                 ; 将栈指针也映射到内核虚拟地址

    mov eax, PAGE_DIR_TABLE_POS                         ; 页目录物理起始地址
    mov cr3, eax

    mov eax, cr0                                        ; 启用 CR0.PG（分页）
    or eax, 0x80000000  
    mov cr0, eax
                                                      
    lgdt [gdt_ptr]                                      ; 分页启用后重新加载 GDTR（使用新基址）

    mov byte [gs:320], 'V'                              ; 标记：显存已切换到虚拟地址映射

; ---------------------- 无限循环保持运行 --------------------------
    jmp $
; ================================================================


; ================================================================
;                       setup_page 函数
; ================================================================
setup_page:
; ------------------ 清空 4096 字节页目录区 --------------------                                                         
    mov ecx, 4096                                       ; 清空 4096 字节页目录区（逐字节清零）
    mov esi, 0                                          ; ESI 作为偏移索引
.clear_page_dir:
    mov byte [PAGE_DIR_TABLE_POS + esi], 0
    inc esi
    loop .clear_page_dir

; ----------------------- 初始化页目录 ------------------------
.create_pde:				                            ; 建立 PDE，PDE[0] 与 PDE[768] 指向同一页表（实现镜像）
    mov eax, PAGE_DIR_TABLE_POS                         ; EAX = 页目录起始物理地址
    add eax, 0x1000 			                        ; 第一个页表紧随页目录之后（页目录占 4KB）
    mov ebx, eax				                        ; EBX 保存第一个页表的地址
    or eax, PG_US_U | PG_RW_W | PG_P	                ; PDE 属性：US=1, RW=1, P=1（用户可访问，读写）
;   EAX 现在为完整的 PDE 值（高 20 位页表地址 + 低 12 位属性）
    mov [PAGE_DIR_TABLE_POS + 0x0], eax                 ; PDE[0] = 第一个页表地址 + 属性
    mov [PAGE_DIR_TABLE_POS + 0xc00], eax               ; PDE[768] = 同一页表（实现高地址映射）
					                                    
    sub eax, 0x1000                                     ; 设置最后一个目录项指向页目录本身（便于动态管理）
    mov [PAGE_DIR_TABLE_POS + 4092], eax	            ; 最后一项含 US 标志，以便用户进程访问页目录

; ------------- 初始化第一个页表：256 项（映射低 1MB）-------------
    mov ecx, 256				                        ; 初始化第一个页表：256 项（映射低 1MB）
    mov esi, 0                                          ; ESI = 条目索引
    mov edx, PG_US_U | PG_RW_W | PG_P	                ; PTE 属性 = 0x7（US=1,RW=1,P=1）
.create_pte:				                            ; 填充 PTE（每项映射一个 4KB 页面）
    mov [ebx+esi*4],edx			                        ; 写入 PTE[i] = 当前物理页基址 + 属性
    add edx,4096                                        ; 递增到下一页物理基址
    inc esi                                             ; 索引自增
    loop .create_pte                                    ; 循环填充，共 256 项

; ------------------- 初始化页目录（高 256 项）------------------
    mov eax, PAGE_DIR_TABLE_POS                         ; 恢复页目录基址
    add eax, 0x2000 		                            ; 指向第二个页表位置
    or eax, PG_US_U | PG_RW_W | PG_P                    ; 设置 PDE 值（属性 + 基址）
    mov ebx, PAGE_DIR_TABLE_POS                         ; EBX = 页目录基址（写入目标）
    mov ecx, 254			                            ; 需要初始化的 PDE 数量
    mov esi, 769                                        ; 起始目录索引（769）
.create_kernel_pde:
    mov [ebx+esi*4], eax                                ; 写入 PDE
    inc esi                                             ; 索引自增
    add eax, 0x1000                                     ; 指向下一个页表基址
    loop .create_kernel_pde                             ; 循环写入剩余 PDE
                             
    ret
; ================================================================

