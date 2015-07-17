#include <linux/init.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/device.h>

#define SUCCESS 0
#define DEVICE_NAME "my_chardev"
#define CLASS_DEVICE_NAME "test_chardev"
#define MY_NDEVICES 2
#define NUMBER_OF_MINOR_DEVICE 1

static dev_t devnum;
static int my_device_Open;
static int Major;		//save Major number in this variable
static int my_ndevices = MY_NDEVICES;		//number of device which can access your driver

static struct cdev *my_cdev = NULL;
static struct class *my_class = NULL;

static int myint = 3;
module_param(myint, int, 0);
MODULE_PARM_DESC(myint, "An Integer type for debug level (1,2,3)");

static ssize_t device_file_read(struct file *file, char __user *buffer, size_t length, loff_t *offset)
{
	printk(KERN_INFO "device_file_read IN");
	return 0;
}

static ssize_t device_file_write(struct file *file, const char __user *buffer, size_t length, loff_t *offset)
{
	printk(KERN_INFO "device_file_write IN");
	return 0;
}

static int device_open(struct inode *node, struct file *file)
{
	my_device_Open++;		//increment count when device is open.
	if(my_device_Open)
	{
		return -EBUSY;
	}
	printk(KERN_INFO "device_open IN");
	return SUCCESS;
}

static int device_release(struct inode *node, struct file *file)
{
	my_device_Open--;		//Decrement count when device is open.
	if(my_device_Open == 0)
	{
		//release device
	}
	printk(KERN_INFO "device_release IN");
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

static int my_init(void)
{
	int ret = 0;
	int firstminor = 0;		//The first minor number in case you are looking for a series of minor numbers for your driver. 
	struct device *device = NULL;

	//print commandline argument
	printk(KERN_INFO "myint's debug level : %d selected.\n=============\n",myint);

	//check for valid number of devices select for driver
	if(my_ndevices <= 0)
	{
		printk(KERN_WARNING "[target] Invalid value of my_ndevices: %d\n", my_ndevices);
		ret = -EINVAL;
		return ret;
	}

	/*
	 * Allocate Major number dynamically
	 * Get a range of minor numbers (starting with 0) to work with NUMBER_OF_MINOR_DEVICE
	 */
	ret = alloc_chrdev_region(&devnum, firstminor, NUMBER_OF_MINOR_DEVICE, DEVICE_NAME);
	if (ret < 0) {
		printk(KERN_INFO "Major number allocation is failed\n");
		return ret; 
	}
	Major = MAJOR(devnum);
	devnum = MKDEV(Major,0);
	printk(KERN_INFO "The major number is %d\n",Major);

	/* Create device class called CLASS_DEVICE_NAME macro(before allocation of devices) 
	 * command : $ ls /sys/class
	 */
	my_class = class_create(THIS_MODULE, CLASS_DEVICE_NAME);
	if (IS_ERR(my_class)) {
		ret = PTR_ERR(my_class);
		printk(KERN_WARNING "class_create() fail with return val: %d\n",ret);
		goto clean_up;
	}


	my_cdev = cdev_alloc( );
	if(my_cdev == NULL)	{
		printk(KERN_WARNING "cdev_alloc() fail. my_cdev not allocated.\n");
		goto clean_up;
	}

	my_cdev->ops = &my_fops;
	my_cdev->count = 0;
	my_cdev->owner = THIS_MODULE;

	cdev_init(my_cdev, my_cdev->ops);		//my_cdev should not be NULL BUG HERE
/*
	my_cdev->ops = &my_fops;
	my_cdev->count = 0;
	my_cdev->owner = THIS_MODULE;
*/
	ret = cdev_add(my_cdev, devnum, 0);
	if(ret < 0)
	{
		printk(KERN_WARNING "cdev_add() failed\nUnable to add cdev\n");
		goto clean_up;
	}

	/* add the driver to /dev/my_chardev -- here
	 * command : $ ls /dev/
	 */
	device = device_create(my_class, NULL, devnum, NULL, DEVICE_NAME);
	if (IS_ERR(device)) {
		ret = PTR_ERR(device);
		printk(KERN_WARNING "[target] Error %d while trying to create %s%d",
		ret, DEVICE_NAME, firstminor);
		goto clean_up;
	}


	return 0; 

clean_up:
	if(my_class) {
		device_destroy(my_class, MKDEV(Major, firstminor));
	}

	if(my_cdev != NULL) {
		cdev_del(my_cdev);
	}

/*	
	if(my_cdev != NULL) {
		kfree(my_cdev);
	}
*/	

	if(my_class) {
		class_destroy(my_class);
	}

	unregister_chrdev_region(devnum, 1);
	return ret;
}

static void my_exit(void)
{
	printk(KERN_INFO "my_exit() called\n");
	if(my_class) {
		device_destroy(my_class, MKDEV(Major, 0));
	}

	if(my_cdev != NULL) {
		cdev_del(my_cdev);
	}

	if(my_class) {
		class_destroy(my_class);
	}

	unregister_chrdev_region(devnum, 1);
	return;
}

module_init(my_init);
module_exit(my_exit);

MODULE_AUTHOR("Ratnesh");
/* 
 * Get rid of taint message by declaring code as GPL. 
 */
MODULE_LICENSE("GPL");
