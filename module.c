#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/stat.h>

#include "dev_speed.h"

MODULE_AUTHOR("Leonardo Lai");
MODULE_DESCRIPTION("Measure speed of a moving object");
MODULE_LICENSE("GPL");

static unsigned int sensors_dist = 100;
module_param(sensors_dist, uint, S_IRUGO);
MODULE_PARM_DESC(sensors_dist, "Distance between PIR sensors (decimeters)");

static int __init speed_module_init(void)
{
    int res;
    
    res = dev_speed_create(sensors_dist);
    if (res < 0) {
        printk(KERN_ERR "Failed to create the speed device.\n");
        return res;
    }
    printk("Speed module loaded\n");
    return 0;
}

static void __exit speed_module_exit(void)
{
    dev_speed_destroy();
    printk("Speed module unloaded\n");
}

module_init(speed_module_init);
module_exit(speed_module_exit);
