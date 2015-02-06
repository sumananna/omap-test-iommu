KERNELDIR=/db/atree/gp/lcpd-rpmsg

obj-m = iommu_dt_test.o iommu_test.o
iommu_dt_test-objs = main_dt.o
iommu_test-objs = main.o

all:
	make ${MAKE_OPTS} -C $(KERNELDIR) SUBDIRS=$(PWD) modules

clean:
	$(RM) -r *.o *.ko *.mod.c .*.cmd .tmp_versions *.symvers modules.order
