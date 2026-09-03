#ifndef __MUSB_SPRD_H__
#define __MUSB_SPRD_H__

#include <linux/clk.h>
#include <linux/dma-mapping.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/soc/sprd/sprd_usbpinmux.h>
#include <linux/usb.h>
#include <linux/usb/role.h>
#include <linux/usb/phy.h>
#include <linux/usb/usb_phy_generic.h>
#include <linux/wait.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>
#include <linux/usb/gadget.h>
#include <linux/usb/sprd_commonphy.h>
#include <linux/usb/sprd_typec.h>
#include <linux/usb/sprd_usbm.h>

#include "musb_core.h"
#include "sprd_musbhsdma.h"

#define DRIVER_DESC "Inventra Dual-Role USB Controller Driver"
#define MUSB_VERSION "6.0"
#define DRIVER_INFO DRIVER_DESC ", v" MUSB_VERSION

#define MUSB_AUTOSUSPEND_DELAY 1000

#define ID			0
#define B_SESS_VLD		1
#define B_SUSPEND		2
#define A_SUSPEND		3
#define A_RECOVER		4
#define B_DATA_DISABLED		5

#define RELAX_WAKE_LOCK_DELAY			(msecs_to_jiffies(8000))
#define CHARGER_DETECT_DELAY			(msecs_to_jiffies(1000))
#define VBUS_REG_CHECK_DELAY			(msecs_to_jiffies(1000))
#define MUSB_RUNTIME_CHECK_DELAY		(msecs_to_jiffies(200))
#define MUSB_UDC_START_CHECK_DELAY		(msecs_to_jiffies(50))
#define MUSB_DATA_ENABLE_CHECK_DELAY		(msecs_to_jiffies(200))
#define MUSB_CHG_WAIT_DETECT_DELAY		(msecs_to_jiffies(500))
#define MUSB_CHG_MAX_WAIT_BC1P2_COUNT		5
#define MUSB_SPRD_CHG_MAX_REDETECT_COUNT	3

/* Pls keep the same definition as PHY */
#define CHARGER_DETECT_DONE		BIT(0)
#define SPRD_USB_PHY_IGNORE_RESET	BIT(29)
#define CHARGER_2NDDETECT_ENABLE	BIT(30)
#define CHARGER_2NDDETECT_SELECT	BIT(31)

enum musb_drd_state {
	DRD_STATE_UNDEFINED = 0,
	DRD_STATE_IDLE,
	DRD_STATE_PERIPHERAL,
	DRD_STATE_PERIPHERAL_SUSPEND,
	DRD_STATE_HOST_IDLE,
	DRD_STATE_HOST,
	DRD_STATE_HOST_RECOVER,
	DRD_STATE_PERIPHERAL_DATA_DIS,
	DRD_STATE_RUNTIME_SUSPENDING,
};

enum usb_chg_detect_state {
	USB_CHG_STATE_UNDETECT = 0,
	USB_CHG_STATE_DETECT,
	USB_CHG_STATE_DETECTED,
	USB_CHG_STATE_RETRY_DETECT,
	USB_CHG_STATE_RETRY_DETECTED,
};

static const char *const state_names[] = {
	[DRD_STATE_UNDEFINED] = "undefined",
	[DRD_STATE_IDLE] = "idle",
	[DRD_STATE_PERIPHERAL] = "peripheral",
	[DRD_STATE_PERIPHERAL_SUSPEND] = "peripheral_suspend",
	[DRD_STATE_HOST_IDLE] = "host_idle",
	[DRD_STATE_HOST] = "host",
	[DRD_STATE_PERIPHERAL_DATA_DIS] = "peripheral_data_disabled",
	[DRD_STATE_RUNTIME_SUSPENDING] = "runtime_suspending",
};

struct sprd_usb_udc {
	struct usb_gadget_driver	*driver;
	struct usb_gadget		*gadget;
	struct device			dev;
	struct list_head		list;
	bool				vbus;
	bool				started;
};

struct musb_reg_info {
	struct regmap		*regmap_ptr;
	u32			args[2];
};

struct sprd_glue {
	struct device			*dev;
	struct musb			*musb;
	struct platform_device		*musb_pdev;
	struct clk			*clk;
	struct clk			*hclk_src_sel;
	struct clk			*hclk_suspend_src;
	struct clk			*hclk_default_src;
	struct phy			*phy;
	struct usb_phy			*xceiv;
	struct regulator		*vbus;
	struct wakeup_source		*pd_wake_lock;
	struct regmap			*pmu;
	struct musb_reg_info		pubsys_bypass;
	struct musb_reg_info		suspend_clk_src_frc_on;
	struct usb_role_switch		*role_sw;

	enum usb_role			role;
	enum usb_dr_mode		dr_mode;
	int				vbus_irq;
	int				usbid_irq;
	spinlock_t			lock;
	struct wakeup_source		*wake_lock;
	struct extcon_dev		*edev;
	struct extcon_dev		*id_edev;
	struct notifier_block		vbus_nb;
	struct notifier_block		audio_nb;
	struct notifier_block        audio_high_speed_nb;
	struct notifier_block		id_nb;

	bool				vbus_active;
	bool				is_audio_dev;
	bool				charging_mode;
	bool				enable_pm_suspend_in_host;
	atomic_t			pm_suspended;
	int				host_disabled;
	u32				usb_pub_slp_poll_offset;
	u32				usb_pub_slp_poll_mask;

	bool				retry_charger_detect;

	unsigned long			inputs;
	struct workqueue_struct		*musb_wq;
	struct workqueue_struct		*sm_usb_wq;
	struct work_struct		resume_work;
	struct work_struct        audio_work;
	struct delayed_work		sm_work;
	struct delayed_work		chg_detect_work;
	enum musb_vbus_id_status	id_state;
	enum musb_drd_state		drd_state;
	enum usb_chg_detect_state	chg_state;
	enum usb_charger_type		chg_type;
	int				wait_chg_detect_count;
	int				retry_chg_detect_count;
	int				start_host_retry_count;
	int				usb_data_enabled;
	bool				gadget_suspend;
	bool				host_recover;
	bool				in_restart;
	atomic_t			musb_runtime_suspended;
	struct mutex			suspend_resume_mutex;
	struct timer_list		relax_wakelock_timer;
	bool				wake_lock_relaxed;
	bool				use_singlefifo;
	bool				use_pdhub_c2c;
};
#endif	/* __MUSB_SPRD_H__ */
