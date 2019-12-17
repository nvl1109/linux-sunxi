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

#define __DEBUG 0
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
static char cmd2_ena[] = {
	0x00,
	0xFF,
	0x19,
	0x02,
	0x01,
	0x00,
	0x80,
	0xFF,
	0x19,
	0x02,
	0x00,
	0x83,
	0xF3,
	0xCA,
	0x00,
	0x90,
	0xC4,
	0x00,
	0x00,
	0xB4,
	0xC0,
	0xC0,
	0x2A,
	0x00,
	0x00,
	0x04,
	0x37,
	0x2B,
	0x00,
	0x00,
	0x07,
	0x7F
};

static struct dsi_cmd_desc jdi_display_on_cmd[] = {
	{10, WAIT_TYPE_US,
		sizeof(cmd2_ena), cmd2_ena},
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
		print_dbg("reset Panel");
		gpiod_set_raw_value(ctx->reset, 1); // High for > 15ms
		usleep_range(20000, 25000);

		gpiod_set_raw_value(ctx->reset, 0); // Low for > 20us
		usleep_range(10000, 15000);

		gpiod_set_raw_value(ctx->reset, 1); // Keep high & wait for > 10ms
		usleep_range(70000, 75000);
	}
	if (!IS_ERR(ctx->tpreset)) {
		print_dbg("reset Touch");
		gpiod_set_raw_value(ctx->tpreset, 0);
		usleep_range(10000, 15000);
		gpiod_set_raw_value(ctx->tpreset, 1);
		usleep_range(50000, 65000);
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
		usleep_range(15000, 17000);
	}
	if (!IS_ERR(ctx->tpreset)) {
		print_dbg("poweroff Touch");
		gpiod_set_raw_value(ctx->tpreset, 0);
		usleep_range(15000, 17000);
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
	int ret = 0;
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

	ret |= mipi_dsi_cmds_tx(jdi_display_on_cmd, ARRAY_SIZE(jdi_display_on_cmd), ctx);
	print_dbg("display on done");
	if (ret < 0) {
		DRM_DEV_ERROR(dev, "Failed to do turn panel on (%d)\n", ret);
		goto fail;
	}

	/* Set both MCU and RGB I/F to 24bpp */
	ret = mipi_dsi_dcs_set_pixel_format(dsi, MIPI_DCS_PIXEL_FMT_24BIT |
					(MIPI_DCS_PIXEL_FMT_24BIT << 4));
	if (ret < 0){
		DRM_DEV_ERROR(dev, "set pixel format ret (%d)\n", ret);
		goto fail;
	}

	/* Software reset */
	ret = mipi_dsi_dcs_soft_reset(dsi);
	if (ret < 0) {
		DRM_DEV_ERROR(dev, "Failed to do Software Reset (%d)\n", ret);
		goto fail;
	}

	usleep_range(15000, 17000);

	/* Set DSI mode */
	ret = mipi_dsi_generic_write(dsi, (u8[]){ 0xC2, 0x0B }, 2);
	if (ret < 0) {
		DRM_DEV_ERROR(dev, "Failed to set DSI mode (%d)\n", ret);
		goto fail;
	}
	/* Set tear ON */
	ret = mipi_dsi_dcs_set_tear_on(dsi, MIPI_DSI_DCS_TEAR_MODE_VBLANK);
	if (ret < 0) {
		DRM_DEV_ERROR(dev, "Failed to set tear ON (%d)\n", ret);
		goto fail;
	}
	/* Set tear scanline */
	ret = mipi_dsi_dcs_set_tear_scanline(dsi, 0x380);
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

	ctx->enabled = true;

	return 0;

fail:
	print_err("FAIL");
	if (ctx->reset != NULL)
		gpiod_set_raw_value(ctx->reset, 0);

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

	print_dbg("pixel clock %u", mode->clock);

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
	if (ret < 0) {
		print_err("timing fail");
		return ret;
	}

	print_dbg("pixel clock %lu", panel->vm.pixelclock);

	of_property_read_u32(np, "panel-width-mm", &panel->width_mm);
	of_property_read_u32(np, "panel-height-mm", &panel->height_mm);

	panel->tpreset = devm_gpiod_get(&dsi->dev, "tpreset", GPIOD_OUT_HIGH);
	if (IS_ERR(panel->tpreset)) {
		print_err("Couldn't get our tpreset GPIO");
	}
	print_dbg("tpreset gpio ok");

	panel->reset = devm_gpiod_get(&dsi->dev, "reset", GPIOD_OUT_HIGH);
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
