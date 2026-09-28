// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#include <linux/clk.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_clk.h>
#include <linux/printk.h>

#include "optee_private.h"

struct sub_domain {
	const char *name;
	int num_clks;
	struct clk_bulk_data *clks;
};

struct ccl_data {
	int num_domains;
	struct sub_domain *domains[];
};

static struct ccl_data *_ccl_data;

static struct ccl_data *get_ccl_data(void) { return _ccl_data ; }

int optee_clkctrl_init(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct device_node *node;
	struct ccl_data *data;
	int num_domain = 0, domain_ind = 0;
	int ret;

	for_each_child_of_node(np, node) {
		num_domain++;
	}

	data = devm_kzalloc(dev, struct_size(data, domains, num_domain), GFP_KERNEL);
	if (!data)
		return -ENOMEM;
	data->num_domains = num_domain;

	for_each_available_child_of_node(np, node) {
		struct sub_domain *domain;
		struct clk *clk;
		int i = 0;

		domain = devm_kzalloc(dev, sizeof(*domain), GFP_KERNEL);
		if (!domain)
			return -ENOMEM;

		domain->name = node->name;

		domain->num_clks = of_clk_get_parent_count(node);
		if (domain->num_clks > 0) {
			domain->clks = devm_kcalloc(dev, domain->num_clks,
						    sizeof(*domain->clks),
						    GFP_KERNEL);
			if (!domain->clks)
				return -ENOMEM;
		}

		for (i = 0; i < domain->num_clks; i++) {
			clk = of_clk_get(node, i);
			if (IS_ERR(clk)) {
				ret = PTR_ERR(clk);
				dev_err(dev, "%pOF: failed to get clk at index %d: %d\n",
					node, i, ret);
				goto err_put_clocks;
			}

			domain->clks[i].clk = clk;

		}

		data->domains[domain_ind++] = domain;
	}

	_ccl_data = data;

	return 0;

err_put_clocks:
	return ret;
}
EXPORT_SYMBOL_GPL(optee_clkctrl_init);

void handle_rpc_func_kree_clock_control(struct optee_msg_arg *arg)
{
	char cn[17];
	u64 *pn = (u64 *)cn;
	u32 en;
	struct ccl_data *data;
	int i;

	if (arg->num_params != 1)
		goto bad;
	if ((arg->params[0].attr & OPTEE_MSG_ATTR_TYPE_MASK) !=
			OPTEE_MSG_ATTR_TYPE_VALUE_INPUT)
		goto bad;

	arg->ret = TEEC_ERROR_NOT_SUPPORTED;

	cn[16] = '\0';
	*pn = arg->params[0].u.value.a;
	*(pn+1) = arg->params[0].u.value.b;
	en = arg->params[0].u.value.c;

	data = get_ccl_data();
	if (!data)
		goto bad;


	for (i = 0; i < data->num_domains; i++) {
		struct sub_domain *domain = data->domains[i];
		int ret;

		if (!strncmp(cn, domain->name, strlen(domain->name))) {
			if (en) {
				ret = clk_bulk_prepare_enable(domain->num_clks, domain->clks);
				if (ret)
					goto bad;
			} else
				clk_bulk_disable_unprepare(domain->num_clks, domain->clks);

			arg->ret = TEEC_SUCCESS;
			break;
		}
	}

	return;
bad:
	arg->ret = TEEC_ERROR_NOT_SUPPORTED;
}
EXPORT_SYMBOL_GPL(handle_rpc_func_kree_clock_control);
