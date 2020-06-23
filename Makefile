mikrobus-y :=	mikrobus_core.o	mikrobus_manifest.o	
obj-m += mikrobus.o

KDIR ?= /lib/modules/`uname -r`/build

all:
	make -C $(KDIR) M=$(PWD) modules
clean:
	make -C $(KDIR) M=$(PWD) clean
install:
	cp *.ko /lib/modules/$(shell uname -r)/kernel/drivers/misc/
	depmod -a
