#include <linux/gpio.h>
#include <linux/uaccess.h>

#include "dev_screen.h"

#define PIN_A	21	// segment A
#define PIN_B	20	// segment B
#define PIN_C	16	// segment C
#define PIN_D	12	// segment D
#define PIN_E	7	// segment E
#define PIN_F	8	// segment F
#define PIN_G	25	// segment G
#define PIN_H	24	// segment dot

#define PIN_3	26	// left-most digit
#define PIN_2	19
#define PIN_1	13
#define PIN_0	6	// right-most digit

static unsigned int digit_segments[][7] = {
	{1, 1, 1, 1, 1, 1, 0},	// 0
	{0, 1, 1, 0, 0, 0, 0},	// 1
	{1, 1, 0, 1, 1, 0, 1},	// 2
	{1, 1, 1, 1, 0, 0, 1},	// 3
	{0, 1, 1, 1, 0, 0, 1},	// 4
	{1, 0, 1, 1, 0, 1, 1},	// 5
	{1, 0, 1, 1, 1, 1, 1},	// 6
	{1, 1, 1, 0, 0, 0, 0},	// 7
	{1, 1, 1, 1, 1, 1, 1},	// 8
	{1, 1, 1, 1, 0, 1, 1}	// 9
};

static struct gpio screen_gpios[] = {
	{ PIN_A, GPIOF_OUT_INIT_LOW, "Screen segment A" },
	{ PIN_B, GPIOF_OUT_INIT_LOW, "Screen segment B" },
	{ PIN_C, GPIOF_OUT_INIT_LOW, "Screen segment C" },
	{ PIN_D, GPIOF_OUT_INIT_LOW, "Screen segment D" },
	{ PIN_E, GPIOF_OUT_INIT_LOW, "Screen segment E" },
	{ PIN_F, GPIOF_OUT_INIT_LOW, "Screen segment F" },
	{ PIN_G, GPIOF_OUT_INIT_LOW, "Screen segment G" },
	{ PIN_H, GPIOF_OUT_INIT_LOW, "Screen segment dot" },
	{ PIN_0, GPIOF_OUT_INIT_LOW, "Screen digit 0" },
	{ PIN_1, GPIOF_OUT_INIT_LOW, "Screen digit 1" },
	{ PIN_2, GPIOF_OUT_INIT_LOW, "Screen digit 2" },
	{ PIN_3, GPIOF_OUT_INIT_LOW, "Screen digit 3" },
};

static struct miscdevice screen_device;  //forward declaration


static int display_digit(unsigned int digit, unsigned int value) {
	unsigned int i;

	if (digit > 3 || value > 9)
		return 1;
	
	// Clear digit pins
	for (i = 0; i < 4; ++i) {
		gpio_set_value(screen_gpios[i + 8].gpio, 0);
	}

	// Set the appropriate digit pin
	gpio_set_value(screen_gpios[digit + 8].gpio, 1);

	// Set the appropriate segments
	for (i = 0; i < 7; ++i) {
		gpio_set_value(screen_gpios[i].gpio, digit_segments[value][i]);
	}
	return 0;
} 

static int screen_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int screen_close(struct inode *inode, struct file *file)
{
    return 0;
}

static ssize_t screen_read(struct file *file, char __user *p, size_t len, loff_t *ppos)
{
    copy_to_user(p, "scalo\n", 6);
    return 6;
}

static ssize_t screen_write(struct file *file, const char __user *p, size_t len, loff_t *ppos)
{
    display_digit(0, 5);
    return 1;
}

int dev_screen_create(struct device *parent) {
    int ret;
    
    screen_device.parent = parent;
    ret = misc_register(&screen_device);
    if (ret)
        return ret;
    
    /* GPIO stuff */
    // Request pin
    ret = gpio_request_array(screen_gpios, ARRAY_SIZE(screen_gpios));
    if (ret) {
        printk(KERN_WARNING "Failed to request screen GPIO pins");
        return ret;
    }
  
    return 0;
}

void dev_screen_destroy(void) {
    
    /* GPIO stuff */
    gpio_free_array(screen_gpios, ARRAY_SIZE(screen_gpios));
    
    misc_deregister(&screen_device);
}


static struct file_operations screen_fops = {
    .owner =        THIS_MODULE,
    .read =         screen_read,
    .write =        screen_write,
    .open =         screen_open,
    .release =      screen_close,
};

static struct miscdevice screen_device = {
    .minor =    MISC_DYNAMIC_MINOR, 
    .name =     "screen", 
    .fops =     &screen_fops,
};
