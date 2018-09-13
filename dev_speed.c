#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/uaccess.h>  // for copy_to_user

#include "dev_speed.h"
#include "dev_screen.h"
#include "dev_pir.h"

static struct miscdevice speed_device;  //forward declaration
static struct task_struct *speed_sampling_thread_desc;
static char *username;
static int username_len;
static struct mutex username_mutex;
static struct mutex dev_speed_mutex;

static int pippo = 3;

static ssize_t pippo_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {
    return sprintf(buf, "%d\n", pippo);
}

static ssize_t pippo_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count) {
    sscanf(buf, "%du", &pippo);
    return count;
}

static struct kobj_attribute pippo_attr = __ATTR(pippo, 0664, pippo_show, pippo_store);

static struct attribute *prova_attrs[] = {
      &pippo_attr.attr,
      NULL,
};

static struct attribute_group attr_group = {
      /* .name  = "nome_gruppo",      // Add if you want a folder */
      .attrs = prova_attrs,
};

static int speed_sampling_thread(void *arg) {

	// COMPLETE VEL CONSUMATA
	while(!kthread_should_stop()) {
		// WAIT VEL DISPONIBILE
		// ELABORA
		// COMPLETE VEL CONSUMATA
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(HZ);
	}
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
	mutex_lock(&username_mutex);
	if (username == NULL) {
		mutex_unlock(&username_mutex);
		return 0;
	}
	if (len > username_len)
		read_bytes = username_len;
	else
		read_bytes = len;
	err = copy_to_user(buf, username, read_bytes);
	if (err) {
		mutex_unlock(&username_mutex);
		return -EFAULT;
	}
	mutex_unlock(&username_mutex);
	return read_bytes;
}


static ssize_t speed_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos) {
	int err;
	if (mutex_trylock(&dev_speed_mutex)) {
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
		mutex_unlock(&username_mutex);
		// Enable IRQ from PIR1 and PIR2
		//enable_irq(irq_pir1);
		//enable_irq(irq_pir2);
		return count;
	}
	return -1;
}

int dev_speed_create(void) {
    	int ret;
    	struct kobject *kobj;
    
    	ret = misc_register(&speed_device);
    	if (ret)
        	return ret;
    
    	kobj = &speed_device.this_device->kobj;

    	ret = sysfs_create_group(kobj, &attr_group);
    	if(ret) {
        	dev_err(speed_device.this_device, "failed to register sysfs\n");
        	return ret;
    	}
    
    	ret = sysfs_create_link(kernel_kobj, kobj, "speed");
	if(ret) {
		dev_err(speed_device.this_device, "Failed to register sysfs\n");
		return ret;
	}

	ret = dev_screen_create(speed_device.this_device);
	if (ret)
    		return ret;
	ret = dev_pir_create(speed_device.this_device);
	if (ret)
		return ret;

	mutex_init(&username_mutex);
	mutex_init(&dev_speed_mutex);

	speed_sampling_thread_desc = kthread_run(speed_sampling_thread, NULL, "speed sampling thread");
	if (IS_ERR(speed_sampling_thread_desc)) {
		printk(KERN_WARNING "Failed to initialize the thread to handle the speed sampling.\n");
		return PTR_ERR(speed_sampling_thread_desc);
	}
	// TODO: GESTIRE CONDIZIONI DI ERRORE!!!
    
	printk("Speed device created (minor = %d)\n", speed_device.minor);
	return 0;
}

void dev_speed_destroy(void) {
	kthread_stop(speed_sampling_thread_desc);
	dev_pir_destroy();
	dev_screen_destroy();
	sysfs_remove_link(kernel_kobj, "speed");
	sysfs_remove_group(&speed_device.this_device->kobj, &attr_group);
	misc_deregister(&speed_device);
}

struct miscdevice* dev_speed_get_ptr(void) {
   	return &speed_device;
}

static struct file_operations speed_fops = {
   	.owner =      	THIS_MODULE,
    	.read =         	speed_read,
	.write =		speed_write,
    	.open =         	speed_open,
    	.release =      	speed_close,
};

static struct miscdevice speed_device = {
    	MISC_DYNAMIC_MINOR, "speed", &speed_fops
};
