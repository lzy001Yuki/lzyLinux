# Makefile for the simple_ramdisk kernel module

# obj-m 表示我们将要编译一个模块(module)
obj-m := simple_ramdisk.o

# KERNEL_DIR 指向你的Linux内核源代码目录
# !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
# !!!  把下面的路径修改为你自己的 linux-5.4.290 路径 !!!
# !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
KERNEL_DIR := /lib/modules/$(shell uname -r)/kernel

# PWD 表示当前工作目录
PWD := $(shell pwd)

all:
	# 使用你指定的 KERNEL_DIR 来编译模块
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) clean