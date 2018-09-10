#include <linux/kthread.h>
#include <linux/uaccess.h>  // for copy_to_user

#include "dev_speed.h"
#include "dev_screen.h"
#include "dev_pir.h"

static struct miscdevice speed_device;  //forward declaration
static struct task_struct *speed_sampling_thread_desc;

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

ssize_t speed_read(struct file *file, char __user *p, size_t len, loff_t *ppos)
{
    copy_to_user(p, "gatto\n", 6);
    return 6;
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
   	.owner =        THIS_MODULE,
    	.read =         speed_read,
    	.open =         speed_open,
    	.release =      speed_close,
};

static struct miscdevice speed_device = {
    	MISC_DYNAMIC_MINOR, "speed", &speed_fops
};
