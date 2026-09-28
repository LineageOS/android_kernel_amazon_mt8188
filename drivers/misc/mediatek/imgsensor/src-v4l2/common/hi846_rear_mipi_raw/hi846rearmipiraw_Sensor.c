// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2022 MediaTek Inc.

/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 hi846rearmipiraw_Sensor.c
 *
 * Project:
 * --------
 *	 ALPS
 *
 * Description:
 * ------------
 *	 Source code of Sensor driver
 *
 *
 *------------------------------------------------------------------------------
 * Upper this line, this part is controlled by CC/CQ. DO NOT MODIFY!!
 *============================================================================
 ****************************************************************************/

#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/atomic.h>
#include <linux/types.h>

#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define_v4l2.h"
#include "kd_imgsensor_errcode.h"

#include "hi846rearmipiraw_Sensor.h"
#include "hi846_rear_ana_gain_table.h"

#include "adaptor-subdrv.h"
#include "adaptor-i2c.h"

#define read_cmos_sensor_8(...) subdrv_i2c_rd_u8(__VA_ARGS__)
#define read_cmos_sensor(...) subdrv_i2c_rd_u16(__VA_ARGS__)
#define write_cmos_sensor_8(...) subdrv_i2c_wr_u8(__VA_ARGS__)
#define write_cmos_sensor(...) subdrv_i2c_wr_u16(__VA_ARGS__)
#define table_write_cmos_sensor(...) subdrv_i2c_wr_regs_u16(__VA_ARGS__)

#define PFX "HI846_rear_camera_sensor"
#define LOG_INF(format, args...) pr_err(PFX "[%s] " format, __func__, ##args)

#define FPT_PDAF_SUPPORT 0

static void sensor_init(struct subdrv_ctx *ctx);

static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = HI846_REAR_SENSOR_ID,
	.checksum_value = 0x55e2a82f,

	.pre = {
		.pclk = 288000000,				//record different mode's pclk
		.linelength  = 3800,				//record different mode's linelength
		.framelength = 2526,			/*record different mode's framelength*/
		.startx = 0,					//record different mode's startx of grabwindow
		.starty = 0,					//record different mode's starty of grabwindow
		.grabwindow_width  = 3264,   /*2592 record different mode's width of grabwindow*/
		.grabwindow_height = 2448,	 /*1944record different mode's height of grabwindow*/
		/*	 following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario	*/
		.mipi_data_lp2hs_settle_dc = 14,
		.mipi_pixel_rate = 288000000,
		.max_framerate = 300,
	},
	.cap = {
		.pclk = 288000000,				//record different mode's pclk
		.linelength  = 3800,				//record different mode's linelength
		.framelength = 2526,			/*record different mode's framelength*/
		.startx = 0,					//record different mode's startx of grabwindow
		.starty = 0,					//record different mode's starty of grabwindow
		.grabwindow_width  = 3264,   /*2592 record different mode's width of grabwindow*/
		.grabwindow_height = 2448,	 /*1944record different mode's height of grabwindow*/
		/*	 following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario	*/
		.mipi_data_lp2hs_settle_dc = 14,
		.mipi_pixel_rate = 288000000,
		.max_framerate = 300,
	},
	.normal_video = {
		.pclk = 288000000,				//record different mode's pclk
		.linelength  = 3800,				//record different mode's linelength
		.framelength = 2526,			/*record different mode's framelength*/
		.startx = 0,					//record different mode's startx of grabwindow
		.starty = 0,					//record different mode's starty of grabwindow
		.grabwindow_width  = 3264,   /*2592 record different mode's width of grabwindow*/
		.grabwindow_height = 2448,	 /*1944record different mode's height of grabwindow*/
		/*	 following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario	*/
		.mipi_data_lp2hs_settle_dc = 14,
		.mipi_pixel_rate = 288000000,
		.max_framerate = 300,
	},
	.hs_video = {
		.pclk = 288000000,				//record different mode's pclk
		.linelength  = 3800,				//record different mode's linelength
		.framelength = 2526,			/*record different mode's framelength*/
		.startx = 0,					//record different mode's startx of grabwindow
		.starty = 0,					//record different mode's starty of grabwindow
		.grabwindow_width  = 3264,   /*2592 record different mode's width of grabwindow*/
		.grabwindow_height = 2448,	 /*1944record different mode's height of grabwindow*/
		/*	 following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario	*/
		.mipi_data_lp2hs_settle_dc = 14,
		.mipi_pixel_rate = 288000000,
		.max_framerate = 300,
	},
	.slim_video = {
		.pclk = 288000000,				//record different mode's pclk
		.linelength  = 3800,				//record different mode's linelength
		.framelength = 2526,			/*record different mode's framelength*/
		.startx = 0,					//record different mode's startx of grabwindow
		.starty = 0,					//record different mode's starty of grabwindow
		.grabwindow_width  = 3264,   /*2592 record different mode's width of grabwindow*/
		.grabwindow_height = 2448,	 /*1944record different mode's height of grabwindow*/
		/*	 following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario	*/
		.mipi_data_lp2hs_settle_dc = 14,
		.mipi_pixel_rate = 288000000,
		.max_framerate = 300,
	},
	.custom1 = {
		.pclk = 288000000,				//record different mode's pclk
		.linelength  = 3800,				//record different mode's linelength
		.framelength = 2526,			/*record different mode's framelength*/
		.startx = 0,					//record different mode's startx of grabwindow
		.starty = 0,					//record different mode's starty of grabwindow
		.grabwindow_width  = 3264,   /*2592 record different mode's width of grabwindow*/
		.grabwindow_height = 2448,	 /*1944record different mode's height of grabwindow*/
		/*	 following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario	*/
		.mipi_data_lp2hs_settle_dc = 14,
		.mipi_pixel_rate = 288000000,
		.max_framerate = 300,
	},

	.margin = 6,
	.min_shutter = 6,
	.min_gain = BASEGAIN,
	.max_gain = 16*BASEGAIN,
	.min_gain_iso = 100,
	.gain_step = 4,
	.gain_type = 3,
	.max_frame_length = 0xffff,
	.ae_shut_delay_frame = 0,

	/* sensor gain delay frame for AE cycle,
	 * 2 frame with ispGain_delay-sensor_gain_delay=2-0=2
	 */
	.ae_sensor_gain_delay_frame = 0,

	.ae_ispGain_delay_frame = 2,	/* isp gain delay frame for AE cycle */
	.ihdr_support = 0,	/* 1, support; 0,not support */
	.ihdr_le_firstline = 0,	/* 1,le first ; 0, se first */
	.sensor_mode_num = 6,	/* support sensor mode num */

	.cap_delay_frame = 3,	/* enter capture delay frame num */
	.pre_delay_frame = 3,	/* enter preview delay frame num */
	.video_delay_frame = 3,	/* enter video delay frame num */

	/* enter high speed video  delay frame num */
	.hs_video_delay_frame = 3,

	.slim_video_delay_frame = 3,	/* enter slim video delay frame num */
	.custom1_delay_frame = 3,	/* enter slim video delay frame num */

	.isp_driving_current = ISP_DRIVING_6MA,	/* mclk driving current */

	/* sensor_interface_type */
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,

	/* 0,MIPI_OPHY_NCSI2;  1,MIPI_OPHY_CSI2 */
	.mipi_sensor_type = MIPI_OPHY_NCSI2,

	/* 0,MIPI_SETTLEDELAY_AUTO; 1,MIPI_SETTLEDELAY_MANNUAL */
	.mipi_settle_delay_mode = 1,
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_Gb,
	.mclk = 24,	/* mclk value, suggest 24 or 26 for 24Mhz or 26Mhz */
	.mipi_lane_num = SENSOR_MIPI_4_LANE,
	.i2c_speed = 400, /*support 1MHz write*/
	/* record sensor support all write id addr,
	 * only supprt 4 must end with 0xff
	 */
	.i2c_addr_table = {0x40,  0xff},
};


/* Sensor output window information */
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[] = {
	{ 3280, 2464,   8,   8,   3264, 2448,   3264, 2448,   0, 0, 3264, 2448,   0, 0, 3264, 2448}, // Preview
	{ 3280, 2464,   8,   8,   3264, 2448,   3264, 2448,   0, 0, 3264, 2448,   0, 0, 3264, 2448}, // capture
	{ 3280, 2464,   8,   8,   3264, 2448,   3264, 2448,   0, 0, 3264, 2448,   0, 0, 3264, 2448}, // video
	{ 3280, 2464,   8,   8,   3264, 2448,   3264, 2448,   0, 0, 3264, 2448,   0, 0, 3264, 2448},  //high speed video
	{ 3280, 2464,   8,   8,   3264, 2448,   3264, 2448,   0, 0, 3264, 2448,   0, 0, 3264, 2448}, // slim video
	{ 3280, 2464,   8,   8,   3264, 2448,   3264, 2448,   0, 0, 3264, 2448,   0, 0, 3264, 2448}, // custom1
};

static void set_dummy(struct subdrv_ctx *ctx)
{
	/* return; //for test */
	write_cmos_sensor(ctx, 0x0006, ctx->frame_length & 0xFFFF );
	write_cmos_sensor(ctx, 0x0008, ctx->line_length & 0xFFFF );
}

static void set_max_framerate(struct subdrv_ctx *ctx, UINT16 framerate, kal_bool min_framelength_en)
{
	kal_uint32 frame_length = ctx->frame_length;

	frame_length = ctx->pclk / framerate * 10 / ctx->line_length;
	if (frame_length >= ctx->min_frame_length)
		ctx->frame_length = frame_length;
	else
		ctx->frame_length = ctx->min_frame_length;

	ctx->dummy_line =
		ctx->frame_length - ctx->min_frame_length;

	if (ctx->frame_length > imgsensor_info.max_frame_length) {
		ctx->frame_length = imgsensor_info.max_frame_length;

		ctx->dummy_line =
			ctx->frame_length - ctx->min_frame_length;
	}
	if (min_framelength_en)
		ctx->min_frame_length = ctx->frame_length;
	set_dummy(ctx);
}

static void write_shutter(struct subdrv_ctx *ctx, kal_uint16 shutter)
{
	kal_uint16 realtime_fps = 0;
	//kal_uint32 frame_length = 0;

	/* 0x3500, 0x3501, 0x3502 will increase VBLANK to get exposure larger than frame exposure */
	/* AE doesn't update sensor gain at capture mode, thus extra exposure lines must be updated here. */

	// OV Recommend Solution
	// if shutter bigger than frame_length, should extend frame length first
	//printk("pangfei shutter %d line %d\n",shutter,__LINE__);

	if (shutter > ctx->min_frame_length - imgsensor_info.margin)
		ctx->frame_length = shutter + imgsensor_info.margin;
	else
		ctx->frame_length = ctx->min_frame_length;
	if (ctx->frame_length > imgsensor_info.max_frame_length)
		ctx->frame_length = imgsensor_info.max_frame_length;

	shutter = (shutter < imgsensor_info.min_shutter) ? imgsensor_info.min_shutter : shutter;
	shutter = (shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin)) ? (imgsensor_info.max_frame_length - imgsensor_info.margin) : shutter;
	if (ctx->autoflicker_en) {
		realtime_fps = ctx->pclk * 10 / (ctx->line_length * ctx->frame_length);
		if(realtime_fps >= 297 && realtime_fps <= 305)
      set_max_framerate(ctx, 296,0);
		else if(realtime_fps >= 147 && realtime_fps <= 150)
      set_max_framerate(ctx, 146,0);
		else{
      write_cmos_sensor(ctx, 0x0006, ctx->frame_length);
		}
	}
	else{
	// Extend frame length
    write_cmos_sensor(ctx, 0x0006, ctx->frame_length);
  }

	// Update Shutter
	write_cmos_sensor(ctx, 0x0074, shutter & 0xFFFF);
}



/*************************************************************************
 * FUNCTION
 *	set_shutter
 *
 * DESCRIPTION
 *	This function set e-shutter of sensor to change exposure time.
 *
 * PARAMETERS
 *	iShutter : exposured lines
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static void set_shutter(struct subdrv_ctx *ctx, kal_uint16 shutter)
{
	ctx->shutter = shutter;

	write_shutter(ctx, shutter);
}

static kal_uint16 gain2reg(struct subdrv_ctx *ctx, const kal_uint32 gain)
{
	kal_uint16 reg_gain = 0x0;
	reg_gain =  gain *16 /BASEGAIN -  1 * 16;
	return (kal_uint16) reg_gain;
}

/*************************************************************************
 * FUNCTION
 *	set_gain
 *
 * DESCRIPTION
 *	This function is to set global gain to sensor.
 *
 * PARAMETERS
 *	iGain : sensor global gain(base: 0x40)
 *
 * RETURNS
 *	the actually gain set to sensor.
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 set_gain(struct subdrv_ctx *ctx, kal_uint32 gain)
{
	kal_uint16 reg_gain;

	/* 0x350A[0:1], 0x350B[0:7] AGC real gain */
	/* [0:3] = N meams N /16 X    */
	/* [4:9] = M meams M X         */
	/* Total gain = M + N /16 X   */

	if (gain < BASEGAIN || gain > 16 * BASEGAIN) {
		LOG_INF("Error gain setting : %d\n",gain);

		if (gain < BASEGAIN)
			gain = BASEGAIN;
		else if (gain > 16 * BASEGAIN)
			gain = 16 * BASEGAIN;
	}

	reg_gain = gain2reg(ctx, gain);
	ctx->gain = reg_gain;

	write_cmos_sensor_8(ctx, 0x0077,reg_gain&0xff);   /* max = 0xf0*/

	return gain;
}

static void set_mirror_flip(struct subdrv_ctx *ctx, kal_uint8 image_mirror)
{
	LOG_INF("image_mirror = %d\n", image_mirror);
   switch (image_mirror) {

        case IMAGE_NORMAL:
            write_cmos_sensor(ctx, 0x000e,0x0000);   /*B*/
            break;

        case IMAGE_H_MIRROR:
            write_cmos_sensor(ctx, 0x000e,0x0100);   /*Gb*/
            break;

        case IMAGE_V_MIRROR:
            write_cmos_sensor(ctx, 0x000e,0x0200);   /*Gr*/
            break;

        case IMAGE_HV_MIRROR:
            write_cmos_sensor(ctx, 0x000e,0x0300);   /*R*/
            break;
        default:
        LOG_INF("Error image_mirror setting\n");
    }
}


static kal_uint32 streaming_control(struct subdrv_ctx *ctx, kal_bool enable)
{
	LOG_INF("streaming_enable(0=Sw Standby,1=streaming): %d\n", enable);

	if (enable) {
		write_cmos_sensor(ctx, 0x0a00, 0x0100);
	} else {
		write_cmos_sensor(ctx, 0x0a00, 0x0000);
	}
	mdelay(5);
	return ERROR_NONE;
}

static kal_uint16 addr_data_pair_init[] = {
0x0066, 0x0101,
0x2000, 0x98E8,
0x2002, 0x00FF,
0x2004, 0x0006,
0x2008, 0x3FFF,
0x200A, 0xC314,
0x2022, 0x4130,
0x2034, 0x1292,
0x2036, 0xC02E,
0x2038, 0x4130,
0x206E, 0xF0B2,
0x2070, 0xFFBF,
0x2072, 0x2004,
0x2074, 0x43C2,
0x2076, 0x82FA,
0x2078, 0x12B0,
0x207A, 0xCAB0,
0x207C, 0x42A2,
0x207E, 0x7324,
0x2080, 0x4130,
0x2082, 0x120B,
0x2084, 0x425B,
0x2086, 0x008C,
0x2088, 0x4292,
0x208A, 0x7300,
0x208C, 0x82F2,
0x208E, 0x4292,
0x2090, 0x7302,
0x2092, 0x82F4,
0x2094, 0x1292,
0x2096, 0xC006,
0x2098, 0x421F,
0x209A, 0x0710,
0x209C, 0x523F,
0x209E, 0x4F82,
0x20A0, 0x82E4,
0x20A2, 0x93C2,
0x20A4, 0x829F,
0x20A6, 0x241E,
0x20A8, 0x403E,
0x20AA, 0xFFFE,
0x20AC, 0x40B2,
0x20AE, 0xEC78,
0x20B0, 0x82EC,
0x20B2, 0x40B2,
0x20B4, 0xEC78,
0x20B6, 0x82EE,
0x20B8, 0x40B2,
0x20BA, 0xEC78,
0x20BC, 0x82F0,
0x20BE, 0x934B,
0x20C0, 0x2405,
0x20C2, 0x4E0F,
0x20C4, 0x503F,
0x20C6, 0xFFD8,
0x20C8, 0x4F82,
0x20CA, 0x82EC,
0x20CC, 0x907B,
0x20CE, 0x0003,
0x20D0, 0x200B,
0x20D2, 0x421F,
0x20D4, 0x82EC,
0x20D6, 0x5E0F,
0x20D8, 0x4F82,
0x20DA, 0x82EE,
0x20DC, 0x5E0F,
0x20DE, 0x4F82,
0x20E0, 0x82F0,
0x20E2, 0x3C02,
0x20E4, 0x432E,
0x20E6, 0x3FE2,
0x20E8, 0x413B,
0x20EA, 0x4130,
0x20EC, 0x421F,
0x20EE, 0x7100,
0x20F0, 0x4F0E,
0x20F2, 0x503E,
0x20F4, 0xFFD8,
0x20F6, 0x4E82,
0x20F8, 0x7A04,
0x20FA, 0x421E,
0x20FC, 0x82EC,
0x20FE, 0x5F0E,
0x2100, 0x4E82,
0x2102, 0x7A06,
0x2104, 0x0B00,
0x2106, 0x7304,
0x2108, 0x0050,
0x210A, 0x40B2,
0x210C, 0xD081,
0x210E, 0x0B88,
0x2110, 0x421E,
0x2112, 0x82EE,
0x2114, 0x5F0E,
0x2116, 0x4E82,
0x2118, 0x7A0E,
0x211A, 0x521F,
0x211C, 0x82F0,
0x211E, 0x4F82,
0x2120, 0x7A10,
0x2122, 0x0B00,
0x2124, 0x7304,
0x2126, 0x007A,
0x2128, 0x40B2,
0x212A, 0x0081,
0x212C, 0x0B88,
0x212E, 0x4392,
0x2130, 0x7A0A,
0x2132, 0x0800,
0x2134, 0x7A0C,
0x2136, 0x0B00,
0x2138, 0x7304,
0x213A, 0x022B,
0x213C, 0x40B2,
0x213E, 0xD081,
0x2140, 0x0B88,
0x2142, 0x0B00,
0x2144, 0x7304,
0x2146, 0x0255,
0x2148, 0x40B2,
0x214A, 0x0081,
0x214C, 0x0B88,
0x214E, 0x9382,
0x2150, 0x7112,
0x2152, 0x2402,
0x2154, 0x4392,
0x2156, 0x760E,
0x2158, 0x4130,
0x215A, 0x120B,
0x215C, 0x120A,
0x215E, 0x4E0A,
0x2160, 0x4F0B,
0x2162, 0x4C0E,
0x2164, 0x4D0F,
0x2166, 0x8A0E,
0x2168, 0x7B0F,
0x216A, 0x2C02,
0x216C, 0x4A0C,
0x216E, 0x4B0D,
0x2170, 0x4C0E,
0x2172, 0x4D0F,
0x2174, 0x413A,
0x2176, 0x413B,
0x2178, 0x4130,
0x217A, 0x120B,
0x217C, 0x120A,
0x217E, 0x1209,
0x2180, 0x1208,
0x2182, 0x1207,
0x2184, 0x1206,
0x2186, 0x1205,
0x2188, 0x42D2,
0x218A, 0x82FA,
0x218C, 0x82A0,
0x218E, 0x403B,
0x2190, 0x00C1,
0x2192, 0x4B6F,
0x2194, 0x4FC2,
0x2196, 0x82D4,
0x2198, 0x43C2,
0x219A, 0x82D5,
0x219C, 0x1292,
0x219E, 0xC046,
0x21A0, 0x4292,
0x21A2, 0x7560,
0x21A4, 0x82F6,
0x21A6, 0x4292,
0x21A8, 0x7562,
0x21AA, 0x82F8,
0x21AC, 0x93CB,
0x21AE, 0x0000,
0x21B0, 0x2452,
0x21B2, 0x4215,
0x21B4, 0x7316,
0x21B6, 0x4216,
0x21B8, 0x7318,
0x21BA, 0x421F,
0x21BC, 0x0710,
0x21BE, 0x4F0E,
0x21C0, 0x430F,
0x21C2, 0x4507,
0x21C4, 0x4608,
0x21C6, 0x8E07,
0x21C8, 0x7F08,
0x21CA, 0x421F,
0x21CC, 0x82E2,
0x21CE, 0x522F,
0x21D0, 0x4F09,
0x21D2, 0x430A,
0x21D4, 0x470D,
0x21D6, 0x480E,
0x21D8, 0x490B,
0x21DA, 0x4A0C,
0x21DC, 0x870B,
0x21DE, 0x780C,
0x21E0, 0x2C02,
0x21E2, 0x490D,
0x21E4, 0x4A0E,
0x21E6, 0x4D0F,
0x21E8, 0x43D2,
0x21EA, 0x01B3,
0x21EC, 0x4D82,
0x21EE, 0x7324,
0x21F0, 0x4292,
0x21F2, 0x7540,
0x21F4, 0x82E8,
0x21F6, 0x4292,
0x21F8, 0x7542,
0x21FA, 0x82EA,
0x21FC, 0x434B,
0x21FE, 0x823F,
0x2200, 0x4F0C,
0x2202, 0x430D,
0x2204, 0x421E,
0x2206, 0x82E8,
0x2208, 0x421F,
0x220A, 0x82EA,
0x220C, 0x5E0C,
0x220E, 0x6F0D,
0x2210, 0x870C,
0x2212, 0x780D,
0x2214, 0x2801,
0x2216, 0x435B,
0x2218, 0x4BC2,
0x221A, 0x82FA,
0x221C, 0x93C2,
0x221E, 0x829A,
0x2220, 0x201A,
0x2222, 0x93C2,
0x2224, 0x82A0,
0x2226, 0x2404,
0x2228, 0x43B2,
0x222A, 0x7540,
0x222C, 0x43B2,
0x222E, 0x7542,
0x2230, 0x93C2,
0x2232, 0x82FA,
0x2234, 0x2410,
0x2236, 0x503E,
0x2238, 0x0003,
0x223A, 0x630F,
0x223C, 0x4E82,
0x223E, 0x82E8,
0x2240, 0x4F82,
0x2242, 0x82EA,
0x2244, 0x450C,
0x2246, 0x460D,
0x2248, 0x8E0C,
0x224A, 0x7F0D,
0x224C, 0x2C04,
0x224E, 0x4582,
0x2250, 0x82E8,
0x2252, 0x4682,
0x2254, 0x82EA,
0x2256, 0x4135,
0x2258, 0x4136,
0x225A, 0x4137,
0x225C, 0x4138,
0x225E, 0x4139,
0x2260, 0x413A,
0x2262, 0x413B,
0x2264, 0x4130,
0x2266, 0x403E,
0x2268, 0x00C2,
0x226A, 0x421F,
0x226C, 0x7314,
0x226E, 0xF07F,
0x2270, 0x000C,
0x2272, 0x5F4F,
0x2274, 0x5F4F,
0x2276, 0xDFCE,
0x2278, 0x0000,
0x227A, 0xF0FE,
0x227C, 0x000F,
0x227E, 0x0000,
0x2280, 0x4130,
0x2282, 0x120B,
0x2284, 0x120A,
0x2286, 0x1209,
0x2288, 0x1208,
0x228A, 0x1207,
0x228C, 0x1206,
0x228E, 0x93C2,
0x2290, 0x00C1,
0x2292, 0x249F,
0x2294, 0x425E,
0x2296, 0x00C2,
0x2298, 0xC35E,
0x229A, 0x425F,
0x229C, 0x82A0,
0x229E, 0xDF4E,
0x22A0, 0x4EC2,
0x22A2, 0x00C2,
0x22A4, 0x934F,
0x22A6, 0x248F,
0x22A8, 0x4217,
0x22AA, 0x7316,
0x22AC, 0x4218,
0x22AE, 0x7318,
0x22B0, 0x4326,
0x22B2, 0xB3E2,
0x22B4, 0x00C2,
0x22B6, 0x2482,
0x22B8, 0x0900,
0x22BA, 0x731C,
0x22BC, 0x0800,
0x22BE, 0x731C,
0x22C0, 0x421A,
0x22C2, 0x7300,
0x22C4, 0x421B,
0x22C6, 0x7302,
0x22C8, 0x421F,
0x22CA, 0x7304,
0x22CC, 0x9F82,
0x22CE, 0x829C,
0x22D0, 0x2C02,
0x22D2, 0x531A,
0x22D4, 0x630B,
0x22D6, 0x4A0E,
0x22D8, 0x4B0F,
0x22DA, 0x821E,
0x22DC, 0x82F2,
0x22DE, 0x721F,
0x22E0, 0x82F4,
0x22E2, 0x2C68,
0x22E4, 0x4A09,
0x22E6, 0x9339,
0x22E8, 0x3460,
0x22EA, 0x0B00,
0x22EC, 0x7304,
0x22EE, 0x0320,
0x22F0, 0x421E,
0x22F2, 0x7300,
0x22F4, 0x421F,
0x22F6, 0x7302,
0x22F8, 0x531E,
0x22FA, 0x630F,
0x22FC, 0x4E0C,
0x22FE, 0x4F0D,
0x2300, 0x821C,
0x2302, 0x82F6,
0x2304, 0x721D,
0x2306, 0x82F8,
0x2308, 0x2C0E,
0x230A, 0x93B2,
0x230C, 0x7560,
0x230E, 0x2003,
0x2310, 0x93B2,
0x2312, 0x7562,
0x2314, 0x2408,
0x2316, 0x4E82,
0x2318, 0x7540,
0x231A, 0x4F82,
0x231C, 0x7542,
0x231E, 0x4E82,
0x2320, 0x82F6,
0x2322, 0x4F82,
0x2324, 0x82F8,
0x2326, 0x4E82,
0x2328, 0x7316,
0x232A, 0x12B0,
0x232C, 0xFE66,
0x232E, 0x0900,
0x2330, 0x730E,
0x2332, 0x403F,
0x2334, 0x7316,
0x2336, 0x4A09,
0x2338, 0x8F29,
0x233A, 0x478F,
0x233C, 0x0000,
0x233E, 0x460C,
0x2340, 0x430D,
0x2342, 0x421E,
0x2344, 0x7300,
0x2346, 0x421F,
0x2348, 0x7302,
0x234A, 0x9C0E,
0x234C, 0x23F8,
0x234E, 0x9D0F,
0x2350, 0x23F6,
0x2352, 0x0B00,
0x2354, 0x7304,
0x2356, 0x01F4,
0x2358, 0x5036,
0x235A, 0x0006,
0x235C, 0x460C,
0x235E, 0x430D,
0x2360, 0x490E,
0x2362, 0x4E0F,
0x2364, 0x5F0F,
0x2366, 0x7F0F,
0x2368, 0xE33F,
0x236A, 0x521E,
0x236C, 0x82E8,
0x236E, 0x621F,
0x2370, 0x82EA,
0x2372, 0x12B0,
0x2374, 0xFD5A,
0x2376, 0x4E82,
0x2378, 0x7540,
0x237A, 0x4F82,
0x237C, 0x7542,
0x237E, 0x403B,
0x2380, 0x7316,
0x2382, 0x421C,
0x2384, 0x82E4,
0x2386, 0x430D,
0x2388, 0x4B2F,
0x238A, 0x590F,
0x238C, 0x4F0E,
0x238E, 0x430F,
0x2390, 0x12B0,
0x2392, 0xFD5A,
0x2394, 0x4E8B,
0x2396, 0x0000,
0x2398, 0x4BA2,
0x239A, 0x82CE,
0x239C, 0x4382,
0x239E, 0x82D0,
0x23A0, 0x12B0,
0x23A2, 0xFE66,
0x23A4, 0xD3D2,
0x23A6, 0x00C2,
0x23A8, 0x3C16,
0x23AA, 0x9329,
0x23AC, 0x3BC8,
0x23AE, 0x4906,
0x23B0, 0x5326,
0x23B2, 0x3FC5,
0x23B4, 0x4A09,
0x23B6, 0x8219,
0x23B8, 0x82CE,
0x23BA, 0x3F95,
0x23BC, 0x0800,
0x23BE, 0x731C,
0x23C0, 0x0900,
0x23C2, 0x731C,
0x23C4, 0x3F7D,
0x23C6, 0x0900,
0x23C8, 0x730C,
0x23CA, 0x0B00,
0x23CC, 0x7304,
0x23CE, 0x01F4,
0x23D0, 0x3FE9,
0x23D2, 0x0900,
0x23D4, 0x732C,
0x23D6, 0x425F,
0x23D8, 0x0788,
0x23DA, 0x4136,
0x23DC, 0x4137,
0x23DE, 0x4138,
0x23E0, 0x4139,
0x23E2, 0x413A,
0x23E4, 0x413B,
0x23E6, 0x4130,
0x23FE, 0xC056,
0x3236, 0xFC22,
0x323A, 0xFCEC,
0x323C, 0xFC82,
0x323E, 0xFD7A,
0x3246, 0xFE82,
0x3248, 0xFC34,
0x324E, 0xFC6E,
0x326A, 0xC374,
0x326C, 0xC37C,
0x326E, 0x0000,
0x3270, 0xC378,
0x32E2, 0x0020,
0x0A00, 0x0000,
0x0E04, 0x0012,
0x002E, 0x1111,
0x0032, 0x1111,
0x0022, 0x0008,
0x0026, 0x0040,
0x0028, 0x0017,
0x002C, 0x09CF,
0x005C, 0x2101,
0x0006, 0x09DE,
0x0008, 0x0ED8,
0x000E, 0x0100,
0x000C, 0x0022,
0x0A22, 0x0000,
0x0A24, 0x0000,
0x0804, 0x0000,
0x0A12, 0x0CC0,
0x0A14, 0x0990,
0x0710, 0x09B0,
0x0074, 0x09D8,
0x0076, 0x0000,
0x051E, 0x0000,
0x0200, 0x0400,
0x0A1A, 0x0C00,
0x0A0C, 0x0010,
0x0A1E, 0x0CCF,
0x0402, 0x0110,
0x0404, 0x00F4,
0x0408, 0x0000,
0x0410, 0x008D,
0x0412, 0x011A,
0x0414, 0x864C,
0x021C, 0x0001,
0x0C00, 0x9950,
0x0C06, 0x0021,
0x0C10, 0x0040,
0x0C12, 0x0040,
0x0C14, 0x0040,
0x0C16, 0x0040,
0x0A02, 0x0100,
0x0A04, 0x014A,
0x0418, 0x0000,
0x0128, 0x0028,
0x012A, 0xFFFF,
0x0120, 0x0046,
0x0122, 0x0376,
0x012C, 0x0020,
0x012E, 0xFFFF,
0x0124, 0x0040,
0x0126, 0x0378,
0x0746, 0x0050,
0x0748, 0x01D5,
0x074A, 0x022B,
0x074C, 0x03B0,
0x0756, 0x043F,
0x0758, 0x3F1D,
0x0B02, 0xE04D,
0x0B10, 0x6821,
0x0B12, 0x0020,
0x0B14, 0x0001,
0x2008, 0x38FD,
0x326E, 0x0000,
0x0900, 0x0300,
0x0902, 0xC319,
0x0914, 0xC109,
0x0916, 0x061A,
0x0918, 0x0407,
0x091A, 0x0A0B,
0x091C, 0x0E08,
0x091E, 0x0A00,
0x090C, 0x0427,
0x090E, 0x0059,
0x0954, 0x0089,
0x0956, 0x0000,
0x0958, 0xA880,
0x095A, 0x9244,
0x0040, 0x0200,
0x0042, 0x0100,
0x0D04, 0x0000,
0x0F08, 0x2F04,
0x0F30, 0x001F,
0x0F36, 0x001F,
0x0F04, 0x3A00,
0x0F32, 0x025A,
0x0F38, 0x025A,
0x0F2A, 0x4124,
0x006A, 0x0100,
0x004C, 0x0100,
0x0044, 0x0001,

};

static kal_uint16 addr_data_pair_preview[] = {
//Sensor Information////////////////////////////
//Sensor	  : Hi-846
//Customer        : MTK
//Image size      : 3264x2448
//MCLK/PCLK       : 24MHz /288Mhz
//MIPI speed(Mbps): 720Mbps x 4Lane
//Frame Length    : 2526
//Line Length     : 3800
//Max Fps         : 30.00fps(vblank>1.0ms)
//Pixel order     : Green 1st (=GB)
//X/Y-flip        : X-flip
//Firmware Ver.   : v10.5.1
////////////////////////////////////////////////
0x002E, 0x1111,
0x0032, 0x1111,
0x0026, 0x0040,
0x002C, 0x09CF,
0x005C, 0x2101,
0x0006, 0x09DE,
0x0008, 0x0ED8,
0x000C, 0x0022,
0x0A22, 0x0000,
0x0A24, 0x0000,
0x0804, 0x0000,
0x0A12, 0x0CC0,
0x0A14, 0x0990,
0x0074, 0x09D8,
0x021C, 0x0001,
0x0A04, 0x014A,
0x0418, 0x0000,
0x0128, 0x0028,
0x012A, 0xFFFF,
0x0120, 0x0046,
0x0122, 0x0376,
0x012C, 0x0020,
0x012E, 0xFFFF,
0x0124, 0x0040,
0x0126, 0x0378,
0x0B02, 0xE04D,
0x0B10, 0x6821,
0x0B12, 0x0020,
0x0B14, 0x0001,
0x2008, 0x38FD,
0x326E, 0x0000,
0x0710, 0x09B0,
0x0900, 0x0300,
0x0902, 0xC319,
0x0914, 0xC109,
0x0916, 0x061A,
0x0918, 0x0407,
0x091A, 0x0A0B,
0x091C, 0x0E08,
0x091E, 0x0A00,
0x090C, 0x0427,
0x090E, 0x0059,
0x0954, 0x0089,
0x0956, 0x0000,
0x0958, 0xA880,
0x095A, 0x9244,
0x0F32, 0x025A,
0x0F38, 0x025A,
0x0F2A, 0x4124,
0x004C, 0x0100,
};

static kal_uint16 addr_data_pair_capture[] = {
//Sensor Information////////////////////////////
//Sensor	  : Hi-846
//Customer        : MTK
//Image size      : 3264x2448
//MCLK/PCLK       : 24MHz /288Mhz
//MIPI speed(Mbps): 720Mbps x 4Lane
//Frame Length    : 2526
//Line Length     : 3800
//Max Fps         : 30.00fps(vblank>1.0ms)
//Pixel order     : Green 1st (=GB)
//X/Y-flip        : X-flip
//Firmware Ver.   : v10.5.1
////////////////////////////////////////////////
0x002E, 0x1111,
0x0032, 0x1111,
0x0026, 0x0040,
0x002C, 0x09CF,
0x005C, 0x2101,
0x0006, 0x09DE,
0x0008, 0x0ED8,
0x000C, 0x0022,
0x0A22, 0x0000,
0x0A24, 0x0000,
0x0804, 0x0000,
0x0A12, 0x0CC0,
0x0A14, 0x0990,
0x0074, 0x09D8,
0x021C, 0x0001,
0x0A04, 0x014A,
0x0418, 0x0000,
0x0128, 0x0028,
0x012A, 0xFFFF,
0x0120, 0x0046,
0x0122, 0x0376,
0x012C, 0x0020,
0x012E, 0xFFFF,
0x0124, 0x0040,
0x0126, 0x0378,
0x0B02, 0xE04D,
0x0B10, 0x6821,
0x0B12, 0x0020,
0x0B14, 0x0001,
0x2008, 0x38FD,
0x326E, 0x0000,
0x0710, 0x09B0,
0x0900, 0x0300,
0x0902, 0xC319,
0x0914, 0xC109,
0x0916, 0x061A,
0x0918, 0x0407,
0x091A, 0x0A0B,
0x091C, 0x0E08,
0x091E, 0x0A00,
0x090C, 0x0427,
0x090E, 0x0059,
0x0954, 0x0089,
0x0956, 0x0000,
0x0958, 0xA880,
0x095A, 0x9244,
0x0F32, 0x025A,
0x0F38, 0x025A,
0x0F2A, 0x4124,
0x004C, 0x0100,
};

static kal_uint16 addr_data_pair_normal_video[] = {
//Sensor Information////////////////////////////
//Sensor	  : Hi-846
//Customer        : MTK
//Image size      : 3264x2448
//MCLK/PCLK       : 24MHz /288Mhz
//MIPI speed(Mbps): 720Mbps x 4Lane
//Frame Length    : 2526
//Line Length     : 3800
//Max Fps         : 30.00fps(vblank>1.0ms)
//Pixel order     : Green 1st (=GB)
//X/Y-flip        : X-flip
//Firmware Ver.   : v10.5.1
////////////////////////////////////////////////
0x002E, 0x1111,
0x0032, 0x1111,
0x0026, 0x0040,
0x002C, 0x09CF,
0x005C, 0x2101,
0x0006, 0x09DE,
0x0008, 0x0ED8,
0x000C, 0x0022,
0x0A22, 0x0000,
0x0A24, 0x0000,
0x0804, 0x0000,
0x0A12, 0x0CC0,
0x0A14, 0x0990,
0x0074, 0x09D8,
0x021C, 0x0001,
0x0A04, 0x014A,
0x0418, 0x0000,
0x0128, 0x0028,
0x012A, 0xFFFF,
0x0120, 0x0046,
0x0122, 0x0376,
0x012C, 0x0020,
0x012E, 0xFFFF,
0x0124, 0x0040,
0x0126, 0x0378,
0x0B02, 0xE04D,
0x0B10, 0x6821,
0x0B12, 0x0020,
0x0B14, 0x0001,
0x2008, 0x38FD,
0x326E, 0x0000,
0x0710, 0x09B0,
0x0900, 0x0300,
0x0902, 0xC319,
0x0914, 0xC109,
0x0916, 0x061A,
0x0918, 0x0407,
0x091A, 0x0A0B,
0x091C, 0x0E08,
0x091E, 0x0A00,
0x090C, 0x0427,
0x090E, 0x0059,
0x0954, 0x0089,
0x0956, 0x0000,
0x0958, 0xA880,
0x095A, 0x9244,
0x0F32, 0x025A,
0x0F38, 0x025A,
0x0F2A, 0x4124,
0x004C, 0x0100,
};

static kal_uint16 addr_data_pair_hs_video[] = {
//Sensor Information////////////////////////////
//Sensor	  : Hi-846
//Customer        : MTK
//Image size      : 3264x2448
//MCLK/PCLK       : 24MHz /288Mhz
//MIPI speed(Mbps): 720Mbps x 4Lane
//Frame Length    : 2526
//Line Length     : 3800
//Max Fps         : 30.00fps(vblank>1.0ms)
//Pixel order     : Green 1st (=GB)
//X/Y-flip        : X-flip
//Firmware Ver.   : v10.5.1
////////////////////////////////////////////////
0x002E, 0x1111,
0x0032, 0x1111,
0x0026, 0x0040,
0x002C, 0x09CF,
0x005C, 0x2101,
0x0006, 0x09DE,
0x0008, 0x0ED8,
0x000C, 0x0022,
0x0A22, 0x0000,
0x0A24, 0x0000,
0x0804, 0x0000,
0x0A12, 0x0CC0,
0x0A14, 0x0990,
0x0074, 0x09D8,
0x021C, 0x0001,
0x0A04, 0x014A,
0x0418, 0x0000,
0x0128, 0x0028,
0x012A, 0xFFFF,
0x0120, 0x0046,
0x0122, 0x0376,
0x012C, 0x0020,
0x012E, 0xFFFF,
0x0124, 0x0040,
0x0126, 0x0378,
0x0B02, 0xE04D,
0x0B10, 0x6821,
0x0B12, 0x0020,
0x0B14, 0x0001,
0x2008, 0x38FD,
0x326E, 0x0000,
0x0710, 0x09B0,
0x0900, 0x0300,
0x0902, 0xC319,
0x0914, 0xC109,
0x0916, 0x061A,
0x0918, 0x0407,
0x091A, 0x0A0B,
0x091C, 0x0E08,
0x091E, 0x0A00,
0x090C, 0x0427,
0x090E, 0x0059,
0x0954, 0x0089,
0x0956, 0x0000,
0x0958, 0xA880,
0x095A, 0x9244,
0x0F32, 0x025A,
0x0F38, 0x025A,
0x0F2A, 0x4124,
0x004C, 0x0100,
};

static kal_uint16 addr_data_pair_slim_video[] = {
//Sensor Information////////////////////////////
//Sensor	  : Hi-846
//Customer        : MTK
//Image size      : 3264x2448
//MCLK/PCLK       : 24MHz /288Mhz
//MIPI speed(Mbps): 720Mbps x 4Lane
//Frame Length    : 2526
//Line Length     : 3800
//Max Fps         : 30.00fps(vblank>1.0ms)
//Pixel order     : Green 1st (=GB)
//X/Y-flip        : X-flip
//Firmware Ver.   : v10.5.1
////////////////////////////////////////////////
0x002E, 0x1111,
0x0032, 0x1111,
0x0026, 0x0040,
0x002C, 0x09CF,
0x005C, 0x2101,
0x0006, 0x09DE,
0x0008, 0x0ED8,
0x000C, 0x0022,
0x0A22, 0x0000,
0x0A24, 0x0000,
0x0804, 0x0000,
0x0A12, 0x0CC0,
0x0A14, 0x0990,
0x0074, 0x09D8,
0x021C, 0x0001,
0x0A04, 0x014A,
0x0418, 0x0000,
0x0128, 0x0028,
0x012A, 0xFFFF,
0x0120, 0x0046,
0x0122, 0x0376,
0x012C, 0x0020,
0x012E, 0xFFFF,
0x0124, 0x0040,
0x0126, 0x0378,
0x0B02, 0xE04D,
0x0B10, 0x6821,
0x0B12, 0x0020,
0x0B14, 0x0001,
0x2008, 0x38FD,
0x326E, 0x0000,
0x0710, 0x09B0,
0x0900, 0x0300,
0x0902, 0xC319,
0x0914, 0xC109,
0x0916, 0x061A,
0x0918, 0x0407,
0x091A, 0x0A0B,
0x091C, 0x0E08,
0x091E, 0x0A00,
0x090C, 0x0427,
0x090E, 0x0059,
0x0954, 0x0089,
0x0956, 0x0000,
0x0958, 0xA880,
0x095A, 0x9244,
0x0F32, 0x025A,
0x0F38, 0x025A,
0x0F2A, 0x4124,
0x004C, 0x0100,
};

static kal_uint16 addr_data_pair_custom1[] = {
//Sensor Information////////////////////////////
//Sensor	  : Hi-846
//Customer        : MTK
//Image size      : 3264x2448
//MCLK/PCLK       : 24MHz /288Mhz
//MIPI speed(Mbps): 720Mbps x 4Lane
//Frame Length    : 2526
//Line Length     : 3800
//Max Fps         : 30.00fps(vblank>1.0ms)
//Pixel order     : Green 1st (=GB)
//X/Y-flip        : X-flip
//Firmware Ver.   : v10.5.1
////////////////////////////////////////////////
0x002E, 0x1111,
0x0032, 0x1111,
0x0026, 0x0040,
0x002C, 0x09CF,
0x005C, 0x2101,
0x0006, 0x09DE,
0x0008, 0x0ED8,
0x000C, 0x0022,
0x0A22, 0x0000,
0x0A24, 0x0000,
0x0804, 0x0000,
0x0A12, 0x0CC0,
0x0A14, 0x0990,
0x0074, 0x09D8,
0x021C, 0x0001,
0x0A04, 0x014A,
0x0418, 0x0000,
0x0128, 0x0028,
0x012A, 0xFFFF,
0x0120, 0x0046,
0x0122, 0x0376,
0x012C, 0x0020,
0x012E, 0xFFFF,
0x0124, 0x0040,
0x0126, 0x0378,
0x0B02, 0xE04D,
0x0B10, 0x6821,
0x0B12, 0x0020,
0x0B14, 0x0001,
0x2008, 0x38FD,
0x326E, 0x0000,
0x0710, 0x09B0,
0x0900, 0x0300,
0x0902, 0xC319,
0x0914, 0xC109,
0x0916, 0x061A,
0x0918, 0x0407,
0x091A, 0x0A0B,
0x091C, 0x0E08,
0x091E, 0x0A00,
0x090C, 0x0427,
0x090E, 0x0059,
0x0954, 0x0089,
0x0956, 0x0000,
0x0958, 0xA880,
0x095A, 0x9244,
0x0F32, 0x025A,
0x0F38, 0x025A,
0x0F2A, 0x4124,
0x004C, 0x0100,
};

static void sensor_init(struct subdrv_ctx *ctx)
{
	LOG_INF("%s E\n", __func__);

	table_write_cmos_sensor(ctx, addr_data_pair_init,
		sizeof(addr_data_pair_init) / sizeof(kal_uint16));
}

static void preview_setting(struct subdrv_ctx *ctx)
{
	LOG_INF("%s E\n", __func__);
	table_write_cmos_sensor(ctx, addr_data_pair_preview,
			sizeof(addr_data_pair_preview) / sizeof(kal_uint16));

}

static void capture_setting(struct subdrv_ctx *ctx, kal_uint16 currefps)
{
	LOG_INF("%s E! currefps:%d\n", __func__, currefps);

	table_write_cmos_sensor(ctx, addr_data_pair_capture,
			sizeof(addr_data_pair_capture) / sizeof(kal_uint16));
}

static void normal_video_setting(struct subdrv_ctx *ctx, kal_uint16 currefps)
{
	LOG_INF("%s currefps:%d", __func__, currefps);

	table_write_cmos_sensor(ctx, addr_data_pair_normal_video,
		sizeof(addr_data_pair_normal_video) / sizeof(kal_uint16));

}

static void hs_video_setting(struct subdrv_ctx *ctx)
{
	LOG_INF("%s", __func__);

	table_write_cmos_sensor(ctx, addr_data_pair_hs_video,
			sizeof(addr_data_pair_hs_video) / sizeof(kal_uint16));
}

static void slim_video_setting(struct subdrv_ctx *ctx)
{
	LOG_INF("%s", __func__);
	table_write_cmos_sensor(ctx, addr_data_pair_slim_video,
		sizeof(addr_data_pair_slim_video) / sizeof(kal_uint16));
}
static void custom1_setting(struct subdrv_ctx *ctx)
{
	LOG_INF("%s", __func__);
	table_write_cmos_sensor(ctx, addr_data_pair_custom1,
		sizeof(addr_data_pair_custom1) / sizeof(kal_uint16));

}

/*************************************************************************
 * FUNCTION
 *	get_imgsensor_id
 *
 * DESCRIPTION
 *	This function get the sensor ID
 *
 * PARAMETERS
 *	*sensorID : return the sensor ID
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static int get_imgsensor_id(struct subdrv_ctx *ctx, UINT32 *sensor_id)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 2;

	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		ctx->i2c_write_id = imgsensor_info.i2c_addr_table[i];
		do {
			*sensor_id = ((read_cmos_sensor_8(ctx, 0x0f17) << 8)
				      | read_cmos_sensor_8(ctx, 0x0f16)) + 1;
			LOG_INF("i2c read sensor_id addr: 0x%x, sensor id: 0x%x imgsensor_info.sensor_id %x\n", ctx->i2c_write_id, *sensor_id,imgsensor_info.sensor_id);
			if (*sensor_id == imgsensor_info.sensor_id) {
				LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n",ctx->i2c_write_id, *sensor_id);
				return ERROR_NONE;
			}
			else{
				LOG_INF("Read sensor id fail, id: 0x%x\n",ctx->i2c_write_id);
				retry--;
			}
		} while (retry > 0);
		i++;
		retry = 2;
	}
	if (*sensor_id != imgsensor_info.sensor_id) {
	/* if Sensor ID is not correct, Must set *sensor_id to 0xFFFFFFFF */
		*sensor_id = 0xFFFFFFFF;
		return ERROR_SENSOR_CONNECT_FAIL;
	}
	return ERROR_NONE;
}


/*************************************************************************
 * FUNCTION
 *	open
 *
 * DESCRIPTION
 *	This function initialize the registers of CMOS sensor
 *
 * PARAMETERS
 *	None
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static int open(struct subdrv_ctx *ctx)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 2;
	kal_uint32 sensor_id = 0;
	LOG_INF("%s", __func__);

	/* sensor have two i2c address 0x6c 0x6d & 0x21 0x20,
	 * we should detect the module used i2c address
	 */
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		ctx->i2c_write_id = imgsensor_info.i2c_addr_table[i];
		do {
			sensor_id = ((read_cmos_sensor_8(ctx, 0x0f17) << 8)
				      | read_cmos_sensor_8(ctx, 0x0f16)) + 1;
			LOG_INF("i2c read sensor_id addr: 0x%x, sensor id: 0x%x imgsensor_info.sensor_id %x\n", ctx->i2c_write_id, sensor_id,imgsensor_info.sensor_id);

			if (sensor_id == imgsensor_info.sensor_id) {
				LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n",
					ctx->i2c_write_id, sensor_id);
				break;
			}
			LOG_INF("Read sensor id fail, id: 0x%x\n",
				ctx->i2c_write_id);
			retry--;
		} while (retry > 0);
		i++;
		if (sensor_id == imgsensor_info.sensor_id)
			break;
		retry = 2;
	}
	if (imgsensor_info.sensor_id != sensor_id)
		return ERROR_SENSOR_CONNECT_FAIL;

	/* initail sequence write in  */
	sensor_init(ctx);


	ctx->autoflicker_en = KAL_FALSE;
	ctx->sensor_mode = IMGSENSOR_MODE_INIT;
	ctx->shutter = 0x9D8;
	ctx->gain = 0x10;
	ctx->pclk = imgsensor_info.pre.pclk;
	ctx->frame_length = imgsensor_info.pre.framelength;
	ctx->line_length = imgsensor_info.pre.linelength;
	ctx->min_frame_length = imgsensor_info.pre.framelength;
	ctx->dummy_pixel = 0;
	ctx->dummy_line = 0;
	ctx->ihdr_mode = 0;
	ctx->test_pattern = KAL_FALSE;
	ctx->current_fps = imgsensor_info.pre.max_framerate;

	return ERROR_NONE;
}



/*************************************************************************
 * FUNCTION
 *	close
 *
 * DESCRIPTION
 *
 *
 * PARAMETERS
 *	None
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static int close(struct subdrv_ctx *ctx)
{
	LOG_INF("E\n");

	return ERROR_NONE;
}


/*************************************************************************
 * FUNCTION
 * preview
 *
 * DESCRIPTION
 *	This function start the sensor preview.
 *
 * PARAMETERS
 *	*image_window : address pointer of pixel numbers in one period of HSYNC
 *  *sensor_config_data : address pointer of line numbers in one period of VSYNC
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 preview(struct subdrv_ctx *ctx, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("%s E\n", __func__);

	ctx->sensor_mode = IMGSENSOR_MODE_PREVIEW;
	ctx->pclk = imgsensor_info.pre.pclk;
	ctx->line_length = imgsensor_info.pre.linelength;
	ctx->frame_length = imgsensor_info.pre.framelength;
	ctx->min_frame_length = imgsensor_info.pre.framelength;
	ctx->autoflicker_en = KAL_FALSE;

	preview_setting(ctx);
	set_mirror_flip(ctx, ctx->mirror);

	return ERROR_NONE;
}

/*************************************************************************
 * FUNCTION
 *	capture
 *
 * DESCRIPTION
 *	This function setup the CMOS sensor in capture MY_OUTPUT mode
 *
 * PARAMETERS
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 capture(struct subdrv_ctx *ctx, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("%s E\n", __func__);
	ctx->sensor_mode = IMGSENSOR_MODE_CAPTURE;

	ctx->pclk = imgsensor_info.cap.pclk;
	ctx->line_length = imgsensor_info.cap.linelength;
	ctx->frame_length = imgsensor_info.cap.framelength;
	ctx->min_frame_length = imgsensor_info.cap.framelength;
	ctx->autoflicker_en = KAL_FALSE;

	capture_setting(ctx, ctx->current_fps);
	set_mirror_flip(ctx, ctx->mirror);

	return ERROR_NONE;
}

static kal_uint32 normal_video(struct subdrv_ctx *ctx,
	MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("%s E\n", __func__);

	ctx->sensor_mode = IMGSENSOR_MODE_VIDEO;
	ctx->pclk = imgsensor_info.normal_video.pclk;
	ctx->line_length = imgsensor_info.normal_video.linelength;
	ctx->frame_length = imgsensor_info.normal_video.framelength;
	ctx->min_frame_length = imgsensor_info.normal_video.framelength;
	ctx->autoflicker_en = KAL_FALSE;

	normal_video_setting(ctx, ctx->current_fps);
	set_mirror_flip(ctx, ctx->mirror);

	return ERROR_NONE;
}

static kal_uint32 hs_video(struct subdrv_ctx *ctx, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			   MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("%s E\n", __func__);

	ctx->sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
	ctx->pclk = imgsensor_info.hs_video.pclk;
	/* ctx->video_mode = KAL_TRUE; */
	ctx->line_length = imgsensor_info.hs_video.linelength;
	ctx->frame_length = imgsensor_info.hs_video.framelength;
	ctx->min_frame_length = imgsensor_info.hs_video.framelength;
	ctx->dummy_line = 0;
	ctx->dummy_pixel = 0;
	/* ctx->current_fps = 300; */
	ctx->autoflicker_en = KAL_FALSE;
	hs_video_setting(ctx);
	set_mirror_flip(ctx, ctx->mirror);

	return ERROR_NONE;
}

static kal_uint32 slim_video(struct subdrv_ctx *ctx,
	MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("%s E\n", __func__);

	ctx->sensor_mode = IMGSENSOR_MODE_SLIM_VIDEO;
	ctx->pclk = imgsensor_info.slim_video.pclk;
	/* ctx->video_mode = KAL_TRUE; */
	ctx->line_length = imgsensor_info.slim_video.linelength;
	ctx->frame_length = imgsensor_info.slim_video.framelength;
	ctx->min_frame_length = imgsensor_info.slim_video.framelength;
	ctx->dummy_line = 0;
	ctx->dummy_pixel = 0;
	/* ctx->current_fps = 300; */
	ctx->autoflicker_en = KAL_FALSE;
	slim_video_setting(ctx);
	set_mirror_flip(ctx, ctx->mirror);

	return ERROR_NONE;
}

static kal_uint32 custom1(struct subdrv_ctx *ctx,
	MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("%s E\n", __func__);

	ctx->sensor_mode = IMGSENSOR_MODE_CUSTOM1;
	ctx->pclk = imgsensor_info.custom1.pclk;
	/* ctx->video_mode = KAL_TRUE; */
	ctx->line_length = imgsensor_info.custom1.linelength;
	ctx->frame_length = imgsensor_info.custom1.framelength;
	ctx->min_frame_length = imgsensor_info.custom1.framelength;
	ctx->dummy_line = 0;
	ctx->dummy_pixel = 0;
	/* ctx->current_fps = 300; */
	ctx->autoflicker_en = KAL_FALSE;
	custom1_setting(ctx);
	set_mirror_flip(ctx, ctx->mirror);

	return ERROR_NONE;
}

static int get_resolution(struct subdrv_ctx *ctx,
	MSDK_SENSOR_RESOLUTION_INFO_STRUCT *sensor_resolution)
{
	int i = 0;

	for (i = SENSOR_SCENARIO_ID_MIN; i < SENSOR_SCENARIO_ID_MAX; i++) {
		if (i < imgsensor_info.sensor_mode_num) {
			sensor_resolution->SensorWidth[i] = imgsensor_winsize_info[i].w2_tg_size;
			sensor_resolution->SensorHeight[i] = imgsensor_winsize_info[i].h2_tg_size;
		} else {
			sensor_resolution->SensorWidth[i] = 0;
			sensor_resolution->SensorHeight[i] = 0;
		}
	}
	return ERROR_NONE;
}

static int get_info(struct subdrv_ctx *ctx, enum MSDK_SCENARIO_ID_ENUM scenario_id,
			   MSDK_SENSOR_INFO_STRUCT *sensor_info,
			   MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	/*LOG_INF("get_info -> scenario_id = %d\n", scenario_id);*/

	sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;

	/* not use */
	sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW;

	/* inverse with datasheet */
	sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;

	sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorInterruptDelayLines = 4;	/* not use */
	sensor_info->SensorResetActiveHigh = FALSE;	/* not use */
	sensor_info->SensorResetDelayCount = 5;	/* not use */

	sensor_info->SensroInterfaceType = imgsensor_info.sensor_interface_type;
	sensor_info->MIPIsensorType = imgsensor_info.mipi_sensor_type;

	sensor_info->SensorOutputDataFormat =
		imgsensor_info.sensor_output_dataformat;

	sensor_info->DelayFrame[SENSOR_SCENARIO_ID_NORMAL_PREVIEW] =
		imgsensor_info.pre_delay_frame;
	sensor_info->DelayFrame[SENSOR_SCENARIO_ID_NORMAL_CAPTURE] =
		imgsensor_info.cap_delay_frame;
	sensor_info->DelayFrame[SENSOR_SCENARIO_ID_NORMAL_VIDEO] =
		imgsensor_info.video_delay_frame;
	sensor_info->DelayFrame[SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO] =
		imgsensor_info.hs_video_delay_frame;
	sensor_info->DelayFrame[SENSOR_SCENARIO_ID_SLIM_VIDEO] =
		imgsensor_info.slim_video_delay_frame;
	sensor_info->DelayFrame[SENSOR_SCENARIO_ID_CUSTOM1] =
		imgsensor_info.custom1_delay_frame;
	sensor_info->SensorMasterClockSwitch = 0;	/* not use */
	sensor_info->SensorDrivingCurrent = imgsensor_info.isp_driving_current;

	/* The frame of setting shutter default 0 for TG int */
	sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame;

	/* The frame of setting sensor gain*/
	sensor_info->AESensorGainDelayFrame =
				imgsensor_info.ae_sensor_gain_delay_frame;

	sensor_info->AEISPGainDelayFrame =
				imgsensor_info.ae_ispGain_delay_frame;

	sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
	sensor_info->IHDR_LE_FirstLine = imgsensor_info.ihdr_le_firstline;
	sensor_info->SensorModeNum = imgsensor_info.sensor_mode_num;

	/* change pdaf support mode to pdaf VC mode */
	#if FPT_PDAF_SUPPORT
		sensor_info->PDAF_Support = PDAF_SUPPORT_CAMSV;
	#else
		sensor_info->PDAF_Support = 0;
	#endif
	sensor_info->SensorMIPILaneNumber = imgsensor_info.mipi_lane_num;
	sensor_info->SensorClockFreq = imgsensor_info.mclk;
	sensor_info->SensorClockDividCount = 3;	/* not use */
	sensor_info->SensorClockRisingCount = 0;
	sensor_info->SensorClockFallingCount = 2;	/* not use */
	sensor_info->SensorPixelClockCount = 3;	/* not use */
	sensor_info->SensorDataLatchCount = 2;	/* not use */
	sensor_info->SensorWidthSampling = 0;	/* 0 is default 1x */
	sensor_info->SensorHightSampling = 0;	/* 0 is default 1x */
	sensor_info->SensorPacketECCOrder = 1;


	return ERROR_NONE;
}


static int control(struct subdrv_ctx *ctx, enum MSDK_SCENARIO_ID_ENUM scenario_id,
			  MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	ctx->current_scenario_id = scenario_id;
	switch (scenario_id) {
	case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		preview(ctx, image_window, sensor_config_data);
		break;
	case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
		capture(ctx, image_window, sensor_config_data);
		break;
	case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
		normal_video(ctx, image_window, sensor_config_data);
		break;
	case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
		hs_video(ctx, image_window, sensor_config_data);
		break;
	case SENSOR_SCENARIO_ID_SLIM_VIDEO:
		slim_video(ctx, image_window, sensor_config_data);
		break;
	case SENSOR_SCENARIO_ID_CUSTOM1:
		custom1(ctx, image_window, sensor_config_data);
		break;
	default:
		LOG_INF("Error ScenarioId setting");
		preview(ctx, image_window, sensor_config_data);
		return ERROR_INVALID_SCENARIO_ID;
	}
	return ERROR_NONE;
}

static kal_uint32 set_video_mode(struct subdrv_ctx *ctx, UINT16 framerate)
{
	/* //LOG_INF("framerate = %d\n ", framerate); */
	/* SetVideoMode Function should fix framerate */
	if (framerate == 0)
		/* Dynamic frame rate */
		return ERROR_NONE;
	if ((framerate == 300) && (ctx->autoflicker_en == KAL_TRUE))
		ctx->current_fps = 296;
	else if ((framerate == 150) && (ctx->autoflicker_en == KAL_TRUE))
		ctx->current_fps = 146;
	else
		ctx->current_fps = framerate;
	set_max_framerate(ctx, ctx->current_fps, 1);

	return ERROR_NONE;
}

static kal_uint32 set_auto_flicker_mode(struct subdrv_ctx *ctx,
	kal_bool enable, UINT16 framerate)
{
	if (enable)		/* enable auto flicker */
		ctx->autoflicker_en = KAL_TRUE;
	else			/* Cancel Auto flick */
		ctx->autoflicker_en = KAL_FALSE;
	return ERROR_NONE;
}

static kal_uint32 set_max_framerate_by_scenario(struct subdrv_ctx *ctx,
	enum MSDK_SCENARIO_ID_ENUM scenario_id,	MUINT32 framerate)
{
	kal_uint32 frame_length;

	switch (scenario_id) {
	case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		frame_length = imgsensor_info.pre.pclk
			/ framerate * 10 / imgsensor_info.pre.linelength;

		ctx->dummy_line =
			(frame_length > imgsensor_info.pre.framelength)
			? (frame_length - imgsensor_info.pre.framelength) : 0;

		ctx->frame_length =
			imgsensor_info.pre.framelength + ctx->dummy_line;

		ctx->min_frame_length = ctx->frame_length;
		if (ctx->frame_length > ctx->shutter)
			set_dummy(ctx);
		break;
	case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
		if (framerate == 0)
			return ERROR_NONE;
		frame_length = imgsensor_info.normal_video.pclk
		    / framerate * 10 / imgsensor_info.normal_video.linelength;

		ctx->dummy_line =
	    (frame_length > imgsensor_info.normal_video.framelength)
	  ? (frame_length - imgsensor_info.normal_video.  framelength) : 0;

		ctx->frame_length =
		 imgsensor_info.normal_video.framelength + ctx->dummy_line;

		ctx->min_frame_length = ctx->frame_length;
		if (ctx->frame_length > ctx->shutter)
			set_dummy(ctx);
		break;

	case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
	if (ctx->current_fps == imgsensor_info.cap.max_framerate) {

		frame_length = imgsensor_info.cap.pclk
			/ framerate * 10 / imgsensor_info.cap.linelength;

		ctx->dummy_line =
		      (frame_length > imgsensor_info.cap.framelength)
		    ? (frame_length - imgsensor_info.cap.  framelength) : 0;

		ctx->frame_length =
			imgsensor_info.cap.framelength + ctx->dummy_line;
		ctx->min_frame_length = ctx->frame_length;
	} else if (ctx->current_fps == imgsensor_info.cap2.max_framerate) {
		frame_length = imgsensor_info.cap2.pclk
			/ framerate * 10 / imgsensor_info.cap2.linelength;
		ctx->dummy_line =
		      (frame_length > imgsensor_info.cap2.framelength)
		    ? (frame_length - imgsensor_info.cap2.  framelength) : 0;

		ctx->frame_length =
			imgsensor_info.cap2.framelength + ctx->dummy_line;

		ctx->min_frame_length = ctx->frame_length;
	} else {
		if (ctx->current_fps != imgsensor_info.cap.max_framerate)
			LOG_INF("Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",
				framerate,
				imgsensor_info.cap.max_framerate / 10);

		frame_length = imgsensor_info.cap.pclk
			/ framerate * 10 / imgsensor_info.cap.linelength;
		ctx->dummy_line =
			(frame_length > imgsensor_info.cap.framelength)
			? (frame_length - imgsensor_info.cap.framelength) : 0;
		ctx->frame_length =
			imgsensor_info.cap.framelength + ctx->dummy_line;
		ctx->min_frame_length = ctx->frame_length;
	}
		if (ctx->frame_length > ctx->shutter)
			set_dummy(ctx);
		break;
	case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
		frame_length = imgsensor_info.hs_video.pclk
			/ framerate * 10 / imgsensor_info.hs_video.linelength;
		ctx->dummy_line =
		  (frame_length > imgsensor_info.hs_video.framelength)
		? (frame_length - imgsensor_info.hs_video.  framelength) : 0;

		ctx->frame_length =
		    imgsensor_info.hs_video.framelength + ctx->dummy_line;

		ctx->min_frame_length = ctx->frame_length;
		if (ctx->frame_length > ctx->shutter)
			set_dummy(ctx);
		break;
	case SENSOR_SCENARIO_ID_SLIM_VIDEO:
		frame_length = imgsensor_info.slim_video.pclk
			/ framerate * 10 / imgsensor_info.slim_video.linelength;

		ctx->dummy_line =
		  (frame_length > imgsensor_info.slim_video.framelength)
		? (frame_length - imgsensor_info.slim_video.  framelength) : 0;

		ctx->frame_length =
		  imgsensor_info.slim_video.framelength + ctx->dummy_line;

		ctx->min_frame_length = ctx->frame_length;
		if (ctx->frame_length > ctx->shutter)
			set_dummy(ctx);
		break;
	case SENSOR_SCENARIO_ID_CUSTOM1:
		frame_length = imgsensor_info.custom1.pclk
			/ framerate * 10 / imgsensor_info.custom1.linelength;

		ctx->dummy_line =
		  (frame_length > imgsensor_info.custom1.framelength)
		? (frame_length - imgsensor_info.custom1.  framelength) : 0;

		ctx->frame_length =
		  imgsensor_info.custom1.framelength + ctx->dummy_line;

		ctx->min_frame_length = ctx->frame_length;
		if (ctx->frame_length > ctx->shutter)
			set_dummy(ctx);
		break;
	default:		/* coding with  preview scenario by default */
		frame_length = imgsensor_info.pre.pclk
			/ framerate * 10 / imgsensor_info.pre.linelength;

		ctx->dummy_line =
			(frame_length > imgsensor_info.pre.framelength)
			? (frame_length - imgsensor_info.pre.framelength) : 0;

		ctx->frame_length =
			imgsensor_info.pre.framelength + ctx->dummy_line;

		ctx->min_frame_length = ctx->frame_length;
		if (ctx->frame_length > ctx->shutter)
			set_dummy(ctx);

		LOG_INF("error scenario_id = %d, we use preview scenario\n",
		scenario_id);
		break;
	}
	return ERROR_NONE;
}


static kal_uint32 get_default_framerate_by_scenario(struct subdrv_ctx *ctx,
	enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 *framerate)
{
	/*LOG_INF("scenario_id = %d\n", scenario_id);*/

	switch (scenario_id) {
	case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		*framerate = imgsensor_info.pre.max_framerate;
		break;
	case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
		*framerate = imgsensor_info.normal_video.max_framerate;
		break;
	case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
		*framerate = imgsensor_info.cap.max_framerate;
		break;
	case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
		*framerate = imgsensor_info.hs_video.max_framerate;
		break;
	case SENSOR_SCENARIO_ID_SLIM_VIDEO:
		*framerate = imgsensor_info.slim_video.max_framerate;
		break;
	case SENSOR_SCENARIO_ID_CUSTOM1:
		*framerate = imgsensor_info.custom1.max_framerate;
		break;
	default:
		break;
	}

	return ERROR_NONE;
}

static kal_uint32 set_test_pattern_mode(struct subdrv_ctx *ctx, kal_bool enable)
{
	LOG_INF("enable: %d\n", enable);

	if (enable) {
		/* 0 : Normal, 1 : Solid Color, 2 : Color Bar, 3 : Shade Color Bar, 4 : PN9 */
		write_cmos_sensor(ctx, 0x0a04, 0x014b);
		write_cmos_sensor(ctx, 0x020a, 0x0200);
	} else {
		write_cmos_sensor(ctx, 0x0a04, 0x014a);
		write_cmos_sensor(ctx, 0x020a, 0x0000);
	}
	ctx->test_pattern = enable;
	return ERROR_NONE;
}

static int feature_control(struct subdrv_ctx *ctx, MSDK_SENSOR_FEATURE_ENUM feature_id,
				  UINT8 *feature_para, UINT32 *feature_para_len)
{
	UINT16 *feature_return_para_16 = (UINT16 *) feature_para;
	UINT16 *feature_data_16 = (UINT16 *) feature_para;
	UINT32 *feature_return_para_32 = (UINT32 *) feature_para;
	UINT32 *feature_data_32 = (UINT32 *) feature_para;
	unsigned long long *feature_data = (unsigned long long *)feature_para;

#if FPT_PDAF_SUPPORT
	struct SET_PD_BLOCK_INFO_T *PDAFinfo;
#endif
	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;

	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data =
		(MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

	/*LOG_INF("feature_id = %d\n", feature_id);*/
	switch (feature_id) {
	case SENSOR_FEATURE_GET_OUTPUT_FORMAT_BY_SCENARIO:
		switch (*feature_data) {
		case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
		case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
		case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
		case SENSOR_SCENARIO_ID_SLIM_VIDEO:
		case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		case SENSOR_SCENARIO_ID_CUSTOM1:
			*(feature_data + 1)
			= (enum ACDK_SENSOR_OUTPUT_DATA_FORMAT_ENUM)
				imgsensor_info.sensor_output_dataformat;
			break;
		}
	break;
	case SENSOR_FEATURE_GET_ANA_GAIN_TABLE:
		if ((void *)(uintptr_t) (*(feature_data + 1)) == NULL) {
			*(feature_data + 0) =
				sizeof(hi846_rear_ana_gain_table);
		} else {
			memcpy((void *)(uintptr_t) (*(feature_data + 1)),
			(void *)hi846_rear_ana_gain_table,
			sizeof(hi846_rear_ana_gain_table));
		}
		break;
	case SENSOR_FEATURE_GET_GAIN_RANGE_BY_SCENARIO:
		*(feature_data + 1) = imgsensor_info.min_gain;
		*(feature_data + 2) = imgsensor_info.max_gain;
		break;
	case SENSOR_FEATURE_GET_BASE_GAIN_ISO_AND_STEP:
		*(feature_data + 0) = imgsensor_info.min_gain_iso;
		*(feature_data + 1) = imgsensor_info.gain_step;
		*(feature_data + 2) = imgsensor_info.gain_type;
		break;
	case SENSOR_FEATURE_GET_MIN_SHUTTER_BY_SCENARIO:
		*(feature_data + 1) = imgsensor_info.min_shutter;
		*(feature_data + 2) = imgsensor_info.exp_step;
		break;
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ_BY_SCENARIO:
		switch (*feature_data) {
		case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.cap.pclk;
			break;
		case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.normal_video.pclk;
			break;
		case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.hs_video.pclk;
			break;
		case SENSOR_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.slim_video.pclk;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM1:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom1.pclk;
			break;
		case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.pre.pclk;
			break;
		}
		break;
	case SENSOR_FEATURE_GET_PERIOD_BY_SCENARIO:
		switch (*feature_data) {
		case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.cap.framelength << 16)
				+ imgsensor_info.cap.linelength;
			break;
		case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.normal_video.framelength << 16)
				+ imgsensor_info.normal_video.linelength;
			break;
		case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.hs_video.framelength << 16)
				+ imgsensor_info.hs_video.linelength;
			break;
		case SENSOR_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.slim_video.framelength << 16)
				+ imgsensor_info.slim_video.linelength;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM1:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.custom1.framelength << 16)
				+ imgsensor_info.custom1.linelength;
			break;
		case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.pre.framelength << 16)
				+ imgsensor_info.pre.linelength;
			break;
		}
		break;

	case SENSOR_FEATURE_GET_PERIOD:
		*feature_return_para_16++ = ctx->line_length;
		*feature_return_para_16 = ctx->frame_length;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
		*feature_return_para_32 = ctx->pclk;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_ESHUTTER:
		set_shutter(ctx, *feature_data);
		break;
	case SENSOR_FEATURE_SET_NIGHTMODE:

		break;
	case SENSOR_FEATURE_SET_GAIN:
		set_gain(ctx, (UINT32) * feature_data);
		break;
	case SENSOR_FEATURE_SET_FLASHLIGHT:
		break;
	case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
		break;

	case SENSOR_FEATURE_SET_REGISTER:
		write_cmos_sensor(ctx, sensor_reg_data->RegAddr,
			sensor_reg_data->RegData);
		break;

	case SENSOR_FEATURE_GET_REGISTER:
		sensor_reg_data->RegData =
			read_cmos_sensor(ctx, sensor_reg_data->RegAddr);
		break;

	case SENSOR_FEATURE_GET_LENS_DRIVER_ID:

		*feature_return_para_32 = LENS_DRIVER_ID_DO_NOT_CARE;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_VIDEO_MODE:
		set_video_mode(ctx, *feature_data);
		break;
	case SENSOR_FEATURE_CHECK_SENSOR_ID:
		get_imgsensor_id(ctx, feature_return_para_32);
		break;
	case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
		set_auto_flicker_mode(ctx, (BOOL) (*feature_data_16),
					*(feature_data_16 + 1));
		break;
	case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
		set_max_framerate_by_scenario(ctx,
	    (enum MSDK_SCENARIO_ID_ENUM) *feature_data, *(feature_data + 1));
		break;

	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
		get_default_framerate_by_scenario(ctx,
			(enum MSDK_SCENARIO_ID_ENUM) *(feature_data),
			  (MUINT32 *) (uintptr_t) (*(feature_data + 1)));
		break;
	case SENSOR_FEATURE_SET_TEST_PATTERN:
		set_test_pattern_mode(ctx, (BOOL) (*feature_data));
		break;

	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
		*feature_return_para_32 = imgsensor_info.checksum_value;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_FRAMERATE:
		LOG_INF("current fps :%d\n", *feature_data_32);
		ctx->current_fps = (UINT16)*feature_data_32;
		break;
	case SENSOR_FEATURE_SET_HDR:
		LOG_INF("ihdr enable :%d\n", *feature_data_32);
		ctx->ihdr_mode = (UINT8)*feature_data_32;
		break;

	case SENSOR_FEATURE_GET_CROP_INFO:
		wininfo =
	(struct SENSOR_WINSIZE_INFO_STRUCT *) (uintptr_t) (*(feature_data + 1));

		switch (*feature_data_32) {
		case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[1],
			       sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[2],
			       sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[3],
			       sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case SENSOR_SCENARIO_ID_SLIM_VIDEO:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[4],
			       sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case SENSOR_SCENARIO_ID_CUSTOM1:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[5],
			       sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		default:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[0],
			       sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		}
		break;
	case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
		LOG_INF("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n",
			(UINT16) *feature_data,
			(UINT16) *(feature_data + 1),
			(UINT16) *(feature_data + 2));
		break;
	case SENSOR_FEATURE_SET_AWB_GAIN:
		break;
	case SENSOR_FEATURE_SET_HDR_SHUTTER:
		LOG_INF("SENSOR_FEATURE_SET_HDR_SHUTTER LE=%d, SE=%d\n",
			(UINT16) *feature_data,
			(UINT16) *(feature_data + 1));
		break;
	case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
		LOG_INF("SENSOR_FEATURE_SET_STREAMING_SUSPEND\n");
		streaming_control(ctx, KAL_FALSE);
		break;
	case SENSOR_FEATURE_SET_STREAMING_RESUME:
		LOG_INF("SENSOR_FEATURE_SET_STREAMING_RESUME, shutter:%llu\n",
			*feature_data);
		if (*feature_data != 0)
			set_shutter(ctx, *feature_data);
		streaming_control(ctx, KAL_TRUE);
		break;
	case SENSOR_FEATURE_GET_BINNING_TYPE:
		switch (*(feature_data + 1)) {
		case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
		case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
		case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
		case SENSOR_SCENARIO_ID_SLIM_VIDEO:
		case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		case SENSOR_SCENARIO_ID_CUSTOM1:
		default:
			*feature_return_para_32 = 1; /*BINNING_AVERAGED*/
			break;
		}

		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_GET_PDAF_INFO:
#if FPT_PDAF_SUPPORT

		LOG_INF("SENSOR_FEATURE_GET_PDAF_INFO scenarioId:%d\n",
			(UINT16) *feature_data);
		PDAFinfo =
			(struct SET_PD_BLOCK_INFO_T *)
				(uintptr_t)(*(feature_data+1));

		switch (*feature_data) {
		case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
		case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
		case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
		case SENSOR_SCENARIO_ID_SLIM_VIDEO:
		case SENSOR_SCENARIO_ID_CUSTOM1:
			memcpy((void *)PDAFinfo,
				(void *)&imgsensor_pd_info,
				sizeof(struct SET_PD_BLOCK_INFO_T));
		default:
			break;
		}
#endif
		break;
	case SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY:

#if FPT_PDAF_SUPPORT
		switch (*feature_data) {
		case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
		case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
		case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
		case SENSOR_SCENARIO_ID_SLIM_VIDEO:
		case SENSOR_SCENARIO_ID_CUSTOM1:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 1;
			break;
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 0;
			break;
		}
#else
		*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 0;
		break;
#endif
	case SENSOR_FEATURE_GET_MIPI_PIXEL_RATE:

		switch (*feature_data) {
		case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.cap.mipi_pixel_rate;
			break;
		case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.normal_video.mipi_pixel_rate;
			break;
		case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.hs_video.mipi_pixel_rate;
			break;
		case SENSOR_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.slim_video.mipi_pixel_rate;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM1:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.slim_video.mipi_pixel_rate;
			break;
		case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.pre.mipi_pixel_rate;
			break;
		}
		break;
	default:
		break;
	}

	return ERROR_NONE;
}

#ifdef IMGSENSOR_VC_ROUTING

static struct mtk_mbus_frame_desc_entry frame_desc_prev[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 3264,
			.vsize = 2448,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cap[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 3264,
			.vsize = 2448,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_vid[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 3264,
			.vsize = 2448,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_hs_vid[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 3264,
			.vsize = 2448,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_slim_vid[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 3264,
			.vsize = 2448,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cust1[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 3264,
			.vsize = 2448,
		},
	},
};

static int get_frame_desc(struct subdrv_ctx *ctx,
		int scenario_id, struct mtk_mbus_frame_desc *fd)
{
	switch (scenario_id) {
	case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		fd->type = MTK_MBUS_FRAME_DESC_TYPE_CSI2;
		fd->num_entries = ARRAY_SIZE(frame_desc_prev);
		memcpy(fd->entry, frame_desc_prev, sizeof(frame_desc_prev));
		break;
	case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
		fd->type = MTK_MBUS_FRAME_DESC_TYPE_CSI2;
		fd->num_entries = ARRAY_SIZE(frame_desc_cap);
		memcpy(fd->entry, frame_desc_cap, sizeof(frame_desc_cap));
		break;
	case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
		fd->type = MTK_MBUS_FRAME_DESC_TYPE_CSI2;
		fd->num_entries = ARRAY_SIZE(frame_desc_vid);
		memcpy(fd->entry, frame_desc_vid, sizeof(frame_desc_vid));
		break;
	case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
		fd->type = MTK_MBUS_FRAME_DESC_TYPE_CSI2;
		fd->num_entries = ARRAY_SIZE(frame_desc_hs_vid);
		memcpy(fd->entry, frame_desc_hs_vid, sizeof(frame_desc_hs_vid));
		break;
	case SENSOR_SCENARIO_ID_SLIM_VIDEO:
		fd->type = MTK_MBUS_FRAME_DESC_TYPE_CSI2;
		fd->num_entries = ARRAY_SIZE(frame_desc_slim_vid);
		memcpy(fd->entry, frame_desc_slim_vid, sizeof(frame_desc_slim_vid));
		break;
	case SENSOR_SCENARIO_ID_CUSTOM1:
		fd->type = MTK_MBUS_FRAME_DESC_TYPE_CSI2;
		fd->num_entries = ARRAY_SIZE(frame_desc_cust1);
		memcpy(fd->entry, frame_desc_cust1, sizeof(frame_desc_cust1));
		break;
	default:
		return -1;
	}

	return 0;
}
#endif

static const struct subdrv_ctx defctx = {

	.ana_gain_def = BASEGAIN * 4,
	.ana_gain_max = BASEGAIN * 16,
	.ana_gain_min = BASEGAIN,
	.ana_gain_step = 1,
	.exposure_def = 0x9D8,
	.exposure_max = 0xffff - 6,
	.exposure_min = 6,
	.exposure_step = 1,
	.frame_time_delay_frame = 2,
	.margin = 6,
	.max_frame_length = 0xffff,

	.mirror = IMAGE_H_MIRROR,	/* mirrorflip information */

	/* IMGSENSOR_MODE enum value,record current sensor mode,such as:
	 * INIT, Preview, Capture, Video,High Speed Video, Slim Video
	 */
	.sensor_mode = IMGSENSOR_MODE_INIT,

	.shutter = 0x9D8,	/* current shutter */
	.gain = BASEGAIN * 4,		/* current gain */
	.dummy_pixel = 0,	/* current dummypixel */
	.dummy_line = 0,	/* current dummyline */
	.current_fps = 300,	/* full size current fps : 24fps for PIP,
				 * 30fps for Normal or ZSD
				 */

	/* auto flicker enable: KAL_FALSE for disable auto flicker,
	 * KAL_TRUE for enable auto flicker
	 */
	.autoflicker_en = KAL_FALSE,

	/* test pattern mode or not.
	* KAL_FALSE for in test pattern mode,
	* KAL_TRUE for normal output
	*/
	.test_pattern = KAL_FALSE,

	/* current scenario id */
	.current_scenario_id = SENSOR_SCENARIO_ID_NORMAL_PREVIEW,

	/* sensor need support LE, SE with HDR feature */
	.ihdr_mode = KAL_FALSE,
	.i2c_write_id = 0x40,	/* record current sensor's i2c write id */
};

static int init_ctx(struct subdrv_ctx *ctx,
		struct i2c_client *i2c_client, u8 i2c_write_id)
{
	memcpy(ctx, &defctx, sizeof(*ctx));
	ctx->i2c_client = i2c_client;
	ctx->i2c_write_id = i2c_write_id;
	return 0;
}

static struct subdrv_ops ops = {
	.get_id = get_imgsensor_id,
	.init_ctx = init_ctx,
	.open = open,
	.get_info = get_info,
	.get_resolution = get_resolution,
	.control = control,
	.feature_control = feature_control,
	.close = close,
#ifdef IMGSENSOR_VC_ROUTING
	.get_frame_desc = get_frame_desc,
#endif
    //.get_csi_param = get_csi_param,
	//.get_temp = get_temp,
};

static struct subdrv_pw_seq_entry pw_seq[] = {
	{HW_ID_PDN, 0, 1},
	{HW_ID_RST, 0, 1},
	{HW_ID_AVDD, 1, 2},
	{HW_ID_AVDD1, 2800000, 1},
	{HW_ID_DVDD, 1, 2},
	{HW_ID_AFVDD, 2800000, 5},
	{HW_ID_MCLK, 24, 0},
	{HW_ID_MCLK_DRIVING_CURRENT, 6, 1},
	{HW_ID_PDN, 1, 2},
	{HW_ID_RST, 1, 3},
};


const struct subdrv_entry hi846_rear_mipi_raw_entry = {
	.name = "hi846_rear_mipi_raw",
	.id = HI846_REAR_SENSOR_ID,
	.pw_seq = pw_seq,
	.pw_seq_cnt = ARRAY_SIZE(pw_seq),
	.ops = &ops,
};

