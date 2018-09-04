#include <linux/uaccess.h> 

#include "dev_screen.h"

static struct miscdevice screen_device;  //forward declaration

static int screen_open(struct inode *inode, struct file *file)
{
    return 0;
}

static int screen_close(struct inode *inode, struct file *file)
{
    return 0;
}

ssize_t screen_read(struct file *file, char __user *p, size_t len, loff_t *ppos)
{
    copy_to_user(p, "scalo\n", 6);
    return 6;
}


int dev_screen_create(struct device *parent) {
    int ret;
    
    screen_device.parent = parent;
    ret = misc_register(&screen_device);
    if (ret)
        return ret;
    
    return 0;
}

void dev_screen_destroy(void) {
    misc_deregister(&screen_device);
}


static struct file_operations screen_fops = {
    .owner =        THIS_MODULE,
    .read =         screen_read,
    .open =         screen_open,
    .release =      screen_close,
};

static struct miscdevice screen_device = {
    .minor =    MISC_DYNAMIC_MINOR, 
    .name =     "screen", 
    .fops =     &screen_fops,
};