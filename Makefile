obj-m+=ht530.o

all:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) modules
	$(CC) test_ht530.c -o test -lpthread 
clean:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) clean
	rm test