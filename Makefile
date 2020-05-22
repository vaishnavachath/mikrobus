mikrobus-y :=	mikrobus_core.o	mikrobus_manifest.o	
obj-m += mikrobus.o
obj-m += mikrobus_port.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
install:
	cp *.ko /lib/modules/$(shell uname -r)/kernel/drivers/misc/
	depmod -a
