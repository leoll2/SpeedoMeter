#ifndef DEV_SCREEN_H
#define DEV_SCREEN_H

#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kobject.h>
#include <linux/miscdevice.h>

int dev_screen_create(struct device *parent);
void dev_screen_destroy(void);

#endif /* DEV_SCREEN_H */

