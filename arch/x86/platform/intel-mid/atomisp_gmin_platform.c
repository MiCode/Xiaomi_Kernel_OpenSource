#include <linux/module.h>
#include <asm/spid.h>

/*
 *   struct atomisp_sensor_caps {
 *       int stream_num;
 *   };
 *   struct atomisp_camera_caps {
 *       int sensor_num;
 *       struct atomisp_sensor_caps sensor[MAX_SENSORS_PER_PORT];
 *   };
 */
const struct atomisp_camera_caps *atomisp_get_default_camera_caps(void)
{
    /* This is near-legacy.  The camera_caps field is ultimately used
     * only in two spots in atomisp_cmd, one checks if it's ==1 and
     * the other if it's <2 (is 0 legal?). */
    return NULL;
}
EXPORT_SYMBOL_GPL(atomisp_get_default_camera_caps);

/*
 *   struct intel_v4l2_subdev_i2c_board_info {
 *       struct i2c_board_info board_info;
 *       int i2c_adapter_id;
 *   };
 *   struct intel_v4l2_subdev_table {
 *       struct intel_v4l2_subdev_i2c_board_info v4l2_subdev;
 *       enum intel_v4l2_subdev_qtype type;
 *       enum atomisp_camera_port port;
 *   };
 *   struct atomisp_platform_data {
 *       struct intel_v4l2_subdev_table *subdevs;
 *       const struct soft_platform_id *spid;
 *   };
 */
const struct atomisp_platform_data *atomisp_get_platform_data(void)
{
    /* This gets enumerated in
     * atomisp_pci_probe->atomisp_register_entites->atomisp_subdev_probe,
     * which means that it's going to be needed before it's available.
     * I think that's the only spot it's needed though.
     *
     * Note that it also talks about stuff like flash and motor
     * devices, which are not going to be uniquely identifiable
     * electronically (i.e. which flash goes with which camera?)  May
     * really need the firmware intervention, or else duplicate with
     * e.g.  module parameters on the subdevices... */
    return NULL;
}
EXPORT_SYMBOL_GPL(atomisp_get_platform_data);

/*
 * Used in a handful of modules.  Focus motor maybe?
 *
 * struct camera_af_platform_data {
 *     int (*power_ctrl)(struct v4l2_subdev *subdev, int flag);
 * };
*/
const struct camera_af_platform_data *camera_get_af_platform_data(void)
{
    return NULL;
}
EXPORT_SYMBOL_GPL(camera_get_af_platform_data);

/* This needs to be initialized at runtime so the various
 * platform-checking macros in spid.h return the correct results.
 * Either that, or we need to fix up the usage of those macros so that
 * it's checking more appropriate runtime-detectable data. */
struct soft_platform_id spid;
EXPORT_SYMBOL(spid);
