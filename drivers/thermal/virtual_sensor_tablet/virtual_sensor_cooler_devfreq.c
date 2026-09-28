/*
 * Copyright 2022 Amazon.com, Inc. or its affiliates. All Rights Reserved.
*/

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/kobject.h>
#include <linux/slab.h>
#include <linux/devfreq.h>
#include <linux/pm_qos.h>
#include "virtual_sensor_thermal.h"

#define HZ_PER_KHZ		1000
#define DRIVER_NAME "vs_devfreq_cooler"
#define virtual_sensor_cooler_devfreq_dprintk(fmt, args...) pr_debug("thermal/cooler/devfreq " fmt, ##args)

struct vs_cooler_devfreq_data {
	struct thermal_cooling_device *cdev;
	struct devfreq *devfreq;
	struct dev_pm_qos_request req_max_freq;
	struct vs_cooler_platform_data vsc_pdata;
};

static int virtual_sensor_get_max_state(struct thermal_cooling_device *cdev,
					unsigned long *state)
{
	struct vs_cooler_devfreq_data *pdata;

	if (!cdev || !(cdev->devdata)) {
		pr_err("%s cdev: %p or cdev->devdata is NULL!\n", __func__, cdev);
		return -EINVAL;
	}
	pdata = cdev->devdata;
	*state = pdata->vsc_pdata.max_state;
	return 0;
}

static int virtual_sensor_get_cur_state(struct thermal_cooling_device *cdev,
					unsigned long *state)
{
	struct vs_cooler_devfreq_data *pdata;

	if (!cdev || !(cdev->devdata)) {
		pr_err("%s cdev: %p or cdev->devdata is NULL!\n", __func__, cdev);
		return -EINVAL;
	}
	pdata = cdev->devdata;
	*state = pdata->vsc_pdata.state;
	return 0;
}

static int virtual_sensor_set_cur_state(struct thermal_cooling_device *cdev,
					unsigned long state)
{
	int level;
	struct vs_cooler_devfreq_data *pdata;

	if (!cdev || !(cdev->devdata)) {
		pr_err("%s cdev: %p or cdev->devdata is NULL!\n", __func__, cdev);
		return -EINVAL;
	}
	pdata = cdev->devdata;
	if (pdata->vsc_pdata.state == state)
		return 0;

	pdata->vsc_pdata.state = (state > pdata->vsc_pdata.max_state) ?
				  pdata->vsc_pdata.max_state : state;

	if (!pdata->vsc_pdata.state)
		level = PM_QOS_MAX_FREQUENCY_DEFAULT_VALUE;
	else
		level = pdata->vsc_pdata.levels[pdata->vsc_pdata.state - 1];

	if (level == pdata->vsc_pdata.level)
		goto out;
	pdata->vsc_pdata.level = level;
	dev_pm_qos_update_request(&pdata->req_max_freq,(level == 0) ?
				  PM_QOS_MAX_FREQUENCY_DEFAULT_VALUE : DIV_ROUND_UP(level, HZ_PER_KHZ));
out:
	return 0;
}

static ssize_t levels_show(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
	struct thermal_cooling_device *cdev =
	    container_of(dev, struct thermal_cooling_device, device);
	struct vs_cooler_devfreq_data *pdata;
	int i;
	int offset = 0;

	if (!cdev || !(cdev->devdata)) {
		pr_err("%s cdev: %p or cdev->devdata is NULL!\n", __func__, cdev);
		return -EINVAL;
	}
	pdata = cdev->devdata;
	for (i = 0; i < pdata->vsc_pdata.max_state; i++)
		offset += scnprintf(buf + offset, PAGE_SIZE - offset, "%d %d\n",
					i + 1, pdata->vsc_pdata.levels[i]);
	return offset;
}

static ssize_t levels_store(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t count)
{
	int level, state;
	struct thermal_cooling_device *cdev =
	    container_of(dev, struct thermal_cooling_device, device);
	struct vs_cooler_devfreq_data *pdata;

	if (!cdev || !(cdev->devdata)) {
		pr_err("%s cdev: %p or cdev->devdata is NULL!\n", __func__, cdev);
		return -EINVAL;
	}
	pdata = cdev->devdata;
	if (sscanf(buf, "%d %d\n", &state, &level) != 2)
		return -EINVAL;
	if (state >= pdata->vsc_pdata.max_state)
		return -EINVAL;
	pdata->vsc_pdata.levels[state] = level;
	return count;
}

static struct thermal_cooling_device_ops cooling_ops = {
	.get_max_state = virtual_sensor_get_max_state,
	.get_cur_state = virtual_sensor_get_cur_state,
	.set_cur_state = virtual_sensor_set_cur_state,
};

static DEVICE_ATTR(levels, 0644, levels_show, levels_store);

static int devfreq_probe(struct platform_device *pdev)
{
	struct vs_cooler_devfreq_data *pdata = NULL;
	int ret = 0;

	virtual_sensor_cooler_devfreq_dprintk("probe\n");
	pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

#ifdef CONFIG_OF
	pr_notice("cooler custom init by DTS!\n");
	cooler_init_cust_data_from_dt(pdev, &pdata->vsc_pdata);
	pdata->devfreq = devfreq_get_devfreq_by_phandle(&pdev->dev, "devfreq", 0);
	if (IS_ERR(pdata->devfreq)) {
		pr_err("Get devfreq by DTS error!\n");
		return -EPROBE_DEFER;
	}
#endif

	pdata->cdev = thermal_cooling_device_register(pdata->vsc_pdata.type,
						      pdata,
						      &cooling_ops);
	if (!pdata->cdev) {
		pr_err("%s Failed to create devfreq cooling device\n",
			__func__);
		ret = -EINVAL;
		goto register_err;
	}
	ret = dev_pm_qos_add_request(pdata->devfreq->dev.parent,
				     &pdata->req_max_freq,
				     DEV_PM_QOS_MAX_FREQUENCY,
				     PM_QOS_MAX_FREQUENCY_DEFAULT_VALUE);
	if (ret < 0)
		goto qos_add_err;

	ret = device_create_file(&pdata->cdev->device, &dev_attr_levels);
	if (ret)
		pr_err("%s Failed to create devfreq cooler levels attr\n", __func__);

	platform_set_drvdata(pdev, pdata);
	kobject_uevent(&pdata->cdev->device.kobj, KOBJ_ADD);
	return 0;

qos_add_err:
	thermal_cooling_device_unregister(pdata->cdev);
register_err:
	devm_kfree(&pdev->dev, pdata);

	return ret;
}

static int devfreq_remove(struct platform_device *pdev)
{
	struct vs_cooler_devfreq_data *pdata = platform_get_drvdata(pdev);

	virtual_sensor_cooler_devfreq_dprintk("remove\n");
	if (pdata) {
		if (pdata->cdev) {
			device_remove_file(&pdata->cdev->device, &dev_attr_levels);
			thermal_cooling_device_unregister(pdata->cdev);
			dev_pm_qos_remove_request(&pdata->req_max_freq);
		}
		devm_kfree(&pdev->dev, pdata);
	}

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id devfreq_of_match_table[] = {
	{.compatible = "amazon,devfreq_cooler", },
	{},
};
MODULE_DEVICE_TABLE(of, devfreq_of_match_table);
#endif

static struct platform_driver devfreq_driver = {
	.probe = devfreq_probe,
	.remove = devfreq_remove,
	.driver     = {
		.name  = DRIVER_NAME,
#ifdef CONFIG_OF
		.of_match_table = devfreq_of_match_table,
#endif
		.owner = THIS_MODULE,
	},
};

static int __init virtual_sensor_cooler_devfreq_init(void)
{
	int err = 0;

	virtual_sensor_cooler_devfreq_dprintk("init\n");
	err = platform_driver_register(&devfreq_driver);
	if (err) {
		pr_err("%s: Failed to register driver %s\n", __func__,
			devfreq_driver.driver.name);
		return err;
	}

	return 0;
}

static void __exit virtual_sensor_cooler_devfreq_exit(void)
{
	virtual_sensor_cooler_devfreq_dprintk("exit\n");
	platform_driver_unregister(&devfreq_driver);
}

module_init(virtual_sensor_cooler_devfreq_init);
module_exit(virtual_sensor_cooler_devfreq_exit);

MODULE_LICENSE("GPL");