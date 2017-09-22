# Upstream Kernels
KERNELDIR ?= /home/suman/projects/opensrc/kernels/linux
#KERNELDIR ?= /home/suman/projects/opensrc/kernels/linux-next
#KERNELDIR ?= /home/suman/projects/opensrc/kernels/linux-stable

# LCPD Kernels
#KERNELDIR ?= /home/suman/projects/lcpd/kernels/ti-4.9
#KERNELDIR ?= /home/suman/projects/lcpd/kernels/lcpd-rpmsg/iommu-4.4

#obj-m = iommu_dt_test.o iommu_test.o
obj-m = iommu_dt_test.o
iommu_dt_test-objs = main_dt.o
#iommu_test-objs = main.o

all:
	make ${MAKE_OPTS} -C $(KERNELDIR) SUBDIRS=$(PWD) modules

clean:
	$(RM) -r *.o *.ko *.mod.c .*.cmd .tmp_versions *.symvers modules.order
