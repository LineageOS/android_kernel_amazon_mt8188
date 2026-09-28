/*
 * Hall sensor driver capable of dealing with more than one sensor.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/input.h>

#include <linux/gpio.h>
#include <linux/proc_fs.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/seq_file.h>
#include <linux/extcon-provider.h>
#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#endif

#if IS_ENABLED(CONFIG_AMAZON_METRICS_LOG)
#include <linux/metricslog.h>
#define METRICS_STR_LEN 128
#endif

#define DELAY_VALUE 50
#define TIMEOUT_VALUE 100

#define KEYBOARD_HALL_IRQ 		"keyboard-hall-irq"
#define SCREEN_HALL_IRQ			"screen-hall-irq"

#define SUPPORT_KEYBOARD_HALL	2

enum cover_status {
	COVER_CLOSED = 0,
	COVER_OPENED
};
struct hall_sensor_data {
	unsigned int gpio_pin;
	int irq;
	char *name;
	int gpio_state;
	int irq_flags;
};
struct hall_sensors {
	struct hall_sensor_data *hall_sensor_data;
	int hall_sensor_num;
};
struct hall_priv {
	struct work_struct work;
	struct work_struct work_keyboard;
	struct workqueue_struct *workqueue;
	enum   cover_status cover[SUPPORT_KEYBOARD_HALL];
	enum   cover_status keyboard_cover[SUPPORT_KEYBOARD_HALL];
	struct input_dev *input;
	struct hall_sensor_data *sensor_data;
	int hall_sensor_num;
	struct wakeup_source *hall_wake_lock;
	struct extcon_dev *edev;
	int irq_flags;
};

static const unsigned int hall_extcon_status[] = {
	EXTCON_BOOKCOVER_STATE,
	EXTCON_NONE,
};

#ifdef CONFIG_PROC_FS
#define HALL_PROC_FILE "driver/hall_sensor"
static int hall_show(struct seq_file *m, void *v)
{
	struct hall_priv *priv = m->private;
	struct hall_sensor_data *sensor_data = priv->sensor_data;

	u8 reg_val = 0;
	int i;

	for (i = 0; i < priv->hall_sensor_num; i++)
		reg_val = reg_val | (sensor_data[i].gpio_state << i);

	seq_printf(m, "0x%x\n", reg_val);
	return 0;
}

static int hall_open(struct inode *inode, struct file *file)
{
	return single_open(file, hall_show, PDE_DATA(inode));
}

static const struct proc_ops proc_fops = {
	//.owner = THIS_MODULE,
	.proc_open = hall_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static void create_hall_proc_file(struct hall_priv *priv)
{

	struct proc_dir_entry *entry;

	entry = proc_create_data(HALL_PROC_FILE, 0644, NULL, &proc_fops, priv);
	if (!entry)
		pr_err("%s: Error creating %s\n", __func__, HALL_PROC_FILE);

}

static void remove_hall_proc_file(void)
{
	remove_proc_entry(HALL_PROC_FILE, NULL);
}
#endif

static void hall_work_func(struct work_struct *work)
{
	struct hall_priv *priv = container_of(work, struct hall_priv, work);
#if IS_ENABLED(CONFIG_AMAZON_METRICS_LOG)
	char metrics_log[METRICS_STR_LEN];
#endif
	struct input_dev *input = priv->input;
	unsigned int gpio_pin[SUPPORT_KEYBOARD_HALL];
	int i;

	for (i = 0; i < priv->hall_sensor_num; i++) {
		gpio_pin[i] = priv->sensor_data[i].gpio_pin;

		msleep(DELAY_VALUE);

		priv->sensor_data[i].gpio_state = gpio_get_value(gpio_pin[i]);
		priv->cover[i] =
			priv->sensor_data[i].gpio_state ? COVER_OPENED : COVER_CLOSED;
	}

	if (priv->hall_sensor_num == SUPPORT_KEYBOARD_HALL) {
		if (priv->cover[0] == COVER_CLOSED || priv->cover[1] == COVER_CLOSED)
			extcon_set_state_sync(priv->edev, EXTCON_BOOKCOVER_STATE, true);
		else
			extcon_set_state_sync(priv->edev, EXTCON_BOOKCOVER_STATE, false);
		// print HALL sensor status
		if (printk_ratelimit())
			pr_info("%s screen hall:%d keyboard hall: %d\n", __func__, priv->cover[0], priv->cover[1]);
	}
#if IS_ENABLED(CONFIG_AMAZON_METRICS_LOG)
	/* Log in metrics */
	//TODO DISPLAY is useless for hall sensor under current implementation
	//Revise metrics format then stop logging fake display state.
	snprintf(metrics_log, METRICS_STR_LEN,
			"bu520:TIMEOUT:COVER=%d;CT;1,DISPLAY=%d;CT;1:NR",
			priv->cover[0], 0);
	log_to_metrics(ANDROID_LOG_INFO, "HallSensor", metrics_log);
#endif
	input_report_switch(input, SW_LID,
			priv->sensor_data[0].gpio_state ? 0 : 1);
	input_sync(input);
}

static void keyboard_work_func(struct work_struct *work)
{
	struct hall_priv *priv = container_of(work, struct hall_priv, work_keyboard);
	unsigned int gpio_pin[SUPPORT_KEYBOARD_HALL];
	int i;

	for (i = 0; i < priv->hall_sensor_num; i++) {
		gpio_pin[i] = priv->sensor_data[i].gpio_pin;

		msleep(DELAY_VALUE);

		priv->sensor_data[i].gpio_state = gpio_get_value(gpio_pin[i]);
		priv->keyboard_cover[i] =
			priv->sensor_data[i].gpio_state ? COVER_OPENED : COVER_CLOSED;
	}

	if (priv->keyboard_cover[0] == COVER_CLOSED || priv->keyboard_cover[1] == COVER_CLOSED)
		extcon_set_state_sync(priv->edev, EXTCON_BOOKCOVER_STATE, true);
	else
		extcon_set_state_sync(priv->edev, EXTCON_BOOKCOVER_STATE, false);
	// print HALL sensor status
	if (printk_ratelimit())
		pr_info("%s screen hall:%d keyboard hall: %d\n", __func__, priv->keyboard_cover[0], priv->keyboard_cover[1]);
}

static irqreturn_t hall_sensor_isr(int irq, void *data)
{
	struct hall_priv *priv =
			(struct hall_priv *)data;
	struct hall_sensor_data *sensor_data = priv->sensor_data;
	unsigned int gpio_pin;

	if (irq == sensor_data[0].irq) {
		gpio_pin = sensor_data[0].gpio_pin;
		priv->sensor_data[0].gpio_state =
					gpio_get_value(gpio_pin);
		irq_set_irq_type(irq, priv->sensor_data[0].gpio_state ?
			IRQF_TRIGGER_FALLING : IRQF_TRIGGER_RISING);

		/* Avoid unnecessary suspend */
		__pm_wakeup_event(priv->hall_wake_lock, TIMEOUT_VALUE);
		queue_work(priv->workqueue, &priv->work);
	}

	return IRQ_HANDLED;
}

static irqreturn_t keyboard_sensor_isr(int irq, void *data)
{
	struct hall_priv *priv =
			(struct hall_priv *)data;
	struct hall_sensor_data *sensor_data = priv->sensor_data;
	unsigned int gpio_pin;

	if (irq == sensor_data[1].irq) {
		gpio_pin = sensor_data[1].gpio_pin;
		priv->sensor_data[1].gpio_state =
					gpio_get_value(gpio_pin);
		irq_set_irq_type(irq, priv->sensor_data[1].gpio_state ?
			IRQF_TRIGGER_FALLING : IRQF_TRIGGER_RISING);

		/* Avoid unnecessary suspend */
		__pm_wakeup_event(priv->hall_wake_lock, TIMEOUT_VALUE);
		queue_work(priv->workqueue, &priv->work_keyboard);
	}

	return IRQ_HANDLED;
}

static int configure_irqs(struct hall_priv *priv)
{
	struct hall_sensor_data *sensor_data = priv->sensor_data;
	int ret = 0;
	unsigned long irqflags = 0;

	unsigned int gpio_pin[SUPPORT_KEYBOARD_HALL];

	gpio_pin[0] = sensor_data[0].gpio_pin;

	sensor_data[0].irq = gpio_to_irq(gpio_pin[0]);

	sensor_data[0].gpio_state = gpio_get_value(gpio_pin[0]);
	priv->cover[0] = sensor_data[0].gpio_state ?
				COVER_OPENED : COVER_CLOSED;
	enable_irq_wake(sensor_data[0].irq);
	irqflags |= IRQF_ONESHOT;
	ret = request_threaded_irq(sensor_data[0].irq, NULL,
		hall_sensor_isr, irqflags,
		SCREEN_HALL_IRQ, priv);

	if (ret < 0)
		return ret;

	irq_set_irq_type(sensor_data[0].irq,
			priv->sensor_data[0].gpio_state ?
			IRQF_TRIGGER_FALLING : IRQF_TRIGGER_RISING);
	priv->sensor_data[0].irq_flags = 0;

	if (SUPPORT_KEYBOARD_HALL == priv->hall_sensor_num) {
		gpio_pin[1] = sensor_data[1].gpio_pin;

		sensor_data[1].irq = gpio_to_irq(gpio_pin[1]);

		sensor_data[1].gpio_state = gpio_get_value(gpio_pin[1]);
		priv->cover[1] = sensor_data[1].gpio_state ?
					COVER_OPENED : COVER_CLOSED;
		priv->keyboard_cover[0] = sensor_data[0].gpio_state ?
					COVER_OPENED : COVER_CLOSED;
		priv->keyboard_cover[1] = sensor_data[1].gpio_state ?
					COVER_OPENED : COVER_CLOSED;
		enable_irq_wake(sensor_data[1].irq);
		irqflags |= IRQF_ONESHOT;
		ret = request_threaded_irq(sensor_data[1].irq, NULL,
			keyboard_sensor_isr, irqflags,
			KEYBOARD_HALL_IRQ, priv);

		if (ret < 0)
			return ret;

		irq_set_irq_type(sensor_data[1].irq,
				priv->sensor_data[1].gpio_state ?
				IRQF_TRIGGER_FALLING : IRQF_TRIGGER_RISING);
		priv->sensor_data[1].irq_flags = 0;
	}

	return ret;
}

struct input_dev *hall_input_device_create(void)
{
	int err = 0;
	struct input_dev *input;

	input = input_allocate_device();
	if (!input) {
		err = -ENOMEM;
		goto exit;
	}

	set_bit(EV_SW, input->evbit);
	set_bit(SW_LID, input->swbit);

	err = input_register_device(input);
	if (err)
		goto exit_input_free;

	return input;

exit_input_free:
	input_free_device(input);
exit:
	return NULL;
}

static int hall_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct input_dev *input;
	struct hall_priv *priv;
	struct hall_sensors *hall_sensors = dev_get_platdata(&pdev->dev);
	int error = 0;
	char *gpio_name[] = {"screen-int-gpio", "keyboard-int-gpio"};
	char *sensor_name[] = {"screen-sensor-name", "keyboard-sensor-name"};

#ifdef CONFIG_OF
	struct hall_sensors hall_sensors_template;
	struct device_node *of_node;
	int i;

	hall_sensors = &hall_sensors_template;
	of_node = pdev->dev.of_node;

	error = of_property_read_u32(of_node, "sensor-num",
			&hall_sensors->hall_sensor_num);
	if (error) {
		dev_err(dev, "Unable to read hall_sensor_num\n");
		return error;
	}

	hall_sensors->hall_sensor_data = kzalloc(sizeof(struct hall_sensor_data)
					* hall_sensors->hall_sensor_num,
					GFP_KERNEL);
	if (!hall_sensors->hall_sensor_data) {
		dev_err(dev, "Failed to allocate hall sensor data\n");
		return -ENOMEM;
	}

	for (i = 0; i < hall_sensors->hall_sensor_num; i++) {

		hall_sensors->hall_sensor_data[i].gpio_pin =
					of_get_named_gpio_flags(
						of_node, gpio_name[i], 0, NULL);
		if (!gpio_is_valid(hall_sensors->hall_sensor_data[i].gpio_pin)) {
			dev_err(dev, "Unable to read gpio_name[%d]\n", i);
			goto fail0;
		}

		error = of_property_read_string_index(of_node, sensor_name[i], 0,
			(const char **)&hall_sensors->hall_sensor_data[i].name);
		if (error) {
			dev_err(dev, "Unable to read sensor-name[%d], sensor_name : %s\n", i, sensor_name[i]);
			goto fail0;
		}

		hall_sensors->hall_sensor_data[i].gpio_state = -1;
	}
#endif

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		dev_err(dev, "Failed to allocate driver data\n");
		error = -ENOMEM;
#ifdef CONFIG_OF
		goto fail0;
#else
		return error;
#endif
	}

	input = hall_input_device_create();
	if (!input) {
		dev_err(dev, "Failed to allocate input device\n");
		error = -ENOMEM;
		goto fail1;
	}

	input->name = pdev->name;
	input->dev.parent = &pdev->dev;
	priv->input = input;
	priv->sensor_data = hall_sensors->hall_sensor_data;
	priv->hall_sensor_num = hall_sensors->hall_sensor_num;
	priv->irq_flags = 0;

	priv->workqueue = create_singlethread_workqueue("workqueue");
	if (!priv->workqueue) {
		dev_err(dev, "Unable to create workqueue\n");
		goto fail2;
	}
	INIT_WORK(&priv->work, hall_work_func);

	if (SUPPORT_KEYBOARD_HALL == hall_sensors->hall_sensor_num) {
		INIT_WORK(&priv->work_keyboard, keyboard_work_func);
	}

	platform_set_drvdata(pdev, priv);

	device_init_wakeup(&pdev->dev, 1);

	priv->hall_wake_lock = wakeup_source_register(NULL, "hall_wake_lock");

	priv->edev = devm_extcon_dev_allocate(dev, hall_extcon_status);
	if (IS_ERR(priv->edev)) {
		dev_err(dev, "Failed to allocate extcon device\n");
		error = -ENOMEM;
		goto fail3;
	}

	error = devm_extcon_dev_register(dev, priv->edev);
	if (error) {
		dev_err(dev, "Failed to register extcon device, err=%d\n",error);
		goto fail3;
	}

	error = configure_irqs(priv);
	if (error) {
		dev_err(dev, "Failed to configure interupts, err=%d\n", error);
		goto fail4;
	}

#ifdef CONFIG_PROC_FS
	create_hall_proc_file(priv);
#endif
	pr_info("hall probe success!\n");
	return 0;

fail4:
	devm_extcon_dev_unregister(dev, priv->edev);
fail3:
	destroy_workqueue(priv->workqueue);
fail2:
	input_unregister_device(priv->input);
fail1:
	kfree(priv);
#ifdef CONFIG_OF
fail0:
	kfree(hall_sensors->hall_sensor_data);
#endif

	return error;
}

static int hall_remove(struct platform_device *pdev)
{
	struct hall_priv *priv = platform_get_drvdata(pdev);
	struct hall_sensor_data *sensor_data = priv->sensor_data;
	int i;

	for (i = 0; i < priv->hall_sensor_num; i++) {
		free_irq(sensor_data[i].irq, priv);
	}

	flush_workqueue(priv->workqueue);
	destroy_workqueue(priv->workqueue);
	input_unregister_device(priv->input);

	wakeup_source_unregister(priv->hall_wake_lock);
#ifdef CONFIG_OF
	kfree(sensor_data);
#endif
	kfree(priv);
	device_init_wakeup(&pdev->dev, 0);

#ifdef CONFIG_PROC_FS
	remove_hall_proc_file();
#endif
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id hall_of_match[] = {
	{.compatible = "will,hall-wh2509f", },
	{},
};

MODULE_DEVICE_TABLE(of, hall_of_match);
#endif

static struct platform_driver hall_driver = {
	.probe = hall_probe,
	.remove = hall_remove,
	.driver = {
		.name = "hall-bu520",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = hall_of_match,
#endif
	}
};

module_platform_driver(hall_driver);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Quanta");
MODULE_DESCRIPTION("Hall sensor driver");
