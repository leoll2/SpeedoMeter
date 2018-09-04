KERNEL_DIR ?= ~/Projects/linux/
KERNEL_BZIMAGE ?= ~/Projects/linux/arch/x86_64/boot/bzImage

obj-m = speed.o
speed-objs = module.o devices.o dev_speed.o dev_screen.o

auto:
	make clean && \
	make all && \
	mv speed.ko tmp/ && \
	cd tmp/ && \
	find . | cpio -H newc -o | gzip > ../mod.gz && \
	cd .. && \
	cat tinyfs.gz mod.gz > newfs.gz && \
	qemu-system-x86_64 -kernel $(KERNEL_BZIMAGE) -initrd newfs.gz

all:
	make -C $(KERNEL_DIR) M=`pwd` modules
clean:
	make -C $(KERNEL_DIR) M=`pwd` clean