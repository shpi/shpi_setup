DRIVER_NAME := shpi
KERNEL_SOURCE_DIR ?= /lib/modules/`uname -r`/build

EXTRA_CFLAGS +=

obj-m := $(DRIVER_NAME).o

#DRIVER_FILES := shpi.o

#$(DRIVER_NAME)-objs:= $(DRIVER_FILES)

modules:
	$(MAKE) -C $(KERNEL_SOURCE_DIR) KCPPFLAGS="$(EXTRA_CFLAGS)" M=$(PWD) modules

modules_install:
	$(MAKE) -C $(KERNEL_SOURCE_DIR) M=$(PWD) modules_install

install: modules_install

clean:
	$(MAKE) -C $(KERNEL_SOURCE_DIR) M=$(PWD) clean
