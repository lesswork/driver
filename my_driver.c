#include <linux/init.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/spinlock_types.h>

#define SUCCESS 0
#define DEVICE_NAME "circular"
#define CLASS_DEVICE_NAME "circbuf"
#define MY_NDEVICES 1
#define NUMBER_OF_MINOR_DEVICE 1
#define BUF_SIZE 10

//Enable it for define more than one /sys/class nodes for communication
//#define GROUP_ATTRS

/* The structure to represent 'my_dev' devices.
 * data - data buffer;
 * buffer_size - size of the data buffer;
 * block_size - maximum number of bytes that can be read or written
 * in one call;
 * my_dev_mutex - a mutex to protect the fields of this structure;
 * cdev - character device structure.
 */
struct my_dev {
	int minor;
	unsigned char *data;
	char buf[BUF_SIZE];
	
	unsigned long buffer_size;

	unsigned long block_size;
	char* head;
	char* tail;

	struct mutex my_dev_mutex;
	spinlock_t my_dev_spinlock;
	struct cdev my_cdev;
	struct device *device;
};

static int my_device_Open;
static int my_ndevices = MY_NDEVICES;		//number of device which can access your driver

static struct device *device = NULL;
static struct cdev *my_cdev = NULL;
static struct class *my_class = NULL;

static int debug = 0;
static int major = 0;
module_param(debug, int, 0);
module_param(major, int, 0);
MODULE_PARM_DESC(debug, "An Integer type for debug level (1,2,3)");

/* We'll use our own macros for printk */
#define CLASS_NAME "Circular Driver"
#define dbg1(format, arg...) do { if (debug > 0) pr_info(CLASS_NAME ": %s: " format, __FUNCTION__, ## arg); } while (0)
#define dbg2(format, arg...) do { if (debug > 1) pr_info(CLASS_NAME ": %s: " format, __FUNCTION__, ## arg); } while (0)
#define dbg3(format, arg...) do { if (debug > 2) pr_info(CLASS_NAME ": %s: " format, __FUNCTION__, ## arg); } while (0)
#define err(format, arg...) pr_err(CLASS_NAME ": " format, ## arg)
#define info(format, arg...) pr_info(CLASS_NAME ": " format, ## arg)
#define warn(format, arg...) pr_warn(CLASS_NAME ": " format, ## arg)

static ssize_t work_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t count)
{
	dbg1("work_store() %s\n",buf);
	return PAGE_SIZE;
}

static ssize_t work_show(struct device* dev, struct device_attribute* attr, char* buf)
{
	dbg1("work_show() %u\n", major);
	return snprintf(buf, 10, "%u\n", major);
}

static DEVICE_ATTR(work, S_IRUGO | S_IWUGO, work_show, work_store);

///////////////////// Group Attribute /////////////////////////

#ifdef GROUP_ATTRS
static struct attribute *dev_attrs[] = {
	&dev_attr_work.attr,
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
	info("device_file_read IN\n");
	//read buffer till empty OR generate error on empty buffer read.

	for(i = 0 ; i< length ; i++)
	{
		if(size != 0 && (read_ptr != write_ptr)) {
			put_user(*tail, buffer++);
			printk("Reading %c \n", *tail);

			if(tail >= buf + BUF_SIZE)
			{
				tail = buf;
			}
			else
			{
				tail++;
			}

			size--;
		}
		else
		{
			return i;
		}
	}

	return length;
}

static ssize_t device_file_write(struct file *file, const char __user *buffer, size_t length, loff_t *offset)
{
	info("device_file_write IN\n");
	//write in buffer till its kernel buffer limit and give buffer overflow error on full buffer write
	int i;

	for(i = 0; i < length; i++) {
		if (head == tail && size == BUF_SIZE)
		{
			err("Kernel buffer Full\n");
			return i;
		}

		if( get_user(*head, buffer++) == -EFAULT)
		{
			err("write to kernel buffer fail\n");
			return -EFAULT;
		}
		printk("Writing %c\n", *head);
		size++;
		if(head >= buf + BUF_SIZE)
		{
			head = buf;
		}
		else
		{
			head++;
		}
	}

	return length;
}

static int device_open(struct inode *node, struct file *file)
{
	struct my_dev *my_devices = NULL;
	info("device_open IN\n");
	if(my_device_Open)
	{
		return -EBUSY;
	}
	my_device_Open++;		//increment count when device is open.

	/* Allocate the array of devices in custom structure*/
	my_devices = (struct my_dev *) kzalloc((sizeof(struct my_dev) * my_ndevices), GFP_KERNEL);
	if(my_devices == NULL)
	{
		err("kzalloc failed. No memory allocated to structure.\n");
		return -ENOMEM;
	}
	my_devices = container_of(node->i_cdev, struct my_dev, my_cdev);
	info("device_open node->i_cdev: %x\n", node->i_cdev);
	info("device_open my_devices->my_cdev: %x\n", &my_devices->my_cdev);

	//cannot open multiple application as it is using static variable here.
	my_devices->data = NULL;
	my_devices->buffer_size = 0;
	//unsigned long block_size;
	//struct mutex my_dev_mutex;
	spin_lock_init(&my_devices->my_dev_spinlock);
	//my_devices->my_cdev = my_cdev;
	my_devices->device = device;

	info("device_open MAJOR(node->i_rdev): %d i_fop: %x\n", MAJOR(node->i_rdev), node->i_fop);

	file->private_data = (struct my_dev *) my_devices;
	return SUCCESS;
}

static int device_release(struct inode *node, struct file *file)
{
	info("device_release IN\n");
	my_device_Open--;		//Decrement count when device is open.
	if(my_device_Open == 0)
	{
		//release device
	}
	kfree(file->private_data);
	return SUCCESS;
}

static struct file_operations my_fops = 
{
	.owner   	= THIS_MODULE,
	.read   	= device_file_read,
	.write		= device_file_write,
	.open		= device_open,
	.release	= device_release
};

static void my_cleanup_module(void)
{
	if(my_class) {
		info("1. device_destroy.\n");
#ifdef GROUP_ATTRS
		kobject_put(example_kobj);
#else
		device_remove_file(device, &dev_attr_work);
#endif
		device_destroy(my_class, MKDEV(major, 0));		//static 0 should be change with other dynamic
	}

	if(my_cdev != NULL) {
		info("2. cdev_del.\n");
		cdev_del(my_cdev);
	}

	if(my_class) {
		info("3. class_destroy.\n");
		class_destroy(my_class);
	}

	unregister_chrdev_region(MKDEV(major, 0), my_ndevices);
	return;
}

static int my_init(void)
{
	int ret = 0;
	int firstminor = 0;		//The first minor number in case you are looking for a series of minor numbers for your driver. 
	dev_t devnum;

	//print commandline argument
	info("\n=============\nFor debug level 1 ,2 ,3 : pass debug=3 as argument\n\
			For Major number select statically : pass major=250 as argument\n=============\n");

	info("debug's debug level : %d selected.\n",debug);
	info("major number argv : %d selected.\n",major);

	//check for valid number of devices select for driver
	if(my_ndevices <= 0)
	{
		err("[target] Invalid value of my_ndevices: %d\n", my_ndevices);
		ret = -EINVAL;
		return ret;
	}

	devnum = MKDEV(major,0);
	if(devnum == 0) {
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
	devnum = MKDEV(major,0);
	info("The major number is %d\n",major);

	/* Create device class called CLASS_DEVICE_NAME macro(before allocation of devices) 
	 * command : $ ls /sys/class
	 */
	my_class = class_create(THIS_MODULE, CLASS_DEVICE_NAME);
	if (IS_ERR(my_class)) {
		ret = PTR_ERR(my_class);
		err("class_create() fail with return val: %d\n",ret);
		goto clean_up;
	}

	//=================================================================
	// we can create dynamic memory by kzalloc also for our device structure
	my_cdev = cdev_alloc( );
	if(my_cdev == NULL)	{
		err("cdev_alloc() fail. my_cdev not allocated.\n");
		goto clean_up;
	}

	info("device_init my_cdev->ops: %x\n", &my_cdev->ops);
	my_cdev->ops = &my_fops;
	my_cdev->count = 0;
	my_cdev->owner = THIS_MODULE;

	/** Initiliaze character dev with fops **/
	cdev_init(my_cdev, my_cdev->ops);		//my_cdev should not be NULL

	/* add the character device to the system
	 *
	 * Here 1 means only 1 minor number, you can give 2 for 2 minor device, 
	 * the last param is the count of minor number enrolled
	 *
	 * You should not call cdev_add until your driver is completely ready to handle operations
	 */
	ret = cdev_add(my_cdev, devnum, 1);
	if(ret < 0)
	{
		err("cdev_add() failed\nUnable to add cdev\n");
		goto clean_up;
	}
	//=================================================================

#ifdef GROUP_ATTRS
	dbg3("device  kzalloc()\n");
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
	if (ret)
		kobject_put(example_kobj);

#endif

	/* add the driver to /dev/my_chardev -- here
	 * command : $ ls /dev/
	 */
	dbg3("device_create()\n");
	//device = device_create(my_class, NULL, devnum, NULL, DEVICE_NAME);
	device = device_create(my_class, NULL, devnum, NULL, DEVICE_NAME "-%d", firstminor);
	if (IS_ERR(device)) {
		ret = PTR_ERR(device);
		err("[target] Error %d while trying to create %s%d", ret, DEVICE_NAME, firstminor);
		goto clean_up;
	}

#ifndef GROUP_ATTRS
	/* Now we can create the sysfs endpoints (don't care about errors).
	 * dev_attr_work come from the DEVICE_ATTR(...) earlier */
	dbg3("device_create_file()\n");
	ret = device_create_file(device, &dev_attr_work);
	if (ret < 0) {
		warn("failed to create write /sys endpoint - continuing without\n");
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
	info("my_exit() called\n");
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
