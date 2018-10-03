#ifndef DEV_PIR_H
#define DEV_PIR_H

#include <linux/completion.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kobject.h>
#include <linux/miscdevice.h>

extern struct timespec t1,
					 t2;
extern struct completion sample_available;
extern struct completion sample_consumed;

extern unsigned int irq_pir1,
				   irq_pir2;

int dev_pir_create(struct device *parent);
void dev_pir_destroy(void);

#endif /* DEV_PIR_H */

