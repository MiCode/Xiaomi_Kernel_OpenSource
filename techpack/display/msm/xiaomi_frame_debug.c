#include "xiaomi_frame_stat.h"
#include "sde_connector.h"
#include <linux/debugfs.h>

#ifdef CONFIG_DEBUG_FS
extern struct frame_stat fm_stat;

int smart_fps_init_debugfs(struct drm_connector *connector)
{
	struct sde_connector *sde_connector;
	struct msm_display_info info;
	struct dsi_display *display;
	struct dsi_panel *panel;
	struct dentry *smart_fps_dir = NULL;

	if (!connector || !connector->debugfs_entry) {
		pr_err("%s: invalid connector\n", __func__);
		goto err_out;
	}

	sde_connector_get_info(connector, &info);
	if (info.intf_type != DRM_MODE_CONNECTOR_DSI) {
		pr_info("%s: only for dsi panel\n", __func__);
		goto err_out;
	}

	sde_connector = to_sde_connector(connector);
	if (!sde_connector) {
		pr_err("%s: invalid sde_connector\n", __func__);
		goto err_out;
	}
	display = (struct dsi_display *) sde_connector->display;
	if (!display) {
		pr_err("%s: invalid display\n", __func__);
		goto err_out;
	}

	panel = display->panel;
	if (!panel || !panel->dfps_caps.smart_fps_support) {
		pr_err("%s: invalid panel or smart isn't supported\n", __func__);
		goto err_out;
	}

	smart_fps_dir = debugfs_create_dir("smart_fps", connector->debugfs_entry);
	if (!smart_fps_dir) {
		pr_err("%s: failed to create smart_fps, error %ld\n", __func__,
		       PTR_ERR(smart_fps_dir));
		goto err_out;
	}

	if (!debugfs_create_bool("support", 0600, smart_fps_dir,
			&panel->dfps_caps.smart_fps_support)) {
		pr_err("%s: failed to create smart_fps/support\n", __func__);
		goto err_out;
	}

	if (!debugfs_create_bool("enable", 0600, smart_fps_dir,
			&fm_stat.enabled)) {
		pr_err("%s: failed to create smart_fps/enable\n", __func__);
		goto err_out;
	}

	if (!debugfs_create_bool("monitor_long_frame", 0600, smart_fps_dir,
			&fm_stat.monitor_long_frame)) {
		pr_err("%s: failed to create smart_fps/monitor_long_frame\n", __func__);
		goto err_out;
	}

	if (!debugfs_create_u32("fps_filter", 0600, smart_fps_dir,
			&fm_stat.fps_filter)) {
		pr_err("%s: failed to create smart_fps/fps_filter\n", __func__);
		goto err_out;
	}

	if (!debugfs_create_u32("idle_time", 0600, smart_fps_dir,
			&panel->mi_cfg.idle_time)) {
		pr_err("%s: failed to create smart_fps/idle_time\n", __func__);
		goto err_out;
	}

	if (!debugfs_create_bool("enable_idle", 0600, smart_fps_dir,
			&fm_stat.enable_idle)) {
		pr_err("%s: failed to create smart_fps/enable_idle\n", __func__);
		goto err_out;
	}

	pr_info("%s: create debugfs for smart_fps\n", __func__);

	return 0;
err_out:
	if (smart_fps_dir) {
		debugfs_remove_recursive(smart_fps_dir);
		smart_fps_dir = NULL;
	}
	return -1;
}
#else
int smart_fps_init_debugfs(struct drm_connector *connector)
{
	return 0;
}
#endif
