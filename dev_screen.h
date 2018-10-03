#ifndef DEV_SCREEN_H
#define DEV_SCREEN_H

#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kobject.h>
#include <linux/miscdevice.h>

int dev_screen_create(struct device *parent);
void dev_screen_destroy(void);
int display_number(unsigned int value, unsigned int msecs);

#endif /* DEV_SCREEN_H */

