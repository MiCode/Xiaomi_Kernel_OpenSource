/*
 * Copyright (c) 2022, Xiaomi, Inc. All rights reserved.
 */
#ifndef _ISPV4_MFD_DEFS_H_
#define _ISPV4_MFD_DEFS_H_

#include <linux/platform_device.h>
#include <media/v4l2-dev.h>
#include <media/v4l2-device.h>
#include <media/ispv4_defs.h>

#define DRV_NAME "ispv4-v4l2"

struct xm_ispv4_rproc;
struct ispv4_rpept_dev;
struct ispv4_ionmap_dev;
enum ispv4_ionmap_region;
struct ispv4_pmic_data;
struct ispv4_ctrl_data;

struct ispv4_asst {
	int log_level;
};

struct ispv4_v4l2_dev {
	struct dentry *debugfs;
	struct ispv4_asst asst;
	struct module *cam_module;
	struct v4l2_device *v4l2_dev;
	struct v4l2_subdev *v4l2_s_isp;
	struct v4l2_subdev *v4l2_s_asst;
	struct video_device video;
	wait_queue_head_t wait;
	struct device *dev;
	atomic_t opened;

	struct {
		bool avalid;
		struct xm_ispv4_rproc *rp;
		int (*load_ddr_param)(struct xm_ispv4_rproc *rp, u8 *buf,
				      int len);
		int (*dump_ddr_param)(struct xm_ispv4_rproc *rp, u8 *buf,
				      int cap);
		void (*register_ipc_cb)(struct xm_ispv4_rproc *rp,
					int (*cb)(void *priv, void *data,
						  int epts, int len,
						  uint32_t addr),
					void *priv);
		void (*register_crash_cb)(struct xm_ispv4_rproc *rp,
					  int (*cb)(void *, int), void *priv);
		void (*register_ddrfb_cb)(struct xm_ispv4_rproc *rp,
					  int (*cb)(void *, int), void *priv);
		void (*register_exception_cb)(struct xm_ispv4_rproc *rp,
					      int (*cb)(void *, void *, int),
					      void *priv);
		void (*register_rpm_ready_cb)(struct xm_ispv4_rproc *rp,
					      int (*cb)(void *, int, bool),
					      void *priv);
		void (*dump_ramlog)(struct xm_ispv4_rproc *rp);
		void (*dump_debuginfo)(struct xm_ispv4_rproc *rp);
		void (*dump_bootinfo)(struct xm_ispv4_rproc *rp);
		int (*boot)(struct xm_ispv4_rproc *rp, bool *);
		int (*shutdown)(struct xm_ispv4_rproc *rp);
		bool (*ddr_kernel_data_avalid)(struct xm_ispv4_rproc *rp);
		bool (*ddr_kernel_data_update)(struct xm_ispv4_rproc *rp);
		bool (*get_boot_status)(struct xm_ispv4_rproc *rp);
	} v4l2_rproc;

	struct {
		bool avalid;
		struct ispv4_rpept_dev *epdev;
		struct xm_ispv4_rproc *rp;
		atomic_t avalid_num;
		// int (*ctrl)(struct ispv4_rpept_dev *epdev, unsigned int cmd,
		// 	    unsigned long arg);
		long (*send)(struct xm_ispv4_rproc *rp, enum xm_ispv4_etps ept,
			     u32 cmd, int len, void *data, bool user_buf,
			     int *msgid);
		long (*recv)(struct xm_ispv4_rproc *rp, enum xm_ispv4_etps ept,
			     int cap, void *data, bool user_buf);
		long (*get_err)(struct xm_ispv4_rproc *rp,
				enum xm_ispv4_etps ept, int cap, void *data,
				bool user_buf);
		int (*register_cb)(struct xm_ispv4_rproc *rp,
				   enum xm_ispv4_etps ept,
				   void (*cb)(void *, void *, void *, int),
				   void *priv);
	} v4l2_rpmsg;

	struct {
		bool avalid;
		struct ispv4_ionmap_dev *dev;
		int (*mapfd)(struct ispv4_ionmap_dev *dev, int fd,
			     enum ispv4_ionmap_region region, u32 *iova);
		int (*unmap)(struct ispv4_ionmap_dev *dev,
			     enum ispv4_ionmap_region region);
		int (*mapfd_no_region)(struct ispv4_ionmap_dev *dev, int fd,
			     u32 *iova);
		int (*unmap_no_region)(struct ispv4_ionmap_dev *dev,
			     int fd);
		int (*remove_any_mappd)(struct ispv4_ionmap_dev *dev);
	} v4l2_ionmap;

	struct {
		bool avalid;
		int (*dbypass_pmic_boot)(void *priv);
	} v4l2_regops;

	struct {
		struct ispv4_pmic_data *data;
		int (*config)(struct ispv4_pmic_data *pdata, enum ispv4_pmic_config cfg);
		int (*regulator)(struct ispv4_pmic_data *pdata, uint32_t ops,
				uint32_t id, uint32_t *en, uint32_t *v);
		bool avalid;
	} v4l2_pmic;

	struct {
		struct ispv4_ctrl_data *data;
		struct platform_device *pdev;
		int (*clk_enable)(struct platform_device *pdev);
		int (*clk_disable)(struct platform_device *pdev);
		int (*ispv4_power_on_seq)(struct platform_device *pdev);
		void (*ispv4_power_off_seq)(struct platform_device *pdev);
		int (*ispv4_get_powerstat)(struct platform_device *pdev);
#if !(IS_ENABLED(CONFIG_MIISP_CHIP))
		void (*ispv4_fpga_reset)(struct platform_device *pdev);
#endif
		void (*mipi_iso_enable)(void *data);
		void (*mipi_iso_disable)(void *data);
		void (*enable_wdt_irq)(void *data);
		void (*disable_wdt_irq)(void *data);
		void (*register_wdt_cb)(void *data, int (*cb)(void *),
					void *cb_priv);
		void (*register_sof_cb)(void *data, int (*cb)(void *),
					void *cb_priv);
		bool avalid;
	} v4l2_ctrl;

	struct {
		bool avalid;
		void *pcidata;
		void *pcidev;
		bool linkup;
		bool sof_registered;
		bool eof_registered;
		bool thermal_registered;
		int (*resume_pci)(void *priv);
		int (*suspend_pci)(void *priv);
		int (*suspend_pci_force)(void *priv);
		int (*hdma_trans)(void *hdma, int dir, void *data, int len,
				  int ep_addr);
		int (*pcie_msi_register)(void* priv, enum pcie_msi msi,
			irq_handler_t thread_fn, const char *name, void *data);
		void (*pcie_msi_unregister)(void* priv, enum pcie_msi msi,
					    void *data);
		 void (*set_isp_time)(void* priv, u64);
	} v4l2_pci;

	struct {
		bool avalid;
		void *spi;
		void *spidev;
		void *pci;
		int (*boot_pci)(void *priv);
		int (*update_phy)(void *priv, u8 *data, int len);
		int (*shutdown_pci)(void *priv);
		int (*spi_speed)(void *priv, u32 speed);
		void (*spi_gettick)(void *priv);
	} v4l2_spi;

	struct device first_master;
	struct device early_master;
	struct device late_master;

	bool first_bound;
	bool early_bound;
	bool late_bound;
	bool probe_finish;
};

#endif
