#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/kthread.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>
#include <linux/sched.h>

#include "dev_screen.h"

#define MIN_REFRESH_DELAY	500
#define MAX_REFRESH_DELAY	600

#define PIN_A	21	// segment A
#define PIN_B	20	// segment B
#define PIN_C	16	// segment C
#define PIN_D	12	// segment D
#define PIN_E	7	// segment E
#define PIN_F	8	// segment F
#define PIN_G	25	// segment G
#define PIN_H	24	// segment dot

#define PIN_3	26	// left-most digit
#define PIN_2	19
#define PIN_1	13
#define PIN_0	11	// right-most digit

static unsigned int digit_segments[][7] = {
	{1, 1, 1, 1, 1, 1, 0},	// 0
	{0, 1, 1, 0, 0, 0, 0},	// 1
	{1, 1, 0, 1, 1, 0, 1},	// 2
	{1, 1, 1, 1, 0, 0, 1},	// 3
	{0, 1, 1, 0, 0, 1, 1},	// 4
	{1, 0, 1, 1, 0, 1, 1},	// 5
	{1, 0, 1, 1, 1, 1, 1},	// 6
	{1, 1, 1, 0, 0, 0, 0},	// 7
	{1, 1, 1, 1, 1, 1, 1},	// 8
	{1, 1, 1, 1, 0, 1, 1}	// 9
};

static unsigned int default_segments[] =
	{0, 0, 0, 0, 0, 0, 1};	// -

static struct gpio screen_gpios[] = {
	{ PIN_A, GPIOF_OUT_INIT_LOW, "Screen segment A" },
	{ PIN_B, GPIOF_OUT_INIT_LOW, "Screen segment B" },
	{ PIN_C, GPIOF_OUT_INIT_LOW, "Screen segment C" },
	{ PIN_D, GPIOF_OUT_INIT_LOW, "Screen segment D" },
	{ PIN_E, GPIOF_OUT_INIT_LOW, "Screen segment E" },
	{ PIN_F, GPIOF_OUT_INIT_LOW, "Screen segment F" },
	{ PIN_G, GPIOF_OUT_INIT_LOW, "Screen segment G" },
	{ PIN_H, GPIOF_OUT_INIT_LOW, "Screen segment dot" },
	{ PIN_0, GPIOF_OUT_INIT_LOW, "Screen digit 0" },
	{ PIN_1, GPIOF_OUT_INIT_LOW, "Screen digit 1" },
	{ PIN_2, GPIOF_OUT_INIT_LOW, "Screen digit 2" },
	{ PIN_3, GPIOF_OUT_INIT_LOW, "Screen digit 3" },
};

static struct miscdevice screen_device;  //forward declaration
static struct task_struct *idle_screen_thread_desc;
static unsigned int last_num_displayed = 10000;
static unsigned int last_num_dot_pos;
static struct mutex last_num_mutex;

static void reset_default_segments(void) 
{
	unsigned int i;
	for (i = 0; i < 7; ++i) {
		gpio_set_value(screen_gpios[i].gpio, default_segments[i]);
	}
}

static void clear_digit_pins(void) 
{
	unsigned int i;
	for (i = 0; i < 4; ++i) {
		gpio_set_value(screen_gpios[i + 8].gpio, 0);
	}
}

static int idle_screen_thread(void *arg) 
{
	unsigned int digit_pos = 0;
	unsigned int i;

	while(!kthread_should_stop()) {
		// Clear all the four digit pins
		for (i = 0; i < 4; ++i) {
			gpio_set_value(screen_gpios[i + 8].gpio, 0);
		}
		// Prepare the default pattern
		reset_default_segments();

		// Show it in the right digit (reversed pos)
		gpio_set_value(screen_gpios[8 + 3 - digit_pos].gpio, 1);
		digit_pos = (digit_pos + 1) % 4;
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(HZ);
	}
	return 0;
}

static int start_idle_screen_thread(void) 
{
	if (idle_screen_thread_desc != NULL) {
		printk(KERN_WARNING "Can't start the idle screen thread because it's already running!\n");
		return 1;
	}
	idle_screen_thread_desc = kthread_run(idle_screen_thread, NULL, "idle_speed_screen");
	if (IS_ERR(idle_screen_thread_desc)) {
		printk(KERN_WARNING "Failed to initialize the screen with the idle pattern.\n");
		return PTR_ERR(idle_screen_thread_desc);
	}
	return 0;
}

static int stop_idle_screen_thread(void) 
{
	if (idle_screen_thread_desc == NULL) {
		printk(KERN_WARNING "Can't stop the idle screen thread because it wasn't running!\n");
		return 1;
	}
	kthread_stop(idle_screen_thread_desc);
	idle_screen_thread_desc = NULL;
	return 0;
}

static int display_digit(unsigned int digit, unsigned int value, bool dot) 
{
	unsigned int i;

	if (digit > 3 || value > 9)
		return 1;
	
	// Clear digit pins
	clear_digit_pins();

	// Set the appropriate digit pin
	gpio_set_value(screen_gpios[digit + 8].gpio, 1);

	// Set the appropriate segments
	for (i = 0; i < 7; ++i) {
		gpio_set_value(screen_gpios[i].gpio, digit_segments[value][i]);
	}
	gpio_set_value(screen_gpios[7].gpio, dot ? 1 : 0);
	return 0;
}

int display_number(unsigned int value, unsigned int msecs, unsigned int dot_pos) {
	unsigned int t;
	unsigned int digit_pos = 0;
	unsigned int digits[4];
	const unsigned int refresh_loops = 1000 * msecs / MIN_REFRESH_DELAY;

	if (value > 9999)
		return 1;
		
	mutex_lock(&last_num_mutex);
	last_num_displayed = value;
	last_num_dot_pos = dot_pos;
	mutex_unlock(&last_num_mutex);

	digits[0] = value % 10;
	digits[1] = value / 10 % 10;
	digits[2] = value / 100 % 10;
	digits[3] = value / 1000;

	stop_idle_screen_thread();

	for (t = 0; t < refresh_loops; ++t) {
		display_digit(digit_pos, digits[digit_pos], digit_pos == dot_pos);
		digit_pos = (digit_pos + 1) % 4;		
		usleep_range(MIN_REFRESH_DELAY, MAX_REFRESH_DELAY);
	}

	start_idle_screen_thread();
	return 0;
}

static int screen_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int screen_close(struct inode *inode, struct file *file)
{
	return 0;
}

static ssize_t screen_read(struct file *file, char __user *p, size_t len, loff_t *ppos)
{
	char buf[32];
	unsigned int i;
	int cnt, divisor = 1;
	
	if (*ppos != 0)
		return 0;
		
	mutex_lock(&last_num_mutex);
	if (last_num_dot_pos < 4) {
		for (i = 0; i < last_num_dot_pos; ++i)
			divisor *= 10;
	}
	if (last_num_displayed >= 10000)	// default value
		cnt = snprintf(buf, 31, "No number displayed yet!\n");
	else
		cnt = snprintf(buf, 7, "%d.%d\n", last_num_displayed / divisor, last_num_displayed % divisor);
	mutex_unlock(&last_num_mutex);
	
	buf[cnt] = '\0';
	if (copy_to_user(p, buf, cnt)) {
		printk(KERN_ERR "Invalid address passed as argument to screen_read()\n");
		return -EFAULT;
	}
	*ppos += cnt;
	return cnt;
}

int dev_screen_create(struct device *parent) 
{
	int ret;    
	
	mutex_init(&last_num_mutex);
		
	// Register the device
	screen_device.parent = parent;
	ret = misc_register(&screen_device);
	if (ret)
		return ret;
    
	// Request GPIO pins
	ret = gpio_request_array(screen_gpios, ARRAY_SIZE(screen_gpios));
	if (ret) {
	      	printk(KERN_WARNING "Failed to request screen GPIO pins\n");
		return ret;
	}

	// Show the default pattern
	ret = start_idle_screen_thread();
	if (ret) {
		printk(KERN_WARNING "Failed to initialize the screen with the idle pattern.\n");
		return ret;
	}
	return 0;
}

void dev_screen_destroy(void) 
{
	// Stop the thread showing the idle pattern on the screen
	stop_idle_screen_thread();

	// Remove leftover output 
	clear_digit_pins();

	// Free the GPIO pins    
	gpio_free_array(screen_gpios, ARRAY_SIZE(screen_gpios));

	// Unregister the device    
	misc_deregister(&screen_device);
}


static struct file_operations screen_fops = {
    .owner = 	THIS_MODULE,
    .read = 	screen_read,
    .open = 	screen_open,
    .release = 	screen_close,
};

static struct miscdevice screen_device = {
    .minor = 	MISC_DYNAMIC_MINOR, 
    .name = 	"screen", 
    .fops = 	&screen_fops,
};
