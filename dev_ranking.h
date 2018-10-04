#ifndef DEV_RANKING_H
#define DEV_RANKING_H

#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kobject.h>
#include <linux/miscdevice.h>

int dev_ranking_create(struct device *parent);
void dev_ranking_destroy(void);
int add_user_to_ranking(char *name, unsigned int time, unsigned int vel);
void flush_ranking(void);
void debug_print_ranking(void);		//DEBUG ONLY

#endif /* DEV_RANKING_H */
