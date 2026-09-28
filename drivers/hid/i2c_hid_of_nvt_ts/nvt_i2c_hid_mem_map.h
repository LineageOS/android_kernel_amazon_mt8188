/*
 * Copyright (C) 2022 Novatek, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#define NVT_ID_BYTE_MAX 6
#define PREFIX_PKT_LENGTH 5

typedef struct nvt_ts_reg {
	uint32_t addr; /* byte in which address */
	uint8_t mask; /* in which bits of that byte */
} nvt_ts_reg_t;

struct nvt_ts_mem_map {
	uint32_t CHIP_VER_TRIM_ADDR;
	uint32_t EVENT_BUF_ADDR;
	uint32_t EVENT_BUF_ST_ADDR;
	uint32_t CUS_ESD_CHECK_ADDR;
	uint32_t ESD_CHECK_ADDR;
	uint32_t RAW_PIPE0_ADDR;
	uint32_t RAW_PIPE1_ADDR;
	uint32_t BASELINE_ADDR;
	uint32_t DIFF_PIPE0_ADDR;
	uint32_t DIFF_PIPE1_ADDR;
	uint32_t PEN_2D_BL_TIP_X_ADDR;
	uint32_t PEN_2D_BL_TIP_Y_ADDR;
	uint32_t PEN_2D_BL_RING_X_ADDR;
	uint32_t PEN_2D_BL_RING_Y_ADDR;
	uint32_t PEN_2D_DIFF_TIP_X_ADDR;
	uint32_t PEN_2D_DIFF_TIP_Y_ADDR;
	uint32_t PEN_2D_DIFF_RING_X_ADDR;
	uint32_t PEN_2D_DIFF_RING_Y_ADDR;
	uint32_t PEN_2D_RAW_TIP_X_ADDR;
	uint32_t PEN_2D_RAW_TIP_Y_ADDR;
	uint32_t PEN_2D_RAW_RING_X_ADDR;
	uint32_t PEN_2D_RAW_RING_Y_ADDR;
	nvt_ts_reg_t ENB_CASC_REG;
	/* FW History */
	uint32_t MMAP_HISTORY_EVENT0;
	uint32_t MMAP_HISTORY_EVENT1;
	/* START for flash FW update */
	uint32_t READ_FLASH_CHECKSUM_ADDR;
	uint32_t RW_FLASH_DATA_ADDR;
	uint32_t Q_WR_CMD_ADDR;
	uint32_t GCM_CODE_ADDR;
	uint32_t FLASH_CMD_ADDR;
	uint32_t FLASH_CMD_ISSUE_ADDR;
	uint32_t FLASH_CKSUM_STATUS_ADDR;
	uint32_t GCM_FLAG_ADDR;
	uint32_t IDLE_INT_EN_ADDR;
	uint32_t SWRST_SIF_ADDR;
	uint32_t CRC_FLAG_ADDR;
	uint32_t BLD_SPE_PUPS_ADDR;
	uint32_t HID_I2C_ENG_ADDR;
	uint32_t HID_I2C_ENG_GET_RPT_LENGTH_ADDR;
	uint32_t SW_IDLE_ADDR;
	uint32_t SUSLO_ADDR;
	uint32_t CMD_REG_ADDR;
	nvt_ts_reg_t PP4IO_EN_REG;
	nvt_ts_reg_t BLD_RD_ADDR_SEL_REG;
	nvt_ts_reg_t BLD_RD_IO_SEL_REG;
	/* END for FW Update Use */
};

struct nvt_ts_flash_map {
	uint32_t flash_normal_fw_start_addr;
	uint32_t flash_pid_addr;
};

struct nvt_ts_bin_map {
	uint32_t bin_pid_addr;
};

static struct nvt_ts_mem_map NT36523N_memory_map = {
	.CHIP_VER_TRIM_ADDR = 0x3F004,
	.EVENT_BUF_ST_ADDR = 0x2FE00,
	.EVENT_BUF_ADDR = 0x2FD00,
	.CUS_ESD_CHECK_ADDR = 0x20854,
	.RAW_PIPE0_ADDR = 0x30FA0,
	.RAW_PIPE1_ADDR = 0x30FA0,
	.BASELINE_ADDR = 0x36510,
	.DIFF_PIPE0_ADDR = 0x373E8,
	.DIFF_PIPE1_ADDR = 0x38068,
	.PEN_2D_BL_TIP_X_ADDR = 0x2988A,
	.PEN_2D_BL_TIP_Y_ADDR = 0x29A1A,
	.PEN_2D_BL_RING_X_ADDR = 0x29BAA,
	.PEN_2D_BL_RING_Y_ADDR = 0x29D3A,
	.PEN_2D_DIFF_TIP_X_ADDR = 0x29ECA,
	.PEN_2D_DIFF_TIP_Y_ADDR = 0x2A05A,
	.PEN_2D_DIFF_RING_X_ADDR = 0x2A1EA,
	.PEN_2D_DIFF_RING_Y_ADDR = 0x2A37A,
	.PEN_2D_RAW_TIP_X_ADDR = 0x2A50A,
	.PEN_2D_RAW_TIP_Y_ADDR = 0x2A69A,
	.PEN_2D_RAW_RING_X_ADDR = 0x2A82A,
	.PEN_2D_RAW_RING_Y_ADDR = 0x2A9BA,
	/* FW History */
	.MMAP_HISTORY_EVENT0 = 0x38D54,
	.MMAP_HISTORY_EVENT1 = 0x38D94,
	/* START for flash FW update */
	.READ_FLASH_CHECKSUM_ADDR = 0x24000,
	.RW_FLASH_DATA_ADDR = 0x24002,
	.GCM_CODE_ADDR = 0x3F780,
	.FLASH_CMD_ADDR = 0x3F783,
	.FLASH_CMD_ISSUE_ADDR = 0x3F78E,
	.FLASH_CKSUM_STATUS_ADDR = 0x3F78F,
	.GCM_FLAG_ADDR = 0x3F793,
	.IDLE_INT_EN_ADDR = 0x3F078,
	.SWRST_SIF_ADDR = 0x3F0FE,
	.CRC_FLAG_ADDR = 0x3F133,
	.BLD_SPE_PUPS_ADDR = 0x3F135,
	.HID_I2C_ENG_ADDR = 0x3F4F8,
	.SW_IDLE_ADDR = 0x3F040,
	.SUSLO_ADDR = 0x3F041,
	.CMD_REG_ADDR = 0x3F4E4,
	/* END for flash FW update */
};

static const struct nvt_ts_flash_map NT36523N_flash_map = {
	.flash_normal_fw_start_addr = 0,
	.flash_pid_addr = 0x32004,
};

static const struct nvt_ts_bin_map NT36523N_bin_map = {
	.bin_pid_addr = 0x2D562,
};

struct nvt_ts_trim_id_table {
	uint8_t id[NVT_ID_BYTE_MAX];
	uint8_t mask[NVT_ID_BYTE_MAX];
	struct nvt_ts_mem_map *mmap;
	const struct nvt_ts_flash_map *fmap;
	const struct nvt_ts_bin_map *bmap;
	const uint8_t prefix_pkt_normal[PREFIX_PKT_LENGTH];
	const uint8_t prefix_pkt_recovery[PREFIX_PKT_LENGTH];
};

#define TRIM_TYPE_MAX 1
static const struct nvt_ts_trim_id_table trim_id_table[] = {
	{
		.id = { 0x17, 0xFF, 0xFF, 0x23, 0x65, 0x03 },
		.mask = { 1, 0, 0, 1, 1, 1 },
		.mmap = &NT36523N_memory_map,
		.fmap = &NT36523N_flash_map,
		.bmap = &NT36523N_bin_map,
		.prefix_pkt_normal = { 0xFF, 0xFF, 0x0B, 0x00, 0x00 },
		.prefix_pkt_recovery = { 0xFF, 0xFF, 0x0B, 0x00, 0x00 },
	},
};
