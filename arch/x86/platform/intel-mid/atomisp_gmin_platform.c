#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/dmi.h>
#include <linux/efi.h>
#include <linux/acpi.h>
#include <linux/delay.h>
#include <media/v4l2-subdev.h>
#include <linux/mfd/intel_soc_pmic.h>
#include <linux/vlv2_plat_clock.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio/consumer.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/atomisp_platform.h>
#include <linux/atomisp_gmin_platform.h>
#include <asm/spid.h>

#define MAX_SUBDEVS 8

/* This needs to be initialized at runtime so the various
 * platform-checking macros in spid.h return the correct results.
 * Either that, or we need to fix up the usage of those macros so that
 * it's checking more appropriate runtime-detectable data. */
struct soft_platform_id spid;
EXPORT_SYMBOL(spid);

#define DEVNAME_PMIC_AXP "INT33F4:00"
#define DEVNAME_PMIC_TI  "INT33F5:00"

/* Should be defined in vlv2_plat_clock API, isn't: */
#define VLV2_CLK_19P2MHZ 1
#define VLV2_CLK_ON      1
#define VLV2_CLK_OFF     2

/* X-Powers AXP288 register hackery */
#define ALDO1_SEL_REG	0x28
#define ALDO1_CTRL3_REG	0x13
#define ALDO1_2P8V	0x16
#define ALDO1_CTRL3_SHIFT 0x05

#define ELDO_CTRL_REG   0x12

#define ELDO1_SEL_REG	0x19
#define ELDO1_1P8V	0x16
#define ELDO1_CTRL_SHIFT 0x00

#define ELDO2_SEL_REG	0x1a
#define ELDO2_1P8V	0x16
#define ELDO2_CTRL_SHIFT 0x01

/* TI SND9039 PMIC register hackery */
#define LDO9_REG	0x49
#define LDO9_2P8V_ON	0x2f
#define LDO9_2P8V_OFF	0x2e

#define LDO10_REG	0x4a
#define LDO10_1P8V_ON	0x59
#define LDO10_1P8V_OFF	0x58

struct gmin_subdev {
	struct v4l2_subdev *subdev;
	int clock_num;
	struct gpio_desc *gpio0;
	struct gpio_desc *gpio1;
	struct regulator *v1p8_reg;
	struct regulator *v2p8_reg;
	bool v1p8_on;
	bool v2p8_on;
};

static struct gmin_subdev gmin_subdevs[MAX_SUBDEVS];

static enum { PMIC_UNSET=0, PMIC_REGULATOR, PMIC_AXP, PMIC_TI } pmic_id;

/* The atomisp uses type==0 for the end-of-list marker, so leave space. */
static struct intel_v4l2_subdev_table pdata_subdevs[MAX_SUBDEVS+1];

static const struct atomisp_platform_data pdata = {
	.subdevs = pdata_subdevs,
	.spid = &spid,
};

/*
 * Something of a hack.  The ECS E7 board drives camera 2.8v from an
 * external regulator instead of the PMIC.  There's a gmin_CamV2P8
 * config variable that specifies the GPIO to handle this particular
 * case, but this needs a broader architecture for handling camera
 * power.
 */
enum { V2P8_GPIO_UNSET = -2, V2P8_GPIO_NONE = -1 };
static int v2p8_gpio = V2P8_GPIO_UNSET;

/*
 * Legacy/stub behavior copied from upstream platform_camera.c.  The
 * atomisp driver relies on these values being non-NULL in a few
 * places, even though they are hard-coded in all current
 * implementations.
 */
const struct atomisp_camera_caps *atomisp_get_default_camera_caps(void)
{
	static const struct atomisp_camera_caps caps = {
		.sensor_num = 1,
		.sensor = {
			{ .stream_num = 1, },
		},
	};
	return &caps;
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
	return &pdata;
}
EXPORT_SYMBOL_GPL(atomisp_get_platform_data);

static int af_power_ctrl(struct v4l2_subdev *subdev, int flag)
{
	return 0;
}

/*
 * Used in a handful of modules.  Focus motor control, I think.  Note
 * that there is no configurability in the API, so this needs to be
 * fixed where it is used.
 *
 * struct camera_af_platform_data {
 *     int (*power_ctrl)(struct v4l2_subdev *subdev, int flag);
 * };
 *
 * Note that the implementation in MCG platform_camera.c is stubbed
 * out anyway (i.e. returns zero from the callback) on BYT.  So
 * neither needed on gmin platforms or supported upstream.
 */
const struct camera_af_platform_data *camera_get_af_platform_data(void)
{
	static struct camera_af_platform_data afpd = {
		.power_ctrl = af_power_ctrl,
	};
	return &afpd;
}
EXPORT_SYMBOL_GPL(camera_get_af_platform_data);

int atomisp_register_i2c_module(struct i2c_client *client,
                                enum intel_v4l2_subdev_type type,
                                enum atomisp_camera_port port)
{
	int i;
	struct i2c_board_info *bi;

	dev_info(&client->dev, "register atomisp i2c module type %d on port %d\n", type, port);

	for (i=0; i < MAX_SUBDEVS; i++)
		if (!pdata.subdevs[i].type)
			break;

	if (pdata.subdevs[i].type)
		return -ENOMEM;

	pdata.subdevs[i].type = type;
	pdata.subdevs[i].port = port;
	pdata.subdevs[i].v4l2_subdev.i2c_adapter_id = client->adapter->nr;

	/* Convert i2c_client to i2c_board_info */
	bi = &pdata.subdevs[i].v4l2_subdev.board_info;
	memcpy(bi->type, client->name, I2C_NAME_SIZE);
	bi->flags = client->flags;
	bi->addr = client->addr;
	bi->irq = client->irq;
	bi->comp_addr_count = client->comp_addr_count;
	bi->comp_addrs = client->comp_addrs;
	bi->irq_flags = client->irq_flags;
	bi->platform_data = plat_data;

	return 0;
}
EXPORT_SYMBOL_GPL(atomisp_register_i2c_module);

struct gmin_cfg_var {
	const char *name, *val;
};

static const struct gmin_cfg_var ffrd8_vars[] = {
	{ "INTCF1B:00_ImxId",    "0x134" },
	{ "INTCF1B:00_CamType",  "1" },
	{ "INTCF1B:00_CsiPort",  "1" },
	{ "INTCF1B:00_CsiLanes", "4" },
	{ "INTCF1B:00_CsiFmt",   "13" },
	{ "INTCF1B:00_CsiBayer", "1" },
	{ "INTCF1B:00_CamClk", "0" },
	{},
};

static const struct gmin_cfg_var mrd7_vars[] = {
	/* GC2235 world-facing camera: */
	{ "INT33F8:00_CamType",  "1" },
	{ "INT33F8:00_CsiPort",  "1" },
	{ "INT33F8:00_CsiLanes", "2" },
	{ "INT33F8:00_CsiFmt",   "13" },
	{ "INT33F8:00_CsiBayer", "0" },
	{ "INT33F8:00_CamClk", "0" },
	/* GC0339 user-facing camera: */
	{ "INT33F9:00_CamType",  "1" },
	{ "INT33F9:00_CsiPort",  "0" },
	{ "INT33F9:00_CsiLanes", "1" },
	{ "INT33F9:00_CsiFmt",   "13" },
	{ "INT33F9:00_CsiBayer", "0" },
	{ "INT33F9:00_CamClk", "1" },

	/* These values are actually for the ecs_e7 board, which sadly
	 * identifies itself with identical DMI data to the MRD7.  But
	 * we can get away with it because the ACPI IDs are
	 * different. */
	{ "gmin_V2P8GPIO", "402" },
	/* OV5693 world-facing camera: */
	{ "INT33BE:00_CamType",  "1" },
	{ "INT33BE:00_CsiPort",  "1" },
	{ "INT33BE:00_CsiLanes", "2" },
	{ "INT33BE:00_CsiFmt",   "13" },
	{ "INT33BE:00_CsiBayer", "2" },
	{ "INT33BE:00_CamClk", "0" },
	{ "INT33BE:00_I2CAddr", "16" }, /* BIOS ACPI bug workaround */
	/* MT9M114 user-facing camera: */
	{ "CRMT1040:00_CamType",  "1" },
	{ "CRMT1040:00_CsiPort",  "0" },
	{ "CRMT1040:00_CsiLanes", "1" },
	{ "CRMT1040:00_CsiFmt",   "13" },
	{ "CRMT1040:00_CsiBayer", "0" }, /* FIXME: correct? */
	{ "CRMT1040:00_CamClk", "1" },
	{},
};

/* Cribbed from MCG defaults in the mt9m114 driver, not actually verified
 * vs. T100 hardware */
static const struct gmin_cfg_var t100_vars[] = {
	{ "INT33F0:00_CamType",  "2" },
	{ "INT33F0:00_CsiPort",  "0" },
	{ "INT33F0:00_CsiLanes", "1" },
	{ "INT33F0:00_CsiFmt",   "0" },
	{ "INT33F0:00_CsiBayer", "0" },
	{ "INT33F0:00_CamClk",   "1" },
	{},
};

static const struct {
	const char *dmi_board_name;
	const struct gmin_cfg_var *vars;
} hard_vars[] = {
	{ "BYT-T FFD8", ffrd8_vars },
	{ "TABLET", mrd7_vars },
	{ "T100TA", t100_vars },
};


#define GMIN_CFG_VAR_EFI_GUID EFI_GUID(0xecb54cd9, 0xe5ae, 0x4fdc, \
				       0xa9, 0x71, 0xe8, 0x77,	   \
				       0x75, 0x60, 0x68, 0xf7)

#define CFG_VAR_NAME_MAX 64

static int gmin_platform_init(struct i2c_client *client)
{
	return 0;
}

static int gmin_platform_deinit(void)
{
	return 0;
}

static int match_i2c_name(struct device *dev, void *name)
{
	return !strcmp(to_i2c_client(dev)->name, (char *)name);
}

static bool i2c_dev_exists(char *name)
{
	return !!bus_find_device(&i2c_bus_type, NULL, name, match_i2c_name);
}

static struct gmin_subdev *gmin_subdev_add(struct v4l2_subdev *subdev)
{
	int i, ret;
	struct device *dev;
        struct i2c_client *client = v4l2_get_subdevdata(subdev);

	if (!pmic_id) {
		if (i2c_dev_exists(DEVNAME_PMIC_AXP))
			pmic_id = PMIC_AXP;
		else if (i2c_dev_exists(DEVNAME_PMIC_TI))
			pmic_id = PMIC_TI;
		else
			pmic_id = PMIC_REGULATOR;
	}

	if (!client)
		return NULL;

	dev = client ? &client->dev : NULL;

	for (i=0; i < MAX_SUBDEVS && gmin_subdevs[i].subdev; i++)
		;
	if (i >= MAX_SUBDEVS)
		return NULL;

	dev_info(dev, "gmin: initializing atomisp module subdev data.\n");

	gmin_subdevs[i].subdev = subdev;
	gmin_subdevs[i].clock_num = gmin_get_var_int(dev, "CamClk", 0);
	gmin_subdevs[i].gpio0 = gpiod_get_index(dev, "cam_gpio0", 0);
	gmin_subdevs[i].gpio1 = gpiod_get_index(dev, "cam_gpio1", 1);

	if (!IS_ERR(gmin_subdevs[i].gpio0)) {
		ret = gpiod_direction_output(gmin_subdevs[i].gpio0, 0);
		if (ret)
			dev_err(dev, "gpio0 set output failed: %d\n", ret);
	} else {
		gmin_subdevs[i].gpio0 = NULL;
	}

	if (!IS_ERR(gmin_subdevs[i].gpio1)) {
		ret = gpiod_direction_output(gmin_subdevs[i].gpio1, 0);
		if (ret)
			dev_err(dev, "gpio1 set output failed: %d\n", ret);
	} else {
		gmin_subdevs[i].gpio1 = NULL;
	}

	if (pmic_id == PMIC_REGULATOR) {
		gmin_subdevs[i].v1p8_reg = regulator_get(dev, "v1p8sx");
		gmin_subdevs[i].v2p8_reg = regulator_get(dev, "v2p85sx");

		/* Note: ideally we would initialize v[12]p8_on to the
		 * output of regulator_is_enabled(), but sadly that
		 * API is broken with the current drivers, returning
		 * "1" for a regulator that will then emit a
		 * "unbalanced disable" WARNing if we try to disable
		 * it. */
	}

	return &gmin_subdevs[i];
}

static struct gmin_subdev *find_gmin_subdev(struct v4l2_subdev *subdev)
{
	int i;
	for (i=0; i < MAX_SUBDEVS; i++)
		if (gmin_subdevs[i].subdev == subdev)
			return &gmin_subdevs[i];
	return gmin_subdev_add(subdev);
}

static int gmin_gpio0_ctrl(struct v4l2_subdev *subdev, int on)
{
	struct gmin_subdev *gs = find_gmin_subdev(subdev);

	if (gs && gs->gpio0) {
		gpiod_set_value(gs->gpio0, on);
		return 0;
	}
	return -EINVAL;
}

static int gmin_gpio1_ctrl(struct v4l2_subdev *subdev, int on)
{
	struct gmin_subdev *gs = find_gmin_subdev(subdev);
	if (gs && gs->gpio1) {
		gpiod_set_value(gs->gpio1, on);
		return 0;
	}
	return -EINVAL;
}

static int axp_regulator_set(int sel_reg, u8 setting, int ctrl_reg, int shift, bool on)
{
	int ret;
	int val;
	u8 val_u8;

	ret = intel_soc_pmic_writeb(sel_reg, setting);
	if (ret)
		return ret;
	val = intel_soc_pmic_readb(ctrl_reg);
	if (val < 0)
		return val;
	val_u8 = (u8)val;
	if (on)
		val |= ((u8)1 << shift);
	else
		val &= ~((u8)1 << shift);
	ret = intel_soc_pmic_writeb(ctrl_reg, val_u8);
	if (ret)
		return ret;

	return 0;
}

static int axp_v1p8_on(void)
{
	int ret;
	ret = axp_regulator_set(ELDO2_SEL_REG, ELDO2_1P8V, ELDO_CTRL_REG,
		ELDO2_CTRL_SHIFT, true);
	if (ret)
		return ret;

	/* This sleep comes out of the gc2235 driver, which is the
	 * only one I currently see that wants to set both 1.8v rails. */
	usleep_range(110, 150);

	ret = axp_regulator_set(ELDO1_SEL_REG, ELDO1_1P8V, ELDO_CTRL_REG,
		ELDO1_CTRL_SHIFT, true);
	if (ret)
		axp_regulator_set(ELDO2_SEL_REG, ELDO2_1P8V, ELDO_CTRL_REG,
				     ELDO2_CTRL_SHIFT, false);
	return ret;
}

static int axp_v1p8_off(void)
{
	int ret;
	ret = axp_regulator_set(ELDO1_SEL_REG, ELDO1_1P8V, ELDO_CTRL_REG,
				ELDO1_CTRL_SHIFT, false);
	ret |= axp_regulator_set(ELDO2_SEL_REG, ELDO2_1P8V, ELDO_CTRL_REG,
				 ELDO2_CTRL_SHIFT, false);
	return ret;
}


static int axp_v2p8_on(void)
{
	int ret;
	ret = axp_regulator_set(ALDO1_SEL_REG, ALDO1_2P8V, ALDO1_CTRL3_REG,
		ALDO1_CTRL3_SHIFT, true);
	return ret;
}

static int axp_v2p8_off(void)
{
	return axp_regulator_set(ALDO1_SEL_REG, ALDO1_2P8V, ALDO1_CTRL3_REG,
				 ALDO1_CTRL3_SHIFT, false);
}

int gmin_v1p8_ctrl(struct v4l2_subdev *subdev, int on)
{
	struct gmin_subdev *gs = find_gmin_subdev(subdev);

	if (gs && gs->v1p8_on == on)
		return 0;
	gs->v1p8_on = on;

	if (gs && gs->v1p8_reg) {
		if (on)
			return regulator_enable(gs->v1p8_reg);
		else
			return regulator_disable(gs->v1p8_reg);
	}

	if (pmic_id == PMIC_AXP) {
		if (on)
			return axp_v1p8_on();
		else
			return axp_v1p8_off();
	}

	if (pmic_id == PMIC_TI) {
		if (on)
			return intel_soc_pmic_writeb(LDO10_REG, LDO10_1P8V_ON);
		else
			return intel_soc_pmic_writeb(LDO10_REG, LDO10_1P8V_OFF);
	}

	return -EINVAL;
}

int gmin_v2p8_ctrl(struct v4l2_subdev *subdev, int on)
{
	struct gmin_subdev *gs = find_gmin_subdev(subdev);
	int ret;

	if (v2p8_gpio == V2P8_GPIO_UNSET) {
		v2p8_gpio = gmin_get_var_int(NULL, "V2P8GPIO", V2P8_GPIO_NONE);
		if (v2p8_gpio != V2P8_GPIO_NONE) {
			pr_info("atomisp_gmin_platform: 2.8v power on GPIO %d\n",
				v2p8_gpio);
			ret = gpio_request(v2p8_gpio, "camera_v2p8");
			if (!ret)
				ret = gpio_direction_output(v2p8_gpio, 0);
			if (ret)
				pr_err("V2P8 GPIO initialization failed\n");
		}
	}


	if (gs && gs->v2p8_on == on)
		return 0;
	gs->v2p8_on = on;

	if (gs && v2p8_gpio >= 0) {
		gpio_set_value(v2p8_gpio, on);
		return 0;
	}

	if (gs && gs->v2p8_reg) {
		if (on)
			return regulator_enable(gs->v2p8_reg);
		else
			return regulator_disable(gs->v2p8_reg);
	}

	if (pmic_id == PMIC_AXP) {
		if (on)
			return axp_v2p8_on();
		else
			return axp_v2p8_off();
	}

	if (pmic_id == PMIC_TI) {
		if (on)
			return intel_soc_pmic_writeb(LDO9_REG, LDO9_2P8V_OFF);
		else
			return intel_soc_pmic_writeb(LDO9_REG, LDO9_2P8V_OFF);
	}

	return -EINVAL;
}

int gmin_flisclk_ctrl(struct v4l2_subdev *subdev, int on)
{
	int ret = 0;
	struct gmin_subdev *gs = find_gmin_subdev(subdev);
	if (on)
		ret = vlv2_plat_set_clock_freq(gs->clock_num, VLV2_CLK_19P2MHZ);
	if (ret)
		return ret;
	return vlv2_plat_configure_clock(gs->clock_num,
					 on ? VLV2_CLK_ON : VLV2_CLK_OFF);
}

static int gmin_csi_cfg(struct v4l2_subdev *sd, int flag)
{
	int port, lanes, format, bayer;
	struct device *dev;
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (!client)
		return -ENODEV;
	dev = &client->dev;

	port = gmin_get_var_int(dev, "CsiPort", -1);
	lanes = gmin_get_var_int(dev, "CsiLanes", -1);
	format = gmin_get_var_int(dev, "CsiFmt", -1);
	bayer = gmin_get_var_int(dev, "CsiBayer", -1);

	if (port < 0 || lanes < 0 || format < 0 || bayer < 0) {
		dev_err(dev, "Incomplete camera CSI configuration\n");
		return -EINVAL;
	}

	return camera_sensor_csi(sd, port, lanes, format, bayer, flag);
}

static struct camera_sensor_platform_data gmin_plat = {
	.gpio0_ctrl = gmin_gpio0_ctrl,
	.gpio1_ctrl = gmin_gpio1_ctrl,
	.v1p8_ctrl = gmin_v1p8_ctrl,
	.v2p8_ctrl = gmin_v2p8_ctrl,
	.flisclk_ctrl = gmin_flisclk_ctrl,
	.platform_init = gmin_platform_init,
	.platform_deinit = gmin_platform_deinit,
	.csi_cfg = gmin_csi_cfg,
};

struct camera_sensor_platform_data *gmin_camera_platform_data(void)
{
	return &gmin_plat;
}
EXPORT_SYMBOL_GPL(gmin_camera_platform_data);

/* Retrieves a device-specific configuration variable.  The dev
 * argument should be a device with an ACPI companion, as all
 * configuration is based on firmware ID. */
int gmin_get_config_var(struct device *dev, const char *var, char *out, size_t *out_len)
{
	struct device *adev;
	char var8[CFG_VAR_NAME_MAX];
	unsigned short var16[CFG_VAR_NAME_MAX];
	u32 efiattr_dummy;
	int i, j, ret;
	unsigned long efilen;

	if (!ACPI_COMPANION(dev))
		return -ENODEV;

	adev = &ACPI_COMPANION(dev)->dev;

	ret = snprintf(var8, sizeof(var8), "%s_%s", dev_name(adev), var);
	if (ret < 0 || ret >= sizeof(var8)-1)
		return -EINVAL;

	/* First check a hard-coded list of board-specific variables.
	 * Some device firmwares lack the ability to set EFI variables at
	 * runtime. */
	for (i = 0; i < ARRAY_SIZE(hard_vars); i++) {
		if (dmi_match(DMI_BOARD_NAME, hard_vars[i].dmi_board_name)) {
			for (j = 0; hard_vars[i].vars[j].name; j++) {
				size_t vl;
				const struct gmin_cfg_var *gv;

				gv = &hard_vars[i].vars[j];
				vl = strlen(gv->val);

				if (strcmp(var8, gv->name))
					continue;
				if (vl > *out_len-1)
					return -ENOSPC;

				memcpy(out, gv->val, min(*out_len, vl+1));
				out[*out_len-1] = 0;
				*out_len = vl;

				return 0;
			}
		}
	}

	/* Our variable names are ASCII by construction, but EFI names
	 * are wide chars.  Convert and zero-pad. */
	memset(var16, 0, sizeof(var16));
	for (i=0; var8[i] && i < sizeof(var8); i++)
		var16[i] = var8[i];

	if (!efi.get_variable)
		return -EINVAL;

	ret = efi.get_variable(var16, &GMIN_CFG_VAR_EFI_GUID, &efiattr_dummy,
			       &efilen, out);
	*out_len = efilen;

	return ret == EFI_SUCCESS ? 0 : -EINVAL;
}
EXPORT_SYMBOL_GPL(gmin_get_config_var);

int gmin_get_var_int(struct device *dev, const char *var, int def)
{
	char val[16];
	size_t len = sizeof(val);
	long result;
	int ret;

	ret = gmin_get_config_var(dev, var, val, &len);
	if (!ret) {
		val[len] = 0;
		ret = kstrtol(val, 0, &result);
	}

	return ret ? def : result;
}
EXPORT_SYMBOL_GPL(gmin_get_var_int);

int camera_sensor_csi(struct v4l2_subdev *sd, u32 port,
		      u32 lanes, u32 format, u32 bayer_order, int flag)
{
        struct i2c_client *client = v4l2_get_subdevdata(sd);
        struct camera_mipi_info *csi = NULL;

        if (flag) {
                csi = kzalloc(sizeof(*csi), GFP_KERNEL);
                if (!csi) {
                        dev_err(&client->dev, "out of memory\n");
                        return -ENOMEM;
                }
                csi->port = port;
                csi->num_lanes = lanes;
                csi->input_format = format;
                csi->raw_bayer_order = bayer_order;
                v4l2_set_subdev_hostdata(sd, (void *)csi);
                csi->metadata_format = ATOMISP_INPUT_FORMAT_EMBEDDED;
                csi->metadata_effective_width = NULL;
                dev_info(&client->dev,
                         "camera pdata: port: %d lanes: %d order: %8.8x\n",
                         port, lanes, bayer_order);
        } else {
                csi = v4l2_get_subdev_hostdata(sd);
                kfree(csi);
        }

        return 0;
}
EXPORT_SYMBOL_GPL(camera_sensor_csi);
