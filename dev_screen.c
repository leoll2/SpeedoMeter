#include <linux/gpio.h>
#include <linux/uaccess.h>

#include "dev_screen.h"

static struct miscdevice screen_device;  //forward declaration
static unsigned int my_pin = 40;
static bool active = false;

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
    active = !active;
    gpio_set_value(my_pin, active);
    return 0;
}

int dev_screen_create(struct device *parent) {
    int ret;
    
    screen_device.parent = parent;
    ret = misc_register(&screen_device);
    if (ret)
        return ret;
    
    
    /* GPIO stuff */
    // Request pin
    ret = gpio_request(my_pin, "mypin");
    if (ret) {
        printk(KERN_WARNING "Failed to request GPIO pin %d\n", my_pin);
        return ret;
    }
    // Set pin as output
    ret = gpio_direction_output(my_pin, 0);   // initially low
    if (ret) {
        printk(KERN_WARNING "Failed to set direction of GPIO pin %d\n", my_pin);
        return ret;
    }
    // Export pin to userspace (optional)
    ret = gpio_export(my_pin, false);
    if (ret) {
        printk(KERN_WARNING "Failed to export GPIO pin %d\n", my_pin);
        return ret;
    }
    
    
    return 0;
}

void dev_screen_destroy(void) {
    
    /* GPIO stuff */
    gpio_unexport(my_pin);
    gpio_free(my_pin);
    
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