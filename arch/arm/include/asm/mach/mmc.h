/*
 *  arch/arm/include/asm/mach/mmc.h
 */
#ifndef ASMARM_MACH_MMC_H
#define ASMARM_MACH_MMC_H

#include <linux/mmc/host.h>
#include <linux/mmc/card.h>
#include <linux/mmc/sdio_func.h>
#include <mach/gpio.h>

#define SDC_DAT1_DISABLE 0
#define SDC_DAT1_ENABLE  1
#define SDC_DAT1_ENWAKE  2
#define SDC_DAT1_DISWAKE 3

struct embedded_sdio_data {
        struct sdio_cis cis;
        struct sdio_cccr cccr;
        struct sdio_embedded_func *funcs;
        int num_funcs;
};

/* This structure keeps information per regulator */
struct msm_mmc_reg_data {
	/* voltage regulator handle */
	struct regulator *reg;
	/* regulator name */
	const char *name;
	/* voltage level to be set */
	unsigned int low_vol_level;
	unsigned int high_vol_level;
	/* Load values for low power and high power mode */
	unsigned int lpm_uA;
	unsigned int hpm_uA;
	/*
	 * is set voltage supported for this regulator?
	 * false => set voltage is not supported
	 * true  => set voltage is supported
	 *
	 * Some regulators (like gpio-regulators, LVS (low voltage swtiches)
	 * PMIC regulators) dont have the capability to call
	 * regulator_set_voltage or regulator_set_optimum_mode
	 * Use this variable to indicate if its a such regulator or not
	 */
	bool set_voltage_sup;
	/* is this regulator enabled? */
	bool is_enabled;
	/* is this regulator needs to be always on? */
	bool always_on;
	/* is low power mode setting required for this regulator? */
	bool lpm_sup;
};

/*
 * This structure keeps information for all the
 * regulators required for a SDCC slot.
 */
struct msm_mmc_slot_reg_data {
	struct msm_mmc_reg_data *vdd_data; /* keeps VDD/VCC regulator info */
	struct msm_mmc_reg_data *vccq_data; /* keeps VCCQ regulator info */
	struct msm_mmc_reg_data *vddp_data; /* keeps VDD Pad regulator info */
};

struct msm_mmc_gpio {
	u32 no;
	const char *name;
	bool is_always_on;
	bool is_enabled;
};

struct msm_mmc_gpio_data {
	struct msm_mmc_gpio *gpio;
	u8 size;
};

struct msm_mmc_pad_pull {
	enum msm_tlmm_pull_tgt no;
	u32 val;
};

struct msm_mmc_pad_pull_data {
	struct msm_mmc_pad_pull *on;
	struct msm_mmc_pad_pull *off;
	u8 size;
};

struct msm_mmc_pad_drv {
	enum msm_tlmm_hdrive_tgt no;
	u32 val;
};

struct msm_mmc_pad_drv_data {
	struct msm_mmc_pad_drv *on;
	struct msm_mmc_pad_drv *off;
	u8 size;
};

struct msm_mmc_pad_data {
	struct msm_mmc_pad_pull_data *pull;
	struct msm_mmc_pad_drv_data *drv;
};

struct msm_mmc_pin_data {
	/*
	 * = 1 if controller pins are using gpios
	 * = 0 if controller has dedicated MSM pads
	 */
	u8 is_gpio;
	u8 cfg_sts;
	struct msm_mmc_gpio_data *gpio_data;
	struct msm_mmc_pad_data *pad_data;
};

struct mmc_platform_data {
	unsigned int ocr_mask;			/* available voltages */
	int built_in;				/* built-in device flag */
	int card_present;			/* card detect state */
	u32 (*translate_vdd)(struct device *, unsigned int);
	unsigned int (*status)(struct device *);
	struct embedded_sdio_data *embedded_sdio;
	int (*register_status_notify)(void (*callback)(int card_present, void *dev_id), void *dev_id);
	/*
	 * XPC controls the maximum current in the
	 * default speed mode of SDXC card.
	 */
	unsigned int xpc_cap;
	/* Supported UHS-I Modes */
	unsigned int uhs_caps;
	void (*sdio_lpm_gpio_setup)(struct device *, unsigned int);
        unsigned int status_irq;
	unsigned int status_gpio;
	/* Indicates the polarity of the GPIO line when card is inserted */
	bool is_status_gpio_active_low;
        unsigned int sdiowakeup_irq;
        unsigned long irq_flags;
        unsigned long mmc_bus_width;
        int (*wpswitch) (struct device *);
	unsigned int msmsdcc_fmin;
	unsigned int msmsdcc_fmid;
	unsigned int msmsdcc_fmax;
	bool nonremovable;
	bool pclk_src_dfab;
	int (*cfg_mpm_sdiowakeup)(struct device *, unsigned);
	unsigned int wpswitch_gpio;
	unsigned char wpswitch_polarity;
	struct msm_mmc_slot_reg_data *vreg_data;
	int is_sdio_al_client;
	unsigned int *sup_clk_table;
	unsigned char sup_clk_cnt;
	struct msm_mmc_pin_data *pin_data;
	bool disable_bam;
	bool disable_runtime_pm;
	bool disable_cmd23;
	u32 swfi_latency;
};

#endif
