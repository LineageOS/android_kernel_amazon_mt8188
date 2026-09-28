// SPDX-License-Identifier: GPL-2.0
/* Goodix's GF316M/GF318M/GF3118M/GF518M/GF5118M/GF516M/GF816M/GF3208/GF5206/GF5216/GF5208
 *  fingerprint sensor linux driver for TEE
 *
 * 2010 - 2022 Goodix Technology.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/fb.h>
#include <linux/pm_wakeup.h>
#include <linux/of_gpio.h>

#if IS_ENABLED(CONFIG_DRM_MEDIATEK)
#include "mtk_disp_notify.h"
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#else
#include <linux/notifier.h>
#endif

#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>

#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif

#ifdef CONFIG_MTK_CLKMGR
#include "mach/mt_clkmgr.h"
#else
#include <linux/clk.h>
#endif
#include <net/sock.h>
#include "gf_spi_tee.h"

/**************************defination**************************/
#define GF_DEV_NAME "goodix_fp"
#define GF_DEV_MAJOR 0	/* assigned */

#define GF_CLASS_NAME "goodix_fp"
#define GF_INPUT_NAME "gf-keys"

#define GF_LINUX_VERSION "V1.01.06"

#define GF_NETLINK_ROUTE 29   /* for GF test temporary */
#define MAX_NL_MSG_LEN 16

#define WAKELOCK_HOLD_TIME 700 /* in ms */

#define GF_WAKESOURCE_ENABLE 1
#define FPR_SUPPORT_DISPALY_EVENTS 0
/*************************************************************/

/* debug log setting */
u8 g_debug_level = ERR_LOG;

/* align=2, 2 bytes align */
/* align=4, 4 bytes align */
/* align=8, 8 bytes align */
#define ROUND_UP(x, align)		((x+(align-1))&~(align-1))

/*************************************************************/
static LIST_HEAD(device_list);
static DEFINE_MUTEX(device_list_lock);

static const struct of_device_id gf_of_match[] = {
	{ .compatible = "mediatek,goodix-fp" },
	{},
};
MODULE_DEVICE_TABLE(of, gf_of_match);

/* for netlink use */
static int pid;
static u8 g_vendor_id;

void gf_netlink_send(struct gf_device *gf_dev, const int command);
static ssize_t gf_debug_show(struct device *dev,
			struct device_attribute *attr, char *buf);

static ssize_t gf_debug_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
	int debug_level;
	struct gf_device *gf_dev = dev_get_drvdata(dev);

	pr_info("[gf] %s: Store debug_level = %s\n", __func__, buf);

	if (sscanf(buf, "%x", &debug_level) == 1) {
		if (debug_level & DEBUG_LOG)
			g_debug_level = debug_level & DEBUG_LOG;
		else if (debug_level & INFO_LOG)
			g_debug_level = debug_level & INFO_LOG;
		else
			g_debug_level = ERR_LOG;

		gf_netlink_send(gf_dev, GF_NETLINK_HAL_CTRL_LOG | debug_level);
	} else
		pr_err("[gf] %s: length = %u,invalid content:%s\n",
			 __func__, count, buf);

	return count;
}

static DEVICE_ATTR(debug, 0644, gf_debug_show, gf_debug_store);

static struct attribute *gf_debug_attrs[] = {
	&dev_attr_debug.attr,
	NULL
};

static const struct attribute_group gf_debug_attr_group = {
	.attrs = gf_debug_attrs
};

/* -------------------------------------------------------------------- */
/* fingerprint chip gpio configuration					*/
/* -------------------------------------------------------------------- */
static int gf_get_gpio_dts_info(struct gf_device *gf_dev)
{
	int ret = -1;
	struct platform_device *pdev = gf_dev->pldev;

	gf_debug(DEBUG_LOG, "%s, from dts pinctrl\n", __func__);
	if (pdev) {
		gf_dev->pinctrl_gpios = devm_pinctrl_get(&pdev->dev);
		if (IS_ERR(gf_dev->pinctrl_gpios)) {
			ret = PTR_ERR(gf_dev->pinctrl_gpios);
			gf_debug(ERR_LOG, "%s can't find fingerprint pinctrl\n", __func__);
			return ret;
		}
	} else {
		gf_debug(ERR_LOG, "%s platform device is null\n", __func__);
		return -1;
	}

	gf_dev->pins_irq = pinctrl_lookup_state(gf_dev->pinctrl_gpios, "fingerprint_irq");
	if (IS_ERR(gf_dev->pins_irq)) {
		ret = PTR_ERR(gf_dev->pins_irq);
		gf_debug(ERR_LOG, "%s can't find fingerprint pinctrl irq\n", __func__);
		return ret;
	}
	gf_dev->pins_reset_high = pinctrl_lookup_state(gf_dev->pinctrl_gpios, "reset_high");
	if (IS_ERR(gf_dev->pins_reset_high)) {
		ret = PTR_ERR(gf_dev->pins_reset_high);
		gf_debug(ERR_LOG, "%s can't find fingerprint pinctrl reset_high\n", __func__);
		return ret;
	}
	gf_dev->pins_reset_low = pinctrl_lookup_state(gf_dev->pinctrl_gpios, "reset_low");
	if (IS_ERR(gf_dev->pins_reset_low)) {
		ret = PTR_ERR(gf_dev->pins_reset_low);
		gf_debug(ERR_LOG, "%s can't find fingerprint pinctrl reset_low\n", __func__);
		return ret;
	}
	gf_dev->pins_power_high = pinctrl_lookup_state(gf_dev->pinctrl_gpios, "power_high");
	if (IS_ERR(gf_dev->pins_power_high)) {
		ret = PTR_ERR(gf_dev->pins_power_high);
		gf_debug(ERR_LOG, "%s can't find fingerprint pinctrl power_high\n", __func__);
		return ret;
	}
	gf_dev->pins_power_low = pinctrl_lookup_state(gf_dev->pinctrl_gpios, "power_low");
	if (IS_ERR(gf_dev->pins_power_low)) {
		ret = PTR_ERR(gf_dev->pins_power_low);
		gf_debug(ERR_LOG, "%s can't find fingerprint pinctrl power_low\n", __func__);
		return ret;
	}
	gf_debug(DEBUG_LOG, "%s, get pinctrl success!\n", __func__);
	return 0;
}

static void gf_hw_power_enable(struct gf_device *gf_dev, u8 onoff)
{
	/* TODO: LDO configure */
	static int enable = 0;

	if (onoff == enable) {
		return;
	} else if (onoff == 1) {
		/* TODO:  set power  according to high situation  */
		pinctrl_select_state(gf_dev->pinctrl_gpios, gf_dev->pins_power_high);
		mdelay(15);
		enable = 1;
	} else if (onoff == 0) {
		/* TODO:  set power  according to low situation  */
		pinctrl_select_state(gf_dev->pinctrl_gpios, gf_dev->pins_power_low);
		mdelay(15);
		enable = 0;
	}
}

static void gf_bypass_flash_gpio_cfg(void)
{
	/* TODO: by pass flash IO config, default connect to GND */
}

static void gf_irq_gpio_cfg(struct gf_device *gf_dev)
{
	struct device_node *node;

	if (!gf_dev || !gf_dev->pldev) {
		gf_debug(ERR_LOG, "%s invalide handle.\n", __func__);
		return;
	}
	pinctrl_select_state(gf_dev->pinctrl_gpios, gf_dev->pins_irq);
	node = gf_dev->pldev->dev.of_node;
	if (node) {
		gf_dev->irq = irq_of_parse_and_map(node, 0);
		gf_debug(INFO_LOG, "%s, gf_irq = %d\n", __func__, gf_dev->irq);
	} else
		gf_debug(ERR_LOG, "%s can't find compatible node\n", __func__);
}

/* delay ms after reset */
static void gf_hw_reset(struct gf_device *gf_dev, u8 delay)
{
	FUNC_ENTRY();
	pinctrl_select_state(gf_dev->pinctrl_gpios, gf_dev->pins_reset_low);
	mdelay(5);
	pinctrl_select_state(gf_dev->pinctrl_gpios, gf_dev->pins_reset_high);

	if (delay) {
		/* delay is configurable */
		mdelay(delay);
	}
}

static void gf_enable_irq(struct gf_device *gf_dev)
{
	if (gf_dev->irq_enable_status == GF_IRQ_ENABLE) {
		gf_debug(ERR_LOG, "%s, irq already enabled\n", __func__);
	} else {
		enable_irq(gf_dev->irq);
		gf_dev->irq_enable_status = GF_IRQ_ENABLE;
		gf_debug(DEBUG_LOG, "%s enable interrupt!\n", __func__);
	}
}

static void gf_disable_irq(struct gf_device *gf_dev)
{
	if (gf_dev->irq_enable_status == GF_IRQ_UNENABLE) {
		gf_debug(ERR_LOG, "%s, irq already disabled\n", __func__);
	} else {
		disable_irq(gf_dev->irq);
		gf_dev->irq_enable_status = GF_IRQ_UNENABLE;
		gf_debug(DEBUG_LOG, "%s disable interrupt!\n", __func__);
	}
}

/* -------------------------------------------------------------------- */
/* netlink functions                 					*/
/* -------------------------------------------------------------------- */
void gf_netlink_send(struct gf_device *gf_dev, const int command)
{
	struct nlmsghdr *nlh = NULL;
	struct sk_buff *skb = NULL;
	int ret;

	gf_debug(INFO_LOG, "[%s] : enter, send command %d\n", __func__, command);
	if (gf_dev->nl_sk == NULL) {
		gf_debug(ERR_LOG, "[%s] : invalid socket\n", __func__);
		return;
	}

	if (pid == 0) {
		gf_debug(ERR_LOG, "[%s] : invalid native process pid\n", __func__);
		return;
	}

	/*alloc data buffer for sending to native*/
	/*malloc data space at least 1500 bytes, which is ethernet data length*/
	skb = alloc_skb(MAX_NL_MSG_LEN, GFP_ATOMIC);
	if (skb == NULL)
		return;

	nlh = nlmsg_put(skb, 0, 0, 0, MAX_NL_MSG_LEN, 0);
	if (!nlh) {
		gf_debug(ERR_LOG, "[%s] : nlmsg_put failed\n", __func__);
		kfree_skb(skb);
		return;
	}

	NETLINK_CB(skb).portid = 0;
	NETLINK_CB(skb).dst_group = 0;

	*(char *)NLMSG_DATA(nlh) = command;
	ret = netlink_unicast(gf_dev->nl_sk, skb, pid, MSG_DONTWAIT);
	if (ret <= 0) {
		gf_debug(ERR_LOG, "[%s] : send failed, ret=%d\n", __func__, ret);
		return;
	}
	gf_debug(INFO_LOG, "[%s] : send done, data length is %d\n", __func__, ret);
}

static void gf_netlink_recv(struct sk_buff *__skb)
{
	struct sk_buff *skb = NULL;
	struct nlmsghdr *nlh = NULL;
	char str[128];
	uint32_t data_len;

	gf_debug(INFO_LOG, "[%s] : enter\n", __func__);

	skb = skb_get(__skb);
	if (skb == NULL) {
		gf_debug(ERR_LOG, "[%s] : skb_get return NULL\n", __func__);
		return;
	}

	/* presume there is 5byte payload at leaset */
	if (skb->len >= NLMSG_SPACE(0)) {
		nlh = nlmsg_hdr(skb);
		data_len = NLMSG_PAYLOAD(nlh, 0);
		memcpy(str, NLMSG_DATA(nlh), min(data_len, (uint32_t)sizeof(str)));
		pid = nlh->nlmsg_pid;
		gf_debug(INFO_LOG, "[%s] : pid: %d, msg: %s\n", __func__, pid, str);

	} else
		gf_debug(ERR_LOG, "[%s] : not enough data length\n", __func__);

	kfree_skb(skb);
}

static int gf_netlink_init(struct gf_device *gf_dev)
{
	struct netlink_kernel_cfg cfg;

	memset(&cfg, 0, sizeof(struct netlink_kernel_cfg));
	cfg.input = gf_netlink_recv;
	gf_dev->nl_sk = netlink_kernel_create(&init_net, GF_NETLINK_ROUTE, &cfg);
	if (gf_dev->nl_sk == NULL) {
		gf_debug(ERR_LOG, "[%s] : netlink create failed\n", __func__);
		return -1;
	}
	gf_debug(INFO_LOG, "[%s] : netlink create success\n", __func__);
	return 0;
}

static int gf_netlink_destroy(struct gf_device *gf_dev)
{
	if (gf_dev->nl_sk != NULL) {
		netlink_kernel_release(gf_dev->nl_sk);
		gf_dev->nl_sk = NULL;
		return 0;
	}
	gf_debug(ERR_LOG, "[%s] : no netlink socket yet\n", __func__);
	return -1;
}

/* -------------------------------------------------------------------- */
/* early suspend callback and suspend/resume functions         		*/
/* -------------------------------------------------------------------- */
#ifdef CONFIG_HAS_EARLYSUSPEND
static void gf_early_suspend(struct early_suspend *handler)
{
	struct gf_device *gf_dev = NULL;

	gf_dev = container_of(handler, struct gf_device, early_suspend);
	gf_debug(INFO_LOG, "[%s] enter\n", __func__);

	gf_netlink_send(gf_dev, GF_NETLINK_SCREEN_OFF);
}

static void gf_late_resume(struct early_suspend *handler)
{
	struct gf_device *gf_dev = NULL;

	gf_dev = container_of(handler, struct gf_device, early_suspend);
	gf_debug(INFO_LOG, "[%s] enter\n", __func__);

	gf_netlink_send(gf_dev, GF_NETLINK_SCREEN_ON);
}
#else

static int gf_fb_notifier_callback(struct notifier_block *self,
			unsigned long event, void *data)
{
#if FPR_SUPPORT_DISPALY_EVENTS
	struct gf_device *gf_dev = NULL;
#if !IS_ENABLED(CONFIG_DRM_MEDIATEK)
	struct fb_event *evdata = data;
#endif
	unsigned int blank;

	FUNC_ENTRY();

	/* If we aren't interested in this event, skip it immediately ... */
	if (event != MTK_DISP_EVENT_BLANK /* MTK_DISP_EARLY_EVENT_BLANK */)
		return 0;

	gf_dev = container_of(self, struct gf_device, notifier);
#if IS_ENABLED(CONFIG_DRM_MEDIATEK)
	blank = *(unsigned int *)data;
#else
	blank = *(int *)evdata->data;
#endif

	gf_debug(INFO_LOG, "[%s] : enter, blank=0x%x\n", __func__, blank);

	switch (blank) {
	case MTK_DISP_BLANK_UNBLANK:
		gf_debug(INFO_LOG, "[%s] : lcd on notify\n", __func__);
		gf_netlink_send(gf_dev, GF_NETLINK_SCREEN_ON);
		break;

	case MTK_DISP_BLANK_POWERDOWN:
		gf_debug(INFO_LOG, "[%s] : lcd off notify\n", __func__);
		gf_netlink_send(gf_dev, GF_NETLINK_SCREEN_OFF);
		break;

	default:
		gf_debug(INFO_LOG, "[%s] : other notifier, ignore\n", __func__);
		break;
	}
	FUNC_EXIT();
	return 0;
#else
	return 0;
#endif
}
#endif /* CONFIG_HAS_EARLYSUSPEND */

/* -------------------------------------------------------------------- */
/* file operation function                                              */
/* -------------------------------------------------------------------- */
static ssize_t gf_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	int retval = 0;

	FUNC_EXIT();
	return retval;
}

static ssize_t gf_write(struct file *filp, const char __user *buf,
			size_t count, loff_t *f_pos)
{
	gf_debug(ERR_LOG, "%s: Not support write opertion\n", __func__);
	return -EFAULT;
}

static irqreturn_t gf_irq(int irq, void *handle)
{
	struct gf_device *gf_dev = (struct gf_device *)handle;

	FUNC_ENTRY();

	gf_dev->irq_count++;
	gf_debug(INFO_LOG, "%s: irq_count:[%d]\n", __func__, gf_dev->irq_count);

#if GF_WAKESOURCE_ENABLE
	__pm_wakeup_event(&gf_dev->fp_wakesrc, WAKELOCK_HOLD_TIME);
#endif

	gf_netlink_send(gf_dev, GF_NETLINK_IRQ);

	FUNC_EXIT();
	return IRQ_HANDLED;
}

static long gf_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct gf_device *gf_dev = NULL;
	struct gf_key gf_key;
	gf_nav_event_t nav_event = GF_NAV_NONE;
	uint32_t nav_input = 0;
	uint32_t key_input = 0;
	int retval = 0;
	u8  buf    = 0;
	u8 netlink_route = GF_NETLINK_ROUTE;
	struct gf_ioc_chip_info info;

	FUNC_ENTRY();
	if (_IOC_TYPE(cmd) != GF_IOC_MAGIC)
		return -EINVAL;

	/* Check access direction once here; don't repeat below.*/
	/* IOC_DIR is from the user perspective, while access_ok is*/
	/* from the kernel perspective; so they look reversed.*/
	if (_IOC_DIR(cmd) & _IOC_READ)
		retval = !access_ok((void __user *)arg, _IOC_SIZE(cmd));
	if (retval == 0 && _IOC_DIR(cmd) & _IOC_WRITE)
		retval |= !access_ok((void __user *)arg, _IOC_SIZE(cmd));
	if (retval)
		return -EINVAL;

	gf_dev = (struct gf_device *)filp->private_data;
	if (!gf_dev) {
		gf_debug(ERR_LOG, "%s: gf_dev IS NULL \n", __func__);
		return -EINVAL;
	}

	switch (cmd) {
	case GF_IOC_INIT:
		gf_debug(INFO_LOG, "%s: GF_IOC_INIT gf init\n", __func__);
		gf_debug(INFO_LOG, "%s: Linux Version %s\n", __func__, GF_LINUX_VERSION);

		if (copy_to_user((void __user *)arg, (void *)&netlink_route, sizeof(u8))) {
			retval = -EFAULT;
			break;
		}
		if (gf_dev->system_status) {
			gf_debug(INFO_LOG, "%s: system re-started\n", __func__);
			break;
		}
		gf_irq_gpio_cfg(gf_dev);
		retval = request_threaded_irq(gf_dev->irq, NULL, gf_irq,
				IRQF_TRIGGER_RISING | IRQF_ONESHOT, "goodix_fp_irq", gf_dev);
		if (!retval)
			gf_debug(INFO_LOG, "%s irq thread request success!\n", __func__);
		else
			gf_debug(ERR_LOG, "%s irq thread request failed, retval=%d\n",
				__func__, retval);
		enable_irq_wake(gf_dev->irq);
		gf_disable_irq(gf_dev);

#if defined(CONFIG_HAS_EARLYSUSPEND)
		gf_debug(INFO_LOG, "[%s] : register_early_suspend\n", __func__);
		gf_dev->early_suspend.level = (EARLY_SUSPEND_LEVEL_DISABLE_FB - 1);
		gf_dev->early_suspend.suspend = gf_early_suspend,
		gf_dev->early_suspend.resume = gf_late_resume,
		register_early_suspend(&gf_dev->early_suspend);
#elif IS_ENABLED(CONFIG_DRM_MEDIATEK)
		gf_dev->notifier.notifier_call = gf_fb_notifier_callback;
		retval = mtk_disp_notifier_register("goodix_fp", &gf_dev->notifier);
		if (retval) {
			gf_debug(ERR_LOG, "Failed to register disp notifier client:%d", retval);
		}
#else
		/* register screen on/off callback */
		gf_dev->notifier.notifier_call = gf_fb_notifier_callback;
		fb_register_client(&gf_dev->notifier);
#endif

		gf_dev->system_status = 1;

		gf_debug(INFO_LOG, "%s: gf init finished\n", __func__);
		break;

	case GF_IOC_CHIP_INFO:
		if (copy_from_user(&info, (struct gf_ioc_chip_info *)arg,
		sizeof(struct gf_ioc_chip_info))) {
			retval = -EFAULT;
			break;
		}
		g_vendor_id = info.vendor_id;

		gf_debug(INFO_LOG, "%s: vendor_id 0x%x\n", __func__, g_vendor_id);
		gf_debug(INFO_LOG, "%s: mode 0x%x\n", __func__, info.mode);
		gf_debug(INFO_LOG, "%s: operation 0x%x\n", __func__, info.operation);
		break;

	case GF_IOC_EXIT:
		gf_debug(INFO_LOG, "%s: GF_IOC_EXIT \n", __func__);
		gf_disable_irq(gf_dev);
		disable_irq_wake(gf_dev->irq);
		if (gf_dev->irq) {
			free_irq(gf_dev->irq, gf_dev);
			gf_dev->irq_count = 0;
			gf_dev->irq = 0;
		}

#ifdef CONFIG_HAS_EARLYSUSPEND
		if (gf_dev->early_suspend.suspend)
			unregister_early_suspend(&gf_dev->early_suspend);
#elif IS_ENABLED(CONFIG_DRM_MEDIATEK)
		if (mtk_disp_notifier_unregister(&gf_dev->notifier))
			gf_debug(ERR_LOG,"GF_IOC_EXIT error occurred while unregistering disp_notifier.\n");
#else
		fb_unregister_client(&gf_dev->notifier);
#endif

		gf_dev->system_status = 0;
		gf_debug(INFO_LOG, "%s: gf exit finished \n", __func__);
		break;

	case GF_IOC_RESET:
		gf_debug(INFO_LOG, "%s: chip reset command\n", __func__);
		gf_hw_reset(gf_dev, 60);
		break;

	case GF_IOC_ENABLE_IRQ:
		gf_debug(INFO_LOG, "%s: GF_IOC_ENABLE_IRQ\n", __func__);
		gf_enable_irq(gf_dev);
		break;

	case GF_IOC_DISABLE_IRQ:
		gf_debug(INFO_LOG, "%s: GF_IOC_DISABLE_IRQ\n", __func__);
		gf_disable_irq(gf_dev);
		break;

	case GF_IOC_ENABLE_SPI_CLK:
		gf_debug(INFO_LOG, "%s: GF_IOC_ENABLE_SPI_CLK ignore.\n", __func__);
		break;

	case GF_IOC_DISABLE_SPI_CLK:
		gf_debug(INFO_LOG, "%s: GF_IOC_DISABLE_SPI_CLK ignore.\n", __func__);
		break;

	case GF_IOC_ENABLE_POWER:
		gf_debug(INFO_LOG, "%s: GF_IOC_ENABLE_POWER\n", __func__);
		gf_hw_power_enable(gf_dev, 1);
		break;

	case GF_IOC_DISABLE_POWER:
		gf_debug(INFO_LOG, "%s: GF_IOC_DISABLE_POWER \n", __func__);
		gf_hw_power_enable(gf_dev, 0);
		break;

	case GF_IOC_INPUT_KEY_EVENT:
		if (copy_from_user(&gf_key, (struct gf_key *)arg, sizeof(struct gf_key))) {
			gf_debug(ERR_LOG, "Failed to copy input key event from user to kernel\n");
			retval = -EFAULT;
			break;
		}

		if (gf_key.key == GF_KEY_HOME) {
			key_input = GF_KEY_INPUT_HOME;
		} else if (gf_key.key == GF_KEY_POWER) {
			key_input = GF_KEY_INPUT_POWER;
		} else if (gf_key.key == GF_KEY_CAMERA) {
			key_input = GF_KEY_INPUT_CAMERA;
		} else {
			/* add special key define */
			key_input = gf_key.key;
		}
		gf_debug(INFO_LOG, "%s: received key event[%d], key=%d, value=%d\n",
				__func__, key_input, gf_key.key, gf_key.value);

		if ((GF_KEY_POWER == gf_key.key || GF_KEY_CAMERA == gf_key.key)
			&& (gf_key.value == 1)) {
			input_report_key(gf_dev->input, key_input, 1);
			input_sync(gf_dev->input);
			input_report_key(gf_dev->input, key_input, 0);
			input_sync(gf_dev->input);
		}

		if (gf_key.key == GF_KEY_HOME) {
			input_report_key(gf_dev->input, key_input, gf_key.value);
			input_sync(gf_dev->input);
		}
	break;

	case GF_IOC_NAV_EVENT:
	    gf_debug(ERR_LOG, "nav event");
		if (copy_from_user(&nav_event, (gf_nav_event_t *)arg, sizeof(gf_nav_event_t))) {
			gf_debug(ERR_LOG, "Failed to copy nav event from user to kernel\n");
			retval = -EFAULT;
			break;
		}

		switch (nav_event) {
		case GF_NAV_FINGER_DOWN:
			gf_debug(ERR_LOG, "nav finger down");
			break;

		case GF_NAV_FINGER_UP:
			gf_debug(ERR_LOG, "nav finger up");
			break;

		case GF_NAV_DOWN:
			nav_input = GF_NAV_INPUT_DOWN;
			gf_debug(ERR_LOG, "nav down");
			break;

		case GF_NAV_UP:
			nav_input = GF_NAV_INPUT_UP;
			gf_debug(ERR_LOG, "nav up");
			break;

		case GF_NAV_LEFT:
			nav_input = GF_NAV_INPUT_LEFT;
			gf_debug(ERR_LOG, "nav left");
			break;

		case GF_NAV_RIGHT:
			nav_input = GF_NAV_INPUT_RIGHT;
			gf_debug(ERR_LOG, "nav right");
			break;

		case GF_NAV_CLICK:
			nav_input = GF_NAV_INPUT_CLICK;
			gf_debug(ERR_LOG, "nav click");
			break;

		case GF_NAV_HEAVY:
			nav_input = GF_NAV_INPUT_HEAVY;
			break;

		case GF_NAV_LONG_PRESS:
			nav_input = GF_NAV_INPUT_LONG_PRESS;
			break;

		case GF_NAV_DOUBLE_CLICK:
			nav_input = GF_NAV_INPUT_DOUBLE_CLICK;
			break;

		default:
			gf_debug(INFO_LOG, "%s: not support nav event nav_event:%d\n",
			__func__, nav_event);
			break;
		}

		if ((nav_event != GF_NAV_FINGER_DOWN) && (nav_event != GF_NAV_FINGER_UP)) {
		    input_report_key(gf_dev->input, nav_input, 1);
		    input_sync(gf_dev->input);
		    input_report_key(gf_dev->input, nav_input, 0);
		    input_sync(gf_dev->input);
		}
		break;

	case GF_IOC_ENTER_SLEEP_MODE:
		gf_debug(INFO_LOG, "%s: GF_IOC_ENTER_SLEEP_MODE ignore\n", __func__);
		break;

	case GF_IOC_GET_FW_INFO:
		gf_debug(INFO_LOG, "%s: GF_IOC_GET_FW_INFO\n", __func__);
		buf = gf_dev->need_update;

		gf_debug(DEBUG_LOG, "%s: firmware info  0x%x\n", __func__, buf);
		if (copy_to_user((void __user *)arg, (void *)&buf, sizeof(u8))) {
			gf_debug(ERR_LOG, "Failed to copy data to user\n");
			retval = -EFAULT;
		}
		break;

	case GF_IOC_REMOVE:
		gf_debug(INFO_LOG, "%s: GF_IOC_REMOVE\n", __func__);

		gf_netlink_destroy(gf_dev);

		mutex_lock(&gf_dev->release_lock);
		if (gf_dev->input == NULL) {
			mutex_unlock(&gf_dev->release_lock);
			break;
		}
		input_unregister_device(gf_dev->input);
		gf_dev->input = NULL;
		mutex_unlock(&gf_dev->release_lock);

		cdev_del(&gf_dev->cdev);
		sysfs_remove_group(&gf_dev->device->kobj, &gf_debug_attr_group);
		device_destroy(gf_dev->class, gf_dev->devno);
		list_del(&gf_dev->device_entry);
		unregister_chrdev_region(gf_dev->devno, 1);
		class_destroy(gf_dev->class);
		gf_hw_power_enable(gf_dev, 0);
		mutex_destroy(&gf_dev->release_lock);
		break;

	default:
		gf_debug(ERR_LOG, "gf doesn't support this command(%x)\n", cmd);
		break;
	}

	FUNC_EXIT();
	return retval;
}

#ifdef CONFIG_COMPAT
static long gf_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int retval = 0;

	FUNC_ENTRY();

	retval = filp->f_op->unlocked_ioctl(filp, cmd, arg);

	FUNC_EXIT();
	return retval;
}
#endif

static unsigned int gf_poll(struct file *filp, struct poll_table_struct *wait)
{
	gf_debug(ERR_LOG, "Not support poll opertion.\n");
	return -EFAULT;
}

/* -------------------------------------------------------------------- */
/* devfs                                                              	*/
/* -------------------------------------------------------------------- */
static ssize_t gf_debug_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct gf_device *gf_dev = dev_get_drvdata(dev);

	gf_debug(INFO_LOG, "%s: Show debug_level = 0x%x\n", __func__, g_debug_level);
	return sprintf(buf, "Usage:\n"
		       "0: Log off (default)\n"
		       "1: Kernel info log on.\n"
		       "2: Kernel debug log on.\n"
		       "4: HAL events log on.\n"
		       "8: HAL dump perf log on.\n"
		       "It allows OR ops, example 0x5, 0xc, ... 0xf parameters.\n"
		       "g_debug_level=0x%x, vendor id 0x%x irq_count = %d \n",
		       g_debug_level, g_vendor_id, gf_dev->irq_count);
}

/* -------------------------------------------------------------------- */
/* device function							*/
/* -------------------------------------------------------------------- */
static int gf_open(struct inode *inode, struct file *filp)
{
	struct gf_device *gf_dev = NULL;
	int status = -ENXIO;

	FUNC_ENTRY();
	mutex_lock(&device_list_lock);
	list_for_each_entry(gf_dev, &device_list, device_entry) {
		if (gf_dev->devno == inode->i_rdev) {
			gf_debug(INFO_LOG, "%s, Found\n", __func__);
			status = 0;
			break;
		}
	}
	mutex_unlock(&device_list_lock);

	if (status == 0) {
		filp->private_data = gf_dev;
		nonseekable_open(inode, filp);
		gf_debug(INFO_LOG, "%s, Success to open device. irq = %d\n", __func__, gf_dev->irq);
	} else {
		gf_debug(ERR_LOG, "%s, No device for minor %d\n", __func__, iminor(inode));
	}
	FUNC_EXIT();
	return status;
}

static int gf_release(struct inode *inode, struct file *filp)
{
	struct gf_device *gf_dev = NULL;
	int    status = 0;

	FUNC_ENTRY();
	gf_dev = filp->private_data;
	if (gf_dev->irq)
		gf_disable_irq(gf_dev);
	gf_dev->need_update = 0;
	FUNC_EXIT();
	return status;
}

static const struct file_operations gf_fops = {
	.owner			= THIS_MODULE,
	/* REVISIT switch to aio primitives, so that userspace*/
	/* gets more complete API coverage.	It'll simplify things*/
	/* too, except for the locking.*/
	.write			= gf_write,
	.read			= gf_read,
	.unlocked_ioctl	= gf_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= gf_compat_ioctl,
#endif
	.open			= gf_open,
	.release		= gf_release,
	.poll			= gf_poll,
};

/*-------------------------------------------------------------------------*/
static int gf_platform_probe(struct platform_device *pldev)
{
	int status = -EINVAL;
	struct gf_device *gf_dev = NULL;
	struct device *dev = &pldev->dev;

	FUNC_ENTRY();
	/* Allocate driver data */
	gf_dev = kzalloc(sizeof(struct gf_device), GFP_KERNEL);
	if (!gf_dev) {
		status = -ENOMEM;
		goto err;
	}
	mutex_init(&gf_dev->release_lock);
	INIT_LIST_HEAD(&gf_dev->device_entry);
	gf_dev->device_count	= 0;
	gf_dev->system_status	= 0;
	gf_dev->need_update	= 0;
	gf_dev->irq_count	= 0;
	gf_dev->irq_enable_status = GF_IRQ_UNUSED;
	gf_dev->pldev		  = pldev;
	dev_set_drvdata(dev, gf_dev);

	gf_get_gpio_dts_info(gf_dev);
	gf_hw_power_enable(gf_dev, 1);
	gf_hw_reset(gf_dev, 10);
	gf_bypass_flash_gpio_cfg();

	/* check firmware Integrity */
	gf_debug(INFO_LOG, "%s, Sensor type : %s.\n", __func__, CONFIG_GOODIX_SENSOR_TYPE);

	/* create class */
	gf_dev->class = class_create(THIS_MODULE, GF_CLASS_NAME);
	if (IS_ERR(gf_dev->class)) {
		gf_debug(ERR_LOG, "%s, Failed to create class.\n", __func__);
		status = -ENODEV;
		goto err_class;
	}

	/* get device no */
	status = alloc_chrdev_region(&gf_dev->devno,
		gf_dev->device_count++, 1, GF_DEV_NAME);
	if (status < 0) {
		gf_debug(ERR_LOG, "%s, Failed to alloc devno.\n", __func__);
		goto err_devno;
	} else {
		gf_debug(INFO_LOG, "%s, major=%d, minor=%d\n",
			__func__, MAJOR(gf_dev->devno), MINOR(gf_dev->devno));
	}

	/* create device */
	gf_dev->device = device_create(gf_dev->class, NULL,
		gf_dev->devno, gf_dev, GF_DEV_NAME);
	if (IS_ERR(gf_dev->device)) {
		gf_debug(ERR_LOG, "%s, Failed to create device.\n", __func__);
		status = -ENODEV;
		goto err_device;
	} else {
		mutex_lock(&device_list_lock);
		list_add(&gf_dev->device_entry, &device_list);
		mutex_unlock(&device_list_lock);
		gf_debug(INFO_LOG, "%s, device create success.\n", __func__);
	}

	/* create sysfs */
	status = sysfs_create_group(&gf_dev->device->kobj, &gf_debug_attr_group);
	if (status) {
		gf_debug(ERR_LOG, "%s, Failed to create sysfs file.\n", __func__);
		goto err_sysfs;
	} else {
		gf_debug(INFO_LOG, "%s, Success create sysfs file.\n", __func__);
	}

	/* cdev init and add */
	cdev_init(&gf_dev->cdev, &gf_fops);
	gf_dev->cdev.owner = THIS_MODULE;
	status = cdev_add(&gf_dev->cdev, gf_dev->devno, 1);
	if (status) {
		gf_debug(ERR_LOG, "%s, Failed to add cdev.\n", __func__);
		goto err_cdev;
	}

	/*register device within input system.*/
	gf_dev->input = input_allocate_device();
	if (gf_dev->input == NULL) {
		gf_debug(ERR_LOG, "%s, Failed to allocate input device.\n", __func__);
		status = -EINVAL;
		goto err_input;
	}
	__set_bit(EV_KEY, gf_dev->input->evbit);
	__set_bit(GF_KEY_INPUT_HOME, gf_dev->input->keybit);
	__set_bit(GF_KEY_INPUT_MENU, gf_dev->input->keybit);
	__set_bit(GF_KEY_INPUT_BACK, gf_dev->input->keybit);
	__set_bit(GF_KEY_INPUT_POWER, gf_dev->input->keybit);
	__set_bit(GF_NAV_INPUT_UP, gf_dev->input->keybit);
	__set_bit(GF_NAV_INPUT_DOWN, gf_dev->input->keybit);
	__set_bit(GF_NAV_INPUT_RIGHT, gf_dev->input->keybit);
	__set_bit(GF_NAV_INPUT_LEFT, gf_dev->input->keybit);
	__set_bit(GF_KEY_INPUT_CAMERA, gf_dev->input->keybit);
	__set_bit(GF_NAV_INPUT_CLICK, gf_dev->input->keybit);
	__set_bit(GF_NAV_INPUT_DOUBLE_CLICK, gf_dev->input->keybit);
	__set_bit(GF_NAV_INPUT_LONG_PRESS, gf_dev->input->keybit);
	__set_bit(GF_NAV_INPUT_HEAVY, gf_dev->input->keybit);
	gf_dev->input->name = GF_INPUT_NAME;
	if (input_register_device(gf_dev->input)) {
		gf_debug(ERR_LOG, "%s, Failed to register input device.\n", __func__);
		status = -ENODEV;
		goto err_input_2;
	}

	/* netlink interface init */
	status = gf_netlink_init(gf_dev);
	if (status == -1) {
		mutex_lock(&gf_dev->release_lock);
		input_unregister_device(gf_dev->input);
		gf_dev->input = NULL;
		mutex_unlock(&gf_dev->release_lock);
		goto err_input;
	}

#if GF_WAKESOURCE_ENABLE
	wakeup_source_add(&gf_dev->fp_wakesrc);
#endif

	gf_debug(INFO_LOG, "%s probe finished\n", __func__);

	FUNC_EXIT();
	return 0;

err_input_2:
	mutex_lock(&gf_dev->release_lock);
	input_free_device(gf_dev->input);
	gf_dev->input = NULL;
	mutex_unlock(&gf_dev->release_lock);

err_input:
	cdev_del(&gf_dev->cdev);

err_cdev:
	sysfs_remove_group(&gf_dev->device->kobj, &gf_debug_attr_group);

err_sysfs:
	device_destroy(gf_dev->class, gf_dev->devno);
	list_del(&gf_dev->device_entry);

err_device:
	unregister_chrdev_region(gf_dev->devno, 1);

err_devno:
	class_destroy(gf_dev->class);

err_class:
	gf_hw_power_enable(gf_dev, 0);
	mutex_destroy(&gf_dev->release_lock);
	dev_set_drvdata(dev, NULL);
	kfree(gf_dev);
	gf_dev = NULL;

err:
	FUNC_EXIT();
	return status;
}

static int gf_platform_remove(struct platform_device *pldev)
{
	struct device *dev = &pldev->dev;
	struct gf_device *gf_dev = dev_get_drvdata(dev);

	FUNC_ENTRY();

#if GF_WAKESOURCE_ENABLE
	wakeup_source_remove(&gf_dev->fp_wakesrc);
#endif

	/* make sure ops on existing fds can abort cleanly */
	if (gf_dev->irq) {
		free_irq(gf_dev->irq, gf_dev);
		gf_dev->irq_count = 0;
		gf_dev->irq = 0;
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	if (gf_dev->early_suspend.suspend)
		unregister_early_suspend(&gf_dev->early_suspend);
#elif IS_ENABLED(CONFIG_DRM_MEDIATEK)
	if (mtk_disp_notifier_unregister(&gf_dev->notifier))
		gf_debug(ERR_LOG,"Remove error occurred while unregistering disp_notifier.\n");
#else
	fb_unregister_client(&gf_dev->notifier);
#endif

	mutex_lock(&gf_dev->release_lock);
	if (gf_dev->input == NULL) {
		mutex_unlock(&gf_dev->release_lock);
		kfree(gf_dev);
		FUNC_EXIT();
		return -EINVAL;
	}
	input_unregister_device(gf_dev->input);
	gf_dev->input = NULL;
	mutex_unlock(&gf_dev->release_lock);

	gf_netlink_destroy(gf_dev);
	cdev_del(&gf_dev->cdev);
	sysfs_remove_group(&gf_dev->device->kobj, &gf_debug_attr_group);
	device_destroy(gf_dev->class, gf_dev->devno);
	list_del(&gf_dev->device_entry);

	unregister_chrdev_region(gf_dev->devno, 1);
	class_destroy(gf_dev->class);
	gf_hw_power_enable(gf_dev, 0);

	dev_set_drvdata(dev, NULL);
	mutex_destroy(&gf_dev->release_lock);

	kfree(gf_dev);
	FUNC_EXIT();
	return 0;
}

/*-------------------------------------------------------------------------*/
static struct platform_driver goodix_fp_driver = {
	.driver = {
		   .name = "goodix_fp",
		   .owner = THIS_MODULE,
		   .of_match_table = gf_of_match,
		},
	.probe = gf_platform_probe,
	.remove = gf_platform_remove
};

static int __init gf_init(void)
{
	int status = 0;

	FUNC_ENTRY();

	status = platform_driver_register(&goodix_fp_driver);
	if (status < 0) {
		gf_debug(ERR_LOG, "%s, Failed to register goodix_fp driver.\n", __func__);
		return -EINVAL;
	}

	FUNC_EXIT();
	return status;
}
module_init(gf_init);

static void __exit gf_exit(void)
{
	FUNC_ENTRY();
	platform_driver_unregister(&goodix_fp_driver);

	FUNC_EXIT();
}
module_exit(gf_exit);

MODULE_AUTHOR("goodix");
MODULE_DESCRIPTION("Goodix Fingerprint driver");
MODULE_LICENSE("GPL");
