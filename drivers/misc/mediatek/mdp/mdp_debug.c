// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <../fs/proc/internal.h>
#include <linux/proc_fs.h>
#include <linux/dma-mapping.h>


#include "cmdq_helper_ext.h"
#include "mdp_debug.h"

#if IS_ENABLED(CONFIG_MTK_DEBUGFS_CREATOR)
#include "debugfs_creator.h"

static struct file_device record_file = {
	.device_name = "record",
	.type = 0,
	.folder_ptr = NULL,
	};
#endif

struct mdp_debug_info g_mdp_debug_info = {
	.fs_record = false,
	.debug = {0},
	.memset_input_buf = 0xFFFFFFFF,
	.enable_crc = false,
	.rdma_config_debug = false,
	.fg_debug = false,
	};

struct mdp_debug_info *mdp_debug_get_info(void)
{
	return &g_mdp_debug_info;
}

void mdp_debug_printf_record(const char *print_msg, ...)
{
#if IS_ENABLED(CONFIG_MTK_DEBUGFS_CREATOR)
	va_list args;

	va_start(args, print_msg);
	debugfs_creator_printf_args(&record_file, PRINT_TYPE_APPEND, print_msg, args);
	va_end(args);
#endif
}

#if IS_ENABLED(CONFIG_MTK_DEBUGFS_CREATOR)
int mdp_debug_enable_fs_record(int argc, char *argv[])
{
	int enable = 0;

	if (argc != 1) {
		CMDQ_ERR("invalid argc:%d\n", argc);
		return -1;
	}

	if (kstrtoint(argv[0], 0, &enable) != 0) {
		CMDQ_ERR("invalid argv[0]:%s", argv[0]);
		return -1;
	}

	mdp_debug_get_info()->fs_record = enable;
	CMDQ_LOG("%s fs_record\n", enable ? "enable" : "disable");

	if (enable)
		record_file.type |= FILE_TYPE_READ;
	else {
		record_file.type &= ~FILE_TYPE_READ;
		debugfd_creator_reset_file(&record_file);
	}

	return 0;
}

/* echo memset val > /proc/mdp/cmd
 * val: 8bit  0 - 256
 * echo memset -1 > /proc/mdp/cmd   // disable memset input buffer feature.
 */
int mdp_debug_memset_input_buf(int argc, char *argv[])
{
	int memset_val = 0;

	if (argc != 1) {
		CMDQ_ERR("invalid argc:%d\n", argc);
		return -1;
	}

	if (kstrtoint(argv[0], 0, &memset_val) != 0) {
		CMDQ_ERR("invalid argv[0]:%s", argv[0]);
		return -1;
	}

	mdp_debug_get_info()->memset_input_buf = memset_val;
	CMDQ_LOG("memset input buffer, val:[0x%08x]\n", memset_val);

	return 0;
}

int mdp_debug_enable_crc(int argc, char *argv[])
{
	int en_crc = 0;

	if (argc != 1) {
		CMDQ_ERR("invalid argc:%d\n", argc);
		return -1;
	}

	if (kstrtoint(argv[0], 0, &en_crc) != 0) {
		CMDQ_ERR("invalid argv[0]:%s", argv[0]);
		return -1;
	}

	mdp_debug_get_info()->enable_crc = en_crc;
	CMDQ_LOG("enable WROT crc:%s\n", en_crc ? "enable" : "disable");

	return 0;
}

int mdp_debug_enable_rdma_config_debug(int argc, char *argv[])
{
	int rdma_config_debug = 0;

	if (argc != 1) {
		CMDQ_ERR("invalid argc:%d\n", argc);
		return -1;
	}

	if (kstrtoint(argv[0], 0, &rdma_config_debug) != 0) {
		CMDQ_ERR("invalid argv[0]:%s", argv[0]);
		return -1;
	}

	mdp_debug_get_info()->rdma_config_debug = rdma_config_debug;
	CMDQ_LOG("%s RDMA config debug\n", rdma_config_debug ? "enable" : "disable");

	return 0;
}

int mdp_debug_enable_fg_debug(int argc, char *argv[])
{
	int fg_debug = 0;

	if (argc != 1) {
		CMDQ_ERR("invalid argc:%d\n", argc);
		return -1;
	}

	if (kstrtoint(argv[0], 0, &fg_debug) != 0) {
		CMDQ_ERR("invalid argv[0]:%s", argv[0]);
		return -1;
	}

	mdp_debug_get_info()->fg_debug = fg_debug;
	CMDQ_LOG("%s fg debug\n", fg_debug ? "enable" : "disable");

	return 0;
}


int mdp_debug_loglevel_set(int argc, char *argv[])
{
	int log_level = 0;

	if (argc != 1) {
		CMDQ_ERR("invalid argc:%d\n", argc);
		return -1;
	}

	if (kstrtoint(argv[0], 0, &log_level) != 0) {
		CMDQ_ERR("invalid argv[0]:%s", argv[0]);
		return -1;
	}

	if (log_level < 0 || log_level >= CMDQ_LOG_LEVEL_MAX)
		log_level = 0;
	CMDQ_ERR("enable log_level:%d\n", log_level);
	cmdq_core_set_log_level(log_level);

	return 0;
}


int mdp_debug_config_param(int argc, char *argv[])
{
	int debug = 0;
	int i = 0;

	if (argc <= 0 || argc >= ARRAY_SIZE(g_mdp_debug_info.debug)) {
		CMDQ_ERR("invalid argc:%d\n", argc);
		return -1;
	}

	for (i = 0; i < argc; i++) {
		if (kstrtoint(argv[i], 0, &debug) != 0) {
			CMDQ_ERR("invalid argv[%d]:%s\n", i, argv[i]);
			return -1;
		}
		g_mdp_debug_info.debug[i] = debug;
		CMDQ_ERR("set debug[%d]:%d\n", i, g_mdp_debug_info.debug[i]);
	}

	return 0;
}


static struct cmd_item items[] = {
	{"fs_record", mdp_debug_enable_fs_record},
	{"debug", mdp_debug_config_param},
	{"memset", mdp_debug_memset_input_buf},
	{"crc", mdp_debug_enable_crc},
	{"log", mdp_debug_loglevel_set},
	{"rdma_config", mdp_debug_enable_rdma_config_debug},
	{"fg_debug", mdp_debug_enable_fg_debug},
};

static struct file_device cmd_file = {
	.device_name = "cmd",
	.type = FILE_TYPE_READ | FILE_TYPE_WRITE,
	.folder_ptr = NULL,
	.write_op = {items, ARRAY_SIZE(items)},
	};
static struct proc_dir_entry *dir_ptr;
#endif

void mdp_debug_init(void)
{
#if IS_ENABLED(CONFIG_MTK_DEBUGFS_CREATOR)
	dir_ptr = proc_mkdir("mdp", NULL);

	cmd_file.folder_ptr = dir_ptr;
	record_file.folder_ptr = dir_ptr;

	debugfs_creator_create_file(&cmd_file);
	debugfs_creator_create_file(&record_file);
#endif
}

void mdp_debug_exit(void)
{
#if IS_ENABLED(CONFIG_MTK_DEBUGFS_CREATOR)
	debugfs_creator_destroy_file(&cmd_file);
	debugfs_creator_destroy_file(&record_file);
#endif
}
