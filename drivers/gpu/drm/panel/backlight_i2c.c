// SPDX-License-Identifier: GPL-2.0
/*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/kobject.h>
#include <linux/platform_device.h>
#include <linux/atomic.h>
#include <linux/of.h>
#include <linux/module.h>

#include "backlight_i2c.h"

static const struct i2c_device_id backlight_i2c_id[] = {
	{"lp8556", 0},
	{"mp3314", 0},
	{"sy7758", 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, backlight_i2c_id);

static int backlight_driver_probe(struct i2c_client *client,
	const struct i2c_device_id *id);
static int backlight_driver_remove(struct i2c_client *client);

#ifdef CONFIG_OF
static const struct of_device_id backlight_id[] = {
	{.compatible = "lp8556"},
	{.compatible = "mp3314"},
	{.compatible = "sy7758"},
	{},
};
MODULE_DEVICE_TABLE(of, backlight_id);
#endif

static struct i2c_driver backlight_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "backlight",
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(backlight_id),
#endif
	},
	.probe = backlight_driver_probe,
	.remove = backlight_driver_remove,
	.id_table = backlight_i2c_id,
};

struct i2c_setting_table lp8556_bled_on_settings[] = {
	{.cmd = 0x01, .data = 0x80},
	{.cmd = 0x16, .data = 0x0F},
	{.cmd = 0xA0, .data = 0x13},
	{.cmd = 0xA1, .data = 0x5E},
	{.cmd = 0xA2, .data = 0x28},
	{.cmd = 0xA9, .data = 0x80},
};

struct i2c_setting_table mp3314_bled_on_settings[] = {
	{.cmd = 0x02, .data = 0x99},
	{.cmd = 0x03, .data = 0x4F},
	{.cmd = 0x04, .data = 0x20},
	{.cmd = 0x06, .data = 0x13},
	{.cmd = 0x07, .data = 0x5E},
	{.cmd = 0x08, .data = 0xA3},
	{.cmd = 0x0E, .data = 0x5E},
	{.cmd = 0x0F, .data = 0x25},
	{.cmd = 0x10, .data = 0x40},
};

struct i2c_setting_table sy7758_bled_on_settings[] = {
	{.cmd = 0x01, .data = 0x80},
	{.cmd = 0x16, .data = 0x0F},
	{.cmd = 0xA0, .data = 0x13},
	{.cmd = 0xA1, .data = 0x5E},
	{.cmd = 0xA2, .data = 0x28},
	{.cmd = 0xA9, .data = 0x80},
};

static LIST_HEAD(backlight_list);
static DEFINE_MUTEX(backlight_i2c_access);

/* I2C Function For Read/Write */
bool backlight_read_byte(struct i2c_client *client, unsigned char cmd, unsigned char *returnData)
{
	char cmd_buf[2] = { 0x00, 0x00 };
	char readData = 0;
	int ret = 0;

	mutex_lock(&backlight_i2c_access);

	cmd_buf[0] = cmd;
	ret = i2c_master_send(client, &cmd_buf[0], 1);
	if (ret < 0) {
		mutex_unlock(&backlight_i2c_access);
		return false;
	}
	ret = i2c_master_recv(client, &cmd_buf[1], 1);
	if (ret < 0) {
		mutex_unlock(&backlight_i2c_access);
		return false;
	}
	readData = cmd_buf[1];
	*returnData = readData;

	mutex_unlock(&backlight_i2c_access);

	return true;
}

bool backlight_write_byte(struct i2c_client *client, unsigned char cmd, unsigned char writeData)
{
	char write_data[2] = { 0 };
	int ret = 0;

	pr_notice("[KE/BACKLIGHT] %s\n", __func__);

	mutex_lock(&backlight_i2c_access);

	write_data[0] = cmd;
	write_data[1] = writeData;
	ret = i2c_master_send(client, write_data, 2);
	if (ret < 0) {
		mutex_unlock(&backlight_i2c_access);
		pr_err("[BACKLIGHT] I2C write fail!!!\n");
		return false;
	}

	mutex_unlock(&backlight_i2c_access);

	return true;
}

static struct backlight_i2c *backlight_kzalloc_init(struct i2c_client *client)
{
	struct backlight_i2c *backlight = NULL;

	backlight = devm_kzalloc(&client->dev, sizeof(struct backlight_i2c),
					GFP_KERNEL);
	if (backlight == NULL) {
		pr_err("[KE/BACKLIGHT] Failed to devm_kzalloc backlight\n");
		return backlight;
	}

	memset(backlight, 0, sizeof(struct backlight_i2c));
	backlight->i2c = client;
	switch (client->addr) {
	case LP8556_SLAVE_ADDR:
		backlight->table = lp8556_bled_on_settings;
		backlight->table_size = ARRAY_SIZE(lp8556_bled_on_settings);
		break;
	case MP3314_SLAVE_ADDR:
		backlight->table = mp3314_bled_on_settings;
		backlight->table_size = ARRAY_SIZE(mp3314_bled_on_settings);
		break;
	case SY7758_SLAVE_ADDR:
		backlight->table = sy7758_bled_on_settings;
		backlight->table_size = ARRAY_SIZE(sy7758_bled_on_settings);
		break;
	default:
		pr_err("[KE/BACKLIGHT] No match backlight ic\n");
		devm_kfree(&client->dev, backlight);
		backlight = NULL;
		break;
	}
	pr_notice("[KE/BACKLIGHT] %s backlight devm_kzalloc and init down\n", backlight->i2c->name);

	return backlight;
}

static int backlight_driver_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int err = -EINVAL;
	struct backlight_i2c *backlight = NULL;

	pr_notice("[KE/BACKLIGHT] name=%s addr=0x%x\n",
		client->name, client->addr);

	backlight = backlight_kzalloc_init(client);
	if (!backlight) {
		err = -EINVAL;
		goto exit;
	}
	i2c_set_clientdata(client, backlight);

	/*add device to total list */
	mutex_lock(&backlight_i2c_access);
	list_add(&backlight->list, &backlight_list);
	mutex_unlock(&backlight_i2c_access);

	return 0;
 exit:
	return err;
}

static int backlight_driver_remove(struct i2c_client *client)
{
	struct backlight_i2c *backlight = i2c_get_clientdata(client);

	pr_notice("[KE/BACKLIGHT] %s\n", __func__);

	mutex_lock(&backlight_i2c_access);
	list_del(&backlight->list);
	mutex_unlock(&backlight_i2c_access);

	if (backlight) {
		devm_kfree(&client->dev, backlight);
		backlight = NULL;
	}
	i2c_unregister_device(client);

	return 0;
}

static int __init backlight_init(void)
{
	pr_notice("[KE/BACKLIGHT] %s\n", __func__);

	if (i2c_add_driver(&backlight_driver) != 0)
		pr_err("[KE/BACKLIGHT] failed to register backlight driver.\n");
	else
		pr_notice("[KE/BACKLIGHT] Success to register backlight driver.\n");

	return 0;
}

static void __exit backlight_exit(void)
{
	i2c_del_driver(&backlight_driver);
}

void backlight_on(void)
{
	bool ret = false;
	unsigned int i = 0;
	struct list_head *pos = NULL;
	struct backlight_i2c *backlight = NULL;

	list_for_each(pos, &backlight_list) {
		backlight = list_entry(pos, struct backlight_i2c, list);
		if (backlight == NULL) {
			pr_err("struct backlight not ready");
			return;
		}

		for (i = 0; i < backlight->table_size; i++) {
			ret = backlight_write_byte(backlight->i2c, backlight->table[i].cmd,
					    		backlight->table[i].data);
			if (ret == false) {
				pr_err("%s backlight_on fail,no match\n", backlight->i2c->name);
				break;
			}
		}
		if (ret == true) {
			pr_notice("%s backlight_on success\n", backlight->i2c->name);
			return;
		}
	}
}
EXPORT_SYMBOL(backlight_on);

module_init(backlight_init);
module_exit(backlight_exit);
MODULE_LICENSE("GPL v2");
