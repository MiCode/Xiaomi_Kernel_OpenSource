#ifndef __INTEL_PMIC_CCSM_H__
#define __INTEL_PMIC_CCSM_H__

struct temp_lookup {
	int temp;
	int raw;
};

struct pmic_regs {
	u16 pmic_id;
	u16 pmic_irqlvl1;
	u16 pmic_mirqlvl1;
	u16 pmic_chgrirq0;
	u16 pmic_schgrirq0;
	u16 pmic_mchgrirq0;
	u16 pmic_chgrirq1;
	u16 pmic_schgrirq1;
	u16 pmic_mchgrirq1;
	u16 pmic_chgrctrl0;
	u16 pmic_chgrctrl1;
	u16 pmic_lowbattdet0;
	u16 pmic_lowbattdet1;
	u16 pmic_battdetctrl;
	u16 pmic_vbusdetctrl;
	u16 pmic_vdcindetctrl;
	u16 pmic_chgrstatus;
	u16 pmic_usbidctrl;
	u16 pmic_usbidstat;
	u16 pmic_wakesrc;
	u16 pmic_usbphyctrl;
	u16 pmic_dbg_usbbc1;
	u16 pmic_dbg_usbbc2;
	u16 pmic_dbg_usbbcstat;
	u16 pmic_usbpath;
	u16 pmic_usbsrcdetstat;
	u16 pmic_chrttaddr;
	u16 pmic_chrttdata;
	u16 pmic_thrmbatzone;
	u16 pmic_thrmzn0h;
	u16 pmic_thrmzn0l;
	u16 pmic_thrmzn1h;
	u16 pmic_thrmzn1l;
	u16 pmic_thrmzn2h;
	u16 pmic_thrmzn2l;
	u16 pmic_thrmzn3h;
	u16 pmic_thrmzn3l;
	u16 pmic_thrmzn4h;
	u16 pmic_thrmzn4l;
	u16 pmic_thrmirq0;
	u16 pmic_mthrmirq0;
	u16 pmic_sthrmirq0;
	u16 pmic_thrmirq1;
	u16 pmic_mthrmirq1;
	u16 pmic_sthrmirq1;
	u16 pmic_thrmirq2;
	u16 pmic_mthrmirq2;
	u16 pmic_sthrmirq2;
};

enum pmic_ccsm_int {
	PMIC_INT_VBUS = 0,
	PMIC_INT_DCIN,
	PMIC_INT_BATTDET,
	PMIC_INT_USBIDFLTDET,
	PMIC_INT_USBIDGNDDET,
	PMIC_INT_USBIDDET,
	PMIC_INT_CTYP,
	PMIC_INT_BZIRQ = 16,
	PMIC_INT_BATCRIT,
	PMIC_INT_BATCRIT_HOTCOLD,
	PMIC_INT_BAT0ALRT0,
	PMIC_INT_BAT0ALRT3,
	PMIC_INT_BAT1ALRT0,
	PMIC_INT_BAT1ALRT3,
	PMIC_INT_EXTCHRGR = 32,
	PMIC_INT_I2CRDCMP,
	PMIC_INT_I2CWRCMP,
	PMIC_INT_I2CERR,
};

struct pmic_ccsm_int_cfg {
	enum pmic_ccsm_int pmic_int;
	u16 ireg;
	u16 mreg;
	u16 sreg;
	u16 mask;
};

/*
 * pmic cove charger driver info
 */
struct intel_pmic_ccsm_platform_data {
	void (*cc_to_reg)(int, u8*);
	void (*cv_to_reg)(int, u8*);
	void (*inlmt_to_reg)(int, u8*);
	int max_tbl_row_cnt;
	unsigned long cache_addr;
	struct temp_lookup *adc_tbl;
	struct pmic_regs *reg_map;
	struct pmic_ccsm_int_cfg *intmap;
	int intmap_size;
	bool usb_compliance;
};

extern int intel_pmic_get_status(void);
extern int intel_pmic_enable_charging(bool);
extern int intel_pmic_set_cc(int);
extern int intel_pmic_set_cv(int);
extern int intel_pmic_set_ilimma(int);
extern int intel_pmic_enable_vbus(bool enable);
/* WA for ShadyCove VBUS removal detect issue */
extern int intel_pmic_handle_low_supply(void);

extern void intel_pmic_ccsm_dump_regs(void);
extern int intel_pmic_get_health(void);
extern int intel_pmic_get_battery_pack_temp(int *);

#endif
