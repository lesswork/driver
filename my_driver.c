#include <linux/init.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/spinlock_types.h>
#include <asm/uaccess.h>
#include <linux/gpio.h>
#include <linux/ioctl.h>
#include <linux/version.h>
#include "gpio.h"

#define SUCCESS 0
#define DEVICE_NAME "pin"
#define CLASS_DEVICE_NAME "pinClass"
#define MY_NDEVICES 4
#define NUMBER_OF_MINOR_DEVICE 1
#define GPIOMAXNAME	32

//Enable it for define more than one /sys/class nodes for communication
//#define GROUP_ATTRS

/* The structure to represent 'my_dev' devices.
 * my_dev_mutex - a mutex to protect the fields of this structure;
 * cdev - character device structure.
 */

struct gp_info {
	char gp_name[GPIOMAXNAME];      /* pin name */
	int gp_pin;				/* pin number */
	int gp_type;			/* pin multiplex type */
	int gp_value;			/* value */
	int gp_flags;			/* pin request configuration flags */
	struct device *device;	/* struct device should stores here */
};

struct my_dev {
	struct gp_info* gpio_info[MY_NDEVICES];

	struct mutex my_dev_mutex;
	struct spinlock gpio_spinlock;
	struct cdev my_cdev;
};


static int my_device_Open;
static int my_ndevices = MY_NDEVICES;		//number of device which can access your driver
//The first minor number in case you are looking for a series of minor numbers for your driver. 
// GPIO1_21 = 32 (GPIO 0) + 21 (GPIO 1)
int firstminor = 32 + 21;

static struct my_dev *my_devices = NULL;
static struct device *device[MY_NDEVICES] = {0, };
static struct cdev *my_cdev = NULL;
static struct class *my_class = NULL;

static int minor = 0;

static int debug = 0;
static int major = 0;
module_param(debug, int, 0);
module_param(major, int, 0);
MODULE_PARM_DESC(debug, "An Integer type for debug level (1,2,3)");

/* We'll use our own macros for printk */
#define CLASS_NAME "pinClass"
#define dbg1(format, arg...) do { if (debug > 0) pr_info(CLASS_NAME ": %s: " format, __FUNCTION__, ## arg); } while (0)
#define dbg2(format, arg...) do { if (debug > 1) pr_info(CLASS_NAME ": %s: " format, __FUNCTION__, ## arg); } while (0)
#define dbg3(format, arg...) do { if (debug > 2) pr_info(CLASS_NAME ": %s: " format, __FUNCTION__, ## arg); } while (0)
#define err(format, arg...) pr_err(CLASS_NAME ": %s: " format, __FUNCTION__, ## arg)
#define info(format, arg...) pr_info(CLASS_NAME ": %s: " format, __FUNCTION__, ## arg)
#define warn(format, arg...) pr_warn(CLASS_NAME ": %s: " format, __FUNCTION__, ## arg)
//#define err(format, arg...) pr_err(CLASS_NAME ": " format, ## arg)
//#define info(format, arg...) pr_info(CLASS_NAME ": " format, ## arg)
//#define warn(format, arg...) pr_warn(CLASS_NAME ": " format, ## arg)

static ssize_t work_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t count)
{
	//code for testing purpose only
	int gpio = 53;
	int err = 0;
	dbg1("Android_store() %s\n",buf);
	info("pin = %d\n", gpio);
	info("write %d\n", ((*buf)-'0'));

	/* GPIO OUTPUT */
	err = gpio_request_one(gpio, GPIOF_OUT_INIT_LOW, "USR0 LED");
	if(err)
		err("gpio_request_one failed.\n");

	gpio_set_value(gpio, ((*buf)-'0'));

	return PAGE_SIZE;
}

static ssize_t work_show(struct device* dev, struct device_attribute* attr, char* buf)
{
	int gpio = 53;
	dbg1("Android_show() %u\n", major);
	gpio_set_value(gpio, 0);

	return snprintf(buf, 10, "%u\n", major);
}

static DEVICE_ATTR(gpioAndroid, S_IRUGO | S_IWUGO, work_show, work_store);

///////////////////// Group Attribute /////////////////////////

#ifdef GROUP_ATTRS
static struct attribute *dev_attrs[] = {
	&dev_attr_gpioAndroid.attr,
	NULL,
};

static struct attribute_group dev_attr_group = {
	.attrs = dev_attrs,
};

static const struct attribute_group *dev_attr_groups[] = {
	&dev_attr_group,
	NULL,
};
static struct kobject *example_kobj;
#endif

///////////////////////////////////////////////////////////////

static ssize_t device_file_read(struct file *file, char __user *buffer, size_t length, loff_t *offset)
{
	struct my_dev *my_devices = (struct my_dev *) file->private_data;
	int value = 0;
	int gpio = iminor(file->f_path.dentry->d_inode);
	dbg3("IN\n");

	value = gpio_get_value(gpio);
	my_devices->gpio_info[gpio - firstminor]->gp_value = value;
	//value returned as integer in user buffer
	*buffer = value;

	return 0;
}

static ssize_t device_file_write(struct file *file, const char __user *buffer, size_t length, loff_t *offset)
{
	struct my_dev *my_devices = (struct my_dev *) file->private_data;
	int value = *buffer - '0';
	int gpio = iminor(file->f_path.dentry->d_inode);
	dbg3("IN buffer : %c\n",*buffer);

	/* GPIO OUTPUT */
	if((my_devices->gpio_info[gpio - firstminor]->gp_value && 1) != (value && 1)) {
		gpio_set_value(gpio, value);
		my_devices->gpio_info[gpio - firstminor]->gp_value = value;
		dbg2("pin = %d write %d\n", gpio, value);
	}
	return 0;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35))
static int device_ioctl(struct inode *node, struct file *file, unsigned int cmd, unsigned long arg)
#else
static long device_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
#endif
{
	int ret = 0;
	int gpioNo = iminor(file->f_path.dentry->d_inode);
	dbg1("/dev/pin-%d ioctl opening\n", gpioNo);

	switch (cmd)
	{
		case EDGE_SETPORT_GPIO_OUTPUT:
			/* GPIO OUTPUT */
			//ret = gpio_request_one(gpioNo, GPIOF_OUT_INIT_LOW, "USR LED");
			dbg1("gpio_direction_output OUTPUT\n");
			ret = gpio_direction_output(gpioNo, 0);
			if(ret < 0) {
				err("\tfailed : %d\n", ret);
				return ret;
			}
			my_devices->gpio_info[gpioNo - firstminor]->gp_flags |= GPIOF_OUT_INIT_LOW;
			my_devices->gpio_info[gpioNo - firstminor]->gp_type |= GPIO_OUTPUT_PORT;
			break;

		case EDGE_SETPORT_GPIO_INPUT:
			/* GPIO INPUT */
			//ret = gpio_request_one(gpioNo, GPIOF_IN, "USR LED");
			dbg1("gpio_direction_output INPUT\n");
			ret = gpio_direction_input(gpioNo);
			if(ret < 0) {
				err("\tfailed : %d\n", ret);
				return ret;
			}
			my_devices->gpio_info[gpioNo - firstminor]->gp_flags |= GPIOF_IN;
			my_devices->gpio_info[gpioNo - firstminor]->gp_type |= GPIO_INPUT_PORT;
			break;

		case EDGE_SETPORT_ADC:
			// Enhancement
		case EDGE_SETPORT_PWM:
			// Enhancement
		default:
			return -EINVAL;
	}
	return ret;
}

static int device_open(struct inode *node, struct file *file)
{
	int minorNo = iminor(node);
	int arrayNo = minorNo - firstminor;
	int ret = 0;

	dbg3("IN\n");
	if(my_device_Open > 4)
	{
		return -EBUSY;
	}
	my_device_Open++;		//increment count when device is open.
	info("/dev/pin-%d opening\n", minorNo);

	my_devices->gpio_info[arrayNo] = (struct gp_info *) kzalloc((sizeof(struct gp_info)), GFP_KERNEL);
	if(my_devices->gpio_info[arrayNo] == NULL)
	{
		err("my_devices->gpio_info failed. No memory allocated to structure.\n");
		return -ENOMEM;
	}
	dbg2("my_Device->gpio_info[%d]) malloc called\n",minorNo - firstminor);

	dbg2("node->i_cdev: %x\n", node->i_cdev);
	dbg2("my_cdev: %x\n", my_cdev);

	my_devices->gpio_info[arrayNo]->device = device[arrayNo];
	dbg2("MAJOR(node->i_rdev): %d i_fop: %x\n", MAJOR(node->i_rdev), node->i_fop);

	/* GPIO OUTPUT */
	ret = gpio_request_one(minorNo, GPIOF_OUT_INIT_LOW, "USR LED");
	if(ret) {
		err("gpio_request_one failed.\n");
	}

	my_devices->gpio_info[minorNo - firstminor]->gp_pin = minorNo;
	file->private_data = (struct my_dev *) my_devices;
	dbg3("OUT\n");

	return SUCCESS;
}

static int device_release(struct inode *node, struct file *file)
{
	struct my_dev *my_devices = (struct my_dev *) file->private_data;
	int minorNo = iminor(node);

	dbg3("IN\n");
	my_device_Open--;       //Decrement count when device is open.

	if(my_devices)
	{
		kfree(my_devices->gpio_info[minorNo - firstminor]);
		my_devices->gpio_info[minorNo - firstminor] = NULL;
		dbg2("my_Device->gpio_info[%d]) free called\n",minorNo - firstminor);
	}

	if(my_device_Open == 0)
	{
		info("device releasing ...\n");
		kfree(my_devices);
		my_devices = NULL;
		file->private_data = NULL;
	}
	gpio_free(minorNo);
	dbg3("OUT\n");
	return SUCCESS;
}

static struct file_operations my_fops = 
{
	.owner   	= THIS_MODULE,
	.read   	= device_file_read,
	.write		= device_file_write,
	.open		= device_open,
	.release	= device_release,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35))
	.ioctl		= device_ioctl
#else
	.unlocked_ioctl		= device_ioctl
#endif
};

static void my_cleanup_module(void)
{
	int i = 0;
	if(my_class) {
		info("1. device_destroy called\n");
		for(i = firstminor ; i <  firstminor + my_ndevices ; i++) {
			device_destroy(my_class, MKDEV(major, i));		//static 0 should be change with other dynamic
		}
	}

	if(my_cdev != NULL) {
		info("2. cdev_del called\n");
		cdev_del(my_cdev);
	}

	if(my_class) {
		info("3. class_destroy called\n");
		class_destroy(my_class);
	}

	unregister_chrdev_region(MKDEV(major, minor), my_ndevices);
	return;
}

static int my_init(void)
{
	int ret = 0;
	int i = 0;
	dev_t devnum;

	//print commandline argument
	info("\n=============\nFor debug level 1 ,2 ,3 : pass debug=3 as argument\n\
			For Major number select statically : pass major=250 as argument\n=============\n");

	dbg2("debug's debug level : %d selected.\n",debug);
	dbg2("major number argv : %d selected.\n",major);

	//check for valid number of devices select for driver
	if(my_ndevices <= 0)
	{
		err("[target] Invalid value of my_ndevices: %d\n", my_ndevices);
		ret = -EINVAL;
		return ret;
	}

	devnum = MKDEV(major,firstminor);
	if(MAJOR(devnum) == 0) {
		/*
		 * Allocate Major number dynamically
		 * Get a range of minor numbers (starting with 0) to work with.
		 */
		ret = alloc_chrdev_region(&devnum, firstminor, my_ndevices, DEVICE_NAME);
	}
	else {
		/*
		 * Allocate Major number statically
		 */
		ret = register_chrdev_region(devnum, my_ndevices, DEVICE_NAME);
	}
	if (ret < 0) {
		err("Major number allocation is failed\n");
		return ret; 
	}
	major = MAJOR(devnum);
	minor = MINOR(devnum);
	devnum = MKDEV(major,minor);
	info("The major number is %d and minor is %d\n",major, minor);

	/* Create device class called CLASS_DEVICE_NAME macro(before allocation of devices) 
	 * command : $ ls /sys/class
	 */
	my_class = class_create(THIS_MODULE, CLASS_DEVICE_NAME);
	if (IS_ERR(my_class)) {
		ret = PTR_ERR(my_class);
		err("class_create() fail with return val: %d\n",ret);
		goto clean_up;
	}

	/* Allocate the array of devices */
	my_devices = (struct my_dev *) kzalloc((sizeof(struct my_dev)), GFP_KERNEL);
	if(my_devices == NULL)
	{
		ret = -ENOMEM;
		err("kzalloc failed. No memory allocated to structure.\n");
		goto clean_up;
	}

	//=================================================================
	// we can create dynamic memory by kzalloc also for our device structure
	my_cdev = cdev_alloc( );
	if(my_cdev == NULL)	{
		err("cdev_alloc() fail. my_cdev not allocated.\n");
		goto clean_up;
	}

	my_cdev->ops = &my_fops;
	//	my_cdev->count = 0;
	my_cdev->owner = THIS_MODULE;

	/** Initiliaze character dev with fops **/
	cdev_init(my_cdev, my_cdev->ops);		//my_cdev should not be NULL

	/* add the character device to the system
	 *
	 * Here my_ndevices means my_ndevices minor number, you can give 2 for 2 minor device, 
	 * the last param is the count of minor number enrolled
	 *
	 * You should not call cdev_add until your driver is completely ready to handle operations
	 */
	ret = cdev_add(my_cdev, devnum, my_ndevices);
	if(ret < 0)
	{
		err("cdev_add() failed\nUnable to add cdev\n");
		// FIXED Don't know about kobject_put should use or not for decrement count.
		// kobject_put(&my_cdev->kobj); = Not require
		goto clean_up;
	}
	//=================================================================

#ifdef GROUP_ATTRS
	//dbg3("device  kzalloc()\n");
	//device = kzalloc(sizeof(struct device),GFP_KERNEL);
	//dbg3("assigning group()\n");
	//device->groups = dev_attr_groups;

	/*
	 * Create a simple kobject with the name of "kobject_example",
	 * located under /sys/kernel/
	 *
	 * As this is a simple directory, no uevent will be sent to
	 * userspace.  That is why this function should not be used for
	 * any type of dynamic kobjects, where the name and number are
	 * not known ahead of time.
	 */
	example_kobj = kobject_create_and_add("kobject_example", kernel_kobj);
	if (!example_kobj)
		return -ENOMEM;

	/* Create the files associated with this kobject */
	ret = sysfs_create_group(example_kobj, *dev_attr_groups);
	if (ret) {
		kobject_put(example_kobj);
	}

#endif

	/* add the driver to /dev/my_chardev -- here
	 * command : $ ls /dev/
	 */
	dbg3("device_create() called\n");
	//device = device_create(my_class, NULL, devnum, NULL, DEVICE_NAME);
	for(i = firstminor ; i <  firstminor + my_ndevices ; i++) {
		device[i - firstminor] = device_create(my_class, NULL, MKDEV(major,i), NULL, DEVICE_NAME "-%d", i);
		if (IS_ERR(device)) {
			ret = PTR_ERR(device);
			err("[target] Error %d while trying to create %s-%d", ret, DEVICE_NAME, firstminor);
			goto clean_up;
		}
		dbg3("device %s-%d created for %d.%d \n",DEVICE_NAME,i, major,i);
	}

#ifndef GROUP_ATTRS
	/* Now we can create the sysfs endpoints (don't care about errors).
	 * dev_attr_work come from the DEVICE_ATTR(...) earlier */
	for(i = firstminor ; i <  firstminor + my_ndevices ; i++) {
		ret = device_create_file(device[i - firstminor], &dev_attr_gpioAndroid);
		if (ret < 0) {
			warn("failed to create write /sys endpoint - continuing without\n");
		}
	}
#endif
	dbg3("OUT\n");
	return 0; 

clean_up:
	my_cleanup_module();
	return ret;
}

static void my_exit(void)
{
	info("IN\n");
	my_cleanup_module();
	return;
}

module_init(my_init);
module_exit(my_exit);

MODULE_AUTHOR("Ratnesh");
/* 
 * Get rid of taint message by declaring code as GPL. 
 */
MODULE_LICENSE("GPL");
