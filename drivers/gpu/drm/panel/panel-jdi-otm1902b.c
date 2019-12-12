// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2017, Free Electrons
 * Author: Maxime Ripard <maxime.ripard@free-electrons.com>
 */

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/fb.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/gpio/consumer.h>

#include <drm/drm_crtc.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>

#include <video/mipi_display.h>

#define __DEBUG 1
#define __DBG_INFO 1
#if __DEBUG
#define print_dbg(fmt,args...) pr_info("LINH otm1902 %s(): DBG " fmt "\n", __func__, ##args)
#else
#define print_dbg(fmt,args...)
#endif /* __DEBUG */
#if __DBG_INFO
#define print_inf(fmt,args...) pr_info("LINH otm1902 %s(): INF " fmt "\n", __func__, ##args)
#else
#define print_inf(fmt,args...)
#endif
#define print_infa(fmt,args...) pr_info("LINH otm1902 %s(): INF " fmt "\n", __func__, ##args)
#define print_warn(fmt,args...) pr_info("LINH otm1902 %s(): WARN " fmt "\n", __func__, ##args)
#define print_err(fmt,args...) pr_err("LINH otm1902 %s(): ERR " fmt "\n", __func__, ##args)

struct otm1920b {
	struct drm_panel	panel;
	struct mipi_dsi_device	*dsi;

	// struct gpio_desc	*power;
	struct gpio_desc	*reset;
	struct gpio_desc	*tpreset;
	u32	timing_mode;
};

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

static char HD720_setting_1_para[] = {
	0x2A,
	0x00, 0x00, 0x02, 0xCF,
};

static char HD1080_setting_1_para[] = {
	0x2a,
	0x00, 0x00, 0x04, 0x37,
};

static char HD720_setting_2_para[] = {
	0x2B,
	0x00, 0x00, 0x04, 0xFF,
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




static inline struct otm1920b *panel_to_otm1920b(struct drm_panel *panel)
{
	return container_of(panel, struct otm1920b, panel);
}

static int mipi_dsi_cmds_tx(struct dsi_cmd_desc *cmds, int cnt, struct otm1920b *ctx)
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

static int otm1920b_prepare(struct drm_panel *panel)
{
	struct otm1920b *ctx = panel_to_otm1920b(panel);

	if (!IS_ERR(ctx->tpreset)) {
		print_dbg("reset Touch");
		gpiod_set_value(ctx->tpreset, 1);
		msleep(5);
		gpiod_set_value(ctx->tpreset, 0);
		msleep(5);
	}
	if (!IS_ERR(ctx->reset)) {
		print_dbg("reset Panel");
		gpiod_set_value(ctx->reset, 0); // High for > 15ms
		msleep(20);

		gpiod_set_value(ctx->reset, 1); // Low for > 20us
		msleep(1);

		gpiod_set_value(ctx->reset, 0); // Keep high & wait for > 10ms
		msleep(20);
	}

	return 0;
}

static int otm1920b_enable(struct drm_panel *panel)
{
	struct otm1920b *ctx = panel_to_otm1920b(panel);
	u16 tmp;
	int ret;

	ctx->dsi->mode_flags |= MIPI_DSI_MODE_LPM;
	print_dbg("start pannel enable");

	ret = mipi_dsi_cmds_tx(cleveredge_inital_1080P_cmds, ARRAY_SIZE(cleveredge_inital_1080P_cmds), ctx);
	print_dbg("clever init 1080p done");
	ret = mipi_dsi_cmds_tx(jdi_display_on_cmd, ARRAY_SIZE(jdi_display_on_cmd), ctx);
	print_dbg("display on 0 done");
	ret = mipi_dsi_cmds_tx(jdi_display_on_cmd1, ARRAY_SIZE(jdi_display_on_cmd1), ctx);
	print_dbg("display on 1 done");
	ret = mipi_dsi_cmds_tx(jdi_cabc_ui_on_cmds, ARRAY_SIZE(jdi_cabc_ui_on_cmds), ctx);
	print_dbg("cabc ui on done");

	return 0;
}

static int otm1920b_disable(struct drm_panel *panel)
{
	struct otm1920b *ctx = panel_to_otm1920b(panel);

	mipi_dsi_cmds_tx(jdi_display_off_cmds, ARRAY_SIZE(jdi_display_off_cmds), ctx);
	print_dbg("display off done");

	return mipi_dsi_dcs_set_display_off(ctx->dsi);
}

static int otm1920b_unprepare(struct drm_panel *panel)
{
	struct otm1920b *ctx = panel_to_otm1920b(panel);

	print_dbg("");
	mipi_dsi_dcs_enter_sleep_mode(ctx->dsi);

	// Keep LCD RESET low
	if (!IS_ERR(ctx->reset)) {
		print_dbg("poweroff Panel");
		gpiod_set_value(ctx->reset, 1);
	}
	if (!IS_ERR(ctx->tpreset)) {
		print_dbg("poweroff Touch");
		gpiod_set_value(ctx->tpreset, 1);
	}

	return 0;
}

#if 0 // ILI9881
static const struct drm_display_mode high_clk_mode = {
	.clock		= 74250,
	.vrefresh	= 60,
	.hdisplay	= 720,
	.hsync_start	= 720 + 34,
	.hsync_end	= 720 + 34 + 100,
	.htotal	= 720 + 34 + 100 + 100,
	.vdisplay	= 1280,
	.vsync_start	= 1280 + 2,
	.vsync_end	= 1280 + 2 + 30,
	.vtotal	= 1280 + 2 + 30 + 20,
};

static const struct drm_display_mode default_mode = {
	.clock		= 62000,
	.vrefresh	= 60,
	.hdisplay	= 720,
	.hsync_start	= 720 + 10,
	.hsync_end	= 720 + 10 + 20,
	.htotal		= 720 + 10 + 20 + 30,
	.vdisplay	= 1280,
	.vsync_start	= 1280 + 10,
	.vsync_end	= 1280 + 10 + 10,
	.vtotal		= 1280 + 10 + 10 + 20,
};
#else
static const struct drm_display_mode high_clk_mode = {
	.clock = 148500,
	.vrefresh = 60,
	.hdisplay = 1080,
	.hsync_start = 1080 + 70,
	.hsync_end = 1080 + 70 + 1,
	.htotal = 1080 + 70 + 1 + 50,
	.vdisplay = 1920,
	.vsync_start = 1920 + 35,
	.vsync_end = 1920 + 35 + 1,
	.vtotal = 1920 + 35 + 1 + 25,
};

static const struct drm_display_mode default_mode = {
	.clock		= 62000,
	.vrefresh	= 60,
	.hdisplay	= 1080,
	.hsync_start	= 1080 + 10,
	.hsync_end	= 1080 + 10 + 20,
	.htotal		= 1080 + 10 + 20 + 30,
	.vdisplay	= 1920,
	.vsync_start	= 1920 + 10,
	.vsync_end	= 1920 + 10 + 10,
	.vtotal		= 1920 + 10 + 10 + 20,
};
#endif

static int otm1920b_get_modes(struct drm_panel *panel)
{
	struct drm_connector *connector = panel->connector;
	struct otm1920b *ctx = panel_to_otm1920b(panel);
	struct drm_display_mode *mode;
	const struct drm_display_mode *display_mode;
	u32 bus_format = MEDIA_BUS_FMT_RGB888_1X24;
	int ret;

	print_dbg("timing mode: %d", ctx->timing_mode);
	switch (ctx->timing_mode) {
		case 0:
			display_mode = &default_mode;
			break;
		case 1:
			display_mode = &high_clk_mode;
			break;
		default:
			print_warn("invalid timing mode %d, fail back to use default mode", ctx->timing_mode);
			display_mode = &default_mode;
			break;

	}

	mode = drm_mode_duplicate(panel->drm, display_mode);
	if (!mode) {
		print_err("failed to add mode %ux%ux@%u",
			display_mode->hdisplay, display_mode->vdisplay,
			display_mode->vrefresh);
		return -ENOMEM;
	}

	drm_mode_set_name(mode);
	print_dbg("drm mode set done");

	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;

	ret = drm_display_info_set_bus_formats(&connector->display_info,
					       &bus_format, 1);
	print_dbg("drm set bus ret %d, bus %d", ret, bus_format);
	if (ret)
		return ret;

	drm_mode_probed_add(connector, mode);

	panel->connector->display_info.width_mm = 64;
	panel->connector->display_info.height_mm = 116;
	print_dbg("drm probed, width %d, height %d", panel->connector->display_info.width_mm, 
		panel->connector->display_info.height_mm);

	return 1;
}

static const struct drm_panel_funcs otm1920b_funcs = {
	.prepare	= otm1920b_prepare,
	.unprepare	= otm1920b_unprepare,
	.enable		= otm1920b_enable,
	.disable	= otm1920b_disable,
	.get_modes	= otm1920b_get_modes,
};

static int otm1920b_dsi_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct device_node *np = dev->of_node;
	struct otm1920b *ctx;
	int ret;
	u32 video_mode;

	print_inf("");

	ctx = devm_kzalloc(&dsi->dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;
	mipi_dsi_set_drvdata(dsi, ctx);
	ctx->dsi = dsi;

	drm_panel_init(&ctx->panel);
	ctx->panel.dev = &dsi->dev;
	ctx->panel.funcs = &otm1920b_funcs;

	ctx->tpreset = devm_gpiod_get(&dsi->dev, "tpreset", GPIOD_OUT_LOW);
	if (IS_ERR(ctx->tpreset)) {
		print_err("Couldn't get our tpreset GPIO");
	}
	print_dbg("tpreset gpio ok");

	ctx->reset = devm_gpiod_get(&dsi->dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(ctx->reset)) {
		print_err("Couldn't get our reset GPIO");
	}
	print_dbg("reset gpio ok");

	ret = drm_panel_add(&ctx->panel);
	print_dbg("drm panel add %d", ret);
	if (ret < 0)
		return ret;

	ret = of_property_read_u32(np, "timing-mode", &ctx->timing_mode);
	print_dbg("timing-mode %d, %d", ret, ctx->timing_mode);
	if (ret < 0) {
		print_err("Failed to get timing-mode, use default timing-mode (%d)", ret);
		ctx->timing_mode = 0;
		return ret;
	}

	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags =  MIPI_DSI_MODE_VIDEO_HSE | MIPI_DSI_MODE_VIDEO |
			   MIPI_DSI_CLOCK_NON_CONTINUOUS;

	ret = of_property_read_u32(np, "video-mode", &video_mode);
	print_dbg("video-mode ret %d, %d", ret, video_mode);
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
			print_warn("invalid video mode %d", video_mode);
			break;

		}
	}

	return mipi_dsi_attach(dsi);
}

static int otm1920b_dsi_remove(struct mipi_dsi_device *dsi)
{
	struct otm1920b *ctx = mipi_dsi_get_drvdata(dsi);

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);

	return 0;
}

static const struct of_device_id otm1920b_of_match[] = {
	{ .compatible = "jdi,otm1902b" },
	{ }
};
MODULE_DEVICE_TABLE(of, otm1920b_of_match);

static struct mipi_dsi_driver otm1920b_dsi_driver = {
	.probe		= otm1920b_dsi_probe,
	.remove		= otm1920b_dsi_remove,
	.driver = {
		.name		= "jdi-otm1920b-dsi",
		.of_match_table	= otm1920b_of_match,
	},
};
module_mipi_dsi_driver(otm1920b_dsi_driver);

MODULE_AUTHOR("Linh Nguyen <nvl1109@gmail.com>");
MODULE_DESCRIPTION("JDI TPM0501010P Controller Driver");
MODULE_LICENSE("GPL v2");
