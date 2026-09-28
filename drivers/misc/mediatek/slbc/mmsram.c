// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/arm-smccc.h>
#include <linux/clk.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <mt-plat/aee.h>
#include <mtk_sip_svc.h>
//#include <mt-plat/mtk_secure_api.h>
#include "mmsram.h"


#define MAX_CLK_NUM	(8)
#define MMSRAM_ENABLE_UT 0

struct mmsram_dev {
	struct device *dev;
	void __iomem *ctrl_base;
	void __iomem *sram_paddr;
	void __iomem *sram_vaddr;
	ssize_t sram_size;
	struct clk *clk[MAX_CLK_NUM];
	const char *clk_name[MAX_CLK_NUM];
};

enum smc_mmsram_request {
	MMSRAM_RW_SETTING,
};

static struct work_struct dump_reg_work;

static struct mmsram_dev *mmsram;
#if !IS_ENABLED(CONFIG_FPGA_EARLY_PORTING)
static atomic_t clk_ref = ATOMIC_INIT(0);
#endif
static bool debug_enable;

/* MTCMOS clocks should be defined before CG clocks in DTS */
static int set_clk_enable(bool is_enable)
{
	int ret = 0;

#if !IS_ENABLED(CONFIG_FPGA_EARLY_PORTING)
	int i, j;

	if (debug_enable)
		pr_notice("%s:%d\n", __func__, is_enable);
	if (is_enable) {
		//ret = pm_runtime_get_sync(&(pm_dev->dev));
		for (i = 0; i < MAX_CLK_NUM; i++) {
			if (mmsram->clk[i])
				ret = clk_prepare_enable(mmsram->clk[i]);

			if (ret) {
				pr_notice("mmsram clk(%s) enable fail:%d\n",
					mmsram->clk_name[i], ret);
				for (j = i - 1; j >= 0; j--)
					clk_disable_unprepare(mmsram->clk[j]);
				return ret;
			}
		}

		atomic_inc(&clk_ref);
	} else {
		for (i = MAX_CLK_NUM - 1; i >= 0; i--)
			if (mmsram->clk[i])
				clk_disable_unprepare(mmsram->clk[i]);
		atomic_dec(&clk_ref);
	}
#endif
	return ret;
}



static void rw_reg_secure(bool is_write)
{
	struct arm_smccc_res res;

	arm_smccc_smc(MTK_SIP_MMSRAM_CONTROL, MMSRAM_RW_SETTING,
			is_write ? 1 : 0, 0, 0, 0, 0, 0, &res);
}


void mmsram_set_secure(bool secure_on)
{
	return;
}
EXPORT_SYMBOL_GPL(mmsram_set_secure);

static void init_mmsram_reg(void)
{
	rw_reg_secure(true);
}

int mmsram_power_on(void)
{
	int ret = 0;

	set_clk_enable(true);
	init_mmsram_reg();
	if (debug_enable)
		pr_notice("mmsram power on\n");
	return ret;
}
EXPORT_SYMBOL_GPL(mmsram_power_on);

void mmsram_power_off(void)
{
	set_clk_enable(false);
	if (debug_enable)
		pr_notice("mmsram power off\n");
}
EXPORT_SYMBOL_GPL(mmsram_power_off);

int enable_mmsram(void)
{
	pr_notice("enable mmsram\n");

	return 0;
}
EXPORT_SYMBOL_GPL(enable_mmsram);

void disable_mmsram(void)
{
	pr_notice("disable mmsram\n");
}
EXPORT_SYMBOL_GPL(disable_mmsram);

void mmsram_get_info(struct mmsram_data *data)
{
	data->paddr = mmsram->sram_paddr;
	data->vaddr = mmsram->sram_vaddr;
	data->size = mmsram->sram_size;
	pr_notice("%s: pa: 0x%p va: 0x%p size:%#lx\n",
		__func__, data->paddr, data->vaddr,
		data->size);
}
EXPORT_SYMBOL_GPL(mmsram_get_info);

#if IS_ENABLED(CONFIG_MTK_SLBC_MMSRAM)
int set_mmsram_data_if(struct mmsram_data *d)
{
	if (mmsram->sram_size) {
		mmsram_get_info(d);
		if (!(d->size)) {
			pr_notice("#@# %s(%d) mmsram is wrong !!!\n",
					__func__, __LINE__);
			return -EINVAL;
		}
		if (debug_enable)
			pr_notice("#@# %s(%d) 0x%p(%#x) 0x%p(%#x) %d\n",
				__func__, __LINE__, d->paddr, d->paddr,
				d->vaddr, d->vaddr, d->size);
		set_clk_enable(true);
		init_mmsram_reg();
		set_clk_enable(false);
	}
	return 0;
}
EXPORT_SYMBOL_GPL(set_mmsram_data_if);

void clr_mmsram_data_if(struct mmsram_data *d)
{
	d->paddr = 0;
	d->vaddr = 0;
}
EXPORT_SYMBOL_GPL(clr_mmsram_data_if);

int mmsram_power_on_if(struct mmsram_data *d)
{
	if (d == 0)
		return -EINVAL;
	return mmsram_power_on();
}
EXPORT_SYMBOL_GPL(mmsram_power_on_if);

int mmsram_power_off_if(struct mmsram_data *d)
{
	if (d == 0)
		return -EINVAL;
	mmsram_power_off();

	return 0;
}
EXPORT_SYMBOL_GPL(mmsram_power_off_if);
#endif


static void dump_reg_func(struct work_struct *work)
{
	if (set_clk_enable(true)) {
		pr_notice("%s: enable clk fail\n", __func__);
		return;
	}

	/* Print debug log */
	rw_reg_secure(false);

	set_clk_enable(false);
#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
	aee_kernel_warning("MMSRAM", "MMSRAM Violation.");
#endif
}

static irqreturn_t mmsram_irq_handler(int irq, void *data)
{
	pr_notice("handle mmsram irq!\n");
	dump_reg_func(&dump_reg_work);
	return IRQ_HANDLED;
}

static int mmsram_remove(struct platform_device *pdev)
{
	//pm_runtime_disable(&pdev->dev);
	return 0;
}
static int mmsram_probe(struct platform_device *pdev)
{
	struct resource *res;
	int irq, err, clk_num, i;

	mmsram = devm_kzalloc(&pdev->dev, sizeof(*mmsram), GFP_KERNEL);
	if (!mmsram)
		return -ENOMEM;

	mmsram->dev = &pdev->dev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_notice(&pdev->dev, "could not get resource for ctrl\n");
		return -EINVAL;
	}
	if (debug_enable)
		dev_notice(&pdev->dev, "probe ctrl pa=%#x size=%#lx\n",
			(void *)res->start, resource_size(res));

	mmsram->ctrl_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(mmsram->ctrl_base)) {
		dev_notice(&pdev->dev,
			"could not ioremap resource for ctrl\n");
		return PTR_ERR(mmsram->ctrl_base);
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res) {
		dev_notice(&pdev->dev, "could not get resource for memory\n");
		return -EINVAL;
	}
	mmsram->sram_paddr = (void *)res->start;
	mmsram->sram_size = resource_size(res);
	mmsram->sram_vaddr =  (void __iomem *) devm_memremap(&pdev->dev,
				res->start, mmsram->sram_size, MEMREMAP_WT);
	if (IS_ERR(mmsram->sram_vaddr)) {
		dev_notice(&pdev->dev,
			"could not ioremap resource for memory\n");
		return PTR_ERR(mmsram->sram_vaddr);
	}

	dev_notice(&pdev->dev, "probe va=%#x pa=%#x size=%#lx\n",
		mmsram->sram_vaddr, mmsram->sram_paddr,
		mmsram->sram_size);
	//pm_runtime_enable(&pdev->dev);

	if (!IS_ENABLED(CONFIG_FPGA_EARLY_PORTING)) {
		clk_num = of_property_read_string_array(pdev->dev.of_node,
			"clock-names", &mmsram->clk_name[0], MAX_CLK_NUM);
		for (i = 0; i < clk_num; i++) {
			mmsram->clk[i] = devm_clk_get(&pdev->dev,
						mmsram->clk_name[i]);
			if (IS_ERR(mmsram->clk[i])) {
				dev_notice(&pdev->dev,
					"could not get mmsram clk(%s) info\n",
					mmsram->clk_name[i]);
				return PTR_ERR(mmsram->clk[i]);
			}
		}
	}

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_notice(&pdev->dev, "failed to get irq (%d)\n", irq);
		return -EINVAL;
	}
	err = devm_request_irq(&pdev->dev, irq, mmsram_irq_handler, IRQF_SHARED,
			       "mtk_mmsram", mmsram);
	if (err) {
		dev_notice(&pdev->dev,
			"failed to register ISR %d (%d)", irq, err);
		return err;
	}

	INIT_WORK(&dump_reg_work, dump_reg_func);

	return 0;
}

#define RESULT_STR_LEN 8
int test_mmsram;
struct mmsram_data *data;
int set_test_mmsram(const char *val, const struct kernel_param *kp)
{
#if MMSRAM_ENABLE_UT
	int result;
	u32 test_case, offset, value;
	const char *test_str = "12345678";
	char result_str[RESULT_STR_LEN + 1] = {0};

	result = sscanf(val, "%d %i %i", &test_case, &offset, &value);
	if (result != 3) {
		pr_notice("invalid input: %s, result(%d)\n", val, result);
		return -EINVAL;
	}
	pr_notice("%s (test_case, offset, value): (%d,%#x,%#x)\n",
		__func__, test_case, offset, value);

	switch (test_case) {
	case 0: /* Initialize */
		data = kzalloc(sizeof(*data), GFP_KERNEL);
		set_mmsram_data_if(data);
		mmsram_power_on_if(data);
		break;
	case 1: /* Uninitialize */
		mmsram_power_off_if(data);
		//disable_mmsram();
		kfree(data);
		break;
	case 2: /* Write value to offset */
		writel(value, data->vaddr + offset);
		value = readl(data->vaddr + offset);
		pr_notice("write vaddr 0x%p = %#x success\n",
			data->vaddr + offset, value);
		break;
	case 3: /* Read value from offset */
		value = readl(data->vaddr + offset);
		pr_notice("read vaddr 0x%p = %#x success\n",
			data->vaddr + offset, value);
		break;
	case 4: /* Write test string to offset */
		memcpy_toio(data->vaddr + offset, test_str, RESULT_STR_LEN);
		pr_notice("write str:%s success\n", test_str);
		break;
	case 5: /* Write test string from offset */
		memcpy_fromio(result_str, data->vaddr, RESULT_STR_LEN);
		result_str[RESULT_STR_LEN] = '\0';
		pr_notice("read str:%s success\n", result_str);
		break;
	default:
		pr_notice("wrong input test_case:%d\n", test_case);
	}
#endif
	return 0;
}
static struct kernel_param_ops test_mmsram_ops = {
	.set = set_test_mmsram,
	.get = param_get_int,
};
module_param_cb(test_mmsram, &test_mmsram_ops, &test_mmsram, 0644);
MODULE_PARM_DESC(test_mmsram, "test mmsram");

static struct kernel_param_ops debug_enable_ops = {
	.set = param_set_bool,
	.get = param_get_bool,
};
module_param_cb(
	debug_enable, &debug_enable_ops, &debug_enable, 0644);
MODULE_PARM_DESC(debug_enable, "enable or disable mmsram debug log");

static const struct of_device_id of_mmsram_match_tbl[] = {
	{
		.compatible = "mediatek,mmsram",
	},
	{}
};

static struct platform_driver mmsram_drv = {
	.probe = mmsram_probe,
	.remove = mmsram_remove,
	.driver = {
		.name = "mtk-mmsram",
		.owner = THIS_MODULE,
		.of_match_table = of_mmsram_match_tbl,
	},
};
static int __init mtk_mmsram_init(void)
{
	s32 status;

	status = platform_driver_register(&mmsram_drv);
	if (status) {
		pr_notice("Failed to register mmsram driver(%d)\n", status);
		return -ENODEV;
	}
	return 0;
}
static void __exit mtk_mmsram_exit(void)
{
	platform_driver_unregister(&mmsram_drv);
}
module_init(mtk_mmsram_init);
module_exit(mtk_mmsram_exit);
MODULE_DESCRIPTION("MTK MMSRAM driver");
MODULE_AUTHOR("Anthony Huang<anthony.huang@mediatek.com>");
MODULE_LICENSE("GPL");
