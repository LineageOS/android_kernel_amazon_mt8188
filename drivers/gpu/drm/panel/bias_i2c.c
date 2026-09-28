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

#include "bias_i2c.h"

static struct i2c_client *new_client;
static const struct i2c_device_id bias_i2c_id[] = { {"lcm_bias", 0}, {} };

static int bias_driver_probe(struct i2c_client *client,
	const struct i2c_device_id *id);
static int bias_driver_remove(struct i2c_client *client);

#ifdef CONFIG_OF
static const struct of_device_id bias_id[] = {
	{.compatible = "lcm_bias"},
	{},
};

MODULE_DEVICE_TABLE(of, bias_id);
#endif

static struct i2c_driver bias_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "lcm_bias",
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(bias_id),
#endif
	},
	.probe = bias_driver_probe,
	.remove = bias_driver_remove,
	.id_table = bias_i2c_id,
};

static DEFINE_MUTEX(bias_i2c_access);

/* I2C Function For Read/Write */
bool bias_read_byte(unsigned char cmd, unsigned char *returnData)
{
	char cmd_buf[2] = { 0x00, 0x00 };
	char readData = 0;
	int ret = 0;

	mutex_lock(&bias_i2c_access);

	cmd_buf[0] = cmd;
	ret = i2c_master_send(new_client, &cmd_buf[0], 1);
	ret = i2c_master_recv(new_client, &cmd_buf[1], 1);
	if (ret < 0) {
		mutex_unlock(&bias_i2c_access);
		return false;
	}

	readData = cmd_buf[1];
	*returnData = readData;

	mutex_unlock(&bias_i2c_access);

	return true;
}

bool bias_write_byte(unsigned char cmd, unsigned char writeData)
{
	char write_data[2] = { 0 };
	int ret = 0;

	pr_notice("[KE/BIAS] %s\n", __func__);

	mutex_lock(&bias_i2c_access);

	write_data[0] = cmd;
	write_data[1] = writeData;

	ret = i2c_master_send(new_client, write_data, 2);
	if (ret < 0) {
		mutex_unlock(&bias_i2c_access);
		pr_err("[BIAS] I2C write fail!!!\n");

		return false;
	}

	mutex_unlock(&bias_i2c_access);

	return true;
}

static int bias_driver_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int err = 0;

	pr_notice("[KE/BIAS] name=%s addr=0x%x\n",
		client->name, client->addr);
	new_client = devm_kzalloc(&client->dev,
		sizeof(struct i2c_client), GFP_KERNEL);
	if (!new_client) {
		err = -ENOMEM;
		goto exit;
	}
	memcpy(new_client, client, sizeof(struct i2c_client));

	return 0;
 exit:
	return err;
}

static int bias_driver_remove(struct i2c_client *client)
{
	pr_notice("[KE/BIAS] %s\n", __func__);

	new_client = NULL;
	i2c_unregister_device(client);

	return 0;
}

static int __init bias_init(void)
{
	pr_notice("[KE/BIAS] %s\n", __func__);

	if (i2c_add_driver(&bias_driver) != 0)
		pr_err("[KE/BIAS] failed to register bias i2c driver.\n");
	else
		pr_notice("[KE/BIAS] Success to register bias i2c driver.\n");

	return 0;
}

static void __exit bias_exit(void)
{
	i2c_del_driver(&bias_driver);
}

struct bias_setting_table bias_sy7758_lp8556_settings[] = {
	{.cmd = 0x00, .data = 0x14},//AVDD +6V
	{.cmd = 0x01, .data = 0x14},//AVEE -6V
};

struct bias_setting_table bias_tps65132_settings[] = {
	{.cmd = 0x00, .data = 0x14},//AVDD +6V
	{.cmd = 0x01, .data = 0x14},//AVEE -6V
	{.cmd = 0x02, .data = 0x82},//DLYN1 5MS, DLYP2 5MS
	{.cmd = 0x03, .data = 0x5B},//SEQU:Vpos->Vneg, SEQD:Vneg->Vpos
};

void bias_on(struct panel_info *info)
{
	bool ret = 0;
	unsigned int i = 0;
	unsigned char vendor = 0;

	ret = bias_read_byte(BIAS_READ_VENDOR, &vendor);
	if (ret == false)
		vendor = BIAS_IC_TPS65132;
	pr_notice("[KE/BIAS] bias_ic_check vendor = %d\n", vendor);

	switch(vendor) {
	case BIAS_IC_TPS65132:
		gpiod_set_value(info->avee, 1);
		usleep_range(5000, 5001);
		for (i = 0; i < ARRAY_SIZE(bias_tps65132_settings); i++) {
			ret = bias_write_byte(bias_tps65132_settings[i].cmd,
						bias_tps65132_settings[i].data);
			if (ret == false) {
				pr_err("bias_on fail\n");
				break;
			}
		}
		gpiod_set_value(info->avdd, 1);
		usleep_range(6000, 7000);
		break;
	default:
		for (i = 0; i < ARRAY_SIZE(bias_sy7758_lp8556_settings); i++) {
			ret = bias_write_byte(bias_sy7758_lp8556_settings[i].cmd,
						bias_sy7758_lp8556_settings[i].data);
			if (ret == false) {
				pr_err("bias_on fail\n");
				break;
			}
		}
		gpiod_set_value(info->avdd, 1);
		usleep_range(5000, 7000);
		gpiod_set_value(info->avee, 1);
		break;
	}
}
EXPORT_SYMBOL(bias_on);

module_init(bias_init);
module_exit(bias_exit);
MODULE_LICENSE("GPL v2");
