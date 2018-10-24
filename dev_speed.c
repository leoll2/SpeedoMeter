#include <linux/completion.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/ktime.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/wait.h>

#include "dev_speed.h"
#include "dev_screen.h"
#include "dev_pir.h"
#include "dev_ranking.h"

static struct miscdevice speed_device;
static struct task_struct *speed_sampling_thread_desc;
static char *username;
static int username_len;
static struct mutex username_mutex;
static struct completion dev_speed_comp;
unsigned int pir_dist;

static ssize_t leaderboard_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) 
{
	int ret;
	char *ranking_str;
	ret = get_ranking_as_str(&ranking_str);
	if (ret <= 0)
		return ret;
	strcpy(buf, ranking_str);
	kfree(ranking_str);
	return ret;
}

static ssize_t leader_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) 
{
	int ret;
	char *leader_str;
	ret = get_leader(&leader_str);
	if (ret <= 0)
		return ret;
	strcpy(buf, leader_str);
	kfree(leader_str);
	return ret;
}

static ssize_t reset_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count) 
{
	flush_ranking();
	return count;
}

static struct kobj_attribute leaderboard_attr = __ATTR_RO(leaderboard);
static struct kobj_attribute leader_attr = __ATTR_RO(leader);
static struct kobj_attribute reset_attr = __ATTR_WO(reset);

static struct attribute *speed_attrs[] = {
      &leaderboard_attr.attr,
      &leader_attr.attr,
      &reset_attr.attr,
      NULL,
};

static struct attribute_group attr_group = {
      /* .name  = "name_of_group",      // Add if you want a folder */
      .attrs = speed_attrs,
};

static int speed_sampling_thread(void *arg) 
{
	long delta_dsec;
	unsigned int vel;
	while(!kthread_should_stop()) {
		while(true) {
			int ret;
			wait_for_completion(&sample_available);
			disable_irq(irq_pir1);
			disable_irq(irq_pir2);
			// if missing data, the thread was woken up by the closing function
			if (t1.tv_sec == 0 || t2.tv_sec == 0)
				break;
				
			// Process the data coming from sensors
			delta_dsec = 10 * (t2.tv_sec - t1.tv_sec) + 
				     (t2.tv_nsec / 100000000) - (t1.tv_nsec / 100000000);
			vel = 10 * pir_dist / delta_dsec;	// decimeters / seconds
			ret = ranking_store_time(username, delta_dsec, vel);
			if (ret)
				printk(KERN_WARNING "Failed to add user to the ranking\n");
			display_number(delta_dsec, 5000, 1);

			t1.tv_sec = 0;
			t2.tv_sec = 0;
			kfree(username);
			username = NULL;
			complete(&dev_speed_comp);
		}
	}
	enable_irq(irq_pir1);	// reenable IRQs as at the beginning
	enable_irq(irq_pir2);
	printk(KERN_DEBUG "Closing speed sampling thread\n");
	return 0;
}

static int speed_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int speed_close(struct inode *inode, struct file *file)
{
	return 0;
}

static ssize_t speed_read(struct file *file, char __user *buf, size_t len, loff_t *ppos)
{
	int err, read_bytes;
	char temp[username_len+2]; 	// '\n' and '\0'
	
	if (*ppos != 0)
		return 0;
	
	mutex_lock(&username_mutex);
	if (username == NULL) {
		mutex_unlock(&username_mutex);
		return 0;
	}
	if (len > username_len)
		read_bytes = username_len;
	else
		read_bytes = len;
	
	read_bytes = snprintf(temp, read_bytes + 1, "%s\n", username);
	temp[read_bytes++] = '\0';
		
	err = copy_to_user(buf, temp, read_bytes);
	if (err) {
		mutex_unlock(&username_mutex);
		return -EFAULT;
	}
	
	mutex_unlock(&username_mutex);
	*ppos += read_bytes;
	return read_bytes;
}


static ssize_t speed_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos) 
{
	int err;
	if (try_wait_for_completion(&dev_speed_comp)) {
		// Store the username
		mutex_lock(&username_mutex);
		if (username) {
			mutex_unlock(&username_mutex);
			return -1;
		}
		username = kmalloc(count, GFP_USER);
		if (username == NULL) {
			mutex_unlock(&username_mutex);
			return -1;
		}
		username_len = count;
		
		err = copy_from_user(username, buf, count);
		if (err) {
			mutex_unlock(&username_mutex);
			return -EFAULT;
		}
		username[count-1] = '\0';	// replace \n with \0
		mutex_unlock(&username_mutex);
		// Enable IRQ from PIR1 and PIR2
		enable_irq(irq_pir1);
		enable_irq(irq_pir2);
		return count;
	}
	return -1;
}

int dev_speed_create(unsigned int sensors_dist) 
{
    	int ret;
    	struct kobject *kobj;
    	
    	pir_dist = sensors_dist;

	/* Register 'speed' device */
    	if (misc_register(&speed_device)) {
    		printk(KERN_ERR "Failed to register 'speed' device as misc.\n");
    		ret = 1;
        	goto exit1;
        }
    
    	/* Add 'speed' and its attributes to sysfs */
    	kobj = &speed_device.this_device->kobj;
    	if (sysfs_create_group(kobj, &attr_group)) {
        	dev_err(speed_device.this_device, "Failed to create sysfs group for 'speed' device.\n");
        	ret = 2;
        	goto exit2;
    	}
    
    	if (sysfs_create_link(kernel_kobj, kobj, "speed")) {
		dev_err(speed_device.this_device, "Failed to add sysfs like for 'speed' device.\n");
		ret = 3;
		goto exit3;
	}

	/* Create 'screen' device */
	if (dev_screen_create(speed_device.this_device)) {
		dev_err(speed_device.this_device, "Failed to create  'screen' device.\n");
		ret = 4;
    		goto exit4;
    	}
    	
    	/* Create 'pir' device */
	if (dev_pir_create(speed_device.this_device)) {
		dev_err(speed_device.this_device, "Failed to create  'pir' device.\n");
		ret = 5;
		goto exit5;
	}
	
	/* Create 'ranking' device */
	if (dev_ranking_create(speed_device.this_device)) {
		dev_err(speed_device.this_device, "Failed to create  'ranking' device.\n");
		ret = 6;
		goto exit6;
	}

	/* Initialize semaphores */
	mutex_init(&username_mutex);
	init_completion(&dev_speed_comp);
	complete(&dev_speed_comp);

	/* Start the thread for processing samples */
	speed_sampling_thread_desc = kthread_run(speed_sampling_thread, NULL, "speed sampling thread");
	if (IS_ERR(speed_sampling_thread_desc)) {
		printk(KERN_ERR "Failed to initialize the thread to handle the speed sampling.\n");
		ret = 7;
		goto exit7;
	}
	
	/* Here means that every previous action succeeded */
	printk("Speed device created (minor = %d)\n", speed_device.minor);
	ret = 0;
	goto exit0;

exit7:
	dev_ranking_destroy();
exit6:
	dev_pir_destroy();
exit5:
	dev_screen_destroy();	
exit4:
	sysfs_remove_link(kernel_kobj, "speed");
exit3:
	sysfs_remove_group(&speed_device.this_device->kobj, &attr_group);
exit2:    
	misc_deregister(&speed_device);
exit1:
exit0:
	return ret;
}

void dev_speed_destroy(void) 
{
	// if the thread was waiting for a sample, wake it up
	complete(&sample_available);
	kthread_stop(speed_sampling_thread_desc);
	dev_ranking_destroy();
	dev_pir_destroy();
	dev_screen_destroy();
	sysfs_remove_link(kernel_kobj, "speed");
	sysfs_remove_group(&speed_device.this_device->kobj, &attr_group);
	misc_deregister(&speed_device);
}

struct miscdevice* dev_speed_get_ptr(void) 
{
   	return &speed_device;
}

static struct file_operations speed_fops = {
   	.owner = 	THIS_MODULE,
    	.read = 	speed_read,
	.write =	speed_write,
    	.open = 	speed_open,
    	.release =	speed_close,
};

static struct miscdevice speed_device = {
    	MISC_DYNAMIC_MINOR, 
	"speed", 
	&speed_fops
};

