#include <linux/backlight.h>
#include <drm/drmP.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>

#include <linux/gpio/consumer.h>
#include <linux/regulator/consumer.h>

#include <video/mipi_display.h>
#include <video/of_videomode.h>
#include <video/videomode.h>

#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <mt-plat/upmu_common.h>

#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "../mediatek/mtk_panel_ext.h"
#include "../mediatek/mtk_log.h"
#include "../mediatek/mtk_drm_graphics_base.h"
#endif

#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
#include "../mediatek/mtk_corner_pattern/mtk_hw_roundedpattern_samsung_s6e8fc01.h"
#endif
struct lcm {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *pm_enable_gpio;
	struct gpio_desc *bias_gpio;

	bool prepared;
	bool enabled;

	int error;

	bool hbm_en;
	bool hbm_wait;

	u32 unset_doze_brightness;
	u32 doze_brightness_state;
	u32 crc_state;
	bool in_aod;
	bool high_refresh_rate;
	char *panel_info;
	bool doze_enable;
	int skip_dimmingon;
	struct delayed_work dimmingon_delayed_work;
	ktime_t set_backlight_time;
#ifdef CONFIG_HWCONF_MANAGER
	struct dsi_panel_mi_count mi_count;
#endif
};
