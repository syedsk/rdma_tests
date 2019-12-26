obj-m := rdma_exp.o

rdma_exp-objs := exp1.o

ccflags-y=-I/usr/src/mlnx-ofa_kernel-4.7/include/


KSRC := /lib/modules/$(shell uname -r)/build
all:
	make -C $(KSRC) M=$(PWD) modules

clean:
	make -C $(KSRC) M=$(PWD) clean

