#ifndef DEV_PIR_H
#define DEV_PIR_H

#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kobject.h>
#include <linux/miscdevice.h>

int dev_pir_create(struct device *parent);
void dev_pir_destroy(void);

#endif /* DEV_PIR_H */

