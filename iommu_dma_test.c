/*
 * OMAP IOMMU DT-based DMA Unit Test module
 *
 * Copyright (C) 2018 Texas Instruments, Inc.
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

#include <linux/err.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/dma-mapping.h>
#include <linux/iommu.h>

#include <asm/dma-iommu.h>

/* load-time options */
int len = 0x00100000;
int count = 1;

module_param(count, int, 0);
module_param(len, int, 0);

struct test_device {
	struct device *dev;
	struct dma_iommu_mapping *mapping;
	dma_addr_t dma_addr[10];
	void *addr[10];
};

static void test_detach_iommu(struct test_device *tdev)
{
	arm_iommu_detach_device(tdev->dev);
	arm_iommu_release_mapping(tdev->mapping);
	tdev->mapping = NULL;
}

static int test_attach_iommu(struct test_device *tdev)
{
	struct dma_iommu_mapping *mapping;
	int ret;

	/*
	 * Create the ARM mapping, used by the ARM DMA mapping core to allocate
	 * VAs. This will allocate a corresponding IOMMU domain.
	 */
	mapping = arm_iommu_create_mapping(&platform_bus_type, SZ_1G, SZ_2G);
	if (IS_ERR(mapping)) {
		dev_err(tdev->dev, "failed to create ARM IOMMU mapping\n");
		ret = PTR_ERR(mapping);
		goto error;
	}

	tdev->mapping = mapping;

	/* Attach the ARM VA mapping to the device. */
	ret = arm_iommu_attach_device(tdev->dev, mapping);
	if (ret < 0) {
		dev_err(tdev->dev, "failed to attach device to VA mapping\n");
		goto error;
	}

	return 0;

error:
	test_detach_iommu(tdev);
	return ret;
}

static void omap_iommu_dma_test_cleanup(struct platform_device *pdev)
{
	int i;
	struct test_device *tdev = platform_get_drvdata(pdev);

	pr_info("%s: entered %pK\n", __func__, tdev);

	count = count > 10 ? 10 : count;
	for (i = 0; i < count; i++) {
		dma_free_coherent(tdev->dev, len, tdev->addr[i],
				  tdev->dma_addr[i]);
		dev_info(tdev->dev, "%d: Freed addr %pK of size 0x%x, dma_addr = %pad\n",
			 i, tdev->addr[i], len, &tdev->dma_addr[i]);
		tdev->addr[i] = NULL;
		tdev->dma_addr[i] = 0;
	}

	test_detach_iommu(tdev);

	pr_info("%s: detached device\n", __func__);
}

static int omap_iommu_dma_test_init(struct platform_device *pdev)
{
	int i, ret = 0;
	struct device *dev = &pdev->dev;
	struct test_device *tdev;
	struct omap_iommu_arch_data *arch_data;

	pr_info("%s: entered\n", __func__);

	/* Enable IOMMU:  */
	dev_info(dev, "Enabling IOMMU...\n");
	if (!iommu_present(dev->bus)) {
		dev_err(dev, "iommu not found\n");
		return ret;
	}

	arch_data = dev->archdata.iommu;
	if (!arch_data) {
		dev_err(dev, "test device node does not have iommus property set\n");
		return -EINVAL;
	}

	tdev = devm_kzalloc(&pdev->dev, sizeof(*tdev), GFP_KERNEL);
	if (!tdev) {
		dev_err(&pdev->dev, "could not allocate memory\n");
		return -ENOMEM;
	}
	tdev->dev = dev;

	/* IOMMU */
	ret = test_attach_iommu(tdev);
	if (ret < 0) {
		dev_err(&pdev->dev, "unable to attach to IOMMU\n");
		goto error_tdev;
	}
	platform_set_drvdata(pdev, tdev);

	/* Setup 'count' mappings:  */
	count = count > 10 ? 10 : count;
	for (i = 0; i < count; i++) {
		tdev->addr[i] = dma_alloc_coherent(dev, len, &tdev->dma_addr[i],
						   GFP_KERNEL | GFP_DMA);
		if (!tdev->addr[i]) {
			ret = -ENOMEM;
			dev_err(dev, "%d failed to allocated memory of len %d\n",
				i, len);
			break;
		}
		dev_info(dev, "%d: Allocated size 0x%x at addr %pK, dma_addr = %pad\n",
			 i, len, tdev->addr[i], &tdev->dma_addr[i]);
	}
	if (ret)
		goto clean_bufs;

	return 0;

clean_bufs:
	while (i--) {
		dma_free_coherent(dev, len, tdev->addr[i], tdev->dma_addr[i]);
		tdev->addr[i] = NULL;
		tdev->dma_addr[i] = 0;
	}
	test_detach_iommu(tdev);
error_tdev:
	return ret;
}

static int omap_iommu_dma_test_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	int ret = 0;

	if (!np) {
		dev_err(dev, "test device is not a DT device\n");
		return -EINVAL;
	}

	ret = omap_iommu_dma_test_init(pdev);
	if (ret)
		dev_err(dev, "omap_iommu_dma_test_init failed, ret = %d\n",
			ret);

	return ret;
}

static int omap_iommu_dma_test_remove(struct platform_device *pdev)
{
	omap_iommu_dma_test_cleanup(pdev);

	return 0;
}

static const struct of_device_id omap_iommu_dma_test_of_match[] = {
	{ .compatible = "ti,omap-iommu-test", },
	{ /* end */ },
};
MODULE_DEVICE_TABLE(of, omap_iommu_dma_test_of_match);

static struct platform_driver omap_iommu_dma_test_driver = {
	.probe	= omap_iommu_dma_test_probe,
	.remove	= omap_iommu_dma_test_remove,
	.driver	= {
		.name   = "omap_iommu_dma_test",
		.of_match_table = omap_iommu_dma_test_of_match,
	},
};

module_platform_driver(omap_iommu_dma_test_driver);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Suman Anna");
