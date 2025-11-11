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

ards_buf times 241 db 0                           ; ARDS 缓冲区（对齐用，合计 256 字节区块）
ards_nr dw 0		                              ; 记录 ARDS（内存描述条目）数量


; ================================================================
;                       loader 主体
; ================================================================
loader_start:

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
    
; ------------------------   加载kernel  -------------------------
    mov eax, KERNEL_START_SECTOR                        ; kernel.bin所在的扇区号
    mov ebx, KERNEL_BIN_BASE_ADDR                       ; 从磁盘读出后，写入到ebx指定的地址
    mov ecx, 200			                            ; 读入的扇区数
    call rd_disk_m_32

; ------------------------- 开启分页机制 --------------------------
    call setup_page                                     ; 建立页目录和页表（分页相关设置）

    mov ebx, [gdt_ptr + 2]                              ; EBX = GDT_BASE（原始线性地址）
    or dword [ebx + 0x18 + 4], 0xc0000000               ; 给视频段描述符高位加上虚拟偏移
    add dword [gdt_ptr + 2], 0xc0000000                 ; GDTR.base 加上内核高地址偏移
    add esp, 0xc0000000                                 ; 栈指针映射到内核虚拟地址

    mov eax, PAGE_DIR_TABLE_POS                         ; 页目录物理起始地址
    mov cr3, eax                                        ; 加载页目录地址到 CR3

    mov eax, cr0                                        ; 启用分页（CR0.PG = 1）
    or eax, 0x80000000
    mov cr0, eax

; ------------------- 重新加载 GDTR（使用新基址） -------------------
    lgdt [gdt_ptr]                                      

; ----------------------------------------------------------------
;                       进入内核执行阶段
; ----------------------------------------------------------------
    jmp SELECTOR_CODE:enter_kernel                      ; 跳转到内核入口（重新加载 CS）
enter_kernel:
    call kernel_init                                    ; 初始化内核段加载
    mov esp, 0xc009f000                                 ; 设置内核栈顶
    jmp KERNEL_ENTRY_POINT                              ; 跳转至内核入口执行

; ================================================================


; ================================================================
;                       setup_page 函数
; ================================================================
setup_page:
; ------------------ 清空 4096 字节页目录区 --------------------                                                         
    mov ecx, 4096
    mov esi, 0
.clear_page_dir:
    mov byte [PAGE_DIR_TABLE_POS + esi], 0
    inc esi
    loop .clear_page_dir

; ----------------------- 初始化页目录 ------------------------
.create_pde:
    mov eax, PAGE_DIR_TABLE_POS                         ; 页目录起始物理地址
    add eax, 0x1000                                     ; 第一个页表地址
    mov ebx, eax                                        ; EBX = 页表地址
    or eax, PG_US_U | PG_RW_W | PG_P                    ; PDE 属性：用户、读写、存在
    mov [PAGE_DIR_TABLE_POS + 0x0], eax                 ; PDE[0]
    mov [PAGE_DIR_TABLE_POS + 0xc00], eax               ; PDE[768] 映射高地址（镜像映射）

    sub eax, 0x1000                                     ; PDE[1023] 指向页目录本身
    mov [PAGE_DIR_TABLE_POS + 4092], eax

; ------------- 初始化第一个页表（低 1MB 区域） -------------
    mov ecx, 256
    mov esi, 0
    mov edx, PG_US_U | PG_RW_W | PG_P
.create_pte:
    mov [ebx + esi * 4], edx
    add edx, 4096
    inc esi
    loop .create_pte

; ------------------- 初始化内核高地址页目录 -------------------
    mov eax, PAGE_DIR_TABLE_POS
    add eax, 0x2000
    or eax, PG_US_U | PG_RW_W | PG_P
    mov ebx, PAGE_DIR_TABLE_POS
    mov ecx, 254
    mov esi, 769
.create_kernel_pde:
    mov [ebx + esi * 4], eax
    inc esi
    add eax, 0x1000
    loop .create_kernel_pde

    ret
; ================================================================


; ================================================================
;                       rd_disk_m_32 函数
; ================================================================
; 功能：从硬盘中读取多个扇区（使用 32 位寄存器版本）
; 参数：
;   eax = 起始 LBA 扇区号
;   ebx = 目标内存起始地址（要将读出的数据存到这里）
;   ecx = 读取的扇区数量
; ================================================================
rd_disk_m_32:
    mov esi, eax                        ; 保存 LBA 起始扇区号到 ESI（后面还要用）
    mov di, cx                          ; 保存读取的扇区数到 DI（16 位寄存器）

; ---------------------- 第 1 步：设置要读取的扇区数 ----------------------
    mov dx, 0x1f2                       ; 端口 0x1F2 = 扇区数量寄存器
    mov al, cl                          ; AL = 扇区数（最多 256）
    out dx, al                          ; 输出到硬盘控制器

; ---------------------- 第 2 步：写入 LBA 地址 --------------------------
    mov eax, esi                        ; 还原 LBA 扇区号
    mov dx, 0x1f3                       ; LBA[7:0] 写入 0x1F3
    out dx, al

    mov cl, 8
    shr eax, cl                         ; 右移 8 位 -> LBA[15:8]
    mov dx, 0x1f4
    out dx, al

    shr eax, cl                         ; 再右移 8 位 -> LBA[23:16]
    mov dx, 0x1f5
    out dx, al

    shr eax, cl                         ; 再右移 8 位 -> LBA[27:24]
    and al, 0x0f                        ; 取低 4 位（LBA 高 4 位）
    or al, 0xe0                         ; 设置高 4 位为 1110，表示 LBA 模式 + 主盘
    mov dx, 0x1f6
    out dx, al

; ---------------------- 第 3 步：发送读命令 0x20 -----------------------
    mov dx, 0x1f7                       ; 命令端口
    mov al, 0x20                        ; 0x20 表示“读扇区命令”
    out dx, al

; ---------------------- 第 4 步：等待硬盘准备好 ------------------------
.not_ready:
    nop
    in al, dx                           ; 从 0x1F7 读状态寄存器
    and al, 0x88                        ; 检查第 7 位 (BUSY) 和 第 3 位 (DRQ)
    cmp al, 0x08                        ; 只有 DRQ=1 且 BUSY=0 才可读
    jnz .not_ready                      ; 否则继续等待

; ---------------------- 第 5 步：从端口读取数据 ------------------------
    mov ax, di                          ; AX = 扇区数
    mov dx, 256                         ; 每扇区 512 字节 = 256 个 16 位字
    mul dx                              ; AX * 256 = 总的 16 位读取次数
    mov cx, ax                          ; CX = 循环次数（总读取次数）
    mov dx, 0x1f0                       ; 数据端口 0x1F0

.go_on_read:
    in ax, dx                           ; 从端口读 16 位数据
    mov [ebx], ax                       ; 写入内存缓冲区
    add ebx, 2                          ; 地址前移 2 字节
    loop .go_on_read                    ; 循环直到读取完所有扇区
    ret
; ================================================================


; ================================================================
;                       mem_cpy 函数
; ================================================================
; 功能：逐字节拷贝内存（dst ← src）
; 参数：
;   [ebp+8]  = 目标地址 dst
;   [ebp+12] = 源地址 src
;   [ebp+16] = 拷贝字节数 size
; ================================================================
mem_cpy:
    cld                                 ; 清除方向标志位 DF，保证地址递增
    push ebp
    mov ebp, esp                        ; 建立栈帧
    push ecx                            ; 保存外层使用的 ECX

    mov edi, [ebp + 8]                  ; EDI = dst
    mov esi, [ebp + 12]                 ; ESI = src
    mov ecx, [ebp + 16]                 ; ECX = size
    rep movsb                           ; 从 [ESI] → [EDI] 拷贝 ECX 字节

    pop ecx                             ; 恢复外层 ECX
    pop ebp
    ret
; ================================================================


; ================================================================
;                       kernel_init 函数
; ================================================================
; 功能：将 ELF 格式的内核镜像从加载区拷贝到对应虚拟地址
; 思路：
;   遍历 ELF Program Header Table (PHT)，
;   对每个有效段 (PT_LOAD)，拷贝其内容到指定 p_vaddr。
; ================================================================
kernel_init:
    xor eax, eax
    xor ebx, ebx
    xor ecx, ecx
    xor edx, edx

    mov dx, [KERNEL_BIN_BASE_ADDR + 42]         ; e_phentsize：每个 program header 的大小
    mov ebx, [KERNEL_BIN_BASE_ADDR + 28]        ; e_phoff：program header 表的文件偏移
    add ebx, KERNEL_BIN_BASE_ADDR               ; 转换成内存地址
    mov cx, [KERNEL_BIN_BASE_ADDR + 44]         ; e_phnum：program header 数量

.each_segment:
    cmp byte [ebx + 0], PT_NULL                 ; p_type == PT_NULL ?
    je .PTNULL                                  ; 如果该表项无效则跳过

; ------------------ 调用 mem_cpy(dst, src, size) ------------------
    push dword [ebx + 16]                       ; 第3个参数：p_filesz（段大小）
    mov eax, [ebx + 4]                          ; p_offset：段在文件中的偏移
    add eax, KERNEL_BIN_BASE_ADDR               ; 加上加载基址 → 段的实际物理地址
    push eax                                    ; 第2个参数：源地址
    push dword [ebx + 8]                        ; 第1个参数：p_vaddr（目标虚拟地址）
    call mem_cpy
    add esp, 12                                 ; 平衡栈

.PTNULL:
    add ebx, edx                                ; 跳到下一个 program header
    loop .each_segment
    ret
; ================================================================

