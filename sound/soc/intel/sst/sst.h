
/*
 *  sst.h - Intel SST Driver for audio engine
 *
 *  Copyright (C) 2008-10 Intel Corporation
 *  Authors:	Vinod Koul <vinod.koul@intel.com>
 *		Harsha Priya <priya.harsha@intel.com>
 *		Dharageswari R <dharageswari.r@intel.com>
 *		KP Jeeja <jeeja.kp@intel.com>
 *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  Common private declarations for SST
 */
#ifndef __SST_H__
#define __SST_H__

#include <linux/dmaengine.h>
#include <linux/pm_runtime.h>
#include <linux/firmware.h>
#include <linux/intel_mid_dma.h>
#include <linux/lnw_gpio.h>
#include <asm/platform_sst.h>
#include <sound/intel_sst_ioctl.h>

#define SST_DRIVER_VERSION "3.0.8"

/* driver names */
#define SST_DRV_NAME "intel_sst_driver"
#define SST_CLV_PCI_ID	0x08E7
#define SST_MRFLD_PCI_ID 0x119A
#define SST_BYT_PCI_ID  0x0F28
#define SST_CHT_PCI_ID 0x22A8

#define SST_SUSPEND_DELAY 2000
#define FW_CONTEXT_MEM (64*1024)
#define SST_ICCM_BOUNDARY 4
#define SST_CONFIG_SSP_SIGN 0x7ffe8001

/* FIXME: All this info should come from platform data
 * move this when the base framework is ready to pass
 * platform data to SST driver
 */
#define MRFLD_FW_VIRTUAL_BASE 0xC0000000
#define MRFLD_FW_DDR_BASE_OFFSET 0x0
#define MRFLD_FW_FEATURE_BASE_OFFSET 0x4
#define MRFLD_FW_BSS_RESET_BIT 0
extern struct intel_sst_drv *sst_drv_ctx;
enum sst_states {
	SST_FW_LOADING = 1,
	SST_FW_RUNNING,
	SST_RESET,
	SST_SHUTDOWN,
	SST_RECOVERY,
};

enum sst_algo_ops {
	SST_SET_ALGO = 0,
	SST_GET_ALGO = 1,
};

#define SST_BLOCK_TIMEOUT	1000

/* SST register map */
#define SST_CSR			0x00
#define SST_PISR		0x08
#define SST_PIMR		0x10
#define SST_ISRX		0x18
#define SST_ISRD		0x20
#define SST_IMRX		0x28
#define SST_IMRD		0x30
#define SST_IPCX		0x38 /* IPC IA-SST */
#define SST_IPCD		0x40 /* IPC SST-IA */
#define SST_ISRSC		0x48
#define SST_ISRLPESC		0x50
#define SST_IMRSC		0x58
#define SST_IMRLPESC		0x60
#define SST_IPCSC		0x68
#define SST_IPCLPESC		0x70
#define SST_CLKCTL		0x78
#define SST_CSR2		0x80
#define SST_TMRCTL              0xC0
#define SST_TMRSTAT             0xC8

#define SST_SHIM_BEGIN		SST_CSR
#define SST_SHIM_END		SST_TMRSTAT
#define SST_SHIM_SIZE		0xD0

#define FW_SIGNATURE_SIZE	4

/* stream states */
enum sst_stream_states {
	STREAM_UN_INIT	= 0,	/* Freed/Not used stream */
	STREAM_RUNNING	= 1,	/* Running */
	STREAM_PAUSED	= 2,	/* Paused stream */
	STREAM_DECODE	= 3,	/* stream is in decoding only state */
	STREAM_INIT	= 4,	/* stream init, waiting for data */
	STREAM_RESET	= 5,	/* force reset on recovery */
};

enum sst_ram_type {
	SST_IRAM	= 1,
	SST_DRAM	= 2,
	SST_DDR	= 5,
	SST_CUSTOM_INFO	= 7,	/* consists of FW binary information */
};

/* SST shim registers to structure mapping  */
union config_status_reg {
	struct {
		u32 mfld_strb:1;
		u32 sst_reset:1;
		u32 clk_sel:3;
		u32 sst_clk:2;
		u32 bypass:3;
		u32 run_stall:1;
		u32 rsvd1:2;
		u32 strb_cntr_rst:1;
		u32 rsvd:18;
	} part;
	u32 full;
};

union interrupt_reg {
	struct {
		u64 done_interrupt:1;
		u64 busy_interrupt:1;
		u64 rsvd:62;
	} part;
	u64 full;
};

union sst_imr_reg {
	struct {
		u32 done_interrupt:1;
		u32 busy_interrupt:1;
		u32 rsvd:30;
	} part;
	u32 full;
};

union sst_pisr_reg {
	struct {
		u32 pssp0:1;
		u32 pssp1:1;
		u32 rsvd0:3;
		u32 dmac:1;
		u32 rsvd1:26;
	} part;
	u32 full;
};

union sst_pimr_reg {
	struct {
		u32 ssp0:1;
		u32 ssp1:1;
		u32 rsvd0:3;
		u32 dmac:1;
		u32 rsvd1:10;
		u32 ssp0_sc:1;
		u32 ssp1_sc:1;
		u32 rsvd2:3;
		u32 dmac_sc:1;
		u32 rsvd3:10;
	} part;
	u32 full;
};

union config_status_reg_mrfld {
	struct {
		u64 lpe_reset:1;
		u64 lpe_reset_vector:1;
		u64 runstall:1;
		u64 pwaitmode:1;
		u64 clk_sel:3;
		u64 rsvd2:1;
		u64 sst_clk:3;
		u64 xt_snoop:1;
		u64 rsvd3:4;
		u64 clk_sel1:6;
		u64 clk_enable:3;
		u64 rsvd4:6;
		u64 slim0baseclk:1;
		u64 rsvd:32;
	} part;
	u64 full;
};

union interrupt_reg_mrfld {
	struct {
		u64 done_interrupt:1;
		u64 busy_interrupt:1;
		u64 rsvd:62;
	} part;
	u64 full;
};

union sst_imr_reg_mrfld {
	struct {
		u64 done_interrupt:1;
		u64 busy_interrupt:1;
		u64 rsvd:62;
	} part;
	u64 full;
};

/*This structure is used to block a user/fw data call to another
fw/user call
*/
struct sst_block {
	bool	condition; /* condition for blocking check */
	int	ret_code; /* ret code when block is released */
	void	*data; /* data to be appsed for block if any */
	u32     size;
	bool	on;
	u32     msg_id;  /*msg_id = msgid in mfld/ctp, mrfld = 0 */
	u32     drv_id; /* = str_id in mfld/ctp, = drv_id in mrfld*/
	struct list_head node;
};

/**
 * struct stream_info - structure that holds the stream information
 *
 * @status : stream current state
 * @prev : stream prev state
 * @ops : stream operation pb/cp/drm...
 * @bufs: stream buffer list
 * @lock : stream mutex for protecting state
 * @pcm_substream : PCM substream
 * @period_elapsed : PCM period elapsed callback
 * @sfreq : stream sampling freq
 * @str_type : stream type
 * @cumm_bytes : cummulative bytes decoded
 * @str_type : stream type
 * @src : stream source
 * @device : output device type (medfield only)
 */
struct stream_info {
	unsigned int		status;
	unsigned int		prev;
	unsigned int		ops;
	struct mutex		lock; /* mutex */
	void			*pcm_substream;
	void (*period_elapsed)	(void *pcm_substream);
	unsigned int		sfreq;
	u32			cumm_bytes;
	void			*compr_cb_param;
	void (*compr_cb)	(void *compr_cb_param);
	void			*drain_cb_param;
	void (*drain_notify)	(void *drain_cb_param);

	unsigned int		num_ch;
	unsigned int		pipe_id;
	unsigned int		str_id;
	unsigned int		task_id;
};

#define SST_FW_SIGN "$SST"
#define SST_FW_LIB_SIGN "$LIB"

/*
 * struct fw_header - FW file headers
 *
 * @signature : FW signature
 * @modules : # of modules
 * @file_format : version of header format
 * @reserved : reserved fields
 */
struct fw_header {
	unsigned char signature[FW_SIGNATURE_SIZE]; /* FW signature */
	u32 file_size; /* size of fw minus this header */
	u32 modules; /*  # of modules */
	u32 file_format; /* version of header format */
	u32 reserved[4];
};

struct fw_module_header {
	unsigned char signature[FW_SIGNATURE_SIZE]; /* module signature */
	u32 mod_size; /* size of module */
	u32 blocks; /* # of blocks */
	u32 type; /* codec type, pp lib */
	u32 entry_point;
};

struct fw_block_info {
	enum sst_ram_type	type;	/* IRAM/DRAM */
	u32			size;	/* Bytes */
	u32			ram_offset; /* Offset in I/DRAM */
	u32			rsvd;	/* Reserved field */
};

struct sst_ipc_msg_wq {
	union ipc_header_mrfld mrfld_header;
	struct ipc_dsp_hdr dsp_hdr;
	char mailbox[SST_MAILBOX_SIZE];
	struct work_struct	wq;
	union ipc_header header;
};

struct sst_dma {
	struct dma_chan *ch;
	struct intel_mid_dma_slave slave;
	struct device *dev;
};

struct sst_runtime_param {
	struct snd_sst_runtime_params param;
};

struct sst_sg_list {
	struct scatterlist *src;
	struct scatterlist *dst;
	int list_len;
	unsigned int sg_idx;
};

struct sst_memcpy_list {
	struct list_head memcpylist;
	void *dstn;
	const void *src;
	u32 size;
	bool is_io;
};

struct sst_debugfs {
#ifdef CONFIG_DEBUG_FS
	struct dentry		*root;
#endif
	int			runtime_pm_status;
	void __iomem            *ssp[SST_MAX_SSP_PORTS];
	void __iomem            *dma_reg[SST_MAX_DMA];
	unsigned char get_params_data[1024];
	ssize_t get_params_len;
};

struct lpe_log_buf_hdr {
	u32 base_addr;
	u32 end_addr;
	u32 rd_addr;
	u32 wr_addr;
};

struct snd_ssp_config {
	int size;
	char bytes[0];
};

struct snd_sst_probe_bytes {
	u16 len;
	char bytes[0];
};

#define PCI_DMAC_CLV_ID 0x08F0
#define PCI_DMAC_MRFLD_ID 0x119B

struct sst_ram_buf {
	u32 size;
	char *buf;
};

/* Firmware Module Information*/

enum sst_lib_dwnld_status {
	SST_LIB_NOT_FOUND = 0,
	SST_LIB_FOUND,
	SST_LIB_DOWNLOADED,
};

struct sst_module_info {
	const char *name; /* Library name */
	u32	id; /* Module ID */
	u32	entry_pt; /* Module entry point */
	u8	status; /* module status*/
	u8	rsvd1;
	u16	rsvd2;
};

/* Structure for managing the Library Region(1.5MB)
 * in DDR in Merrifield
 */
struct sst_mem_mgr {
	phys_addr_t current_base;
	int avail;
	unsigned int count;
};

struct sst_dump_buf {
	/* buffers for iram-dram dump crash */
	struct sst_ram_buf iram_buf;
	struct sst_ram_buf dram_buf;
};

struct sst_ipc_reg {
	int ipcx;
	int ipcd;
};

struct sst_shim_regs64 {
	u64 csr;
	u64 pisr;
	u64 pimr;
	u64 isrx;
	u64 isrd;
	u64 imrx;
	u64 imrd;
	u64 ipcx;
	u64 ipcd;
	u64 isrsc;
	u64 isrlpesc;
	u64 imrsc;
	u64 imrlpesc;
	u64 ipcsc;
	u64 ipclpesc;
	u64 clkctl;
	u64 csr2;
	u64 tmrctl;
};

struct sst_vtsv_cache {
	void *file1_in_mem;
	u32 size1;
	void *file2_in_mem;
	u32 size2;
};

struct sst_monitor_lpe {
	u64 prev_match_val;
	struct work_struct mwork;
	struct timer_list sst_timer;
	u32 interval;
};

/***
 *
 * struct intel_sst_drv - driver ops
 *
 * @sst_state : current sst device state
 * @pci_id : PCI device id loaded
 * @shim : SST shim pointer
 * @mailbox : SST mailbox pointer
 * @iram : SST IRAM pointer
 * @dram : SST DRAM pointer
 * @pdata : SST info passed as a part of pci platform data
 * @shim_phy_add : SST shim phy addr
 * @shim_regs64: Struct to save shim registers
 * @ipc_dispatch_list : ipc messages dispatched
 * @rx_list : to copy the process_reply/process_msg from DSP
 * @ipc_post_msg_wq : wq to post IPC messages context
 * @ipc_post_msg : wq to post reply from FW context
 * @mad_ops : MAD driver operations registered
 * @mad_wq : MAD driver wq
 * @post_msg_wq : wq to post IPC messages
 * @streams : sst stream contexts
 * @list_lock : sst driver list lock (deprecated)
 * @ipc_spin_lock : spin lock to handle audio shim access and ipc queue
 * @rx_msg_lock : spin lock to handle the rx messages from the DSP
 * @scard_ops : sst card ops
 * @pci : sst pci device struture
 * @dev : pointer to current device struct
 * @sst_lock : sst device lock
 * @stream_lock : sst stream lock
 * @pvt_id : sst private id
 * @stream_cnt : total sst active stream count
 * @pb_streams : total active pb streams
 * @cp_streams : total active cp streams
 * @audio_start : audio status
 * @qos		: PM Qos struct
 * firmware_name : Firmware / Library name
 */
struct intel_sst_drv {
	int			sst_state;
	int			irq_num;
	unsigned int		pci_id;
	bool			use_32bit_ops;
	void __iomem		*ddr;
	void __iomem		*shim;
	void __iomem		*mailbox;
	void __iomem		*iram;
	void __iomem		*dram;
	void __iomem		*ssp0;
#if 0
	void __iomem		*dma0;
	void __iomem		*dma1;
#endif
	unsigned int		mailbox_add;
	unsigned int		iram_base;
	unsigned int		dram_base;
	unsigned int		shim_phy_add;
	unsigned int		iram_end;
	unsigned int		dram_end;
	unsigned int		ddr_end;
	unsigned int		ddr_base;
	unsigned int		mailbox_recv_offset;
	atomic_t		pm_usage_count;
	struct sst_shim_regs64	*shim_regs64;
	struct list_head        block_list;
	struct list_head	ipc_dispatch_list;
	struct sst_platform_info *pdata;
	struct sst_ipc_msg_wq   ipc_post_msg;
	struct list_head	rx_list;
	struct work_struct      ipc_post_msg_wq;
	wait_queue_head_t	wait_queue;
	struct workqueue_struct *mad_wq;
	struct workqueue_struct *recovery_wq; /*to queue work once recovery is triggered*/
	struct workqueue_struct *post_msg_wq;
	unsigned int		tstamp;
	struct stream_info	streams[MAX_NUM_STREAMS+1]; /*str_id 0 is not used*/
	spinlock_t		ipc_spin_lock; /* lock for Shim reg access and ipc queue */
	spinlock_t              block_lock; /* lock for adding block to block_list */
	spinlock_t              pvt_id_lock; /* lock for allocating private id */
	spinlock_t		rx_msg_lock;
	struct pci_dev		*pci;
	struct device		*dev;
	unsigned int		pvt_id;
	struct mutex            sst_lock;
	struct mutex		stream_lock;
	unsigned int		stream_cnt;
	unsigned int		*fw_cntx;
	unsigned int		fw_cntx_size;
	unsigned int		csr_value;
	struct sst_dma		dma;
	void			*fw_in_mem;
	struct sst_runtime_param runtime_param;
	unsigned int		device_input_mixer;
	struct mutex		mixer_ctrl_lock;
	struct dma_async_tx_descriptor *desc;
	struct sst_sg_list	fw_sg_list, library_list;
	struct intel_sst_ops	*ops;
	struct sst_debugfs	debugfs;
	struct pm_qos_request	*qos;
	struct sst_info	info;
	unsigned int		use_dma;
	unsigned int		use_lli;
	atomic_t		fw_clear_context;
	atomic_t		fw_clear_cache;
	bool			lib_dwnld_reqd;
	/* list used during FW download in memcpy mode */
	struct list_head	memcpy_list;
	/* list used during LIB download in memcpy mode */
	struct list_head	libmemcpy_list;
	/* holds the stucts of iram/dram local buffers for dump*/
	struct sst_dump_buf	dump_buf;
	/* Lock for CSR register change */
	struct mutex		csr_lock;
	/* byte control to set the probe stream */
	struct snd_sst_probe_bytes *probe_bytes;
	/* contains the ipc registers */
	struct sst_ipc_reg	ipc_reg;
	/* IMR region Library space memory manager */
	struct sst_mem_mgr      lib_mem_mgr;
	/* Contains the cached vtsv files*/
	struct sst_vtsv_cache	vcache;
	/* To store external lpe timer info and recovery timer info*/
	struct sst_monitor_lpe monitor_lpe;
	/* Pointer to device ID, now for same PCI_ID, HID will be
	 * will be different for FDK and EDK2. This will be used
	 * for devices where PCI or ACPI id is same but HID is
	 * different
	 */
	const char *hid;
	/* Holder for firmware name. Due to async call it needs to be
	 * persistent till worker thread gets called
	 */
	char firmware_name[30];
};

extern struct intel_sst_drv *sst_drv_ctx;
extern struct sst_platform_info byt_rvp_platform_data;
extern struct sst_platform_info byt_ffrd8_platform_data;
extern struct sst_platform_info cht_platform_data;

/* misc definitions */
#define FW_DWNL_ID 0xFF

struct sst_fill_config {
	u32 sign;
	struct sst_board_config_data sst_bdata;
	struct sst_platform_config_data sst_pdata;
	u32 shim_phy_add;
	u32 mailbox_add;
} __packed;

struct intel_sst_ops {
	irqreturn_t (*interrupt) (int, void *);
	irqreturn_t (*irq_thread) (int, void *);
	void (*clear_interrupt) (void);
	int (*start) (void);
	int (*reset) (void);
	void (*process_reply) (struct ipc_post *msg);
	void (*post_message) (struct work_struct *work);
	int (*sync_post_message) (struct ipc_post *msg);
	void (*process_message) (struct ipc_post *msg);
	void (*set_bypass)(bool set);
	int (*save_dsp_context) (struct intel_sst_drv *sst);
	void (*restore_dsp_context) (void);
	int (*alloc_stream) (char *params, struct sst_block *block);
	void (*post_download)(struct intel_sst_drv *sst);
	void (*do_recovery)(struct intel_sst_drv *sst);
};

int sst_alloc_stream(char *params, struct sst_block *block);
int sst_pause_stream(int id);
int sst_resume_stream(int id);
int sst_drop_stream(int id);
int sst_next_track(void);
int sst_free_stream(int id);
int sst_start_stream(int str_id);
int sst_send_byte_stream_mrfld(void *sbytes);
int sst_send_probe_bytes(struct intel_sst_drv *sst);
int sst_set_stream_param(int str_id, struct snd_sst_params *str_param);
int sst_set_metadata(int str_id, char *params);
int sst_get_stream(struct snd_sst_params *str_param);
int sst_get_stream_allocated(struct snd_sst_params *str_param,
				struct snd_sst_lib_download **lib_dnld);
int sst_drain_stream(int str_id, bool partial_drain);


int sst_sync_post_message_mfld(struct ipc_post *msg);
void sst_post_message_mfld(struct work_struct *work);
void sst_process_message_mfld(struct ipc_post *msg);
void sst_process_reply_mfld(struct ipc_post *msg);
int sst_start_mfld(void);
int intel_sst_reset_dsp_mfld(void);
void intel_sst_clear_intr_mfld(void);
void intel_sst_set_bypass_mfld(bool set);

int sst_sync_post_message_mrfld(struct ipc_post *msg);
void sst_post_message_mrfld(struct work_struct *work);
void sst_process_message_mrfld(struct ipc_post *msg);
void sst_process_reply_mrfld(struct ipc_post *msg);
int sst_start_mrfld(void);
int intel_sst_reset_dsp_mrfld(void);
void intel_sst_clear_intr_mrfld(void);
void sst_process_mad_ops(struct work_struct *work);

long intel_sst_ioctl(struct file *file_ptr, unsigned int cmd,
			unsigned long arg);
int intel_sst_open_cntrl(struct inode *i_node, struct file *file_ptr);
int intel_sst_release_cntrl(struct inode *i_node, struct file *file_ptr);

int sst_load_fw(void);
int sst_load_library(struct snd_sst_lib_download *lib, u8 ops);
int sst_load_all_modules_elf(struct intel_sst_drv *ctx,
		struct sst_module_info *mod_table, int mod_table_size);
int sst_get_next_lib_mem(struct sst_mem_mgr *mgr, int size,
			unsigned long *lib_base);
void sst_post_download_ctp(struct intel_sst_drv *ctx);
void sst_post_download_mrfld(struct intel_sst_drv *ctx);
void sst_post_download_byt(struct intel_sst_drv *ctx);
int sst_get_block_stream(struct intel_sst_drv *sst_drv_ctx);
void sst_memcpy_free_resources(void);

int sst_wait_interruptible(struct intel_sst_drv *sst_drv_ctx,
				struct sst_block *block);
int sst_wait_timeout(struct intel_sst_drv *sst_drv_ctx,
			struct sst_block *block);
int sst_create_ipc_msg(struct ipc_post **arg, bool large);
int sst_download_fw(void);
int free_stream_context(unsigned int str_id);
void sst_clean_stream(struct stream_info *stream);
int intel_sst_register_compress(struct intel_sst_drv *sst);
int intel_sst_remove_compress(struct intel_sst_drv *sst);
void sst_cdev_fragment_elapsed(int str_id);
int sst_send_sync_msg(int ipc, int str_id);
int sst_get_num_channel(struct snd_sst_params *str_param);
int sst_get_sfreq(struct snd_sst_params *str_param);
int intel_sst_check_device(void);
int sst_alloc_stream_ctp(char *params, struct sst_block *block);
int sst_alloc_stream_mrfld(char *params, struct sst_block *block);
void sst_restore_fw_context(void);
struct sst_block *sst_create_block(struct intel_sst_drv *ctx,
				u32 msg_id, u32 drv_id);
int sst_create_block_and_ipc_msg(struct ipc_post **arg, bool large,
		struct intel_sst_drv *sst_drv_ctx, struct sst_block **block,
		u32 msg_id, u32 drv_id);
int sst_free_block(struct intel_sst_drv *ctx, struct sst_block *freed);
int sst_wake_up_block(struct intel_sst_drv *ctx, int result,
		u32 drv_id, u32 ipc, void *data, u32 size);
int sst_alloc_drv_context(struct device *dev);
int sst_request_firmware_async(struct intel_sst_drv *ctx);
int sst_driver_ops(struct intel_sst_drv *sst);
struct sst_platform_info *sst_get_acpi_driver_data(const char *hid);
int sst_acpi_probe(struct platform_device *pdev);
int sst_acpi_remove(struct platform_device *pdev);
void sst_save_shim64(struct intel_sst_drv *ctx, void __iomem *shim,
		     struct sst_shim_regs64 *shim_regs);
void sst_firmware_load_cb(const struct firmware *fw, void *context);
int sst_send_vtsv_data_to_fw(struct intel_sst_drv *ctx);

void sst_do_recovery_mrfld(struct intel_sst_drv *sst);
void sst_do_recovery(struct intel_sst_drv *sst);
void sst_trigger_recovery(struct work_struct *work);
void sst_update_timer(struct intel_sst_drv *sst_drv_ctx);
void sst_do_recovery(struct intel_sst_drv *sst);
long intel_sst_ioctl_dsp(unsigned int cmd,
		struct snd_ppp_params *algo_params, unsigned long arg);

int sst_recovery_init(struct intel_sst_drv *sst_drv_ctx);
void sst_recovery_free(struct intel_sst_drv *sst_drv_ctx);
int sst_set_timer(struct sst_monitor_lpe *monitor_lpe, bool enable);
void sst_timer_cb(unsigned long data);
void sst_dump_to_buffer(const void *from, size_t from_len, char *buf);

extern int intel_scu_ipc_simple_command(int, int);

static inline int sst_pm_runtime_put(struct intel_sst_drv *sst_drv)
{
	int ret;

	pm_runtime_mark_last_busy(sst_drv->dev);
	ret = pm_runtime_put_autosuspend(sst_drv->dev);
	if (ret < 0)
		return ret;
	atomic_dec(&sst_drv->pm_usage_count);

	pr_debug("%s: count is %d now..\n", __func__,
			atomic_read(&sst_drv->pm_usage_count));
	return 0;
}
/*
 * sst_fill_header - inline to fill sst header
 *
 * @header : ipc header
 * @msg : IPC message to be sent
 * @large : is ipc large msg
 * @str_id : stream id
 *
 * this function is an inline function that sets the headers before
 * sending a message
 */
static inline void sst_fill_header(union ipc_header *header,
				int msg, int large, int str_id)
{
	header->part.msg_id = msg;
	header->part.str_id = str_id;
	header->part.large = large;
	header->part.done = 0;
	header->part.busy = 1;
	header->part.data = 0;
}


static inline void sst_fill_header_mrfld(union ipc_header_mrfld *header,
				int msg, int task_id, int large, int drv_id)
{
	header->full = 0;
	header->p.header_high.part.msg_id = msg;
	header->p.header_high.part.task_id = task_id;
	header->p.header_high.part.large = large;
	header->p.header_high.part.drv_id = drv_id;
	header->p.header_high.part.done = 0;
	header->p.header_high.part.busy = 1;
	header->p.header_high.part.res_rqd = 1;
}

static inline void sst_fill_header_dsp(struct ipc_dsp_hdr *dsp, int msg,
					int pipe_id, int len)
{
	dsp->cmd_id = msg;
	dsp->mod_index_id = 0xff;
	dsp->pipe_id = pipe_id;
	dsp->length = len;
	dsp->mod_id = 0;
}

#define MAX_BLOCKS 15
/* sst_assign_pvt_id - assign a pvt id for stream
 *
 * @sst_drv_ctx : driver context
 *
 * this inline function assigns a private id for calls that dont have stream
 * context yet, should be called with lock held
 */
static inline unsigned int sst_assign_pvt_id(struct intel_sst_drv *sst_drv_ctx)
{
	unsigned int local;

	spin_lock(&sst_drv_ctx->pvt_id_lock);
	sst_drv_ctx->pvt_id++;
	if (sst_drv_ctx->pvt_id > MAX_BLOCKS)
		sst_drv_ctx->pvt_id = 1;
	local = sst_drv_ctx->pvt_id;
	spin_unlock(&sst_drv_ctx->pvt_id_lock);
	return local;
}


/*
 * sst_init_stream - this function initialzes stream context
 *
 * @stream : stream struture
 * @codec : codec for stream
 * @sst_id : stream id
 * @ops : stream operation
 * @slot : stream pcm slot
 * @device : device type
 *
 * this inline function initialzes stream context for allocated stream
 */
static inline void sst_init_stream(struct stream_info *stream,
		int codec, int sst_id, int ops, u8 slot)
{
	stream->status = STREAM_INIT;
	stream->prev = STREAM_UN_INIT;
	stream->ops = ops;
}

static inline void sst_set_gpio_conf(const struct sst_gpio_config *gpio_conf)
{
#if 0
	lnw_gpio_set_alt(gpio_conf->i2s_rx_alt, gpio_conf->alt_function);
	lnw_gpio_set_alt(gpio_conf->i2s_tx_alt, gpio_conf->alt_function);
	lnw_gpio_set_alt(gpio_conf->i2s_frame, gpio_conf->alt_function);
	lnw_gpio_set_alt(gpio_conf->i2s_clock, gpio_conf->alt_function);
#endif
}


/*
 * sst_validate_strid - this function validates the stream id
 *
 * @str_id : stream id to be validated
 *
 * returns 0 if valid stream
 */
static inline int sst_validate_strid(int str_id)
{
	if (str_id <= 0 || str_id > sst_drv_ctx->info.max_streams) {
		pr_err("SST ERR: invalid stream id : %d, max %d\n",
					str_id, sst_drv_ctx->info.max_streams);
		return -EINVAL;
	} else
		return 0;
}

static inline int sst_shim_write(void __iomem *addr, int offset, int value)
{
	writel(value, addr + offset);
	return 0;
}

static inline u32 sst_shim_read(void __iomem *addr, int offset)
{

	return readl(addr + offset);
}

static inline u32 sst_reg_read(void __iomem *addr, int offset)
{

	return readl(addr + offset);
}

static inline u64 sst_reg_read64(void __iomem *addr, int offset)
{
	u64 val = 0;

	memcpy_fromio(&val, addr + offset, sizeof(val));

	return val;
}

static inline int sst_shim_write64(void __iomem *addr, int offset, u64 value)
{
	memcpy_toio(addr + offset, &value, sizeof(value));
	return 0;
}

static inline u64 sst_shim_read64(void __iomem *addr, int offset)
{
	u64 val = 0;

	memcpy_fromio(&val, addr + offset, sizeof(val));
	return val;
}

static inline void
sst_set_fw_state_locked(struct intel_sst_drv *sst_drv_ctx, int sst_state)
{
	mutex_lock(&sst_drv_ctx->sst_lock);
	sst_drv_ctx->sst_state = sst_state;
	mutex_unlock(&sst_drv_ctx->sst_lock);
}

static inline struct stream_info *get_stream_info(int str_id)
{
	if (sst_validate_strid(str_id))
		return NULL;
	return &sst_drv_ctx->streams[str_id];
}

static inline int get_stream_id_mrfld(u32 pipe_id)
{
	int i;

	for (i = 1; i <= sst_drv_ctx->info.max_streams; i++)
		if (pipe_id == sst_drv_ctx->streams[i].pipe_id)
			return i;

	pr_debug("%s: no such pipe_id(%u)", __func__, pipe_id);
	return -1;
}

int register_sst(struct device *);
int unregister_sst(struct device *);
int sst_platform_cb(struct sst_platform_cb_params *cb_params);
#ifdef CONFIG_DEBUG_FS
void sst_debugfs_init(struct intel_sst_drv *sst);
void sst_debugfs_exit(struct intel_sst_drv *sst);
#else
static inline void sst_debugfs_init(struct intel_sst_drv *sst)
{
}

static inline void sst_debugfs_exit(struct intel_sst_drv *sst)
{
}
#endif /* CONFIG_DEBUG_FS */

/*
 * FW should use virtual address 0xC000_0000 to map to the DDR
 * reserved 2MB region at 512MB boundary. Currently the address of
 * DDR region allocated by IA FW is not 512MB aligned. So FW is
 * statically linking the DDR region at 0xDF600000. So we need to
 * use the translated address to identify the DDR regions in the FW
 * ELF binary.
 */
static inline u32 relocate_imr_addr_mrfld(u32 base_addr)
{
	/* Get the difference from 512MB aligned base addr */
	/* relocate the base */
	base_addr = MRFLD_FW_VIRTUAL_BASE + (base_addr % (512 * 1024 * 1024));
	return base_addr;
}

static inline void sst_add_to_dispatch_list_and_post(struct intel_sst_drv *sst,
						struct ipc_post *msg)
{
	unsigned long irq_flags;
	spin_lock_irqsave(&sst->ipc_spin_lock, irq_flags);
	list_add_tail(&msg->node, &sst->ipc_dispatch_list);
	spin_unlock_irqrestore(&sst->ipc_spin_lock, irq_flags);
	sst->ops->post_message(&sst->ipc_post_msg_wq);
}
#endif
