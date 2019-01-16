#ifndef __CLDMA_PLATFORM_H__
#define __CLDMA_PLATFORM_H__

// this is the platform header file for CLDMA MODEM, not just CLDMA!

/* Modem WDT */
#define WDT_MD_MODE		(0x00)
#define WDT_MD_LENGTH	(0x04)
#define WDT_MD_RESTART	(0x08)
#define WDT_MD_STA		(0x0C)
#define WDT_MD_SWRST	(0x1C)
#define WDT_MD_MODE_KEY	(0x0000220E)

/* CCIF */
#define APCCIF_CON    (0x00)
#define APCCIF_BUSY   (0x04)
#define APCCIF_START  (0x08)
#define APCCIF_TCHNUM (0x0C)
#define APCCIF_RCHNUM (0x10)
#define APCCIF_ACK    (0x14)
#define APCCIF_CHDATA (0x100)
#define APCCIF_SRAM_SIZE 512
// channel usage
#define EXCEPTION_NONE (0)
// AP to MD
#define H2D_EXCEPTION_ACK (1)
#define H2D_EXCEPTION_CLEARQ_ACK (2)
#define H2D_FORCE_MD_ASSERT (3)
// MD to AP
#define D2H_EXCEPTION_INIT (1)
#define D2H_EXCEPTION_INIT_DONE (2)
#define D2H_EXCEPTION_CLEARQ_DONE (3)
#define D2H_EXCEPTION_ALLQ_RESET (4)
// peer
#define AP_MD_PEER_WAKEUP (5)
#define AP_MD_SEQ_ERROR (6)

struct md_hw_info
{
	// HW info - Register Address
	unsigned long cldma_ap_ao_base;
	unsigned long cldma_md_ao_base;
	unsigned long cldma_ap_pdn_base;
	unsigned long cldma_md_pdn_base;    
	unsigned long md_rgu_base;
	unsigned long md_boot_slave_Vector;
	unsigned long md_boot_slave_Key;
	unsigned long md_boot_slave_En;
	unsigned long ap_ccif_base;
	unsigned long md_ccif_base;
	unsigned int sram_size;

	// HW info - Interrutpt ID
	unsigned int cldma_irq_id;
	unsigned int ap_ccif_irq_id;
	unsigned int md_wdt_irq_id;
	unsigned int ap2md_bus_timeout_irq_id;

	// HW info - Interrupt flags
	unsigned long cldma_irq_flags;
	unsigned long ap_ccif_irq_flags;
	unsigned long md_wdt_irq_flags;
	unsigned long ap2md_bus_timeout_irq_flags;
};

int ccci_modem_remove(struct platform_device *dev);
void ccci_modem_shutdown(struct platform_device *dev);
int ccci_modem_suspend(struct platform_device *dev, pm_message_t state);
int ccci_modem_resume(struct platform_device *dev);
int ccci_modem_pm_suspend(struct device *device);
int ccci_modem_pm_resume(struct device *device);
int ccci_modem_pm_restore_noirq(struct device *device);
int md_cd_power_on(struct ccci_modem *md);
int md_cd_power_off(struct ccci_modem *md, unsigned int timeout);
int md_cd_let_md_go(struct ccci_modem *md);
void md_cd_lock_cldma_clock_src(int locked);
int md_cd_bootup_cleanup(struct ccci_modem *md, int success);
int md_cd_low_power_notify(struct ccci_modem *md, LOW_POEWR_NOTIFY_TYPE type, int level);
void cldma_dump_register(struct ccci_modem *md);
int md_cd_get_modem_hw_info(struct platform_device *dev_ptr, struct ccci_dev_cfg *dev_cfg, struct md_hw_info *hw_info);
int md_cd_io_remap_md_side_register(struct ccci_modem *md);
void md_cd_dump_debug_register(struct ccci_modem *md);
void md_cd_check_emi_state(struct ccci_modem *md, int polling);

#endif //__CLDMA_PLATFORM_H__
