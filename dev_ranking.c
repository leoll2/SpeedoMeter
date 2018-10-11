#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/string.h>

#include "dev_ranking.h"

static struct miscdevice ranking_device;  //forward declaration
static struct list_head ranking_head;
static struct mutex ranking_mutex;
static const char format[] = "%-16s | %-16u | %-16u\n";


struct user {
	char name[32];
	unsigned int best_time;
	unsigned int best_vel;
	struct list_head ul;
};

// Implicitly that the caller already holds a lock on the ranking
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

int add_new_user(char *name, unsigned int time, unsigned int vel) {
	struct user *new_user = kmalloc(sizeof(struct user), GFP_KERNEL);
	if (!new_user)
		return -1;
	strncpy(new_user->name, name, 31);
	new_user->best_time = time;
	new_user->best_vel = vel;
	mutex_lock(&ranking_mutex);
	list_add(&new_user->ul, find_pos_to_insert(time));
	mutex_unlock(&ranking_mutex);
	return 0;
}

int update_user(char *name, unsigned int time, unsigned int vel)  {
	struct user *u, *next;
	mutex_lock(&ranking_mutex);
	list_for_each_entry_safe(u, next, &ranking_head, ul) {
		if (!strncmp(name, u->name, 32)) {
			if (time < u->best_time) {
				u->best_time = time;
				u->best_vel = vel;
				//Reposition
				list_del(&u->ul);
				list_add(&u->ul, find_pos_to_insert(time));	//no issues because it returns thereafter
			}
			mutex_unlock(&ranking_mutex);
			return 0;
		}
	}
	mutex_unlock(&ranking_mutex);
	return 1;
}

int ranking_store_time(char *name, unsigned int time, unsigned int vel) {
	if (!update_user(name, time, vel))
		return 0;
	return add_new_user(name, time, vel);
}

// buf must be at least 128 bytes long, otherwise buffer overflow may occur.
unsigned int write_header(char *buf) {
	const char header_format[] = "%-16s | %-16s | %-16s\n%.*s\n";
	const char hline[] = "================================================";
	return snprintf(buf, 127, header_format, "USER", "TIME (ds)", "SPEED", 48, hline);
}

int get_ranking_as_str(char **pbuf) {
	unsigned int total_count = 0;
	unsigned int w_offset = 0;
	char temp[128];
	struct user *u;
	
	// Compute the number of bytes to be allocated
	total_count += write_header(temp); //snprintf(temp, 127, header_format, "USER", "TIME (ds)", "SPEED", 48, hline);
	mutex_lock(&ranking_mutex);
	list_for_each_entry(u, &ranking_head, ul) {
		total_count += snprintf(temp, 127, format, u->name, u->best_time, u->best_vel);
	}
	// Allocate memory dynamically
	*pbuf = (char*)kmalloc((total_count + 1) * sizeof(char), GFP_KERNEL);
	if (!*pbuf)
		return -1;
	// Generate the string
	w_offset = write_header(temp); //snprintf(temp, 127, header_format, "USER", "TIME (ds)", "SPEED", 48, hline);
	strncpy(*pbuf, temp, w_offset  );
	list_for_each_entry(u, &ranking_head, ul) {
		int cnt;
		cnt = snprintf(temp, 127, format, u->name, u->best_time, u->best_vel);
		strncpy(*pbuf + w_offset, temp, cnt);
		w_offset += cnt;
	}
	mutex_unlock(&ranking_mutex);
	// Add NUL at the end
	(*pbuf)[total_count] = '\0';
	return total_count;
}

int get_leader(char **plead) {
	struct user *u_first;
	int cnt;
	
	*plead = kmalloc(256 * sizeof(char), GFP_KERNEL);
	if (!(*plead))
		return -1;
	
	mutex_lock(&ranking_mutex);
	
	// If empty ranking
	if (ranking_head.next == &ranking_head)
		cnt = snprintf(*plead, 127, "There is no leader yet!\n");
	else {
		cnt = write_header(*plead);
		u_first = list_entry(ranking_head.next, struct user, ul);
		cnt += snprintf(*plead + cnt, 127, format, u_first->name, u_first->best_time, u_first->best_vel);
	}
	(*plead)[255] = '\0';
	mutex_unlock(&ranking_mutex);
	return cnt;
}

void debug_print_ranking() {
	struct user *u;
	mutex_lock(&ranking_mutex);
	list_for_each_entry(u, &ranking_head, ul) {
		printk(KERN_DEBUG "User: %s  time: %u  vel: %u\n", u->name, u->best_time, u->best_vel);
	}
	mutex_unlock(&ranking_mutex);
}

void flush_ranking(void) {
	struct user *u, *next;
	mutex_lock(&ranking_mutex);
	list_for_each_entry_safe(u, next, &ranking_head, ul) {
		list_del(&u->ul);
		kfree(u);
	}
	mutex_unlock(&ranking_mutex);
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
	char *s;
	int cnt;
	if (*ppos != 0)
		return 0;
	cnt = get_ranking_as_str(&s);
	if (cnt <= 0)
		return 0;
	if (len < cnt)
		cnt = len;
	copy_to_user(p, s, cnt);
	kfree(s);
	*ppos += cnt;
	return cnt;
}

int dev_ranking_create(struct device *parent) {
	int ret;    
	
	INIT_LIST_HEAD(&ranking_head);
	mutex_init(&ranking_mutex);
		
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
    .open =	ranking_open,
    .release =	ranking_close,
};

static struct miscdevice ranking_device = {
    .minor =  	MISC_DYNAMIC_MINOR, 
    .name =  	"ranking", 
    .fops =     	&ranking_fops,
};
