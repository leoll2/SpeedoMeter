#ifndef DEV_RANKING_H
#define DEV_RANKING_H

#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kobject.h>
#include <linux/miscdevice.h>

int dev_ranking_create(struct device *parent);
void dev_ranking_destroy(void);
int ranking_store_time(char *name, unsigned int time, unsigned int vel);
void flush_ranking(void);
int get_ranking_as_str(char **pbuf);
int get_leader(char **plead);

#endif /* DEV_RANKING_H */
