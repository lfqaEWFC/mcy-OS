#定义一大堆变量，实质就是将需要多次重复用到的语句定义一个变量方便使用与替换
BUILD_DIR=./build
ENTRY_POINT=0xc0001500
HD60M_PATH=/home/mcy-mcy/文档/bochs/hd60M.img
AS=nasm
CC=gcc
LD=ld
LIB= -I lib/ -I lib/kernel/ -I lib/user/ -I kernel/ -I device/ -I thread/ -I userprog/ -I fs/ -I shell/
ASFLAGS= -f elf
CFLAGS= -Wall $(LIB) -c -fno-builtin -W -Wstrict-prototypes -Wmissing-prototypes -m32 -fno-stack-protector

#-Wall warning wall的意思，产生尽可能多警告信息，-fno-builtin不要采用内部函数，
#-W 会显示警告，但是只显示编译器认为会出现错误的警告
#-Wstrict-prototypes 要求函数声明必须有参数类型，否则发出警告。-Wmissing-prototypes 必须要有函数声明，否则发出警告

LDFLAGS= -Ttext $(ENTRY_POINT) -e main -Map $(BUILD_DIR)/kernel.map -m elf_i386
#-Map,生成map文件，就是通过编译器编译之后，生成的程序、数据及IO空间信息的一种映射文件
#里面包含函数大小，入口地址等一些重要信息

OBJS=$(BUILD_DIR)/main.o $(BUILD_DIR)/init.o $(BUILD_DIR)/memory.o \
	$(BUILD_DIR)/interrupt.o $(BUILD_DIR)/timer.o $(BUILD_DIR)/kernel.o \
	$(BUILD_DIR)/print.o $(BUILD_DIR)/debug.o $(BUILD_DIR)/string.o \
	$(BUILD_DIR)/list.o $(BUILD_DIR)/bitmap.o $(BUILD_DIR)/switch.o \
	$(BUILD_DIR)/thread.o $(BUILD_DIR)/sync.o $(BUILD_DIR)/concole.o \
	$(BUILD_DIR)/keyboard.o $(BUILD_DIR)/ioquene.o $(BUILD_DIR)/tss.o \
	$(BUILD_DIR)/process.o $(BUILD_DIR)/syscall.o $(BUILD_DIR)/syscall_init.o \
	$(BUILD_DIR)/stdio.o $(BUILD_DIR)/stdio-kernel.o $(BUILD_DIR)/ide.o \
	$(BUILD_DIR)/fs.o $(BUILD_DIR)/file.o $(BUILD_DIR)/inode.o \
	$(BUILD_DIR)/dir.o $(BUILD_DIR)/fork.o $(BUILD_DIR)/shell.o \
	$(BUILD_DIR)/buildin_cmd.o
#顺序最好是调用在前，实现在后
	
######################编译C内核代码###################################################
# $@表示规则中目标文件名的集合这里就是$(BUILD_DIR)/main.o  
# $<表示规则中依赖文件的第一个，这里就是kernle/main.c 

$(BUILD_DIR)/main.o:kernel/main.c
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/init.o:kernel/init.c
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/interrupt.o:kernel/interrupt.c
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/timer.o:device/timer.c
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/debug.o:lib/kernel/debug.c 
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/string.o:lib/kernel/string.c
	$(CC) $(CFLAGS) -o $@ $<
	
$(BUILD_DIR)/list.o:lib/kernel/list.c
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/memory.o:lib/kernel/memory.c
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/bitmap.o:lib/kernel/bitmap.c
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/stdio-kernel.o:lib/kernel/stdio-kernel.c
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/thread.o:thread/thread.c
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/sync.o:thread/sync.c
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/concole.o:device/concole.c
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/keyboard.o:device/keyboard.c
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/ioquene.o:device/ioquene.c
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/ide.o:device/ide.c
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/tss.o:userprog/tss.c
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/process.o:userprog/process.c
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/fork.o:userprog/fork.c
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/syscall.o:lib/user/syscall.c
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/syscall_init.o:userprog/syscall-init.c
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/stdio.o:lib/stdio.c
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/fs.o:fs/fs.c
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/file.o:fs/file.c
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/inode.o:fs/inode.c
	$(CC) $(CFLAGS) -o $@ $<
	
$(BUILD_DIR)/dir.o:fs/dir.c
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/shell.o:shell/shell.c
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR)/buildin_cmd.o:shell/buildin_cmd.c
	$(CC) $(CFLAGS) -o $@ $<

###################编译汇编内核代码#####################################################

$(BUILD_DIR)/kernel.o:kernel/kernel.S 
	$(AS) $(ASFLAGS) -o $@ $<

$(BUILD_DIR)/print.o:lib/kernel/print.S
	$(AS) $(ASFLAGS) -o $@ $<

$(BUILD_DIR)/switch.o:thread/switch.S
	$(AS) $(ASFLAGS) -o $@ $<

##################链接所有内核目标文件##################################################
# $^表示规则中所有依赖文件的集合，如果有重复，会自动去重

$(BUILD_DIR)/kernel.bin:$(OBJS)
	$(LD) $(LDFLAGS) -o $@ $^

.PHONY:mk_dir hd clean build all	#定义了5个伪目标
mk_dir:
	if [ ! -d $(BUILD_DIR) ];then mkdir $(BUILD_DIR);fi 
#判断build文件夹是否存在，如果不存在，则创建

hd:
	dd if=$(BUILD_DIR)/kernel.bin of=$(HD60M_PATH) bs=512 count=200 seek=9 conv=notrunc
	
clean:
	@cd $(BUILD_DIR) && rm -f ./* && echo "remove ./build all done"
#-f, --force忽略不存在的文件，从不给出提示，执行make clean就会删除build下所有文件

build: $(BUILD_DIR)/kernel.bin
#执行build需要依赖kernel.bin，但是一开始没有，就会递归执行之前写好的语句编译kernel.bin

all: mk_dir build hd
#make all 就是依次执行mk_dir build hd
