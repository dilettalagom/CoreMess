obj-m += module_test.o

all:
	KCPPFLAGS="-Wall -Werror" make -C  /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
