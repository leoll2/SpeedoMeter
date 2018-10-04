#include <linux/list.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/string.h>

#include "dev_ranking.h"

static struct miscdevice ranking_device;  //forward declaration

struct user {
	char name[32];
	unsigned int best_time;
	unsigned int best_vel;
	struct list_head ul;
};

static struct list_head ranking_head;

struct list_head *find_pos_to_insert(unsigned int time) {
	struct list_head *p;
	struct user *u_next;
	
	// If the list is empty
	if (ranking_head.next == &ranking_head) {
		return &ranking_head;
	}
	
	// If the element should be the first in the list
	u_next = list_entry(ranking_head.next, struct user, ul);
	if (time <= u_next->best_time) {
		return &ranking_head;
	}
	
	// Else iterate over the list to find the appropriate position
	list_for_each(p, &ranking_head) {
		if (p->next != &ranking_head) {
			u_next = list_entry(p->next, struct user, ul);
			if (time <= u_next->best_time)
				return p;
		}
	}
	return ranking_head.prev;
} 

int add_user_to_ranking(char *name, unsigned int time, unsigned int vel) {
	struct list_head *pos;
	struct user *new_user = kmalloc(sizeof(struct user), GFP_KERNEL);
	if (!new_user)
		return -1;
	strncpy(new_user->name, name, 31);
	new_user->best_time = time;
	list_add(&new_user->ul, find_pos_to_insert(time));
	return 0;
}

void debug_print_ranking() {
	struct user *u;
	list_for_each_entry(u, &ranking_head, ul) {
		printk(KERN_DEBUG "User: %s  time: %u  vel: %u\n", u->name, u->best_time, u->best_vel);
	}
}

void flush_ranking(void) {
	struct user *u, *next;
	list_for_each_entry_safe(u, next, &ranking_head, ul) {
		list_del(&u->ul);
		kfree(u);
	}
	printk(KERN_DEBUG "List freed!!!\n");
}

static int ranking_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int ranking_close(struct inode *inode, struct file *file)
{
    return 0;
}

static ssize_t ranking_read(struct file *file, char __user *p, size_t len, loff_t *ppos)
{
    copy_to_user(p, "balice\n", 7);
    return 7;
}

static ssize_t ranking_write(struct file *file, const char __user *p, size_t len, loff_t *ppos)
{
	return 1;
}

int dev_ranking_create(struct device *parent) {
	int ret;    
	
	INIT_LIST_HEAD(&ranking_head);
		
	// Register the device
	ranking_device.parent = parent;
	ret = misc_register(&ranking_device);
	if (ret)
		return ret;
    
	return 0;
}

void dev_ranking_destroy(void) {
	flush_ranking();
	// Unregister the device    
	misc_deregister(&ranking_device);
}


static struct file_operations ranking_fops = {
    .owner =  	THIS_MODULE,
    .read =	ranking_read,
    .write =	ranking_write,
    .open =	ranking_open,
    .release =	ranking_close,
};

static struct miscdevice ranking_device = {
    .minor =  	MISC_DYNAMIC_MINOR, 
    .name =  	"ranking", 
    .fops =     	&ranking_fops,
};
