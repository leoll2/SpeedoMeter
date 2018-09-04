VM_KERNEL_DIR ?= ~/Projects/linux/
VM_KERNEL_BZIMAGE ?= ~/Projects/linux/arch/x86_64/boot/bzImage
RPI_KERNEL_DIR ?= /lib/modules/`uname -r`/build/


obj-m = speed.o
speed-objs = module.o devices.o dev_speed.o dev_screen.o

default:
	echo "Please specify if you run on Raspberry (rpi) or virtual machine (vm)"

vm:
	make vm_clean && \
	make vm_all && \
	mv speed.ko tmp/ && \
	cd tmp/ && \
	find . | cpio -H newc -o | gzip > ../mod.gz && \
	cd .. && \
	cat tinyfs.gz mod.gz > newfs.gz && \
	qemu-system-x86_64 -kernel $(VM_KERNEL_BZIMAGE) -initrd newfs.gz

vm_all:
	make -C $(VM_KERNEL_DIR) M=`pwd` modules
vm_clean:
	make -C $(VM_KERNEL_DIR) M=`pwd` clean
	
rpi:
	make rpi_clean && \
	make rpi_all

rpi_all:
	make -C $(RPI_KERNEL_DIR) M=`pwd` modules
	
rpi_clean:
	make -C $(RPI_KERNEL_DIR) M=`pwd` clean

