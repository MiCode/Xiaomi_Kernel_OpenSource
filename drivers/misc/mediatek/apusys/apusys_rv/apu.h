/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef APU_H_
#define APU_H_
#include <linux/platform_device.h>
#include <linux/spinlock.h>

#include "apu_ipi.h"


struct mtk_apu {
	struct rproc *rproc;
	struct device *dev;
	void *md32_tcm;
	void *apu_sctrl_reviser;
	void *apu_wdt;
	void *apu_ao_ctl;
	void *md32_sysctrl;
	void *md32_debug_apb;
	void *apu_mbox;
	struct ion_client *ion_client;
	void *coredump_buf;
	dma_addr_t coredump_da;
	int wdt_irq_number;
	int mbox0_irq_number;
	spinlock_t reg_lock;

	/* Buffer to place execution area */
	void *code_buf;
	dma_addr_t code_da;

	/* Buffer to place config area */
	struct config_v1 *conf_buf;
	dma_addr_t conf_da;

	/* to synchronize boot status of remote processor */
	struct apu_run run;

	/* to prevent multiple ipi_send run concurrently */
	struct mutex send_lock;
	struct apu_ipi_desc ipi_desc[APU_IPI_MAX];
	bool ipi_id_ack[APU_IPI_MAX]; /* per-ipi ack */
	wait_queue_head_t ack_wq; /* for waiting for ipi ack */

	/* ipi share buffer */
	dma_addr_t recv_buf_da;
	struct mtk_share_obj *recv_buf;
	dma_addr_t send_buf_da;
	struct mtk_share_obj *send_buf;

	struct rproc_subdev *rpmsg_subdev;
};

#define TCM_SIZE            (128UL * 1024UL)
#define DRAM_SIZE           (1024UL * 1024UL)
/* first 128kB is only for bootstrap */
#define DRAM_DUMP_SIZE      (DRAM_SIZE - TCM_SIZE)
#define CONFIG_SIZE         (round_up(sizeof(struct config_v1), PAGE_SIZE))
#define REG_SIZE            (4UL * 151UL)
#define TBUF_SIZE           (4UL * 32UL)
#define CACHE_DUMP_SIZE     (37UL * 1024UL)
#define DRAM_OFFSET         (0x00000UL)
#define DRAM_DUMP_OFFSET    (TCM_SIZE)
#define TCM_OFFSET          (0x1d000000UL)
#define APU_IOMMU_BOUNDARY  (0x3UL)
#define DRAM_IOVA_ADDR      (DRAM_OFFSET | APU_IOMMU_BOUNDARY << 32)

struct apu_coredump {
	char tcmdump[TCM_SIZE];
	char ramdump[DRAM_DUMP_SIZE];
	char regdump[REG_SIZE];
	char tbufdump[TBUF_SIZE];
};
#define COREDUMP_SIZE       (round_up(sizeof(struct apu_coredump), PAGE_SIZE))

int apu_mem_init(struct mtk_apu *apu);
void apu_mem_remove(struct mtk_apu *apu);

int apu_config_setup(struct mtk_apu *apu);
void apu_config_remove(struct mtk_apu *apu);

void apu_ipi_remove(struct mtk_apu *apu);
int apu_ipi_init(struct platform_device *pdev, struct mtk_apu *apu);
int apu_ipi_register(struct mtk_apu *apu, u32 id,
		ipi_handler_t handler, void *priv);
int apu_ipi_send(struct mtk_apu *apu, u32 id, void *data, u32 len,
		 u32 wait_ms);

int apu_sysfs_init(struct platform_device *pdev);
void apu_sysfs_remove(struct platform_device *pdev);
void apu_setup_reviser(struct mtk_apu *apu, int boundary, int ns, int domain);
void apu_reset_mp(struct mtk_apu *apu);
void apu_setup_boot(struct mtk_apu *apu);
void apu_start_mp(struct mtk_apu *apu);
void apu_stop_mp(struct mtk_apu *apu);
int apu_coredump_init(struct mtk_apu *apu);
void apu_coredump_remove(struct mtk_apu *apu);
void apu_setup_dump(struct mtk_apu *apu, dma_addr_t da);

extern int reviser_set_init_info(struct mtk_apu *apu);
extern int vpu_set_init_info(struct mtk_apu *apu);
extern int power_set_chip_info(struct mtk_apu *apu);
#endif /* APU_H_ */
