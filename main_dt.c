/*
 * OMAP IOMMU DT-based Unit Test module
 *
 * Copyright (C) 2014-2015 Texas Instruments, Inc.
 *
 * Suman Anna <s-anna@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/notifier.h>
#include <linux/err.h>
#include <linux/sched.h>
#include <linux/device.h>
#include <linux/of_platform.h>

#include <linux/iommu.h>
#include <linux/omap-iommu.h>
#include <linux/platform_data/iommu-omap.h>

/* load-time options */
unsigned int pa = 0x95000000;
unsigned int da = 0xA0000000;
int len = 0x00100000;
int count = 1;

module_param(count, int, 0);
module_param(pa, uint, 0);
module_param(da, uint, 0);
module_param(len, int, 0);

static struct iommu_domain *domain;

/* define locally to build */
/*struct omap_iommu_arch_data {
        const char *name;
        struct omap_iommu *iommu_dev;
};*/

static int omap_iommu_test_fault(struct iommu_domain *domain, struct device *dev,
				 unsigned long iova, int flags, void *token)
{
	dev_err(dev, "iommu fault: da 0x%lx flags 0x%x\n", iova, flags);

	/*
	 * Let the iommu core know we're not really handling this fault;
	 * we are just printing a trace.
	 */
	return -ENOSYS;
}

static void omap_iommu_test_cleanup(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	pr_info("%s: iommu_test_cleanup entered\n", __func__);
	if (dev && domain) {
		iommu_detach_device(domain, dev);
		iommu_domain_free(domain);
		/*iommu_group_remove_device(dev);*/
		pr_info("%s: detached device and freed domain\n", __func__);
	}
}

static int omap_iommu_test_init(struct platform_device *pdev)
{
	int i, ret = 0;
	struct device *dev = &pdev->dev;
	const char *obj_name, *device_name;
	struct omap_iommu_arch_data *arch_data;
	/*struct iommu_group *group;*/

	pr_info("%s: iommu_test_init entered\n", __func__);

	if (!dev->of_node) {
		dev_err(dev, "test device is not a DT device\n");
		return -ENODEV;
	}

	/* Enable IOMMU:  */
	dev_info(dev, "Enabling IOMMU...\n");
	if (!iommu_present(dev->bus)) {
		dev_err(dev, "iommu not found\n");
		return ret;
	}

	/* Create a device group and add the device to it. */
	/*group = iommu_group_alloc();
	if (IS_ERR(group)) {
		dev_err(dev, "failed to allocate IOMMU group\n");
		return PTR_ERR(group);
	}

	ret = iommu_group_add_device(group, dev);
	iommu_group_put(group);
	if (ret < 0) {
		dev_err(dev, "failed to add device to IOMMU group\n");
		return ret;
	}*/

	domain = iommu_domain_alloc(dev->bus);
	if (!domain) {
		dev_err(dev, "can't alloc iommu domain\n");
		return -ENOMEM;
	}

	iommu_set_fault_handler(domain, omap_iommu_test_fault, NULL);

	arch_data = dev->archdata.iommu;
	if (!arch_data) {
		dev_err(dev, "test device node does not have iommus property set\n");
		return -EINVAL;
	}

	/*
	 * omap_iommu_attach_device() expects dev->archdata.iommu->name to be
	 * set.  This should be set automatically within the OMAP IOMMU driver
	 * for DT-based clients, so just sanity check here
	 */
	obj_name = dev->of_node->name;
	device_name = dev_name(dev);
	dev_info(dev, "dev->of_node->name: %s dev_name %s\n", obj_name, device_name);

	/*dev_info(dev, "testing IOMMU %s\n", arch_data->name);*/

	ret = iommu_attach_device(domain, dev);
	if (ret) {
		dev_err(dev, "can't attach iommu device: %d\n", ret);
		goto free_domain;
	}

	/* Setup 'count' mappings:  */
	for (i = 0; i < count; i++) {
		pa += len; da += len;

		/* test reprogram */
		/* if (i == 2) {
			da -= len;
			pa += 0x10000000;
		} */

		dev_info(dev, "Mapping da 0x%x, pa 0x%x, len 0x%x\n",
			 da, pa, len);
		ret = iommu_map(domain, da, pa, len, 0);
		if (ret)
			dev_err(dev, "failed to map pa to da: %d\n", ret);
	}
	return 0;

free_domain:
	iommu_domain_free(domain);
	return ret;
}

static int omap_iommu_test_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	int ret = 0;

	if (!np) {
		pr_err("invalid node pointer\n");
		return -EINVAL;
	}

	ret = omap_iommu_test_init(pdev);
	if (ret)
		pr_err("omap_iommu_test_init failed, ret = %d\n", ret);

	return 0;
}

static int omap_iommu_test_remove(struct platform_device *pdev)
{
	omap_iommu_test_cleanup(pdev);
	return 0;
}

static const struct of_device_id omap_iommu_test_of_match[] = {
	{ .compatible = "ti,omap-iommu-test",  .data = NULL },
	{ /* end */ },
};
MODULE_DEVICE_TABLE(of, omap_iommu_test_of_match);

static struct platform_driver omap_iommu_test_driver = {
	.probe	= omap_iommu_test_probe,
	.remove	= omap_iommu_test_remove,
	.driver	= {
		.name   = "omap_iommu_test",
		.of_match_table = of_match_ptr(omap_iommu_test_of_match),
	},
};

module_platform_driver(omap_iommu_test_driver);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Suman Anna");
