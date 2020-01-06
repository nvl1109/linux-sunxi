// SPDX-License-Identifier: GPL-2.0
/*
 * i.MX drm driver - Raydium MIPI-DSI panel driver
 *
 * Copyright (C) 2017 NXP
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <drm/drmP.h>
#include <drm/drm_crtc.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/regulator/consumer.h>
#include <video/mipi_display.h>
#include <video/of_videomode.h>
#include <video/videomode.h>
#include <video/mipi_display.h>

#define JDI_BL_ENABLE 0

#define __DEBUG 1
#define __DBG_INFO 1
#if __DEBUG
#define print_dbg(fmt,args...) pr_info("LINH jdi %s(): DBG " fmt "\n", __func__, ##args)
#else
#define print_dbg(fmt,args...)
#endif /* __DEBUG */
#if __DBG_INFO
#define print_inf(fmt,args...) pr_info("LINH jdi %s(): INF " fmt "\n", __func__, ##args)
#else
#define print_inf(fmt,args...)
#endif
#define print_infa(fmt,args...) pr_info("LINH jdi %s(): INF " fmt "\n", __func__, ##args)
#define print_warn(fmt,args...) pr_info("LINH jdi %s(): WARN " fmt "\n", __func__, ##args)
#define print_err(fmt,args...) pr_err("LINH jdi %s(): ERR " fmt "\n", __func__, ##args)

#define WAIT_TYPE_US 1
#define WAIT_TYPE_MS 2
#define WAIT_TYPE_S  3

struct dsi_cmd_desc {
	int wait;
	int waittype;
	int dlen;
	char *payload;
};


/*******************************************************************************
** Power ON Sequence(sleep mode to Normal mode)
*/
static char adrsft1[] = {
	0x00,
	0x00,
};

static char cmd2_ena1[] = {
	0xff,
	0x19, 0x02, 0x01, 0x00,
};

static char adrsft2[] = {
	0x00,
	0x80,
};

static char cmd2_ena2[] = {
	0xff,
	0x19, 0x02,
};

static char adrsft3[] = {
	0x00,
	0x83,
};

static char adrsft4[] = {
	0xf3,
	0xca,
};

static char adrsft5[] = {
	0x00,
	0x90,
};

static char adrsft6[] = {
	0xc4,
	0x00,
};

static char adrsft7[] = {
	0x00,
	0xb4,
};

static char adrsft8[] = {
	0xc0,
	0xc0,
};

static char adrsft9[] = {
	0x00,
	0x87,
};

static char pixel_eyes_setting[] = {
	0xa4,
	0x15,
};

static char adrsft10[] = {
	0x00,
	0x00,
};
/*
static char caset_data[] = {
	0x2A,
	0x00,0x00,0x04, 0x37,
};
static char paset_data[] = {
	0x2B,
	0x00,0x00,0x07,0x7f,
};
*/
static char tear_on[] = {
	0x35,
	0x00,
};

static char bl_enable[] = {
	0x53,
	0x24,
};
/*******************************************************************************
**display effect
*/
//ce
//0x00 0x00
static char orise_clever_mode[] = {
	0x59,
	0x03,
};

static char addr_shift_a0[] = {
	0x00,
	0xa0,
};

static char ce_d6_1[] = {
	0xD6,
	0x03, 0x01, 0x00, 0x03, 0x03,
	0x00, 0xfd, 0x00, 0x03, 0x06,
	0x06, 0x02
};

static char addr_shift_b0[] = {
	0x00,
	0xb0,
};

static char ce_d6_2[] = {
	0xD6,
	0x00, 0x00, 0x66, 0xb3, 0xcd,
	0xb3, 0xcd, 0xb3, 0xa6, 0xb3,
	0xcd, 0xb3
};

static char addr_shift_c0[] = {
	0x00,
	0xc0,
};

static char ce_d6_3[] = {
	0xD6,
	0x26, 0x00, 0x89, 0x77, 0x89,
	0x77, 0x89, 0x77, 0x6f, 0x77,
	0x89, 0x77
};

static char addr_shift_d0[] = {
	0x00,
	0xd0,
};

static char ce_d6_4[] = {
	0xD6,
	0x26, 0x3c, 0x44, 0x3c, 0x44,
	0x3c, 0x44, 0x3c, 0x37, 0x3c,
	0x44, 0x3c
};

//0x00 0x80
static char ce_cmd[] = {
	0xD6,
	0x3A,
};

//sharpness
//0x00 0x00
static char sp_cmd[] = {
	0x59,
	0x03,
};

static char sp_shift_0x90[] = {
	0x00,
	0x90,
};

static char sp_D7_1[] = {
	0xD7,
	0x83,
};

static char sp_shift_0x92[] = {
	0x00,
	0x92,
};

static char sp_D7_2[] = {
	0xD7,
	0xff,
};

static char sp_shift_0x93[] = {
	0x00,
	0x93,
};

static char sp_D7_3[] = {
	0xD7,
	0x00,
};

//CABC
//0x00 0x00
//0x59 0x03
//0x00 0x80
static char cabc_ca[] = {
	0xCA,
	0x80,0x88,0x90,0x98,0xa0,
	0xa8,0xb0,0xb8,0xc0,0xc7,
	0xcf,0xd7,0xdf,0xe7,0xef,
	0xf7,0xcc,0xff,0xa5,0xff,
	0x80,0xff,0x53,0x53,0x53,
};
//0x00 0x00
static char cabc_c6_G1[] = {
	0xc6,
	0x10,
};
static char cabc_c7_G1[] = {
	0xC7,
	0xf0,0x8e,0xbc,0x9d,0xac,
	0x9c,0xac,0x9b,0xab,0x8c,
	0x67,0x55,0x45,0x44,0x44,
	0x44,0x44,0x44
};

//0x00 0x00
static char cabc_c6_G2[] = {
	0xc6,
	0x11,
};
static char cabc_c7_G2[] = {
	0xC7,
	0xf0,0xac,0xab,0xbc,0xba,
	0x9b,0xab,0xba,0xb8,0xab,
	0x78,0x56,0x55,0x44,0x44,
	0x44,0x44,0x44
};

//0x00 0x00
static char cabc_c6_G3[] = {
	0xc6,
	0x12,
};
static char cabc_c7_G3[] = {
	0xC7,
	0xf0,0xab,0xaa,0xab,0xab,
	0xab,0xaa,0xaa,0xa9,0x9b,
	0x8a,0x67,0x55,0x45,0x44,
	0x44,0x44,0x44
};

//0x00 0x00
static char cabc_c6_G4[] = {
	0xc6,
	0x13,
};
static char cabc_c7_G4[] = {
	0xC7,
	0xf0,0xaa,0xaa,0xab,0x9b,
	0x9b,0xaa,0xa9,0xa9,0xa9,
	0x9a,0x78,0x56,0x55,0x44,
	0x44,0x44,0x44
};

//0x00 0x00
static char cabc_c6_G5[] = {
	0xc6,
	0x14,
};
static char cabc_c7_G5[] = {
	0xC7,
	0xf0,0xa9,0xaa,0xab,0x9a,
	0xaa,0xa9,0xa9,0x8a,0xa9,
	0x99,0x8a,0x67,0x55,0x55,
	0x44,0x44,0x33
};

//0x00 0x00
static char cabc_c6_G6[] = {
	0xc6,
	0x15,
};
static char cabc_c7_G6[] = {
	0xC7,
	0xf0,0xa8,0xaa,0xaa,0x7b,
	0xab,0x99,0x9a,0x99,0x99,
	0x8b,0xa9,0x55,0x55,0x55,
	0x55,0x45,0x44
};

//0x00 0x00
static char cabc_c6_G7[] = {
	0xc6,
	0x16,
};
static char cabc_c7_G7[] = {
	0xC7,
	0xe0,0x99,0x7b,0x8d,0x7c,
	0x7b,0x8c,0x89,0x7b,0x8a,
	0x8a,0x89,0x68,0x55,0x55,
	0x55,0x55,0x55
};

//0x00 0x00
static char cabc_c6_G8[] = {
	0xc6,
	0x17,
};
static char cabc_c7_G8[] = {
	0xC7,
	0xf0,0x97,0xaa,0xaa,0x89,
	0xaa,0xa8,0x88,0x9a,0xa7,
	0xa8,0xa7,0x66,0x66,0x66,
	0x56,0x55,0x55
};

//0x00 0x00
static char cabc_c6_G9[] = {
	0xc6,
	0x18,
};
static char cabc_c7_G9[] = {
	0xC7,
	0xf0,0x87,0x9a,0x9b,0x8a,
	0xa9,0xa8,0x88,0xa9,0x87,
	0x9a,0x88,0x89,0x67,0x56,
	0x55,0x55,0x55
};

//0x00 0x00
static char cabc_c6_G10[] = {
	0xc6,
	0x19,
};
static char cabc_c7_G10[] = {
	0xC7,
	0xe0,0x97,0x8a,0x9b,0x8a,
	0x99,0x99,0x98,0xa8,0x87,
	0x8a,0x79,0x8a,0x67,0x66,
	0x56,0x55,0x55
};

//0x00 0x00
static char cabc_c6_G11[] = {
	0xc6,
	0x1a,
};
static char cabc_c7_G11[] = {
	0xC7,
	0xe0,0xa6,0x89,0xaa,0xa9,
	0x98,0x8a,0x88,0x89,0x79,
	0x7a,0x7a,0x98,0x78,0x66,
	0x66,0x56,0x45
};

//0x00 0x00
static char cabc_c6_G12[] = {
	0xc6,
	0x1b,
};
static char cabc_c7_G12[] = {
	0xC7,
	0xb0,0x99,0x99,0x99,0x9a,
	0x98,0x88,0x88,0x89,0x88,
	0x98,0x79,0x88,0x8a,0x67,
	0x66,0x66,0x55
};

//0x00 0x00
static char cabc_c6_G13[] = {
	0xc6,
	0x1c,
};
static char cabc_c7_G13[] = {
	0xC7,
	0xe0,0x96,0x89,0x9a,0x89,
	0xb7,0x88,0x88,0x88,0x88,
	0x88,0x89,0x87,0xa8,0x98,
	0x58,0x55,0x55
};

//0x00 0x00
static char cabc_c6_G14[] = {
	0xc6,
	0x1d,
};
static char cabc_c7_G14[] = {
	0xC7,
	0xc0,0x88,0x89,0x99,0x8a,
	0xa7,0x89,0x88,0x88,0x97,
	0x97,0x97,0x78,0x98,0x79,
	0xa8,0x48,0x34
};

//0x00 0x00
static char cabc_c6_G15[] = {
	0xc6,
	0x1e,
};
static char cabc_c7_G15[] = {
	0xC7,
	0xc0,0x88,0x89,0x99,0x8a,
	0xa7,0x89,0x88,0x88,0x97,
	0x97,0x97,0x78,0x98,0x79,
	0xa8,0x48,0x34
};

//0x00 0x00
static char cabc_c6_G16[] = {
	0xc6,
	0x1f,
};
static char cabc_c7_G16[] = {
	0xC7,
	0xc0,0x88,0x89,0x99,0x8a,
	0xa7,0x89,0x88,0x88,0x97,
	0x97,0x97,0x78,0x98,0x79,
	0xa8,0x48,0x34
};

static char cabc_shift_UI[] = {
	0x00,
	0x90,
};

static char cabc_UI_mode[] = {
	0xCA,
	0xE6, 0xFF,
};

static char cabc_shift_STILL[] = {
	0x00,
	0x92,
};

static char cabc_STILL_mode[] = {
	0xCA,
	0xA5, 0xFF,
};

static char cabc_shift_moving[] = {
	0x00,
	0x94,
};

static char cabc_moving_mode[] = {
	0xCA,
	0x80, 0xFF,
};

//0x00 0x00
static char cabc_disable_curve[] = {
	0xc6,
	0x00,
};

//0x00 0x00
static char cabc_disable_setting[] = {
	0x59,
	0x00,
};

static char cabc_53[] = {
	0x53,
	0x2c,
};
static char cabc_set_mode_UI[] = {
	0x55,
	0x91,
};
/*static char cabc_set_mode_STILL[] = {
	0x55,
	0x92,
};*/
static char cabc_set_mode_MOVING[] = {
	0x55,
	0x93,
};

/*******************************************************************************
** Power OFF Sequence(Normal to power off)
*/
static char exit_sleep[] = {
	0x11,
};

static char display_on[] = {
	0x29,
};

static char display_off[] = {
	0x28,
};

static char enter_sleep[] = {
	0x10,
};

static char Delay_TE[] = {
	0x44,
	0x07, 0x80,
};

/*static char soft_reset[] = {
	0x01,
};*/

static char clever_edge_shift[] = {
	0x00,
	0x81,
};

static char clever_edge_value[] = {
	0xc0,
	0x73,
};

#if 1
static char for_b5_disturb_shift1[] = {
	0x00,
	0x82,
};
static char for_b5_disturb_value1[] = {
	0xc4,
	0x00,
};
static char for_b5_disturb_shift2[] = {
	0x00,
	0x83,
};
static char for_b5_disturb_value2[] = {
	0xc4,
	0x02,
};
static char for_b5_disturb_shift3[] = {
	0x00,
	0x80,
};
static char for_b5_disturb_value3[] = {
	0xa5,
	0x0c,
};
static char for_b5_disturb_shift4[] = {
	0x00,
	0x81,
};
static char for_b5_disturb_value4[] = {
	0xa5,
	0x04,
};
static char for_b5_disturb_shift5[] = {
	0x00,
	0x83,
};
static char for_b5_disturb_value5[] = {
	0xa4,
	0x20,
};
static char for_b5_disturb_shift6[] = {
	0x00,
	0x89,
};
static char for_b5_disturb_value6[] = {
	0xa4,
	0x00,
};
static char for_b5_disturb_shift7[] = {
	0x00,
	0xe2,
};
static char for_b5_disturb_value7[] = {
	0xf5,
	0x02,
};
#endif

static struct dsi_cmd_desc jdi_display_on_cmd[] = {
	{10, WAIT_TYPE_US,
		sizeof(adrsft1), adrsft1},
	{10, WAIT_TYPE_US,
		sizeof(cmd2_ena1), cmd2_ena1},
	{10, WAIT_TYPE_US,
		sizeof(adrsft2), adrsft2},
	{10, WAIT_TYPE_US,
		sizeof(cmd2_ena2), cmd2_ena2},
	//CE
	{10, WAIT_TYPE_US,
		sizeof(adrsft1), adrsft1},
	{10, WAIT_TYPE_US,
		sizeof(orise_clever_mode), orise_clever_mode},
	{10, WAIT_TYPE_US,
		sizeof(addr_shift_a0), addr_shift_a0},
	{10, WAIT_TYPE_US,
		sizeof(ce_d6_1), ce_d6_1},
	{10, WAIT_TYPE_US,
		sizeof(addr_shift_b0), addr_shift_b0},
	{10, WAIT_TYPE_US,
		sizeof(ce_d6_2), ce_d6_2},
	{10, WAIT_TYPE_US,
		sizeof(addr_shift_c0), addr_shift_c0},
	{10, WAIT_TYPE_US,
		sizeof(ce_d6_3), ce_d6_3},
	{10, WAIT_TYPE_US,
		sizeof(addr_shift_d0), addr_shift_d0},
	{10, WAIT_TYPE_US,
		sizeof(ce_d6_4), ce_d6_4},
	{10, WAIT_TYPE_US,
		sizeof(adrsft2), adrsft2},
	{10, WAIT_TYPE_US,
		sizeof(ce_cmd), ce_cmd},
	//sharpness
	{10, WAIT_TYPE_US,
		sizeof(adrsft1), adrsft1},
	{10, WAIT_TYPE_US,
		sizeof(sp_cmd), sp_cmd},
	{10, WAIT_TYPE_US,
		sizeof(sp_shift_0x90), sp_shift_0x90},
	{10, WAIT_TYPE_US,
		sizeof(sp_D7_1), sp_D7_1},
	{10, WAIT_TYPE_US,
		sizeof(sp_shift_0x92), sp_shift_0x92},
	{10, WAIT_TYPE_US,
		sizeof(sp_D7_2), sp_D7_2},
	{10, WAIT_TYPE_US,
		sizeof(sp_shift_0x93), sp_shift_0x93},
	{10, WAIT_TYPE_US,
		sizeof(sp_D7_3), sp_D7_3},
	//cabc
	{10, WAIT_TYPE_US,
		sizeof(adrsft2), adrsft2},
	{10, WAIT_TYPE_US,
		sizeof(cabc_ca), cabc_ca},
	{10, WAIT_TYPE_US,
		sizeof(adrsft1), adrsft1},
	{10, WAIT_TYPE_US,
		sizeof(cabc_c6_G1), cabc_c6_G1},
	{10, WAIT_TYPE_US,
		sizeof(adrsft1), adrsft1},
	{10, WAIT_TYPE_US,
		sizeof(cabc_c7_G1), cabc_c7_G1},
	{10, WAIT_TYPE_US,
		sizeof(adrsft1), adrsft1},
	{10, WAIT_TYPE_US,
		sizeof(cabc_c6_G2), cabc_c6_G2},
	{10, WAIT_TYPE_US,
		sizeof(adrsft1), adrsft1},
	{10, WAIT_TYPE_US,
		sizeof(cabc_c7_G2), cabc_c7_G2},
	{10, WAIT_TYPE_US,
		sizeof(adrsft1), adrsft1},
	{10, WAIT_TYPE_US,
		sizeof(cabc_c6_G3), cabc_c6_G3},
	{10, WAIT_TYPE_US,
		sizeof(adrsft1), adrsft1},
	{10, WAIT_TYPE_US,
		sizeof(cabc_c7_G3), cabc_c7_G3},
	{10, WAIT_TYPE_US,
		sizeof(adrsft1), adrsft1},
	{10, WAIT_TYPE_US,
		sizeof(cabc_c6_G4), cabc_c6_G4},
	{10, WAIT_TYPE_US,
		sizeof(adrsft1), adrsft1},
	{10, WAIT_TYPE_US,
		sizeof(cabc_c7_G4), cabc_c7_G4},
	{10, WAIT_TYPE_US,
		sizeof(adrsft1), adrsft1},
	{10, WAIT_TYPE_US,
		sizeof(cabc_c6_G5), cabc_c6_G5},
	{10, WAIT_TYPE_US,
		sizeof(adrsft1), adrsft1},
	{10, WAIT_TYPE_US,
		sizeof(cabc_c7_G5), cabc_c7_G5},
	{10, WAIT_TYPE_US,
		sizeof(adrsft1), adrsft1},
	{10, WAIT_TYPE_US,
		sizeof(cabc_c6_G6), cabc_c6_G6},
	{10, WAIT_TYPE_US,
		sizeof(adrsft1), adrsft1},
	{10, WAIT_TYPE_US,
		sizeof(cabc_c7_G6), cabc_c7_G6},
	{10, WAIT_TYPE_US,
		sizeof(adrsft1), adrsft1},
	{10, WAIT_TYPE_US,
		sizeof(cabc_c6_G7), cabc_c6_G7},
	{10, WAIT_TYPE_US,
		sizeof(adrsft1), adrsft1},
	{10, WAIT_TYPE_US,
		sizeof(cabc_c7_G7), cabc_c7_G7},
	{10, WAIT_TYPE_US,
		sizeof(adrsft1), adrsft1},
	{10, WAIT_TYPE_US,
		sizeof(cabc_c6_G8), cabc_c6_G8},
	{10, WAIT_TYPE_US,
		sizeof(adrsft1), adrsft1},
	{10, WAIT_TYPE_US,
		sizeof(cabc_c7_G8), cabc_c7_G8},
	{10, WAIT_TYPE_US,
		sizeof(adrsft1), adrsft1},
	{10, WAIT_TYPE_US,
		sizeof(cabc_c6_G9), cabc_c6_G9},
	{10, WAIT_TYPE_US,
		sizeof(adrsft1), adrsft1},
	{10, WAIT_TYPE_US,
		sizeof(cabc_c7_G9), cabc_c7_G9},
	{10, WAIT_TYPE_US,
		sizeof(adrsft1), adrsft1},
	{10, WAIT_TYPE_US,
		sizeof(cabc_c6_G10), cabc_c6_G10},
	{10, WAIT_TYPE_US,
		sizeof(adrsft1), adrsft1},
	{10, WAIT_TYPE_US,
		sizeof(cabc_c7_G10), cabc_c7_G10},
	{10, WAIT_TYPE_US,
		sizeof(adrsft1), adrsft1},
	{10, WAIT_TYPE_US,
		sizeof(cabc_c6_G11), cabc_c6_G11},
	{10, WAIT_TYPE_US,
		sizeof(adrsft1), adrsft1},
	{10, WAIT_TYPE_US,
		sizeof(cabc_c7_G11), cabc_c7_G11},
	{10, WAIT_TYPE_US,
		sizeof(adrsft1), adrsft1},
	{10, WAIT_TYPE_US,
		sizeof(cabc_c6_G12), cabc_c6_G12},
	{10, WAIT_TYPE_US,
		sizeof(adrsft1), adrsft1},
	{10, WAIT_TYPE_US,
		sizeof(cabc_c7_G12), cabc_c7_G12},
	{10, WAIT_TYPE_US,
		sizeof(adrsft1), adrsft1},
	{10, WAIT_TYPE_US,
		sizeof(cabc_c6_G13), cabc_c6_G13},
	{10, WAIT_TYPE_US,
		sizeof(adrsft1), adrsft1},
	{10, WAIT_TYPE_US,
		sizeof(cabc_c7_G13), cabc_c7_G13},
	{10, WAIT_TYPE_US,
		sizeof(adrsft1), adrsft1},
	{10, WAIT_TYPE_US,
		sizeof(cabc_c6_G14), cabc_c6_G14},
	{10, WAIT_TYPE_US,
		sizeof(adrsft1), adrsft1},
	{10, WAIT_TYPE_US,
		sizeof(cabc_c7_G14), cabc_c7_G14},
	{10, WAIT_TYPE_US,
		sizeof(adrsft1), adrsft1},
	{10, WAIT_TYPE_US,
		sizeof(cabc_c6_G15), cabc_c6_G15},
	{10, WAIT_TYPE_US,
		sizeof(adrsft1), adrsft1},
	{10, WAIT_TYPE_US,
		sizeof(cabc_c7_G15), cabc_c7_G15},
	{10, WAIT_TYPE_US,
		sizeof(adrsft1), adrsft1},
	{10, WAIT_TYPE_US,
		sizeof(cabc_c6_G16), cabc_c6_G16},
	{10, WAIT_TYPE_US,
		sizeof(adrsft1), adrsft1},
	{10, WAIT_TYPE_US,
		sizeof(cabc_c7_G16), cabc_c7_G16},
	{10, WAIT_TYPE_US,
		sizeof(cabc_shift_UI), cabc_shift_UI},
	{10, WAIT_TYPE_US,
		sizeof(cabc_UI_mode), cabc_UI_mode},
	{10, WAIT_TYPE_US,
		sizeof(cabc_shift_STILL), cabc_shift_STILL},
	{10, WAIT_TYPE_US,
		sizeof(cabc_STILL_mode), cabc_STILL_mode},
	{10, WAIT_TYPE_US,
		sizeof(cabc_shift_moving), cabc_shift_moving},
	{10, WAIT_TYPE_US,
		sizeof(cabc_moving_mode), cabc_moving_mode},
	{10, WAIT_TYPE_US,
		sizeof(adrsft1), adrsft1},
	{10, WAIT_TYPE_US,
		sizeof(cabc_disable_curve), cabc_disable_curve},
	{10, WAIT_TYPE_US,
		sizeof(adrsft1), adrsft1},
	{10, WAIT_TYPE_US,
		sizeof(cabc_disable_setting), cabc_disable_setting},
	{10, WAIT_TYPE_US,
		sizeof(cabc_53), cabc_53},
	{1, WAIT_TYPE_MS,
		sizeof(adrsft3), adrsft3},
	{10, WAIT_TYPE_US,
		sizeof(adrsft4), adrsft4},
	{10, WAIT_TYPE_US,
		sizeof(adrsft5), adrsft5},
	{10, WAIT_TYPE_US,
		sizeof(adrsft6), adrsft6},
	{10, WAIT_TYPE_US,
		sizeof(adrsft7), adrsft7},
	{10, WAIT_TYPE_US,
		sizeof(adrsft8), adrsft8},
	{10, WAIT_TYPE_US,
		sizeof(adrsft9), adrsft9},
	{10, WAIT_TYPE_US,
		sizeof(pixel_eyes_setting), pixel_eyes_setting},
	{10, WAIT_TYPE_US,
		sizeof(adrsft10), adrsft10},
};

static struct dsi_cmd_desc jdi_display_on_cmd1[] = {
	{10, WAIT_TYPE_US,
		sizeof(tear_on), tear_on},
	//{10, WAIT_TYPE_US,
	//	sizeof(caset_data), caset_data},
	//{10, WAIT_TYPE_US,
	//	sizeof(paset_data), paset_data},
	{200, WAIT_TYPE_US,
		sizeof(bl_enable), bl_enable},
	{200, WAIT_TYPE_US,
		sizeof(Delay_TE), Delay_TE},
	{10, WAIT_TYPE_US,
		sizeof(clever_edge_shift), clever_edge_shift},
	{10, WAIT_TYPE_US,
		sizeof(clever_edge_value), clever_edge_value},
	{10, WAIT_TYPE_MS,
		sizeof(exit_sleep), exit_sleep},
	{100, WAIT_TYPE_MS,
		sizeof(display_on), display_on},
#if 1
	{200, WAIT_TYPE_US,
		sizeof(for_b5_disturb_shift7), for_b5_disturb_shift1},
	{200, WAIT_TYPE_US,
		sizeof(for_b5_disturb_value7), for_b5_disturb_value1},
	{200, WAIT_TYPE_US,
		sizeof(for_b5_disturb_shift7), for_b5_disturb_shift2},
	{200, WAIT_TYPE_US,
		sizeof(for_b5_disturb_value7), for_b5_disturb_value2},
	{200, WAIT_TYPE_US,
		sizeof(for_b5_disturb_shift7), for_b5_disturb_shift3},
	{200, WAIT_TYPE_US,
		sizeof(for_b5_disturb_value7), for_b5_disturb_value3},
	{200, WAIT_TYPE_US,
		sizeof(for_b5_disturb_shift7), for_b5_disturb_shift4},
	{200, WAIT_TYPE_US,
		sizeof(for_b5_disturb_value7), for_b5_disturb_value4},
	{200, WAIT_TYPE_US,
		sizeof(for_b5_disturb_shift7), for_b5_disturb_shift5},
	{200, WAIT_TYPE_US,
		sizeof(for_b5_disturb_value7), for_b5_disturb_value5},
	{200, WAIT_TYPE_US,
		sizeof(for_b5_disturb_shift7), for_b5_disturb_shift6},
	{200, WAIT_TYPE_US,
		sizeof(for_b5_disturb_value7), for_b5_disturb_value6},
	{200, WAIT_TYPE_US,
		sizeof(for_b5_disturb_shift7), for_b5_disturb_shift7},
	{200, WAIT_TYPE_US,
		sizeof(for_b5_disturb_value7), for_b5_disturb_value7},
#endif
};

static struct dsi_cmd_desc jdi_cabc_ui_on_cmds[] = {
	{10, WAIT_TYPE_US,
		sizeof(cabc_set_mode_UI), cabc_set_mode_UI},
};

/*static struct dsi_cmd_desc jdi_cabc_still_on_cmds[] = {
	{10, WAIT_TYPE_US,
		sizeof(cabc_set_mode_STILL), cabc_set_mode_STILL},
};*/

static struct dsi_cmd_desc jdi_cabc_moving_on_cmds[] = {
	{10, WAIT_TYPE_US,
		sizeof(cabc_set_mode_MOVING), cabc_set_mode_MOVING},
};
static struct dsi_cmd_desc jdi_display_off_cmds[] = {
	{60, WAIT_TYPE_MS,
		sizeof(display_off), display_off},
	{120, WAIT_TYPE_MS,
		sizeof(enter_sleep), enter_sleep}
};

static char command_2_enable[] = {
	0x00,
	0x00,
};

static char command_2_enable_1_para[] = {
	0xFF,
	0x19, 0x02, 0x01,
};

static char command_2_enable_2[] = {
	0x00,
	0x80,
};

static char command_2_enable_2_para[] = {
	0xFF,
	0x19, 0x02,
};

static char HD1080_setting_1_para[] = {
	0x2a,
	0x00, 0x00, 0x04, 0x37,
};

static char HD1080_setting_2_para[] = {
	0x2b,
	0x00, 0x00, 0x07, 0x7f,
};

static char cleveredge_1_5x_para[] = {
	0x1C,
	0x05,
};

static char cleveredge_disable[] = {
	0x1C,
	0x00,
};

static char cleveredge_P1[] = {
	0x00,
	0x91,
};

static char cleveredge_P1_para[] = {
	0xD7,
	0xC8,
};

static char cleveredge_P2[] = {
	0x00,
	0x93,
};

static char cleveredge_P2_para[] = {
	0xD7,
	0x08,
};

static char cleveredge_use_setting[] = {
	0x00,
	0xAC,
};

static char cleveredge_use_setting_para[] = {
	0xC0,
	0x04,
};

static char command_2_disable_para[] = {
	0xFF,
	0xFF, 0xFF, 0xFF, 0xFF,
};

static char command_clevermode[] = {
	0x59,
	0x03,
};

#if 0
static struct dsi_cmd_desc cleveredge_inital_720P_cmds[] = {
	{100, WAIT_TYPE_US,
		sizeof(command_2_enable), command_2_enable},
	{100, WAIT_TYPE_US,
		sizeof(command_2_enable_1_para), command_2_enable_1_para},
	{100, WAIT_TYPE_US,
		sizeof(command_2_enable_2), command_2_enable_2},
	{100, WAIT_TYPE_US,
		sizeof(command_2_enable_2_para), command_2_enable_2_para},
	{100, WAIT_TYPE_US,
		sizeof(command_2_enable), command_2_enable},
	{100, WAIT_TYPE_US,
		sizeof(command_clevermode), command_clevermode},
	{100, WAIT_TYPE_US,
		sizeof(command_2_enable), command_2_enable},
	{100, WAIT_TYPE_US,
		sizeof(HD720_setting_1_para), HD720_setting_1_para},
	{100, WAIT_TYPE_US,
		sizeof(command_2_enable), command_2_enable},
	{100, WAIT_TYPE_US,
		sizeof(HD720_setting_2_para), HD720_setting_2_para},
	{100, WAIT_TYPE_US,
		sizeof(command_2_enable), command_2_enable},
	{100, WAIT_TYPE_US,
		sizeof(cleveredge_1_5x_para), cleveredge_1_5x_para},
	{100, WAIT_TYPE_US,
		sizeof(cleveredge_P1), cleveredge_P1},
	{100, WAIT_TYPE_US,
		sizeof(cleveredge_P1_para), cleveredge_P1_para},
	{100, WAIT_TYPE_US,
		sizeof(cleveredge_P2), cleveredge_P2},
	{100, WAIT_TYPE_US,
		sizeof(cleveredge_P2_para), cleveredge_P2_para},
	{100, WAIT_TYPE_US,
		sizeof(cleveredge_use_setting), cleveredge_use_setting},
	{100, WAIT_TYPE_US,
		sizeof(cleveredge_use_setting_para), cleveredge_use_setting_para},
	{100, WAIT_TYPE_US,
		sizeof(command_2_enable), command_2_enable},
	{100, WAIT_TYPE_US,
		sizeof(command_2_disable_para), command_2_disable_para}
};
#endif

static struct dsi_cmd_desc cleveredge_inital_1080P_cmds[] = {
	{100, WAIT_TYPE_US,
		sizeof(command_2_enable), command_2_enable},
	{100, WAIT_TYPE_US,
		sizeof(command_2_enable_1_para), command_2_enable_1_para},
	{100, WAIT_TYPE_US,
		sizeof(command_2_enable_2), command_2_enable_2},
	{100, WAIT_TYPE_US,
		sizeof(command_2_enable_2_para), command_2_enable_2_para},
	{100, WAIT_TYPE_US,
		sizeof(command_2_enable), command_2_enable},
	{100, WAIT_TYPE_US,
		sizeof(HD1080_setting_1_para), HD1080_setting_1_para},
	{100, WAIT_TYPE_US,
		sizeof(command_2_enable), command_2_enable},
	{100, WAIT_TYPE_US,
		sizeof(HD1080_setting_2_para), HD1080_setting_2_para},
	{100, WAIT_TYPE_US,
		sizeof(command_2_enable), command_2_enable},
	{100, WAIT_TYPE_US,
		sizeof(cleveredge_disable), cleveredge_disable},
	{10, WAIT_TYPE_US,
		sizeof(sp_shift_0x93), sp_shift_0x93},
	{10, WAIT_TYPE_US,
		sizeof(sp_D7_3), sp_D7_3},
};

static const u32 jdi_bus_formats[] = {
	MEDIA_BUS_FMT_RGB888_1X24,
	MEDIA_BUS_FMT_RGB666_1X18,
	MEDIA_BUS_FMT_RGB565_1X16,
};

struct jdi_panel {
	struct drm_panel base;
	struct mipi_dsi_device *dsi;

	struct gpio_desc *reset;
	struct gpio_desc *tpreset;
#if JDI_BL_ENABLE
	struct backlight_device *backlight;
#endif

	bool prepared;
	bool enabled;

	struct videomode vm;
	u32 width_mm;
	u32 height_mm;
};

static inline struct jdi_panel *to_jdi_panel(struct drm_panel *panel)
{
	return container_of(panel, struct jdi_panel, base);
}

static int mipi_dsi_cmds_tx(struct dsi_cmd_desc *cmds, int cnt, struct jdi_panel *ctx)
{
	int i;
	int ret;
	struct dsi_cmd_desc *cm = cmds;

	for (i = 0; i < cnt; i++) {
		ret = mipi_dsi_dcs_write_buffer(ctx->dsi, cm->payload, cm->dlen);
		if (ret < 0) {
			print_err("dsi write buf %dbytes failed, %d", cm->dlen, ret);
		}

		if (cm->wait) {
			if (cm->waittype == WAIT_TYPE_US)
				udelay(cm->wait);
			else if (cm->waittype == WAIT_TYPE_MS)
				mdelay(cm->wait);
			else
				mdelay(cm->wait * 1000);
		}
		cm++;
	}

	return cnt;
}

static int color_format_from_dsi_format(enum mipi_dsi_pixel_format format)
{
	switch (format) {
	case MIPI_DSI_FMT_RGB565:
		return 0x55;
	case MIPI_DSI_FMT_RGB666:
	case MIPI_DSI_FMT_RGB666_PACKED:
		return 0x66;
	case MIPI_DSI_FMT_RGB888:
		return 0x77;
	default:
		return 0x77; /* for backward compatibility */
	}
};

static int jdi_panel_prepare(struct drm_panel *panel)
{
	struct jdi_panel *ctx = to_jdi_panel(panel);

	print_dbg("prepared %d", ctx->prepared);
	if (ctx->prepared)
		return 0;

	if (!IS_ERR(ctx->reset)) {
		gpiod_set_raw_value(ctx->reset, 1); // High for > 15ms
		msleep(20);

		print_dbg("reset Panel");
		gpiod_set_raw_value(ctx->reset, 0); // Low for > 20us
		msleep(20);

		print_dbg("panel reseted");
		gpiod_set_raw_value(ctx->reset, 1); // Keep high & wait for > 10ms
		msleep(20);
	}
	if (!IS_ERR(ctx->tpreset)) {
		print_dbg("reset Touch");
		gpiod_set_raw_value(ctx->tpreset, 0);
		msleep(20);
		gpiod_set_raw_value(ctx->tpreset, 1);
		msleep(60);
	}

	ctx->prepared = true;

	return 0;
}

static int jdi_panel_unprepare(struct drm_panel *panel)
{
	struct jdi_panel *ctx = to_jdi_panel(panel);
	struct device *dev = &ctx->dsi->dev;

	print_dbg("prepared %d", ctx->prepared);
	if (!ctx->prepared)
		return 0;

	if (ctx->enabled) {
		DRM_DEV_ERROR(dev, "Panel still enabled!\n");
		return -EPERM;
	}

	if (!IS_ERR(ctx->reset)) {
		print_dbg("poweroff Panel");
		gpiod_set_raw_value(ctx->reset, 0);
		msleep(20);
	}
	if (!IS_ERR(ctx->tpreset)) {
		print_dbg("poweroff Touch");
		gpiod_set_raw_value(ctx->tpreset, 0);
		msleep(20);
	}

	ctx->prepared = false;

	return 0;
}

static int jdi_panel_enable(struct drm_panel *panel)
{
	struct jdi_panel *ctx = to_jdi_panel(panel);
	struct mipi_dsi_device *dsi = ctx->dsi;
	struct device *dev = &dsi->dev;
	int color_format = color_format_from_dsi_format(dsi->format);
	int ret;
	u16 brightness;
	u8 tmp;
#if JDI_BL_ENABLE
	u16 brightness;
#endif

	print_dbg("enabled %d, prepared %d", ctx->enabled, ctx->prepared);
	if (ctx->enabled)
		return 0;

	if (!ctx->prepared) {
		DRM_DEV_ERROR(dev, "Panel not prepared!\n");
		return -EPERM;
	}

	dsi->mode_flags |= MIPI_DSI_MODE_LPM;

	ret = mipi_dsi_cmds_tx(cleveredge_inital_1080P_cmds, ARRAY_SIZE(cleveredge_inital_1080P_cmds), ctx);
	print_dbg("clever init 1080p done");
	ret |= mipi_dsi_cmds_tx(jdi_display_on_cmd, ARRAY_SIZE(jdi_display_on_cmd), ctx);
	print_dbg("display on 0 done");
	ret |= mipi_dsi_cmds_tx(jdi_display_on_cmd1, ARRAY_SIZE(jdi_display_on_cmd1), ctx);
	print_dbg("display on 1 done");
	ret |= mipi_dsi_cmds_tx(jdi_cabc_ui_on_cmds, ARRAY_SIZE(jdi_cabc_ui_on_cmds), ctx);
	print_dbg("cabc ui on done");
	if (ret < 0) {
		DRM_DEV_ERROR(dev, "Failed to do turn panel on (%d)\n", ret);
		goto fail;
	}

	// /* Set both MCU and RGB I/F to 24bpp */
	// ret = mipi_dsi_dcs_set_pixel_format(dsi, MIPI_DCS_PIXEL_FMT_24BIT |
	// 				(MIPI_DCS_PIXEL_FMT_24BIT << 4));
	// if (ret < 0){
	// 	DRM_DEV_ERROR(dev, "set pixel format ret (%d)\n", ret);
	// 	goto fail;
	// }

	/* Software reset */
	ret = mipi_dsi_dcs_soft_reset(dsi);
	if (ret < 0) {
		DRM_DEV_ERROR(dev, "Failed to do Software Reset (%d)\n", ret);
		goto fail;
	}

	usleep_range(15000, 17000);

	/* Set tear ON */
	ret = mipi_dsi_dcs_set_tear_on(dsi, MIPI_DSI_DCS_TEAR_MODE_VHBLANK);
	if (ret < 0) {
		DRM_DEV_ERROR(dev, "Failed to set tear ON (%d)\n", ret);
		goto fail;
	}
	/* Set tear scanline */
	ret = mipi_dsi_dcs_set_tear_scanline(dsi, 0);
	if (ret < 0) {
		DRM_DEV_ERROR(dev, "Failed to set tear scanline (%d)\n", ret);
		goto fail;
	}
	/* Set pixel format */
	ret = mipi_dsi_dcs_set_pixel_format(dsi, color_format);
	DRM_DEV_DEBUG_DRIVER(dev, "Interface color format set to 0x%x\n",
				color_format);
	if (ret < 0) {
		DRM_DEV_ERROR(dev, "Failed to set pixel format (%d)\n", ret);
		goto fail;
	}

#if JDI_BL_ENABLE
	/* Set display brightness */
	brightness = ctx->backlight->props.brightness;
	ret = mipi_dsi_dcs_set_display_brightness(dsi, brightness);
	if (ret < 0) {
		DRM_DEV_ERROR(dev, "Failed to set display brightness (%d)\n",
			      ret);
		goto fail;
	}
#endif

	/* Exit sleep mode */
	ret = mipi_dsi_dcs_exit_sleep_mode(dsi);
	if (ret < 0) {
		DRM_DEV_ERROR(dev, "Failed to exit sleep mode (%d)\n", ret);
		goto fail;
	}

	usleep_range(5000, 10000);

	ret = mipi_dsi_dcs_set_display_on(dsi);
	if (ret < 0) {
		DRM_DEV_ERROR(dev, "Failed to set display ON (%d)\n", ret);
		goto fail;
	}

#if JDI_BL_ENABLE
	backlight_enable(ctx->backlight);
#endif

	brightness = 0;
	ret = mipi_dsi_dcs_get_display_brightness(dsi, &brightness);
	print_dbg("get brightness ret %d, val %u", ret, brightness);

	tmp = 0;
	ret = mipi_dsi_dcs_get_power_mode(ctx->dsi, &tmp);
	print_dbg("get power mode ret %d, val %u", ret, tmp);

	tmp = 0;
	ret = mipi_dsi_dcs_get_pixel_format(ctx->dsi, &tmp);
	print_dbg("get pixel format ret %d, val %u", ret, tmp);

	ctx->enabled = true;

	return 0;

fail:
	if (ctx->reset != NULL)
		gpiod_set_value(ctx->reset, 1);

	return ret;
}

static int jdi_panel_disable(struct drm_panel *panel)
{
	struct jdi_panel *ctx = to_jdi_panel(panel);
	struct mipi_dsi_device *dsi = ctx->dsi;
	struct device *dev = &dsi->dev;
	int ret;

	print_dbg("enabled %d, prepared %d", ctx->enabled, ctx->prepared);

	if (!ctx->enabled)
		return 0;

	dsi->mode_flags |= MIPI_DSI_MODE_LPM;

#if JDI_BL_ENABLE
	backlight_disable(ctx->backlight);
#endif

	usleep_range(10000, 15000);

	ret = mipi_dsi_dcs_set_display_off(dsi);
	if (ret < 0) {
		DRM_DEV_ERROR(dev, "Failed to set display OFF (%d)\n", ret);
		return ret;
	}

	usleep_range(5000, 10000);

	ret = mipi_dsi_dcs_enter_sleep_mode(dsi);
	if (ret < 0) {
		DRM_DEV_ERROR(dev, "Failed to enter sleep mode (%d)\n", ret);
		return ret;
	}

	ctx->enabled = false;

	return 0;
}

static int jdi_panel_get_modes(struct drm_panel *panel)
{
	struct jdi_panel *ctx = to_jdi_panel(panel);
	struct device *dev = &ctx->dsi->dev;
	struct drm_connector *connector = panel->connector;
	struct drm_display_mode *mode;
	u32 *bus_flags = &connector->display_info.bus_flags;
	int ret;

	mode = drm_mode_create(connector->dev);
	if (!mode) {
		DRM_DEV_ERROR(dev, "Failed to create display mode!\n");
		return 0;
	}

	drm_display_mode_from_videomode(&ctx->vm, mode);
	mode->width_mm = ctx->width_mm;
	mode->height_mm = ctx->height_mm;
	connector->display_info.width_mm = ctx->width_mm;
	connector->display_info.height_mm = ctx->height_mm;
	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;

	if (ctx->vm.flags & DISPLAY_FLAGS_DE_HIGH)
		*bus_flags |= DRM_BUS_FLAG_DE_HIGH;
	if (ctx->vm.flags & DISPLAY_FLAGS_DE_LOW)
		*bus_flags |= DRM_BUS_FLAG_DE_LOW;
	if (ctx->vm.flags & DISPLAY_FLAGS_PIXDATA_NEGEDGE)
		*bus_flags |= DRM_BUS_FLAG_PIXDATA_NEGEDGE;
	if (ctx->vm.flags & DISPLAY_FLAGS_PIXDATA_POSEDGE)
		*bus_flags |= DRM_BUS_FLAG_PIXDATA_POSEDGE;

	ret = drm_display_info_set_bus_formats(&connector->display_info,
			jdi_bus_formats, ARRAY_SIZE(jdi_bus_formats));
	if (ret)
		return ret;

	drm_mode_probed_add(panel->connector, mode);

	return 1;
}

#if JDI_BL_ENABLE
static int jdi_bl_get_brightness(struct backlight_device *bl)
{
	struct mipi_dsi_device *dsi = bl_get_data(bl);
	struct jdi_panel *ctx = mipi_dsi_get_drvdata(dsi);
	struct device *dev = &dsi->dev;
	u16 brightness;
	int ret;

	if (!ctx->prepared)
		return 0;

	DRM_DEV_DEBUG_DRIVER(dev, "\n");

	dsi->mode_flags &= ~MIPI_DSI_MODE_LPM;

	ret = mipi_dsi_dcs_get_display_brightness(dsi, &brightness);
	if (ret < 0)
		return ret;

	bl->props.brightness = brightness;

	return brightness & 0xff;
}

static int jdi_bl_update_status(struct backlight_device *bl)
{
	struct mipi_dsi_device *dsi = bl_get_data(bl);
	struct jdi_panel *ctx = mipi_dsi_get_drvdata(dsi);
	struct device *dev = &dsi->dev;
	int ret = 0;

	if (!ctx->prepared)
		return 0;

	DRM_DEV_DEBUG_DRIVER(dev, "New brightness: %d\n", bl->props.brightness);

	dsi->mode_flags &= ~MIPI_DSI_MODE_LPM;

	ret = mipi_dsi_dcs_set_display_brightness(dsi, bl->props.brightness);
	if (ret < 0)
		return ret;

	return 0;
}

static const struct backlight_ops jdi_bl_ops = {
	.update_status = jdi_bl_update_status,
	.get_brightness = jdi_bl_get_brightness,
};
#endif

static const struct drm_panel_funcs jdi_panel_funcs = {
	.prepare = jdi_panel_prepare,
	.unprepare = jdi_panel_unprepare,
	.enable = jdi_panel_enable,
	.disable = jdi_panel_disable,
	.get_modes = jdi_panel_get_modes,
};

/*
 * The clock might range from 66MHz (30Hz refresh rate)
 * to 132MHz (60Hz refresh rate)
 */
static const struct display_timing ctx_default_timing = {
	.pixelclock = { 66000000, 132000000, 132000000 },
	.hactive = { 1080, 1080, 1080 },
	.hfront_porch = { 20, 20, 20 },
	.hsync_len = { 2, 2, 2 },
	.hback_porch = { 34, 34, 34 },
	.vactive = { 1920, 1920, 1920 },
	.vfront_porch = { 10, 10, 10 },
	.vsync_len = { 2, 2, 2 },
	.vback_porch = { 4, 4, 4 },
	.flags = DISPLAY_FLAGS_HSYNC_LOW |
		 DISPLAY_FLAGS_VSYNC_LOW |
		 DISPLAY_FLAGS_DE_LOW |
		 DISPLAY_FLAGS_PIXDATA_NEGEDGE,
};

static int jdi_panel_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct device_node *np = dev->of_node;
	struct device_node *timings;
	struct jdi_panel *panel;
	int ret;
	u32 video_mode;
#if JDI_BL_ENABLE
	struct backlight_properties bl_props;
#endif

	panel = devm_kzalloc(&dsi->dev, sizeof(*panel), GFP_KERNEL);
	if (!panel)
		return -ENOMEM;

	mipi_dsi_set_drvdata(dsi, panel);

	panel->dsi = dsi;

	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags =  MIPI_DSI_MODE_VIDEO_HSE | MIPI_DSI_MODE_VIDEO |
			   MIPI_DSI_CLOCK_NON_CONTINUOUS;

	ret = of_property_read_u32(np, "video-mode", &video_mode);
	if (!ret) {
		switch (video_mode) {
		case 0:
			/* burst mode */
			dsi->mode_flags |= MIPI_DSI_MODE_VIDEO_BURST;
			break;
		case 1:
			/* non-burst mode with sync event */
			break;
		case 2:
			/* non-burst mode with sync pulse */
			dsi->mode_flags |= MIPI_DSI_MODE_VIDEO_SYNC_PULSE;
			break;
		default:
			dev_warn(dev, "invalid video mode %d\n", video_mode);
			break;

		}
	}

	ret = of_property_read_u32(np, "dsi-lanes", &dsi->lanes);
	if (ret < 0) {
		dev_err(dev, "Failed to get dsi-lanes property (%d)\n", ret);
		return ret;
	}

	/*
	 * 'display-timings' is optional, so verify if the node is present
	 * before calling of_get_videomode so we won't get console error
	 * messages
	 */
	timings = of_get_child_by_name(np, "display-timings");
	if (timings) {
		of_node_put(timings);
		ret = of_get_videomode(np, &panel->vm, 0);
	} else {
		videomode_from_timing(&ctx_default_timing, &panel->vm);
	}
	if (ret < 0)
		return ret;

	of_property_read_u32(np, "panel-width-mm", &panel->width_mm);
	of_property_read_u32(np, "panel-height-mm", &panel->height_mm);

	panel->tpreset = devm_gpiod_get(&dsi->dev, "tpreset", GPIOD_OUT_LOW);
	if (IS_ERR(panel->tpreset)) {
		print_err("Couldn't get our tpreset GPIO");
	}
	print_dbg("tpreset gpio ok");

	panel->reset = devm_gpiod_get(&dsi->dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(panel->reset)) {
		print_err("Couldn't get our reset GPIO");
	}
	print_dbg("reset gpio ok");

#if JDI_BL_ENABLE
	memset(&bl_props, 0, sizeof(bl_props));
	bl_props.type = BACKLIGHT_RAW;
	bl_props.brightness = 255;
	bl_props.max_brightness = 255;

	panel->backlight = devm_backlight_device_register(
				dev, dev_name(dev),
				dev, dsi,
				&jdi_bl_ops, &bl_props);
	if (IS_ERR(panel->backlight)) {
		ret = PTR_ERR(panel->backlight);
		dev_err(dev, "Failed to register backlight (%d)\n", ret);
		return ret;
	}
#endif

	drm_panel_init(&panel->base);
	panel->base.funcs = &jdi_panel_funcs;
	panel->base.dev = dev;
	dev_set_drvdata(dev, panel);

	ret = drm_panel_add(&panel->base);

	if (ret < 0)
		return ret;

	ret = mipi_dsi_attach(dsi);
	if (ret < 0)
		drm_panel_remove(&panel->base);

	return ret;
}

static int jdi_panel_remove(struct mipi_dsi_device *dsi)
{
	struct jdi_panel *ctx = mipi_dsi_get_drvdata(dsi);
	struct device *dev = &dsi->dev;
	int ret;

	ret = mipi_dsi_detach(dsi);
	if (ret < 0)
		DRM_DEV_ERROR(dev, "Failed to detach from host (%d)\n",
			ret);

	drm_panel_remove(&ctx->base);

	return 0;
}

static void jdi_panel_shutdown(struct mipi_dsi_device *dsi)
{
	struct jdi_panel *ctx = mipi_dsi_get_drvdata(dsi);

	jdi_panel_disable(&ctx->base);
	jdi_panel_unprepare(&ctx->base);
}

#ifdef CONFIG_PM
static int jdi_panel_suspend(struct device *dev)
{
	struct jdi_panel *ctx = dev_get_drvdata(dev);

	if (!ctx->reset)
		return 0;

	devm_gpiod_put(dev, ctx->reset);
	ctx->reset = NULL;

	return 0;
}

static int jdi_panel_resume(struct device *dev)
{
	struct jdi_panel *ctx = dev_get_drvdata(dev);

	if (ctx->reset)
		return 0;

	ctx->reset = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset))
		ctx->reset = NULL;

	return PTR_ERR_OR_ZERO(ctx->reset);
}

#endif

static const struct dev_pm_ops jdi_pm_ops = {
	SET_RUNTIME_PM_OPS(jdi_panel_suspend, jdi_panel_resume, NULL)
	SET_SYSTEM_SLEEP_PM_OPS(jdi_panel_suspend, jdi_panel_resume)
};

static const struct of_device_id jdi_of_match[] = {
	{ .compatible = "jdi,tpm0501010p", },
	{ }
};
MODULE_DEVICE_TABLE(of, jdi_of_match);

static struct mipi_dsi_driver jdi_panel_driver = {
	.driver = {
		.name = "panel-jdi-tpm0501010p",
		.of_match_table = jdi_of_match,
		.pm	= &jdi_pm_ops,
	},
	.probe = jdi_panel_probe,
	.remove = jdi_panel_remove,
	.shutdown = jdi_panel_shutdown,
};
module_mipi_dsi_driver(jdi_panel_driver);

MODULE_AUTHOR("Linh Nguyen <nvl1109@gmail.com>");
MODULE_DESCRIPTION("DRM Driver for JDI TPM0501010P MIPI DSI panel");
MODULE_LICENSE("GPL v2");
