#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/uaccess.h>

#include "dev_pir.h"

#define PIN_PIR1	15 	// PIR1
#define PIN_PIR2 18	// PIR2

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
    copy_to_user(p, "beppe\n", 6);
    return 6;
}

static irq_handler_t pir_irq_handler(unsigned int irq, void *dev, struct pt_regs *regs) {
	struct miscdevice *pdev = (struct miscdevice *)dev;
	if (pdev == &pir1_device)
		printk(KERN_DEBUG "Interrupt received from PIR1");
	else if (pdev == &pir2_device)
		printk(KERN_DEBUG "Interrupt received from PIR2");
	else
		printk(KERN_WARNING "Interrupt received from unknown PIR device!");
	return (irq_handler_t) IRQ_HANDLED;
}

int dev_pir_create(struct device *parent) {
	int ret;    
		
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

	gpio_set_debounce(pir_gpios[0].gpio, 1000);	// FOR BUTTON ONLY!!!

	// Request IRQ line
	ret = request_irq(gpio_to_irq(pir_gpios[0].gpio), (irq_handler_t) pir_irq_handler,
				     IRQF_TRIGGER_RISING, "pir_gpio_handler", (void *)&pir1_device);
	if (ret) {
		printk(KERN_WARNING "Failed to request interrupt line for GPIO PIR1.\n");
		return ret;
	}
	ret = request_irq(gpio_to_irq(pir_gpios[1].gpio), (irq_handler_t) pir_irq_handler,
				     IRQF_TRIGGER_RISING, "pir_gpio_handler", (void *)&pir2_device);
	if (ret) {
		printk(KERN_WARNING "Failed to request interrupt line for GPIO PIR2.\n");
		return ret;
	}

	return 0;
}

void dev_pir_destroy(void) {
	// Release the interrupt line
	free_irq(gpio_to_irq(pir_gpios[0].gpio), (void *)&pir1_device);
	free_irq(gpio_to_irq(pir_gpios[1].gpio), (void *)&pir2_device);

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
