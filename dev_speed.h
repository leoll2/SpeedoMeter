#ifndef DEV_SPEED_H
#define DEV_SPEED_H

#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kobject.h>
#include <linux/miscdevice.h>

int dev_speed_create(unsigned int sensors_dist);
void dev_speed_destroy(void);
struct miscdevice* dev_speed_get_ptr(void);

#endif /* DEV_SPEED_H */

