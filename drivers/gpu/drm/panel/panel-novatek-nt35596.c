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

#define SHARP_BL_ENABLE 0

#define WAIT_TYPE_US 1
#define WAIT_TYPE_MS 2
#define WAIT_TYPE_S  3

struct dsi_cmd_desc {
	int wait;
	int waittype;
	int dlen;
	char *payload;
};

static char page_0[] = { 0xFF, 0x00 };
static char tear_on[] = { 0x35, 0x01 };
static char cmd_vbp[] = { 0xD3, 0x06 };
static char cmd_vfp[] = { 0xD4, 0x04 };

static struct dsi_cmd_desc sharp_display_on_cmd[] = {
	{1, WAIT_TYPE_MS,
		sizeof(cmd_vbp), cmd_vbp},
	{1, WAIT_TYPE_MS,
		sizeof(cmd_vfp), cmd_vfp},
	{1, WAIT_TYPE_MS,
		sizeof(tear_on), tear_on},
};

static const u32 sharp_bus_formats[] = {
	MEDIA_BUS_FMT_RGB888_1X24,
	MEDIA_BUS_FMT_RGB666_1X18,
	MEDIA_BUS_FMT_RGB565_1X16,
};

struct sharp_panel {
	struct drm_panel base;
	struct mipi_dsi_device *dsi;

	struct gpio_desc *reset;
	struct gpio_desc *tpreset;
#if SHARP_BL_ENABLE
	struct backlight_device *backlight;
#endif

	bool prepared;
	bool enabled;

	struct videomode vm;
	u32 width_mm;
	u32 height_mm;
	u32 vrefresh;
};

static inline struct sharp_panel *to_sharp_panel(struct drm_panel *panel)
{
	return container_of(panel, struct sharp_panel, base);
}

static int mipi_dsi_cmds_tx(struct dsi_cmd_desc *cmds, int cnt, struct sharp_panel *ctx)
{
	int i;
	int ret;
	struct dsi_cmd_desc *cm = cmds;

	for (i = 0; i < cnt; i++) {
		ret = mipi_dsi_dcs_write_buffer(ctx->dsi, cm->payload, cm->dlen);
		if (ret < 0) {
			DRM_DEV_ERROR(&ctx->dsi->dev, "dsi write buf index %d, cmd[0] 0x%x %dbytes failed, %d\n", i, cm->payload[0], cm->dlen, ret);
			return ret;
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

static int sharp_panel_prepare(struct drm_panel *panel)
{
	struct sharp_panel *ctx = to_sharp_panel(panel);

	if (ctx->prepared)
		return 0;

	if (!IS_ERR(ctx->reset)) {
		gpiod_set_raw_value(ctx->reset, 1); // High for > 20ms
		msleep(20);

		gpiod_set_raw_value(ctx->reset, 0); // Low for > 20us
		msleep(20);

		gpiod_set_raw_value(ctx->reset, 1); // Keep high & wait for > 10ms
		msleep(20);
	}
	if (!IS_ERR(ctx->tpreset)) {
		gpiod_set_raw_value(ctx->tpreset, 0);
		msleep(20);
		gpiod_set_raw_value(ctx->tpreset, 1);
		msleep(60);
	}

	ctx->prepared = true;

	return 0;
}

static int sharp_panel_unprepare(struct drm_panel *panel)
{
	struct sharp_panel *ctx = to_sharp_panel(panel);
	struct device *dev = &ctx->dsi->dev;

	if (!ctx->prepared)
		return 0;

	if (ctx->enabled) {
		DRM_DEV_ERROR(dev, "Panel still enabled!\n");
		return -EPERM;
	}

	if (!IS_ERR(ctx->reset)) {
		gpiod_set_raw_value(ctx->reset, 0);
		msleep(20);
	}
	if (!IS_ERR(ctx->tpreset)) {
		gpiod_set_raw_value(ctx->tpreset, 0);
		msleep(20);
	}

	ctx->prepared = false;

	return 0;
}

static int sharp_panel_enable(struct drm_panel *panel)
{
	struct sharp_panel *ctx = to_sharp_panel(panel);
	struct mipi_dsi_device *dsi = ctx->dsi;
	struct device *dev = &dsi->dev;
	int color_format = color_format_from_dsi_format(dsi->format);
	int ret = 0;
	u16 brightness = 0;
	u8 tmp = 0;

	if (ctx->enabled)
		return 0;

	if (!ctx->prepared) {
		DRM_DEV_ERROR(dev, "Panel not prepared!\n");
		return -EPERM;
	}

	dsi->mode_flags |= MIPI_DSI_MODE_LPM;

	ret |= mipi_dsi_cmds_tx(sharp_display_on_cmd, ARRAY_SIZE(sharp_display_on_cmd), ctx);
	if (ret < 0) {
		DRM_DEV_ERROR(dev, "Failed to do turn panel on (%d)\n", ret);
		goto fail;
	}

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

#if SHARP_BL_ENABLE
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

#if SHARP_BL_ENABLE
	backlight_enable(ctx->backlight);
#endif

	brightness = 0;
	ret = mipi_dsi_dcs_get_display_brightness(dsi, &brightness);
	dev_dbg(dev, "get brightness ret %d, val %u\n", ret, brightness);

	tmp = 0;
	ret = mipi_dsi_dcs_get_power_mode(ctx->dsi, &tmp);
	dev_dbg(dev, "get power mode ret %d, val %u\n", ret, tmp);

	tmp = 0;
	ret = mipi_dsi_dcs_get_pixel_format(ctx->dsi, &tmp);
	dev_dbg(dev, "get pixel format ret %d, val %u\n", ret, tmp);

	ctx->enabled = true;

	return 0;

fail:
	if (ctx->reset != NULL)
		gpiod_set_value(ctx->reset, 1);

	return ret;
}

static int sharp_panel_disable(struct drm_panel *panel)
{
	struct sharp_panel *ctx = to_sharp_panel(panel);
	struct mipi_dsi_device *dsi = ctx->dsi;
	struct device *dev = &dsi->dev;
	int ret;

	if (!ctx->enabled)
		return 0;

	dsi->mode_flags |= MIPI_DSI_MODE_LPM;

#if SHARP_BL_ENABLE
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

static int sharp_panel_get_modes(struct drm_panel *panel)
{
	struct sharp_panel *ctx = to_sharp_panel(panel);
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
	mode->vrefresh = ctx->vrefresh;
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
			sharp_bus_formats, ARRAY_SIZE(sharp_bus_formats));
	if (ret)
		return ret;

	drm_mode_probed_add(panel->connector, mode);

	return 1;
}

#if SHARP_BL_ENABLE
static int sharp_bl_get_brightness(struct backlight_device *bl)
{
	struct mipi_dsi_device *dsi = bl_get_data(bl);
	struct sharp_panel *ctx = mipi_dsi_get_drvdata(dsi);
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

static int sharp_bl_update_status(struct backlight_device *bl)
{
	struct mipi_dsi_device *dsi = bl_get_data(bl);
	struct sharp_panel *ctx = mipi_dsi_get_drvdata(dsi);
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

static const struct backlight_ops sharp_bl_ops = {
	.update_status = sharp_bl_update_status,
	.get_brightness = sharp_bl_get_brightness,
};
#endif

static const struct drm_panel_funcs sharp_panel_funcs = {
	.prepare = sharp_panel_prepare,
	.unprepare = sharp_panel_unprepare,
	.enable = sharp_panel_enable,
	.disable = sharp_panel_disable,
	.get_modes = sharp_panel_get_modes,
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

static int sharp_panel_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct device_node *np = dev->of_node;
	struct device_node *timings;
	struct sharp_panel *panel;
	int ret;
	u32 video_mode;
#if SHARP_BL_ENABLE
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
	of_property_read_u32(np, "vrefresh", &panel->vrefresh);

	panel->tpreset = devm_gpiod_get(&dsi->dev, "tpreset", GPIOD_OUT_LOW);
	if (IS_ERR(panel->tpreset)) {
		dev_err(dev, "Couldn't get our tpreset GPIO\n");
	}

	panel->reset = devm_gpiod_get(&dsi->dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(panel->reset)) {
		dev_err(dev, "Couldn't get our reset GPIO\n");
		return -EINVAL;
	}

#if SHARP_BL_ENABLE
	memset(&bl_props, 0, sizeof(bl_props));
	bl_props.type = BACKLIGHT_RAW;
	bl_props.brightness = 255;
	bl_props.max_brightness = 255;

	panel->backlight = devm_backlight_device_register(
				dev, dev_name(dev),
				dev, dsi,
				&sharp_bl_ops, &bl_props);
	if (IS_ERR(panel->backlight)) {
		ret = PTR_ERR(panel->backlight);
		dev_err(dev, "Failed to register backlight (%d)\n", ret);
		return ret;
	}
#endif

	drm_panel_init(&panel->base);
	panel->base.funcs = &sharp_panel_funcs;
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

static int sharp_panel_remove(struct mipi_dsi_device *dsi)
{
	struct sharp_panel *ctx = mipi_dsi_get_drvdata(dsi);
	struct device *dev = &dsi->dev;
	int ret;

	ret = mipi_dsi_detach(dsi);
	if (ret < 0)
		DRM_DEV_ERROR(dev, "Failed to detach from host (%d)\n",
			ret);

	drm_panel_remove(&ctx->base);

	return 0;
}

static void sharp_panel_shutdown(struct mipi_dsi_device *dsi)
{
	struct sharp_panel *ctx = mipi_dsi_get_drvdata(dsi);

	sharp_panel_disable(&ctx->base);
	sharp_panel_unprepare(&ctx->base);
}

#ifdef CONFIG_PM
static int sharp_panel_suspend(struct device *dev)
{
	struct sharp_panel *ctx = dev_get_drvdata(dev);

	if (!ctx->reset)
		return 0;

	devm_gpiod_put(dev, ctx->reset);
	ctx->reset = NULL;

	return 0;
}

static int sharp_panel_resume(struct device *dev)
{
	struct sharp_panel *ctx = dev_get_drvdata(dev);

	if (ctx->reset)
		return 0;

	ctx->reset = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset))
		ctx->reset = NULL;

	return PTR_ERR_OR_ZERO(ctx->reset);
}

#endif

static const struct dev_pm_ops sharp_pm_ops = {
	SET_RUNTIME_PM_OPS(sharp_panel_suspend, sharp_panel_resume, NULL)
	SET_SYSTEM_SLEEP_PM_OPS(sharp_panel_suspend, sharp_panel_resume)
};

static const struct of_device_id sharp_of_match[] = {
	{ .compatible = "sharp,ls050t1sx18", },
	{ }
};
MODULE_DEVICE_TABLE(of, sharp_of_match);

static struct mipi_dsi_driver sharp_panel_driver = {
	.driver = {
		.name = "nt35596-dsi",
		.of_match_table = sharp_of_match,
		.pm	= &sharp_pm_ops,
	},
	.probe = sharp_panel_probe,
	.remove = sharp_panel_remove,
	.shutdown = sharp_panel_shutdown,
};
module_mipi_dsi_driver(sharp_panel_driver);

MODULE_AUTHOR("Linh Nguyen <nvl1109@gmail.com>");
MODULE_DESCRIPTION("DRM Driver for SHARP LS050T1SX18 MIPI DSI panel");
MODULE_LICENSE("GPL v2");
