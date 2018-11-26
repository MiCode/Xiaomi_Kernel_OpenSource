#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/regmap.h>
#include <linux/atomic.h>
#include <linux/of.h>
#include <linux/gpio.h>
#include "3dsl_eeprom_dev.h"
#include "cam_req_mgr_dev.h"
#include "3dsl_eeprom_soc.h"
#include "3dsl_eeprom_core.h"
#include "cam_debug_util.h"
struct sl_eeprom_ctrl_t       *g_e_ctrl = NULL;

/**
 * sl_eeprom_subdev_ioctl - ioctl  func
 * @filp:     file structure
 *@cmd: cmd arg
 *@arg: user arg
 *
 * Returns success or failure
 */
static long sl_eeprom_subdev_ioctl(struct file *filp,
	unsigned int cmd, unsigned long arg)
{
	int                       rc     = 0;
	struct sl_eeprom_ctrl_t *e_ctrl = g_e_ctrl;
	if (!e_ctrl) {
		CAM_ERR(CAM_SL_EEPROM, "e_ctrl is NULL");
		return -EINVAL;
	}
	mutex_lock(&(e_ctrl->eeprom_mutex));
	switch (cmd) {
	case DL_IOC_PWR_UP:
		sl_eeprom_power_up_wrapper(e_ctrl,  (void*)arg);
		break;
	case DL_IOC_READ_DATA:
		sl_eeprom_read_eeprom_wrapper(e_ctrl, (void*)arg);
		break;
	case DL_IOC_WRITE_DATA:
		sl_eeprom_write_eeprom_wrapper(e_ctrl,  (void*)arg);
		break;
	case DL_IOC_PWR_DOWN:
		sl_eeprom_power_down_wrapper(e_ctrl,  (void*)arg);
		break;
	default:
		rc = -ENOIOCTLCMD;
		break;
	}
	mutex_unlock(&(e_ctrl->eeprom_mutex));
	return rc;
}

static const struct file_operations dl_eeprom_fops =
{
	.owner = THIS_MODULE,
	.release = NULL,
	.open = NULL,
	.read = NULL,
	.write = NULL,
	.unlocked_ioctl = sl_eeprom_subdev_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = sl_eeprom_subdev_ioctl,
#endif
};

/**
 * sl_eeprom_update_i2c_info -i2c info update
 * @e_ctrl:     ctrl structure
 *@i2c_info: i2c info
 *
 * Returns success or failure
 */
int32_t sl_eeprom_update_i2c_info(struct sl_eeprom_ctrl_t *e_ctrl,
	struct sl_eeprom_i2c_info_t *i2c_info)
{
	struct cam_sensor_cci_client        *cci_client = NULL;
	if (e_ctrl->io_master_info.master_type == CCI_MASTER) {
		cci_client = e_ctrl->io_master_info.cci_client;
		if (!cci_client) {
			CAM_ERR(CAM_SL_EEPROM, "failed: cci_client %pK",
				cci_client);
			return -EINVAL;
		}
		cci_client->cci_i2c_master = e_ctrl->cci_i2c_master;
		cci_client->sid = (i2c_info->slave_addr) >> 1;
		cci_client->retries = 3;
		cci_client->id_map = 0;
		cci_client->i2c_freq_mode = i2c_info->i2c_freq_mode;
		CAM_DBG(CAM_SL_EEPROM, " cci client info %d %d %d %d %d", cci_client->cci_i2c_master,
			cci_client->sid, cci_client->retries, cci_client->id_map,  cci_client->i2c_freq_mode);
	}
	return 0;
}

/**
 * sl_eeprom_init_subdev -init subdev
 * @e_ctrl:     ctrl structure
 *
 * Returns success or failure
 */
static int sl_eeprom_init_subdev(struct sl_eeprom_ctrl_t *e_ctrl)
{
	int rc = 0;
	strlcpy(e_ctrl->dev_info.device_name, SL_EEPROM_NAME,
		sizeof(e_ctrl->dev_info.device_name));
	strlcpy(e_ctrl->dev_info.class_name, DL_CLASS_NAME,
		sizeof(e_ctrl->dev_info.class_name));
	e_ctrl->dev_info.chr_class = class_create(THIS_MODULE, DL_CLASS_NAME);
	if (e_ctrl->dev_info.chr_class == NULL) {
		CAM_ERR(CAM_SL_EEPROM,  "Failed to create class.\n");
		rc = -EINVAL;
	}
	rc = alloc_chrdev_region(&e_ctrl->dev_info.dev_num, 0, 1, SL_EEPROM_NAME);
	if (rc < 0) {
		CAM_ERR(CAM_SL_EEPROM,  "Failed to allocate chrdev region\n");
		rc = -EINVAL;
	}
	e_ctrl->dev_info.chr_dev = device_create(e_ctrl->dev_info.chr_class, NULL,
					e_ctrl->dev_info.dev_num, e_ctrl, SL_EEPROM_NAME);
	if (IS_ERR(e_ctrl->dev_info.chr_dev)) {
		CAM_ERR(CAM_SL_EEPROM, "Failed to create char device\n");
		rc = -ENODEV;
	}

	cdev_init(&(e_ctrl->dev_info.cdev), &dl_eeprom_fops);
	e_ctrl->dev_info.cdev.owner = THIS_MODULE;

	rc = cdev_add(&(e_ctrl->dev_info.cdev), e_ctrl->dev_info.dev_num, 1);
	dev_set_drvdata(e_ctrl->dev_info.chr_dev, (void*)e_ctrl);
	if (rc < 0) {
		CAM_ERR(CAM_SL_EEPROM,  "Failed to add cdev\n");
		rc = -ENODEV;
	}
	return rc;
}

/**
 * sl_eeprom_platform_driver_probe -probe info
 * @pdev:    dev info
 *
 * Returns success or failure
 */
static int32_t sl_eeprom_platform_driver_probe(
	struct platform_device *pdev)
{
	int32_t                         rc = 0;
	struct sl_eeprom_ctrl_t       *e_ctrl = NULL;
	struct sl_eeprom_soc_private  *soc_private = NULL;
	CAM_DBG(CAM_SL_EEPROM, "sl_eeprom_platform_driver_probe enter");
	e_ctrl = kzalloc(sizeof(struct sl_eeprom_ctrl_t), GFP_KERNEL);
	if (!e_ctrl){
		return -ENOMEM;
	}
	e_ctrl->soc_info.pdev = pdev;
	e_ctrl->soc_info.dev = &pdev->dev;
	e_ctrl->soc_info.dev_name = pdev->name;
	e_ctrl->eeprom_device_type = MSM_CAMERA_PLATFORM_DEVICE;
	e_ctrl->cal_data.mapdata = NULL;
	e_ctrl->cal_data.map = NULL;
	e_ctrl->userspace_probe = false;
	e_ctrl->io_master_info.master_type = CCI_MASTER;
	e_ctrl->io_master_info.cci_client = kzalloc(
		sizeof(struct cam_sensor_cci_client), GFP_KERNEL);
	if (!e_ctrl->io_master_info.cci_client) {
		rc = -ENOMEM;
		goto free_e_ctrl;
	}

	soc_private = kzalloc(sizeof(struct sl_eeprom_soc_private),
		GFP_KERNEL);
	if (!soc_private) {
		rc = -ENOMEM;
		goto free_cci_client;
	}
	e_ctrl->soc_info.soc_private = soc_private;
	soc_private->power_info.dev = &pdev->dev;
	/* Initialize mutex */
	mutex_init(&(e_ctrl->eeprom_mutex));

	rc = sl_eeprom_parse_dt(e_ctrl);
	if (rc) {
		CAM_ERR(CAM_SL_EEPROM, "failed: soc init rc %d", rc);
		goto free_soc;
	}

	rc = sl_eeprom_update_i2c_info(e_ctrl, &soc_private->i2c_info);
	if (rc) {
		CAM_ERR(CAM_SL_EEPROM, "failed: to update i2c info rc %d", rc);
		goto free_soc;
	}

	rc = sl_eeprom_init_subdev(e_ctrl);
	if (rc)
		goto free_soc;

	e_ctrl->bridge_intf.device_hdl = -1;
	e_ctrl->bridge_intf.ops.get_dev_info = NULL;
	e_ctrl->bridge_intf.ops.link_setup = NULL;
	e_ctrl->bridge_intf.ops.apply_req = NULL;

	platform_set_drvdata(pdev, e_ctrl);
	e_ctrl->sl_eeprom_state = CAM_SL_EEPROM_INIT;
	g_e_ctrl = e_ctrl;
	return rc;
free_soc:
	kfree(soc_private);
free_cci_client:
	kfree(e_ctrl->io_master_info.cci_client);
free_e_ctrl:
	kfree(e_ctrl);
	return rc;
}

/**
 * sl_eeprom_platform_driver_remove -driver remove func
 * @pdev:    dev info
 *
 * Returns success or failure
 */static int sl_eeprom_platform_driver_remove(struct platform_device *pdev)
{
	int                        i;
	struct sl_eeprom_ctrl_t  *e_ctrl;
	struct cam_hw_soc_info    *soc_info;

	e_ctrl = platform_get_drvdata(pdev);
	if (!e_ctrl) {
		CAM_ERR(CAM_SL_EEPROM, "eeprom device is NULL");
		return -EINVAL;
	}

	soc_info = &e_ctrl->soc_info;

	for (i = 0; i < soc_info->num_clk; i++)
		devm_clk_put(soc_info->dev, soc_info->clk[i]);

	kfree(soc_info->soc_private);
	kfree(e_ctrl->io_master_info.cci_client);
	kfree(e_ctrl);
	return 0;
}

static const struct of_device_id sl_eeprom_dt_match[] = {
	{ .compatible = "qcom,sl_eeprom" },
	{ }
};

MODULE_DEVICE_TABLE(of, sl_eeprom_dt_match);

static struct platform_driver sl_eeprom_platform_driver = {
	.driver = {
		.name = "qcom,sl_eeprom",
		.owner = THIS_MODULE,
		.of_match_table = sl_eeprom_dt_match,
	},
	.probe = sl_eeprom_platform_driver_probe,
	.remove = sl_eeprom_platform_driver_remove,
};

static int __init sl_eeprom_driver_init(void)
{
	int rc = 0;
	rc = platform_driver_register(&sl_eeprom_platform_driver);
	if (rc < 0) {
		CAM_ERR(CAM_SL_EEPROM, "platform_driver_register failed rc = %d", rc);
		return rc;
	}
	return rc;
}

static void __exit sl_eeprom_driver_exit(void)
{
	platform_driver_unregister(&sl_eeprom_platform_driver);
}

module_init(sl_eeprom_driver_init);
module_exit(sl_eeprom_driver_exit);
MODULE_DESCRIPTION("3DSL EEPROM driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("xiaomi camera");
MODULE_VERSION("1.0");

