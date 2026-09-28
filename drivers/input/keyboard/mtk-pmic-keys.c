// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2022 MediaTek, Inc.
 *
 * Author: Chen Zhong <chen.zhong@mediatek.com>
 */

#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/reboot.h>
#include <linux/pm_wakeup.h>
#include <linux/regmap.h>
#include <linux/workqueue.h>
#include <linux/mfd/mt6323/registers.h>
#include <linux/mfd/mt6359p/registers.h>
#include <linux/mfd/mt6363/registers.h>
#include <linux/mfd/mt6397/registers.h>
#include <linux/mfd/mt6363/core.h>
#include <linux/mfd/mt6397/core.h>

#define MTK_PMIC_PWRKEY_INDEX			0
#define MTK_PMIC_HOMEKEY_INDEX			1
#define MTK_PMIC_MAX_KEY_COUNT			2
#define MT6397_PWRKEY_RST_SHIFT			6
#define MT6397_HOMEKEY_RST_SHIFT		5
#define MT6397_RST_DU_SHIFT			8
#define MT6359_PWRKEY_RST_SHIFT			9
#define MT6359_HOMEKEY_RST_SHIFT		8
#define MT6359_RST_DU_SHIFT			12
#define MT6363_PWRKEY_RST_SHIFT			2
#define MT6363_HOMEKEY_RST_SHIFT		4
#define MT6363_RST_DU_SHIFT			6
#define PWRKEY_RST_EN				1
#define HOMEKEY_RST_EN				1
#define RST_DU_MASK				3
#define RST_MODE_MASK				3
#define RST_PWRKEY_MODE				0
#define RST_PWRKEY_HOME_MODE			1
#define RST_PWRKEY_HOME2_MODE			2
#define RST_PWRKEY_HOME_HOME2_MODE		3
#define INVALID_VALUE				0

#if IS_ENABLED(CONFIG_AMAZON_MINERVA_METRICS_LOG)
#include <linux/metricslog.h>
#define PWRKEY_METRICS_STR_LEN 512
static struct work_struct metrics_work;
static void pwrkey_log_to_metrics(struct work_struct *data);
#endif

static bool pwrkey_press;

struct mtk_pmic_keys_regs {
	u32 deb_reg;
	u32 deb_mask;
	u32 intsel_reg;
	u32 intsel_mask;
};

#define MTK_PMIC_KEYS_REGS(_deb_reg, _deb_mask,		\
	_intsel_reg, _intsel_mask)			\
{							\
	.deb_reg		= _deb_reg,		\
	.deb_mask		= _deb_mask,		\
	.intsel_reg		= _intsel_reg,		\
	.intsel_mask		= _intsel_mask,		\
}

#define RELEASE_IRQ_INTERVAL	2
struct mtk_pmic_regs {
	const struct mtk_pmic_keys_regs keys_regs[MTK_PMIC_MAX_KEY_COUNT];
	bool release_irq;
	u32 pmic_rst_reg;
	u32 pmic_rst_para_reg;
	u32 pwrkey_rst_shift;
	u32 homekey_rst_shift;
	u32 rst_du_shift;
};

static const struct mtk_pmic_regs mt6397_regs = {
	.keys_regs[MTK_PMIC_PWRKEY_INDEX] =
		MTK_PMIC_KEYS_REGS(MT6397_CHRSTATUS,
		0x8, MT6397_INT_RSV, 0x10),
	.keys_regs[MTK_PMIC_HOMEKEY_INDEX] =
		MTK_PMIC_KEYS_REGS(MT6397_OCSTATUS2,
		0x10, MT6397_INT_RSV, 0x8),
	.release_irq = false,
	.pmic_rst_reg = MT6397_TOP_RST_MISC,
	.pwrkey_rst_shift = MT6397_PWRKEY_RST_SHIFT,
	.homekey_rst_shift = MT6397_HOMEKEY_RST_SHIFT,
	.rst_du_shift = MT6397_RST_DU_SHIFT,
};

static const struct mtk_pmic_regs mt6323_regs = {
	.keys_regs[MTK_PMIC_PWRKEY_INDEX] =
		MTK_PMIC_KEYS_REGS(MT6323_CHRSTATUS,
		0x2, MT6323_INT_MISC_CON, 0x10),
	.keys_regs[MTK_PMIC_HOMEKEY_INDEX] =
		MTK_PMIC_KEYS_REGS(MT6323_CHRSTATUS,
		0x4, MT6323_INT_MISC_CON, 0x8),
	.release_irq = false,
	.pmic_rst_reg = MT6323_TOP_RST_MISC,
	.pwrkey_rst_shift = MT6397_PWRKEY_RST_SHIFT,
	.homekey_rst_shift = MT6397_HOMEKEY_RST_SHIFT,
	.rst_du_shift = MT6397_RST_DU_SHIFT,
};

static const struct mtk_pmic_regs mt6359p_regs = {
	.keys_regs[MTK_PMIC_PWRKEY_INDEX] =
		MTK_PMIC_KEYS_REGS(0x2a,
		0x2, MT6359P_PSC_TOP_INT_CON0, 0x1),
	.keys_regs[MTK_PMIC_HOMEKEY_INDEX] =
		MTK_PMIC_KEYS_REGS(INVALID_VALUE,
		INVALID_VALUE, MT6359P_PSC_TOP_INT_CON0, 0x2),
	.release_irq = true,
	.pmic_rst_reg = MT6359P_TOP_RST_MISC,
	.pwrkey_rst_shift = MT6359_PWRKEY_RST_SHIFT,
	.homekey_rst_shift = MT6359_HOMEKEY_RST_SHIFT,
	.rst_du_shift = MT6359_RST_DU_SHIFT,
};

static const struct mtk_pmic_regs mt6363_regs = {
	.keys_regs[MTK_PMIC_PWRKEY_INDEX] =
		MTK_PMIC_KEYS_REGS(MT6363_TOPSTATUS,
		0x1, MT6363_PSC_TOP_INT_CON0, 0x0),
	.keys_regs[MTK_PMIC_HOMEKEY_INDEX] =
		MTK_PMIC_KEYS_REGS(MT6363_TOPSTATUS,
		0x3, MT6363_PSC_TOP_INT_CON0, 0x1),
	.release_irq = true,
	.pmic_rst_reg = MT6363_STRUP_CON11,
	.pmic_rst_para_reg = MT6363_STRUP_CON12,
	.pwrkey_rst_shift = MT6363_PWRKEY_RST_SHIFT,
	.homekey_rst_shift = MT6363_HOMEKEY_RST_SHIFT,
	.rst_du_shift = MT6363_RST_DU_SHIFT,
};

struct mtk_pmic_keys_info {
	struct mtk_pmic_keys *keys;
	const struct mtk_pmic_keys_regs *regs;
	unsigned int keycode;
	int irq;
	int release_irq_num;
	struct wakeup_source *suspend_lock;
};

struct mtk_pmic_keys {
	struct input_dev *input_dev;
	struct device *dev;
	struct regmap *regmap;
	struct mtk_pmic_keys_info keys[MTK_PMIC_MAX_KEY_COUNT];
	struct workqueue_struct *wq;
	struct delayed_work kpoc_reboot_work;
	ktime_t press_time;
};

enum mtk_pmic_keys_lp_mode {
	LP_DISABLE,
	LP_ONEKEY,
	LP_TWOKEY,
};

struct tag_bootmode {
	u32 size;
	u32 tag;
	u32 bootmode;
	u32 boottype;
};

#define KPOC_LONG_PRESS_TIME 1000 /* ms */

#ifdef CONFIG_KPD_VOLUME_KEY_SWAP
static u32 keycode_swap;
extern u32 kpd_get_linux_key_code(u32 keycode, bool pressed);
#endif

static void do_kpoc_reboot(struct work_struct *work)
{
	struct delayed_work *dwork = container_of(work, struct delayed_work, work);
	struct mtk_pmic_keys *pmic_keys = container_of(dwork, struct mtk_pmic_keys, kpoc_reboot_work);

	dev_notice(pmic_keys->dev, "[%s] Press power key for %d ms during KPOC, reboot device\n",
		   __func__, KPOC_LONG_PRESS_TIME);
	kernel_restart(NULL);
}

static int is_kpoc_mode(struct device *dev)
{
	static int kpoc = 0xff;
	struct device_node *boot_node = NULL;
	struct tag_bootmode *tag = NULL;

	if (kpoc != 0xff)
		goto out;

	boot_node = of_parse_phandle(dev->of_node, "bootmode", 0);
	if (!boot_node)
		dev_notice(dev, "%s: failed to get boot mode phandle\n", __func__);
	else {
		tag = (struct tag_bootmode *)of_get_property(boot_node, "atag,boot", NULL);
		if (!tag) {
			dev_notice(dev, "%s: failed to get atag,boot\n", __func__);
			goto out;
		}

		if (tag->bootmode == 8 || tag->bootmode == 9)
			kpoc = 1;
		else
			kpoc = 0;
		dev_dbg(dev, "%s: kpoc %d\n", __func__, kpoc);
	}

out:
	if (kpoc == 0xff)
		kpoc = 0;

	return kpoc;
}
static void mtk_pmic_keys_lp_reset_setup(struct mtk_pmic_keys *keys,
		const struct mtk_pmic_regs *pmic_regs)
{
	int ret;
	u32 long_press_mode, long_press_debounce;
	u32 pmic_rst_reg = pmic_regs->pmic_rst_reg;
	u32 pmic_rst_para_reg = pmic_regs->pmic_rst_para_reg;
	u32 pwrkey_rst_shift =
		PWRKEY_RST_EN << pmic_regs->pwrkey_rst_shift;
	u32 homekey_rst_shift =
		RST_MODE_MASK << pmic_regs->homekey_rst_shift;

	if (INVALID_VALUE == pmic_rst_para_reg) {
		pmic_rst_para_reg = pmic_rst_reg;
		homekey_rst_shift = HOMEKEY_RST_EN << pmic_regs->homekey_rst_shift;;
	}

	ret = of_property_read_u32(keys->dev->of_node,
		"power-off-time-sec", &long_press_debounce);
	if (ret)
		long_press_debounce = 0;

	regmap_update_bits(keys->regmap, pmic_rst_para_reg,
			   RST_DU_MASK << pmic_regs->rst_du_shift,
			   long_press_debounce << pmic_regs->rst_du_shift);

	ret = of_property_read_u32(keys->dev->of_node,
		"mediatek,long-press-mode", &long_press_mode);
	if (ret)
		long_press_mode = LP_DISABLE;

	switch (long_press_mode) {
	case LP_ONEKEY:
		regmap_update_bits(keys->regmap, pmic_rst_reg,
				   pwrkey_rst_shift,
				   pwrkey_rst_shift);
		regmap_update_bits(keys->regmap, pmic_rst_para_reg,
				   homekey_rst_shift,
				   RST_PWRKEY_MODE);
		break;
	case LP_TWOKEY:
		regmap_update_bits(keys->regmap, pmic_rst_reg,
				   pwrkey_rst_shift,
				   pwrkey_rst_shift);
		regmap_update_bits(keys->regmap, pmic_rst_para_reg,
				   homekey_rst_shift,
				   homekey_rst_shift);
		break;
	case LP_DISABLE:
		regmap_update_bits(keys->regmap, pmic_rst_reg,
				   pwrkey_rst_shift,
				   0);
		regmap_update_bits(keys->regmap, pmic_rst_para_reg,
				   homekey_rst_shift,
				   RST_PWRKEY_MODE);
		break;
	default:
		break;
	}
}

bool check_pwrkey_status(void)
{
	return pwrkey_press;
}
EXPORT_SYMBOL(check_pwrkey_status);

static irqreturn_t mtk_pmic_keys_release_irq_handler_thread(
				int irq, void *data)
{
	struct mtk_pmic_keys_info *info = data;
	long long interval;

#ifdef CONFIG_KPD_VOLUME_KEY_SWAP
	if (info->keycode == KEY_VOLUMEDOWN || info->keycode == KEY_VOLUMEUP)
		input_report_key(info->keys->input_dev, keycode_swap, 0);
	else
		input_report_key(info->keys->input_dev, info->keycode, 0);
#else
	input_report_key(info->keys->input_dev, info->keycode, 0);
#endif
	input_sync(info->keys->input_dev);
	if (info->suspend_lock)
		__pm_relax(info->suspend_lock);
	dev_dbg(info->keys->dev, "release key =%d using PMIC\n",
			info->keycode);

	pwrkey_press = false;

	if (is_kpoc_mode(info->keys->dev) && info->keycode == KEY_POWER) {
		interval = ktime_ms_delta(ktime_get_boottime(), info->keys->press_time);
		if (interval < KPOC_LONG_PRESS_TIME)
			cancel_delayed_work(&info->keys->kpoc_reboot_work);
		dev_info(info->keys->dev, "pwrkey duration=%lld, lp time %d\n",
			 interval, KPOC_LONG_PRESS_TIME);
	}
	dev_dbg(info->keys->dev, "release key =%d using PMIC\n", info->keycode);

#if IS_ENABLED(CONFIG_AMAZON_MINERVA_METRICS_LOG)
	pwrkey_press = false;
	schedule_work(&metrics_work);
#endif

	return IRQ_HANDLED;
}

static irqreturn_t mtk_pmic_keys_irq_handler_thread(int irq, void *data)
{
	struct mtk_pmic_keys_info *info = data;
	u32 key_deb, pressed;

	if (info->release_irq_num > 0) {
		pressed = 1;
	} else {
		regmap_read(info->keys->regmap, info->regs->deb_reg, &key_deb);
		key_deb &= info->regs->deb_mask;
		pressed = !key_deb;
	}

#ifdef CONFIG_KPD_VOLUME_KEY_SWAP
	if (info->keycode == KEY_VOLUMEDOWN || info->keycode == KEY_VOLUMEUP) {
		keycode_swap = kpd_get_linux_key_code(info->keycode, pressed);
		input_report_key(info->keys->input_dev, keycode_swap, pressed);
	} else {
		input_report_key(info->keys->input_dev, info->keycode, pressed);
	}
#else
	input_report_key(info->keys->input_dev, info->keycode, pressed);
#endif
	input_sync(info->keys->input_dev);

	if (pressed && info->suspend_lock)
		__pm_stay_awake(info->suspend_lock);
	else if (info->suspend_lock)
		__pm_relax(info->suspend_lock);
	dev_dbg(info->keys->dev, "(%s) key =%d using PMIC\n",
		 pressed ? "pressed" : "released", info->keycode);

	pwrkey_press = true;

	if (pressed && is_kpoc_mode(info->keys->dev) && info->keycode == KEY_POWER) {
		info->keys->press_time = ktime_get_boottime();
		if (queue_delayed_work(info->keys->wq, &info->keys->kpoc_reboot_work,
				       msecs_to_jiffies(KPOC_LONG_PRESS_TIME)))
			dev_info(info->keys->dev, "kpoc_reboot work already queued!");
	}

#if IS_ENABLED(CONFIG_AMAZON_MINERVA_METRICS_LOG)
	pwrkey_press = true;
	schedule_work(&metrics_work);
#endif

	return IRQ_HANDLED;
}

static int mtk_pmic_key_setup(struct mtk_pmic_keys *keys,
		struct mtk_pmic_keys_info *info)
{
	int ret;

	info->keys = keys;

	ret = regmap_update_bits(keys->regmap, info->regs->intsel_reg,
				 info->regs->intsel_mask,
				 info->regs->intsel_mask);
	if (ret < 0)
		return ret;

	ret = devm_request_threaded_irq(keys->dev, info->irq, NULL,
					mtk_pmic_keys_irq_handler_thread,
					IRQF_ONESHOT | IRQF_TRIGGER_HIGH,
					"mtk-pmic-keys", info);
	if (ret) {
		dev_err(keys->dev, "Failed to request IRQ: %d: %d\n",
			info->irq, ret);
		return ret;
	}
	if (info->release_irq_num > 0) {
		ret = devm_request_threaded_irq(keys->dev,
				info->release_irq_num,
				NULL, mtk_pmic_keys_release_irq_handler_thread,
				IRQF_ONESHOT | IRQF_TRIGGER_HIGH,
				"mtk-pmic-keys", info);
		if (ret) {
			dev_dbg(keys->dev, "Failed to request IRQ: %d: %d\n",
				info->release_irq_num, ret);
			return ret;
		}
	}

	input_set_capability(keys->input_dev, EV_KEY, info->keycode);

	return 0;
}

static int __maybe_unused mtk_pmic_keys_suspend(struct device *dev)
{
	struct mtk_pmic_keys *keys = dev_get_drvdata(dev);
	int index;

	for (index = 0; index < MTK_PMIC_MAX_KEY_COUNT; index++) {
		if (keys->keys[index].suspend_lock)
			enable_irq_wake(keys->keys[index].irq);
	}

	return 0;
}

static int __maybe_unused mtk_pmic_keys_resume(struct device *dev)
{
	struct mtk_pmic_keys *keys = dev_get_drvdata(dev);
	int index;

	for (index = 0; index < MTK_PMIC_MAX_KEY_COUNT; index++) {
		if (keys->keys[index].suspend_lock)
			disable_irq_wake(keys->keys[index].irq);
	}

	return 0;
}

static SIMPLE_DEV_PM_OPS(mtk_pmic_keys_pm_ops, mtk_pmic_keys_suspend,
			mtk_pmic_keys_resume);

static const struct of_device_id of_mtk_pmic_keys_match_tbl[] = {
	{
		.compatible = "mediatek,mt6359p-keys",
		.data = &mt6359p_regs,
	}, {
		.compatible = "mediatek,mt6397-keys",
		.data = &mt6397_regs,
	}, {
		.compatible = "mediatek,mt6323-keys",
		.data = &mt6323_regs,
	}, {
		.compatible = "mediatek,mt6363-keys",
		.data = &mt6363_regs,
	}, {
		/* sentinel */
	}
};
MODULE_DEVICE_TABLE(of, of_mtk_pmic_keys_match_tbl);

#if IS_ENABLED(CONFIG_AMAZON_MINERVA_METRICS_LOG)
static void pwrkey_log_to_metrics(struct work_struct *data)
{
	char *action;
	char buf[PWRKEY_METRICS_STR_LEN];
	const struct amazon_logger_ops *amazon_logger = amazon_logger_ops_get();

	action = (pwrkey_press) ? "press" : "release";

	if (amazon_logger && amazon_logger->minerva_metrics_log) {
		amazon_logger->minerva_metrics_log(buf, PWRKEY_METRICS_STR_LEN,
		"%s:%s:100:%s,%s,report_action_is_action=%s;SY:us-east-1",
		METRICS_PWRKEY_GROUP_ID, METRICS_PWRKEY_SCHEMA_ID,
		PREDEFINED_ESSENTIAL_KEY, PREDEFINED_DEVICE_ID_KEY, action);
	}
}
#endif

static int mtk_pmic_keys_probe(struct platform_device *pdev)
{
	int error, index = 0;
	unsigned int keycount;
	struct mt6397_chip *pmic_chip;
	struct device_node *node = pdev->dev.of_node, *child;
	struct mtk_pmic_keys *keys;
	const struct mtk_pmic_regs *mtk_pmic_regs;
	struct input_dev *input_dev;
#ifdef CONFIG_KPD_VOLUME_KEY_SWAP
	unsigned int volume_swap_key = 0;
#endif
	const struct of_device_id *of_id =
		of_match_device(of_mtk_pmic_keys_match_tbl, &pdev->dev);

	keys = devm_kzalloc(&pdev->dev, sizeof(*keys), GFP_KERNEL);
	if (!keys)
		return -ENOMEM;

	keys->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!keys->regmap) {
		pmic_chip =  dev_get_drvdata(pdev->dev.parent);
		if (!pmic_chip || !pmic_chip->regmap) {
			dev_info(keys->dev, "failed to get pmic key regmap\n");
			return -ENODEV;
		}

		keys->regmap = pmic_chip->regmap;
	}

	keys->dev = &pdev->dev;
	mtk_pmic_regs = of_id->data;

	keys->input_dev = input_dev = devm_input_allocate_device(keys->dev);
	if (!input_dev) {
		dev_err(keys->dev, "input allocate device fail.\n");
		return -ENOMEM;
	}

	input_dev->name = "mtk-pmic-keys";
	input_dev->id.bustype = BUS_HOST;
	input_dev->id.vendor = 0x0001;
	input_dev->id.product = 0x0001;
	input_dev->id.version = 0x0001;

	__set_bit(EV_KEY, input_dev->evbit);
	keycount = of_get_available_child_count(node);
	if (keycount > MTK_PMIC_MAX_KEY_COUNT) {
		dev_err(keys->dev, "too many keys defined (%d)\n", keycount);
		return -EINVAL;
	}

	for_each_child_of_node(node, child) {
		keys->keys[index].regs = &mtk_pmic_regs->keys_regs[index];

		keys->keys[index].irq = platform_get_irq(pdev, index);
		if (keys->keys[index].irq < 0)
			return keys->keys[index].irq;
		if (mtk_pmic_regs->release_irq) {
			keys->keys[index].release_irq_num = platform_get_irq(
						pdev,
						index + RELEASE_IRQ_INTERVAL);
			if (keys->keys[index].release_irq_num < 0)
				return keys->keys[index].release_irq_num;
		}

		error = of_property_read_u32(child,
			"linux,keycodes", &keys->keys[index].keycode);
		if (error) {
			dev_err(keys->dev,
				"failed to read key:%d linux,keycode property: %d\n",
				index, error);
			of_node_put(child);
			return error;
		}

		if (of_property_read_bool(child, "wakeup-source"))
			keys->keys[index].suspend_lock =
				wakeup_source_register(NULL, "pwrkey wakelock");

		error = mtk_pmic_key_setup(keys, &keys->keys[index]);
		if (error) {
			of_node_put(child);
			return error;
		}

		index++;
	}

#ifdef CONFIG_KPD_VOLUME_KEY_SWAP
	error = of_property_read_u32(node, "mediatek,volume-swap-key",
		&volume_swap_key);
	if (error)
		dev_err(keys->dev,"failed to read volume-swap-key\n");

	if (volume_swap_key)
		input_set_capability(keys->input_dev, EV_KEY, volume_swap_key);
#endif

	error = input_register_device(input_dev);
	if (error) {
		dev_err(&pdev->dev,
			"register input device failed (%d)\n", error);
		return error;
	}

	mtk_pmic_keys_lp_reset_setup(keys, mtk_pmic_regs);

	platform_set_drvdata(pdev, keys);

	if (is_kpoc_mode(&pdev->dev)) {
		keys->wq = alloc_ordered_workqueue("kpoc_reboot", 0);
		if (!keys->wq) {
			dev_notice(&pdev->dev, "[%s] cannot alloc kpoc workqueue\n", __func__);
			return -ENOMEM;
		}
		INIT_DELAYED_WORK(&keys->kpoc_reboot_work, do_kpoc_reboot);
	}

#if IS_ENABLED(CONFIG_AMAZON_MINERVA_METRICS_LOG)
	INIT_WORK(&metrics_work, pwrkey_log_to_metrics);
#endif

	return 0;
}

static struct platform_driver pmic_keys_pdrv = {
	.probe = mtk_pmic_keys_probe,
	.driver = {
		   .name = "mtk-pmic-keys",
		   .of_match_table = of_mtk_pmic_keys_match_tbl,
		   .pm = &mtk_pmic_keys_pm_ops,
	},
};

module_platform_driver(pmic_keys_pdrv);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Chen Zhong <chen.zhong@mediatek.com>");
MODULE_DESCRIPTION("MTK pmic-keys driver v0.1");
