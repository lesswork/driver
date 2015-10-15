obj-m:= my_driver.o
KDIR := /lib/modules/$(shell uname -r)/build
#KDIR := /home/ratnesh/Training/rowboat-kernel
PWD := $(shell pwd)
default:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules
clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
