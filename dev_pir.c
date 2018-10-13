#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/rtc.h>
#include <linux/sched.h>
#include <linux/uaccess.h>

#include "dev_pir.h"

#define PIN_PIR1	15 	// PIR1
#define PIN_PIR2	18	// PIR2

struct timespec t1,
			     t2;
static char last_irq_time_pir1[64];
static char last_irq_time_pir2[64];
struct completion sample_available;
struct completion sample_consumed;

unsigned int irq_pir1,
		       irq_pir2;

static struct gpio pir_gpios[] = {
	{ PIN_PIR1, GPIOF_IN, "PIR 1" },
	{ PIN_PIR2, GPIOF_IN, "PIR 2" },
};

static struct miscdevice pir1_device,  //forward declaration
					    pir2_device;

static int pir_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int pir_close(struct inode *inode, struct file *file)
{
    return 0;
}

static ssize_t pir_read(struct file *file, char __user *p, size_t len, loff_t *ppos)
{
	char buf[128];
	int cnt;
	if (*ppos != 0)
		return 0;
	cnt = snprintf(buf, 127, "PIR1: \t%s\nPIR2: \t%s\n", 
				strlen(last_irq_time_pir1) ? last_irq_time_pir1 : "never", 
				strlen(last_irq_time_pir2) ? last_irq_time_pir2 : "never");
	buf[cnt] = '\0';
	if (copy_to_user(p, buf, cnt)) {
		printk(KERN_ERR "Invalid address passed as argument to pir_read()\n");
		return -EFAULT;
	}
	*ppos += cnt;
	return cnt;
}

void save_irq_time(char* buf, time64_t tv_sec, long tv_nsec) {
	struct rtc_time tm;
	unsigned long local_time = (u32)(2 * 3600 + tv_sec - (sys_tz.tz_minuteswest * 60));
	rtc_time_to_tm(local_time, &tm);
	
	snprintf(buf, 63, "%04d-%02d-%02d %02d:%02d:%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
			tm.tm_hour, tm.tm_min, tm.tm_sec);
	buf[63] = '\0';
}

static irq_handler_t pir_irq_handler(unsigned int irq, void *dev, struct pt_regs *regs) {
	struct miscdevice *pdev = (struct miscdevice *)dev;
	if (pdev == &pir1_device) {
		if (t1.tv_sec == 0) {
			getnstimeofday(&t1);
			save_irq_time(last_irq_time_pir1, t1.tv_sec, t1.tv_nsec);
		}
	}
	else if (pdev == &pir2_device) {
		if (t1.tv_sec != 0 && t2.tv_sec == 0) {
			getnstimeofday(&t2);
			save_irq_time(last_irq_time_pir2, t2.tv_sec, t2.tv_nsec);
			complete(&sample_available);
		}
	}
	else {
		printk(KERN_WARNING "Interrupt received from unknown PIR device!\n");
		return (irq_handler_t) IRQ_NONE;
	}
	return (irq_handler_t) IRQ_HANDLED;
}

int dev_pir_create(struct device *parent) {
	int ret;    
	
	t1.tv_sec = t2.tv_sec = 0;
	last_irq_time_pir1[0] = last_irq_time_pir2[0] = '\0';
		
	// Register the first PIR device
	pir1_device.parent = parent;
	ret = misc_register(&pir1_device);
	if (ret)
		return ret;

	// Register the first PIR device
	pir2_device.parent = parent;
	ret = misc_register(&pir2_device);
	if (ret)
		return ret;
    
	// Request GPIO pins
	ret = gpio_request_array(pir_gpios, ARRAY_SIZE(pir_gpios));
	if (ret) {
	      	printk(KERN_WARNING "Failed to request PIR GPIO pins\n");
		return ret;
	}

	irq_pir1 = gpio_to_irq(pir_gpios[0].gpio);
	irq_pir2 = gpio_to_irq(pir_gpios[1].gpio);

	// Request IRQ line
	if (request_irq(irq_pir1, 
				(irq_handler_t) pir_irq_handler,
				IRQF_TRIGGER_RISING, 
				"pir_gpio_handler", 
				(void *)&pir1_device)) {
		printk(KERN_ERR "GPIO PIR1: cannot register IRQ\n");
		return -EIO;
	}
	if (request_irq(irq_pir2, 
				(irq_handler_t) pir_irq_handler,
				IRQF_TRIGGER_RISING, 
				"pir_gpio_handler", 
				(void *)&pir2_device)) {
		printk(KERN_ERR "GPIO PIR2: cannot register IRQ\n");
		return -EIO;
	}
	
	disable_irq(irq_pir1);	// Start with interrupts disabled
	disable_irq(irq_pir2);

	init_completion(&sample_available);
	init_completion(&sample_consumed);
	complete(&sample_consumed);

	return 0;
}

void dev_pir_destroy(void) {
	// Release the interrupt line
	free_irq(irq_pir1, (void *)&pir1_device);
	free_irq(irq_pir2, (void *)&pir2_device);

	// Free the GPIO pins    
	gpio_free_array(pir_gpios, ARRAY_SIZE(pir_gpios));

	// Unregister the devices
	misc_deregister(&pir1_device);
	misc_deregister(&pir2_device);
}


static struct file_operations pir_fops = {
    .owner =   	THIS_MODULE,
    .read =        pir_read,
    .open =       pir_open,
    .release =   pir_close,
};

static struct miscdevice pir1_device = {
    .minor =   	MISC_DYNAMIC_MINOR, 
    .name =   	"pir1", 
    .fops =    	&pir_fops,
};

static struct miscdevice pir2_device = {
    .minor =   	MISC_DYNAMIC_MINOR, 
    .name =   	"pir2", 
    .fops =    	&pir_fops,
};
