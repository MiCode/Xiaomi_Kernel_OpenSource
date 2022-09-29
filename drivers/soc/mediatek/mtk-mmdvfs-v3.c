// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#include <linux/clk.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/remoteproc/mtk_ccu.h>
#include <linux/sched/clock.h>
#include <linux/slab.h>
#include <linux/suspend.h>
#include <soc/mediatek/mmdvfs_v3.h>
#include <soc/mediatek/smi.h>
#include <linux/rpmsg.h>
#include <linux/rpmsg/mtk_rpmsg.h>
#include <linux/iommu.h>

#include "clk-mtk.h"
#include "mtk-mmdvfs-v3.h"

#include "vcp.h"
#include "vcp_reg.h"
#include "vcp_status.h"

#include "mtk-mmdvfs-ftrace.h"

static u8 mmdvfs_clk_num;
static struct mtk_mmdvfs_clk *mtk_mmdvfs_clks;

static u8 mmdvfs_pwr_opp[PWR_MMDVFS_NUM];
static struct clk *mmdvfs_pwr_clk[PWR_MMDVFS_NUM];

#define MMDVFS_VCP_IPI_DATA_OFFSET 0xffc
static void *mmdvfs_vcp_ipi_data_base;
static u64 mmdvfs_vcp_ipi_data;
static DEFINE_MUTEX(mmdvfs_vcp_ipi_mutex);

#define MMDVFS_VB_ENABLE_OFFSET 0xff4
#define MMDVFS_AGING_ENABLE_OFFSET 0xff8
static void *mmdvfs_vb_enable_base;
static void *mmdvfs_aging_enable_base;

#define MMDVFS_VMM_CEIL_ENABLE_OFFSET 0xff0
static void *mmdvfs_vmm_ceil_enable_base;

static int log_level;
static bool mmdvfs_init_done;
static DEFINE_MUTEX(mmdvfs_vcp_pwr_mutex);
static int vcp_power;

static phys_addr_t mmdvfs_vcp_base;
static bool mmdvfs_free_run;

static struct device *cam_larb_dev;
static DEFINE_MUTEX(mmdvfs_vmm_pwr_mutex);
static int vmm_power;
static int last_vote_step[PWR_MMDVFS_NUM];
static int last_force_step[PWR_MMDVFS_NUM];

static struct platform_device *ccu_pdev;
static struct rproc *ccu_rproc;
static DEFINE_MUTEX(mmdvfs_ccu_pwr_mutex);
static int ccu_power;
struct ipi_callbacks clk_cb;
static DEFINE_MUTEX(mmdvfs_clkmux_mutex);
static unsigned int clk_en_cnt_expected;
static unsigned int clk_en_cnt;

static int last_ccu_freq = -1;
static unsigned int ccu_pwr_usage[CCU_PWR_USR_NUM];
static unsigned int vcp_pwr_usage[VCP_PWR_USR_NUM];

static struct notifier_block vcp_ready_notifier;

enum mmdvfs_log_level {
	log_ipi,
	log_ccf_cb,
	log_vcp,
	log_clk,
};

static inline struct mtk_mmdvfs_clk *to_mtk_mmdvfs_clk(struct clk_hw *hw)
{
	return container_of(hw, struct mtk_mmdvfs_clk, clk_hw);
}

int mtk_mmdvfs_enable_ccu(bool enable, unsigned int usr_id)
{

	int ret = 0;

	if (!mmdvfs_free_run) {
		MMDVFS_DBG("mmdvfs_v3 not supported");
		return 0;
	}

	if (!ccu_rproc) {
		MMDVFS_ERR("there is no ccu_rproc");
		return -EINVAL;
	}

	if (usr_id >= CCU_PWR_USR_NUM) {
		MMDVFS_ERR("usr_id:%u >= usr_num:%u", usr_id, CCU_PWR_USR_NUM);
		return -EINVAL;
	}

	mutex_lock(&mmdvfs_ccu_pwr_mutex);
	if (enable) {
		if (ccu_power == 0) {
#if IS_ENABLED(CONFIG_MTK_CCU_DEBUG)
			ret = rproc_bootx(ccu_rproc, RPROC_UID_MMDVFS);
#else
			ret = rproc_boot(ccu_rproc);
#endif
			if (ret) {
				MMDVFS_ERR("boot ccu rproc fail");
				mutex_unlock(&mmdvfs_ccu_pwr_mutex);
				return ret;
			}
		}
		ccu_power++;
		ccu_pwr_usage[usr_id]++;
	} else {
		if (ccu_power == 0) {
			MMDVFS_ERR("disable vcp when vcp_power==0");
			mutex_unlock(&mmdvfs_ccu_pwr_mutex);
			return -EINVAL;
		}
		if (ccu_power == 1) {
#if IS_ENABLED(CONFIG_MTK_CCU_DEBUG)
			rproc_shutdownx(ccu_rproc, RPROC_UID_MMDVFS);
#else
			rproc_shutdown(ccu_rproc);
#endif
		}
		ccu_power--;
		ccu_pwr_usage[usr_id]--;
	}

	mutex_unlock(&mmdvfs_ccu_pwr_mutex);

	return ret;
}
EXPORT_SYMBOL_GPL(mtk_mmdvfs_enable_ccu);

static void reset_ccu_power(void)
{
	mutex_lock(&mmdvfs_ccu_pwr_mutex);
	if (ccu_power > 0) {
#if IS_ENABLED(CONFIG_MTK_CCU_DEBUG)
		rproc_shutdownx(ccu_rproc, RPROC_UID_MMDVFS);
#else
		rproc_shutdown(ccu_rproc);
#endif
		ccu_power = 0;
		MMDVFS_DBG("check ccu_power > 0 and shutdown ccu");
	}

	mutex_unlock(&mmdvfs_ccu_pwr_mutex);
}

static void check_ccu_pwr(void)
{
	int i;
	bool has_error = false;

	for (i = 0; i < CCU_PWR_USR_NUM; i++) {
		if (ccu_pwr_usage[i] > 0) {
			MMDVFS_ERR("usr:%u is not disabled usage:%u", i, ccu_pwr_usage[i]);
			has_error = true;
		}
	}
	WARN_ON(has_error);
}


static int mtk_mmdvfs_enable_vmm(int enable)
{
	int result = 0, ret = 0;

	if (!cam_larb_dev) {
		MMDVFS_ERR("cam_larb_dev is null");
		return -EINVAL;
	}

	mutex_lock(&mmdvfs_vmm_pwr_mutex);
	if (enable) {
		if (vmm_power == 0) {
			ret = mtk_smi_larb_get(cam_larb_dev);
			if (ret)
				MMDVFS_ERR("mtk_smi_larb_get failed:%d", ret);
			else
				MMDVFS_DBG("power on vmm successfully");
		}
		vmm_power++;
	} else {
		if (vmm_power == 0) {
			MMDVFS_ERR("power off vmm when vmm_power==0");
			mutex_unlock(&mmdvfs_vmm_pwr_mutex);
			return -1;
		}
		if (vmm_power == 1) {
			mtk_smi_larb_put(cam_larb_dev);
			MMDVFS_DBG("power off vmm successfully");
		}
		vmm_power--;
	}
	mutex_unlock(&mmdvfs_vmm_pwr_mutex);

	return result;
}

static int set_vmm_enable(const char *val, const struct kernel_param *kp)
{
	int result, enable;

	result = kstrtoint(val, 0, &enable);
	if (result) {
		MMDVFS_ERR("failed: %d", result);
		return result;
	}

	result = mtk_mmdvfs_enable_vmm(enable);


	return result;
}

static struct kernel_param_ops enable_vmm_ops = {
	.set = set_vmm_enable,
};
module_param_cb(enable_vmm, &enable_vmm_ops, NULL, 0644);
MODULE_PARM_DESC(enable_vmm, "enable vmm");

bool mtk_is_mmdvfs_init_done(void)
{
	if (!mmdvfs_free_run) {
		MMDVFS_DBG("mmdvfs_v3 not supported");
		return false;
	}

	return mmdvfs_init_done;
}
EXPORT_SYMBOL_GPL(mtk_is_mmdvfs_init_done);

/*
static int reset_vcp_power(void)
{
	int ret = 0;

	mutex_lock(&mmdvfs_vcp_pwr_mutex);
	if (vcp_power > 0) {
		ret = vcp_deregister_feature_ex(MMDVFS_FEATURE_ID);
		if (ret) {
			MMDVFS_ERR("vcp_deregister_feature failed:%d", ret);
			mutex_unlock(&mmdvfs_vcp_pwr_mutex);
			return ret;
		}
		vcp_power = 0;
		MMDVFS_DBG("check vcp_power > 0 and power off vcp");
	}
	mutex_unlock(&mmdvfs_vcp_pwr_mutex);

	return ret;
}
*/

static void check_vcp_pwr(void)
{
	int i;
	bool has_error = false;

	for (i = 0; i < VCP_PWR_USR_NUM; i++) {
		if (vcp_pwr_usage[i] > 0) {
			MMDVFS_ERR("usr:%u is not disabled usage:%u", i, vcp_pwr_usage[i]);
			has_error = true;
		}
	}
	// Todo: mark it until vcp-enable API removed from mmqos
	//WARN_ON(has_error);
}

int mtk_mmdvfs_enable_vcp(bool enable, unsigned int usr_id)
{
	int ret;

	if (!mmdvfs_free_run) {
		MMDVFS_DBG("mmdvfs_v3 not supported");
		return 0;
	}

	if (is_vcp_suspending_ex())
		return -EBUSY;

	mutex_lock(&mmdvfs_vcp_pwr_mutex);
	if (enable) {
		if (vcp_power == 0) {
			ret = vcp_register_feature_ex(MMDVFS_FEATURE_ID);
			if (ret) {
				MMDVFS_ERR("vcp_register_feature failed:%d", ret);
				mutex_unlock(&mmdvfs_vcp_pwr_mutex);
				return ret;
			}
		}
		vcp_power++;
		vcp_pwr_usage[usr_id]++;
	} else {
		if (vcp_power == 0) {
			MMDVFS_ERR("disable vcp when vcp_power==0");
			mutex_unlock(&mmdvfs_vcp_pwr_mutex);
			return -EINVAL;
		}
		if (vcp_power == 1) {
			ret = vcp_deregister_feature_ex(MMDVFS_FEATURE_ID);
			if (ret) {
				MMDVFS_ERR("vcp_deregister_feature failed:%d", ret);
				mutex_unlock(&mmdvfs_vcp_pwr_mutex);
				return ret;
			}
		}
		vcp_power--;
		vcp_pwr_usage[usr_id]--;
	}
	if (log_level & 1 << log_vcp)
		MMDVFS_DBG("enable:%d vcp_power:%d usr_id:%u usage:%u",
			enable, vcp_power, usr_id, vcp_pwr_usage[usr_id]);
	mutex_unlock(&mmdvfs_vcp_pwr_mutex);

	return 0;
}
EXPORT_SYMBOL_GPL(mtk_mmdvfs_enable_vcp);

void *mtk_mmdvfs_vcp_get_base(phys_addr_t *pa)
{
	struct mtk_ipi_device *vcp_ipi_dev = vcp_get_ipidev();
	phys_addr_t va = vcp_get_reserve_mem_virt_ex(MMDVFS_MEM_ID);
	struct iommu_domain *domain;

	if (!mmdvfs_free_run || !va) {
		MMDVFS_DBG("mmdvfs_v3 not supported");
		return NULL;
	}

	domain = iommu_get_domain_for_dev(&vcp_ipi_dev->mrpdev->pdev->dev);
	if (domain) {
		*pa = iommu_iova_to_phys(
			domain, vcp_get_reserve_mem_phys_ex(MMDVFS_MEM_ID));
	} else {
		MMDVFS_ERR("domain is null");
		return NULL;
	}

	return (void *)va;
}
EXPORT_SYMBOL_GPL(mtk_mmdvfs_vcp_get_base);

static inline bool mmdvfs_vcp_ipi_base_get(void)
{
	phys_addr_t pa = 0ULL;
	static bool flags;

	if (flags)
		return mmdvfs_vcp_ipi_data_base ? true : false;

	if (!mmdvfs_vcp_base) // iova
		mmdvfs_vcp_base = vcp_get_reserve_mem_phys_ex(MMDVFS_MEM_ID);

	if (!mmdvfs_vcp_ipi_data_base) // va
		mmdvfs_vcp_ipi_data_base = mtk_mmdvfs_vcp_get_base(&pa);

	MMDVFS_DBG("pa:%pa iova:%pa va:%#llx offset:%#x",
		&pa, &mmdvfs_vcp_base, mmdvfs_vcp_ipi_data_base,
		MMDVFS_VCP_IPI_DATA_OFFSET);

	if (!mmdvfs_aging_enable_base && mmdvfs_vcp_ipi_data_base)
		mmdvfs_aging_enable_base = mmdvfs_vcp_ipi_data_base + MMDVFS_AGING_ENABLE_OFFSET;

	if (!mmdvfs_vb_enable_base && mmdvfs_vcp_ipi_data_base)
		mmdvfs_vb_enable_base = mmdvfs_vcp_ipi_data_base + MMDVFS_VB_ENABLE_OFFSET;

	if (!mmdvfs_vmm_ceil_enable_base)
		mmdvfs_vmm_ceil_enable_base =
			mmdvfs_vcp_ipi_data_base + MMDVFS_VMM_CEIL_ENABLE_OFFSET;

	if (mmdvfs_vcp_ipi_data_base)
		mmdvfs_vcp_ipi_data_base += MMDVFS_VCP_IPI_DATA_OFFSET;

	flags = true;
	return mmdvfs_vcp_ipi_data_base ? true : false;
}

static inline u64 mmdvfs_update_vcp_ipi_data(const u64 slot)
{
	u32 val;

	if (!mmdvfs_vcp_ipi_base_get())
		return slot;

	val = readl(mmdvfs_vcp_ipi_data_base);
	mmdvfs_vcp_ipi_data = (mmdvfs_vcp_ipi_data & GENMASK(63, 32)) | (u32)val;

	if (log_level & (1 << log_ipi))
		MMDVFS_DBG("base:%#llx val:%#x data:%#llx",
			mmdvfs_vcp_ipi_data_base, val, mmdvfs_vcp_ipi_data);

	return mmdvfs_vcp_ipi_data;
}

static int mmdvfs_vcp_ipi_send(const u8 func, const u8 idx, const u8 opp,
	const u8 ack) // ap > vcp
{
	struct mmdvfs_ipi_data slot = {func, idx, opp, ack, 0U};
	const u64 data = *(u64 *)(unsigned long *)&slot;
	int retry = 0, ret, gen;

	if (!mtk_is_mmdvfs_init_done()) {
		MMDVFS_DBG("mmdvfs_v3 init not ready");
		return -ENODEV;
	}

	if (is_vcp_suspending_ex())
		return -EBUSY;

	while (!is_vcp_ready_ex(VCP_A_ID)) {
		if (++retry > 100) {
			MMDVFS_ERR("VCP_A_ID:%d not ready", VCP_A_ID);
			return -ETIMEDOUT;
		}
		usleep_range(1000, 2000);
	}

	if (!mmdvfs_vcp_ipi_base_get()) {
		MMDVFS_DBG("get failed va:%#llx", mmdvfs_vcp_ipi_data_base);
		return -ENOMEM;
	}

	if (mmdvfs_vcp_base && ack == MAX_OPP) {
		slot.ack = mmdvfs_vcp_base >> 32;
		slot.base = (u32)mmdvfs_vcp_base;
	}

	mutex_lock(&mmdvfs_vcp_ipi_mutex);
	mmdvfs_vcp_ipi_data = *(u64 *)(unsigned long *)&slot;
	writel(data, mmdvfs_vcp_ipi_data_base);

	gen = vcp_cmd_ex(VCP_GET_GEN);
	ret = mtk_ipi_send(vcp_get_ipidev(), IPI_OUT_MMDVFS, IPI_SEND_WAIT,
		&slot, PIN_OUT_SIZE_MMDVFS, IPI_TIMEOUT_MS);

	if (ret != IPI_ACTION_DONE) {
		MMDVFS_ERR("mtk_ipi_send failed:%d slot:%#llx data:%#llx",
			ret, *(u64 *)(unsigned long *)&slot, mmdvfs_vcp_ipi_data);
		mutex_unlock(&mmdvfs_vcp_ipi_mutex);
		return ret;
	}

	retry = 0;
	while ((mmdvfs_update_vcp_ipi_data(data) & 0xff) == func) {
		if (++retry > 10000) {
			ret = IPI_COMPL_TIMEOUT;
			MMDVFS_ERR("ipi ack timeout:%d slot:%#llx data:%#llx",
				ret, *(u64 *)(unsigned long *)&slot, mmdvfs_vcp_ipi_data);
			if (gen == vcp_cmd_ex(VCP_GET_GEN))
				vcp_cmd_ex(VCP_SET_HALT);
			break;
		}
		udelay(10);
	}

	mutex_unlock(&mmdvfs_vcp_ipi_mutex);
	return ret;
}

static int mtk_mmdvfs_set_rate(struct clk_hw *hw, unsigned long rate,
		unsigned long parent_rate)
{
	struct mtk_mmdvfs_clk *mmdvfs_clk = to_mtk_mmdvfs_clk(hw);
	u8 i, opp, level, pwr_opp = MAX_OPP, user_opp = MAX_OPP;
	struct mmdvfs_ipi_data slot;
	int ret = 0;
	unsigned int img_clk = rate / 1000000UL;

	for (i = 0; i < mmdvfs_clk->freq_num; i++)
		if (rate <= mmdvfs_clk->freqs[i])
			break;

	level = (i == mmdvfs_clk->freq_num) ? (i-1) : i;
	opp = (mmdvfs_clk->freq_num - level - 1);
	ftrace_user_opp_v3_vmm(mmdvfs_clk->user_id, opp);
	if (log_level & 1 << log_ccf_cb)
		MMDVFS_DBG("clk=%u user=%u freq=%lu old_opp=%d new_opp=%d",
			mmdvfs_clk->clk_id, mmdvfs_clk->user_id, rate, mmdvfs_clk->opp, opp);
	if (mmdvfs_clk->opp == opp)
		return 0;
	mmdvfs_clk->opp = opp;

	for (i = 0; i < mmdvfs_clk_num; i++) {
		if (mmdvfs_clk->pwr_id == mtk_mmdvfs_clks[i].pwr_id
			&& mtk_mmdvfs_clks[i].opp < pwr_opp)
			pwr_opp = mtk_mmdvfs_clks[i].opp;
	}

	if (!mmdvfs_free_run) {
		return 0;
	}

	if (!mtk_is_mmdvfs_init_done()) {
		MMDVFS_DBG("mmdvfs_v3 init not ready");
		return 0;
	}

	/* Choose max step among all users of special independence */
	if (mmdvfs_clk->spec_type == SPEC_MMDVFS_ALONE) {
		for (i = 0; i < mmdvfs_clk_num; i++) {
			if (mmdvfs_clk->user_id == mtk_mmdvfs_clks[i].user_id
				&& mtk_mmdvfs_clks[i].opp < user_opp)
				user_opp = mtk_mmdvfs_clks[i].opp;
		}
		if (mmdvfs_free_run)
			ret = mmdvfs_vcp_ipi_send(FUNC_SET_OPP,
				mmdvfs_clk->user_id, user_opp, MAX_OPP);
	} else {
		if (pwr_opp == mmdvfs_pwr_opp[mmdvfs_clk->pwr_id])
			return 0;
		if (mmdvfs_free_run) {
			if (mmdvfs_clk->ipi_type == IPI_MMDVFS_CCU) {
				if (ccu_pdev) {
					ret = mtk_ccu_rproc_ipc_send(
						ccu_pdev,
						MTK_CCU_FEATURE_ISPDVFS,
						4, /*DVFS_IMG_CLK*/
						(void *)&img_clk, sizeof(unsigned int));
					if (ret)
						MMDVFS_ERR("mtk_ccu_rproc_ipc_send fail(%d)", ret);
				}
			} else {
				ret = mmdvfs_vcp_ipi_send(FUNC_SET_OPP,
					mmdvfs_clk->user_id, pwr_opp, MAX_OPP);
			}
		}
	}

	mmdvfs_pwr_opp[mmdvfs_clk->pwr_id] = pwr_opp;

	slot = *(struct mmdvfs_ipi_data *)(u32 *)&mmdvfs_vcp_ipi_data;
	if (log_level & 1 << log_ipi)
		MMDVFS_DBG("ret:%d slot:%#x idx:%hhu opp:%hhu", ret, slot, slot.idx, slot.ack);
	return 0;
}

static long mtk_mmdvfs_round_rate(struct clk_hw *hw, unsigned long rate,
			       unsigned long *parent_rate)
{
	struct mtk_mmdvfs_clk *mmdvfs_clk = to_mtk_mmdvfs_clk(hw);

	if (log_level & 1 << log_ccf_cb)
		MMDVFS_DBG("clk=%u rate=%lu parent_rate=%lu\n",
			mmdvfs_clk->clk_id, rate, *parent_rate);

	return rate;
}

static unsigned long mtk_mmdvfs_recalc_rate(struct clk_hw *hw,
					 unsigned long parent_rate)
{
	struct mtk_mmdvfs_clk *mmdvfs_clk = to_mtk_mmdvfs_clk(hw);
	unsigned long ret;
	u8 level;

	if (mmdvfs_clk->opp == MAX_OPP)
		return 0;

	level = mmdvfs_clk->freq_num - mmdvfs_clk->opp - 1;
	ret = mmdvfs_clk->freqs[level];
	if (log_level & 1 << log_ccf_cb)
		MMDVFS_DBG("clk=%u parent_rate=%lu freq=%lu",
			mmdvfs_clk->clk_id, parent_rate, ret);
	return ret;
}

static const struct clk_ops mtk_mmdvfs_req_ops = {
	.set_rate = mtk_mmdvfs_set_rate,
	.round_rate	= mtk_mmdvfs_round_rate,
	.recalc_rate	= mtk_mmdvfs_recalc_rate,
};

static int mmdvfs_set_vcp_init(void)
{
	int i, ret;
	struct mmdvfs_ipi_data slot;

	ret = mmdvfs_vcp_ipi_send(FUNT_VMRC_SET_VCP_INIT, MAX_OPP, MAX_OPP, MAX_OPP);

	slot = *(struct mmdvfs_ipi_data *)(u32 *)&mmdvfs_vcp_ipi_data;
	MMDVFS_DBG("ret:%d slot:%#x ack:%hhu", ret, slot, slot.ack);

	mutex_lock(&mmdvfs_clkmux_mutex);
	if (clk_en_cnt == clk_en_cnt_expected) {
		mutex_unlock(&mmdvfs_clkmux_mutex);
		return 0;
	}

	for (i = 0; i < 32; i++) {
		if ((clk_en_cnt & (1 << i)) == (clk_en_cnt_expected & (1 << i)))
			continue;

		MMDVFS_DBG("i:%d expected:%d", i, (clk_en_cnt_expected & (1 << i)) ? 1 : 0);
		ret = mmdvfs_vcp_ipi_send(FUNC_SET_CLK,
			i, (clk_en_cnt_expected & (1 << i)) ? 1 : 0, MAX_OPP);
	}
	clk_en_cnt = clk_en_cnt_expected;
	mutex_unlock(&mmdvfs_clkmux_mutex);

	return 0;
}

int mtk_mmdvfs_camera_notify(const bool genpd_update, const bool enable)
{
	if (!mtk_is_mmdvfs_init_done()) {
		MMDVFS_DBG("mmdvfs_v3 init not ready");
		return 0;
	}

	MMDVFS_DBG("genpd_update:%u enable:%u", genpd_update, enable);
	if (genpd_update)
		mtk_mmdvfs_enable_vcp(enable, VCP_PWR_USR_MMDVFS_CAM_NOTIFY);

	return 0;
}
EXPORT_SYMBOL_GPL(mtk_mmdvfs_camera_notify);

int mtk_mmdvfs_camera_notify_from_mmqos(const bool enable)
{

	if (!mmdvfs_vmm_ceil_enable_base) {
		MMDVFS_ERR("mmdvfs_vmm_ceil_enable_base is NULL");
		return -EINVAL;
	}

	writel(enable ? 1 : 0, mmdvfs_vmm_ceil_enable_base);
	if (is_vcp_ready_ex(VCP_A_ID))
		mmdvfs_set_vcp_init();

	MMDVFS_DBG("enable:%u", enable);

	return 0;
}
EXPORT_SYMBOL_GPL(mtk_mmdvfs_camera_notify_from_mmqos);

int mtk_mmdvfs_v3_set_force_step(u16 pwr_idx, s16 opp)
{
	struct mmdvfs_ipi_data slot;
	int *last, ret;

	if (!mtk_is_mmdvfs_init_done()) {
		MMDVFS_DBG("mmdvfs_v3 init not ready");
		return 0;
	}

	if (pwr_idx >= PWR_MMDVFS_NUM || opp >= MAX_OPP) {
		MMDVFS_ERR("wrong pwr_idx:%hu opp:%hd", pwr_idx, opp);
		return -EINVAL;
	}

	last = &last_force_step[pwr_idx];
	if (*last == opp)
		return 0;

	if (*last == -1) {
		mtk_mmdvfs_enable_vcp(true, VCP_PWR_USR_MMDVFS_FORCE_STEP);

		if (pwr_idx == PWR_MMDVFS_VMM)
			mtk_mmdvfs_enable_vmm(true);
	}

	ret = mmdvfs_vcp_ipi_send(FUNC_FORCE_OPP, pwr_idx, opp == -1 ? MAX_OPP : opp, MAX_OPP);
	slot = *(struct mmdvfs_ipi_data *)(u32 *)&mmdvfs_vcp_ipi_data;
	MMDVFS_DBG("ret:%d slot:%#x idx:%hhu opp:%hhu", ret, slot, slot.idx, slot.ack);

	if (opp == -1) {
		if (pwr_idx == PWR_MMDVFS_VMM)
			mtk_mmdvfs_enable_vmm(false);

		mtk_mmdvfs_enable_vcp(false, VCP_PWR_USR_MMDVFS_FORCE_STEP);
	}
	*last = opp;

	return ret;
}
EXPORT_SYMBOL_GPL(mtk_mmdvfs_v3_set_force_step);

static int mmdvfs_set_force_step(const char *val, const struct kernel_param *kp)
{
	int ret;
	u16 idx = 0;
	s16 opp = 0;

	ret = sscanf(val, "%hu %hd", &idx, &opp);
	if (ret != 2 || idx >= PWR_MMDVFS_NUM || opp >= MAX_OPP) {
		MMDVFS_ERR("input failed:%d idx:%hu opp:%hd", ret, idx, opp);
		return -EINVAL;
	}

	ret = mtk_mmdvfs_v3_set_force_step(idx, opp);
	if (ret)
		MMDVFS_ERR("mtk_mmdvfs_v3_set_force_step failed:%d idx:%hu opp:%hd",
			ret, idx, opp);

	return ret;
}

static struct kernel_param_ops mmdvfs_force_step_ops = {
	.set = mmdvfs_set_force_step,
};
module_param_cb(force_step, &mmdvfs_force_step_ops, NULL, 0644);
MODULE_PARM_DESC(force_step, "force mmdvfs to specified step");

int mtk_mmdvfs_v3_set_vote_step(u16 pwr_idx, s16 opp)
{
	u32 freq = 0;
	int i, *last, ret = 0;

	if (!mtk_is_mmdvfs_init_done()) {
		MMDVFS_DBG("mmdvfs_v3 init not ready");
		return 0;
	}

	if (pwr_idx >= PWR_MMDVFS_NUM || opp >= MAX_OPP) {
		MMDVFS_ERR("failed:%d pwr_idx:%hu opp:%hd", ret, pwr_idx, opp);
		return -EINVAL;
	}

	last = &last_vote_step[pwr_idx];
	if (*last == opp)
		return 0;

	if (*last == -1) {
		mtk_mmdvfs_enable_vcp(true, VCP_PWR_USR_MMDVFS_VOTE_STEP);

		if (pwr_idx == PWR_MMDVFS_VMM)
			mtk_mmdvfs_enable_vmm(true);
	}

	for (i = mmdvfs_clk_num - 1; i >= 0; i--)
		if (pwr_idx == mtk_mmdvfs_clks[i].pwr_id) {
			if (opp >= mtk_mmdvfs_clks[i].freq_num) {
				MMDVFS_ERR("i:%d invalid opp:%hd freq_num:%hhu",
					i, opp, mtk_mmdvfs_clks[i].freq_num);
				break;
			}

			freq = (opp == -1) ? 0 : mtk_mmdvfs_clks[i].freqs[
				mtk_mmdvfs_clks[i].freq_num - 1 - opp];
			ret = clk_set_rate(mmdvfs_pwr_clk[pwr_idx], freq);
			if (ret)
				MMDVFS_ERR("clk_set_rate failed:%d pwr_idx:%hu freq:%hu",
					ret, pwr_idx, freq);
			break;
		}

	MMDVFS_DBG("pwr_idx:%hu clk:%p opp:%hd i:%d freq_num:%hhu freq:%u", pwr_idx,
		mmdvfs_pwr_clk[pwr_idx], opp, i, mtk_mmdvfs_clks[i].freq_num, freq);

	if (opp == -1) {
		if (pwr_idx == PWR_MMDVFS_VMM)
			mtk_mmdvfs_enable_vmm(false);

		mtk_mmdvfs_enable_vcp(false, VCP_PWR_USR_MMDVFS_VOTE_STEP);
	}
	*last = opp;

	return ret;
}
EXPORT_SYMBOL_GPL(mtk_mmdvfs_v3_set_vote_step);

static int mmdvfs_set_vote_step(const char *val, const struct kernel_param *kp)
{
	int ret;
	u16 idx = 0;
	s16 opp = 0;

	ret = sscanf(val, "%hu %hd", &idx, &opp);
	if (ret != 2 || idx >= PWR_MMDVFS_NUM || opp >= MAX_OPP) {
		MMDVFS_ERR("failed:%d idx:%hu opp:%hd", ret, idx, opp);
		return -EINVAL;
	}

	ret = mtk_mmdvfs_v3_set_vote_step(idx, opp);
	if (ret)
		MMDVFS_ERR("mtk_mmdvfs_v3_set_vote_step failed:%d idx:%hu opp:%hd",
			ret, idx, opp);

	return ret;
}

static struct kernel_param_ops mmdvfs_vote_step_ops = {
	.set = mmdvfs_set_vote_step,
};
module_param_cb(vote_step, &mmdvfs_vote_step_ops, NULL, 0644);
MODULE_PARM_DESC(vote_step, "vote mmdvfs to specified step");

static int mmdvfs_vcp_notifier_callback(struct notifier_block *nb, unsigned long action, void *data)
{

	switch (action) {
	case VCP_EVENT_READY:
		mmdvfs_set_vcp_init();
		break;
	}
	return NOTIFY_DONE;
}

static int mmdvfs_set_vmm_init(const char *val, const struct kernel_param *kp)
{
	int ret;
	unsigned int bin, aging;

	ret = sscanf(val, "%u %u", &bin, &aging);
	if (ret != 2) {
		MMDVFS_ERR("input failed:%d val:%s", ret, val);
		return -EINVAL;
	}

	if (!mmdvfs_vb_enable_base) {
		MMDVFS_ERR("mmdvfs_vb_enable_base is NULL");
		return ret;
	}

	if (!mmdvfs_aging_enable_base) {
		MMDVFS_ERR("mmdvfs_aging_enable_base is NULL");
		return ret;
	}

	writel(bin, mmdvfs_vb_enable_base);
	writel(aging, mmdvfs_aging_enable_base);
	mtk_mmdvfs_v3_set_vote_step(PWR_MMDVFS_VMM, 0);
	mtk_mmdvfs_v3_set_vote_step(PWR_MMDVFS_VMM, -1);

	MMDVFS_DBG("ret:%d bin:%u aging:%u", ret, bin, aging);

	return ret;
}

static struct kernel_param_ops mmdvfs_vmm_init_ops = {
	.set = mmdvfs_set_vmm_init,
};
module_param_cb(vmm_init, &mmdvfs_vmm_init_ops, NULL, 0644);
MODULE_PARM_DESC(vmm_init, "set vmm initialization");

static int lpm_spm_suspend_pm_event(struct notifier_block *notifier,
			unsigned long pm_event, void *unused)
{
	int i;

	switch (pm_event) {
	case PM_HIBERNATION_PREPARE:
		return NOTIFY_DONE;
	case PM_RESTORE_PREPARE:
		return NOTIFY_DONE;
	case PM_POST_HIBERNATION:
		return NOTIFY_DONE;
	case PM_SUSPEND_PREPARE:
		MMDVFS_DBG("start suspend");
		for (i = 0; i < PWR_MMDVFS_NUM; i++) {
			if (last_vote_step[i] != -1)
				mtk_mmdvfs_v3_set_vote_step(i, -1);
			if (last_force_step[i] != -1)
				mtk_mmdvfs_v3_set_force_step(i, -1);
		}
		check_ccu_pwr();
		check_vcp_pwr();
		//enable_aoc_iso(true);
		reset_ccu_power();
		//reset_vcp_power();
		return NOTIFY_DONE;
	case PM_POST_SUSPEND:
		return NOTIFY_DONE;
	}
	return NOTIFY_OK;
}

static struct notifier_block lpm_spm_suspend_pm_notifier_func = {
	.notifier_call = lpm_spm_suspend_pm_event,
	.priority = 0,
};

int mmdvfs_set_ccu_ipi(const char *val, const struct kernel_param *kp)
{
	int freq = 0;
	int ret, retry = 0;

	ret = kstrtou32(val, 0, &freq);
	if (ret) {
		MMDVFS_ERR("failed:%d freq:%d", ret, freq);
		return ret;
	}

	if (ccu_pdev) {
		if (last_ccu_freq < 0 && freq >= 0) {
			mtk_mmdvfs_enable_vcp(true, VCP_PWR_USR_MMDVFS_CCU_TEST);
			mtk_mmdvfs_enable_ccu(true, CCU_PWR_USR_MMDVFS_CCU_TEST);
			while (!is_vcp_ready_ex(VCP_A_ID)) {
				if (++retry > 100) {
					MMDVFS_ERR("VCP_A_ID:%d not ready", VCP_A_ID);
					return -ETIMEDOUT;
				}
				usleep_range(1000, 2000);
			}
		}

		last_ccu_freq = freq;

		if (freq < 0) {
			mtk_mmdvfs_enable_ccu(false, CCU_PWR_USR_MMDVFS_CCU_TEST);
			mtk_mmdvfs_enable_vcp(false, VCP_PWR_USR_MMDVFS_CCU_TEST);
			return 0;
		}
		ret = mtk_ccu_rproc_ipc_send(
			ccu_pdev,
			MTK_CCU_FEATURE_ISPDVFS,
			4, /*DVFS_IMG_CLK*/
			(void *)&freq, sizeof(unsigned int));
		if (ret)
			MMDVFS_ERR("mtk_ccu_rproc_ipc_send fail(%d)", ret);
		else
			MMDVFS_DBG("mtk_ccu_rproc_ipc_send freq:%u done", freq);
	} else {
		MMDVFS_DBG("ccu_pdev is not ready");
	}

	return 0;
}

static struct kernel_param_ops mmdvfs_ccu_ipi_ops = {
	.set = mmdvfs_set_ccu_ipi,
};
module_param_cb(ccu_ipi_test, &mmdvfs_ccu_ipi_ops, NULL, 0644);
MODULE_PARM_DESC(ccu_ipi_test, "trigger ccu ipi test");

int mmdvfs_dump_setting(char *buf, const struct kernel_param *kp)
{
	struct mmdvfs_ipi_data slot;
	int i, len = 0, ret;

	if (!mtk_is_mmdvfs_init_done()) {
		MMDVFS_DBG("mmdvfs_v3 init not ready");
		return 0;
	}

	len += snprintf(buf + len, PAGE_SIZE - len, "user request opp");
	mtk_mmdvfs_enable_vcp(true, VCP_PWR_USR_MMDVFS_DUMP_SETTING);

	for (i = 0; i < USER_NUM; i += 3) {
		ret = mmdvfs_vcp_ipi_send(FUNC_GET_OPP, i, i + 1, i + 2);

		slot = *(struct mmdvfs_ipi_data *)(u32 *)&mmdvfs_vcp_ipi_data;
		MMDVFS_DBG("slot:%#x i:%d opp:%hhu %hhu %hhu",
			slot, i, slot.idx, slot.opp, slot.ack);

		if (i + 1 >= USER_NUM) {
			len += snprintf(buf + len, PAGE_SIZE - len,
				", %hhu", slot.idx);
			break;
		}

		if (i + 2 >= USER_NUM) {
			len += snprintf(buf + len, PAGE_SIZE - len,
				", %hhu, %hhu", slot.idx, slot.opp);
			break;
		}

		len += snprintf(buf + len, PAGE_SIZE - len,
			", %hhu, %hhu, %hhu", slot.idx, slot.opp, slot.ack);
	}

	mtk_mmdvfs_enable_vcp(false, VCP_PWR_USR_MMDVFS_DUMP_SETTING);
	len += snprintf(buf + len, PAGE_SIZE - len, "\n");

	if (len >= PAGE_SIZE) {
		len = PAGE_SIZE - 1;
		buf[len] = '\n';
	}

	return len;
}

static struct kernel_param_ops mmdvfs_dump_setting_ops = {
	.get = mmdvfs_dump_setting,
};
module_param_cb(dump_setting, &mmdvfs_dump_setting_ops, NULL, 0444);
MODULE_PARM_DESC(dump_setting, "dump mmdvfs current setting");

int mmdvfs_set_vcp_stress(const char *val, const struct kernel_param *kp)
{
	u16 test = 0, para = 0;
	int ret;

	if (!mtk_is_mmdvfs_init_done()) {
		MMDVFS_DBG("mmdvfs_v3 init not ready");
		return 0;
	}

	ret = sscanf(val, "%hu %hu", &test, &para);
	if (ret != 2) {
		MMDVFS_ERR("input failed:%d test:%hu para:%hu", ret, test, para);
		return -EINVAL;
	}

	switch (test) {
	case 0:
		if (para)
			mtk_mmdvfs_enable_vcp(true, VCP_PWR_USR_MMDVFS_VCP_STRESS);
		ret = mmdvfs_vcp_ipi_send(FUNC_STRESS, para, MAX_OPP, MAX_OPP);
		if (!para)
			mtk_mmdvfs_enable_vcp(false, VCP_PWR_USR_MMDVFS_VCP_STRESS);
		break;
	case 1:
		ret = vcp_register_feature_ex(MMDVFS_FEATURE_ID);
		if (ret)
			MMDVFS_DBG("vcp register feature:%d", GCE_FEATURE_ID);
		while (!ret)
			ret = is_vcp_ready_ex(VCP_A_ID);
		ret = vcp_deregister_feature_ex(MMDVFS_FEATURE_ID);
		if (ret)
			MMDVFS_DBG("vcp deregister feature:%d", GCE_FEATURE_ID);
		break;
	}

	return 0;
}

static struct kernel_param_ops mmdvfs_vcp_stress_ops = {
	.set = mmdvfs_set_vcp_stress,
};
module_param_cb(vcp_stress, &mmdvfs_vcp_stress_ops, NULL, 0644);
MODULE_PARM_DESC(vcp_stress, "trigger mmdvfs vcp stress");

static int mtk_mmdvfs_clk_ctrl(const u8 clk_idx, const bool enable)
{
	int ret = 0;

	if (!mmdvfs_free_run) {
		//MMDVFS_DBG("mmdvfs_v3 not supported");
		return 0;
	}

	if (!mtk_is_mmdvfs_init_done()) {
		if (log_level & (1 << log_clk))
			MMDVFS_DBG("mmdvfs_v3 init not ready idx:%hhu ena:%d",
				clk_idx, enable);
		return ret;
	}

	if (is_vcp_suspending_ex())
		return 0;

	mutex_lock(&mmdvfs_clkmux_mutex);
	/* do not send IPI if clk never enable before */
	if (!enable && (clk_en_cnt & (1 << clk_idx)) != (1 << clk_idx)) {
		mutex_unlock(&mmdvfs_clkmux_mutex);
		return 0;
	}

	if (enable)
		clk_en_cnt_expected |= 1 << clk_idx;
	else
		clk_en_cnt_expected &= ~(1 << clk_idx);

	ret = mmdvfs_vcp_ipi_send(FUNC_SET_CLK, clk_idx, enable, MAX_OPP);

	if (!ret) {
		if (enable)
			clk_en_cnt |= 1 << clk_idx;
		else
			clk_en_cnt &= ~(1 << clk_idx);
	}
	mutex_unlock(&mmdvfs_clkmux_mutex);

	if (log_level & (1 << log_clk)) {
		struct mmdvfs_ipi_data slot =
			*(struct mmdvfs_ipi_data *)(u32 *)&mmdvfs_vcp_ipi_data;

		MMDVFS_DBG("ipi:%#x slot:%#x idx:%hhu ena:%hhu en_cnt: 0x%x",
			ret, slot, slot.idx, slot.opp, clk_en_cnt);
	}

	return 0;
}

static int mtk_mmdvfs_clk_enable(const u8 clk_idx)
{
	return mtk_mmdvfs_clk_ctrl(clk_idx, true);
}

static int mtk_mmdvfs_clk_disable(const u8 clk_idx)
{
	return mtk_mmdvfs_clk_ctrl(clk_idx, false);
}

int mmdvfs_get_vcp_log(char *buf, const struct kernel_param *kp)
{
	struct mmdvfs_ipi_data slot;
	int len = 0, ret;

	if (!mtk_is_mmdvfs_init_done()) {
		MMDVFS_DBG("mmdvfs_v3 init not ready");
		return 0;
	}

	mtk_mmdvfs_enable_vcp(true, VCP_PWR_USR_MMDVFS_GET_VCP_LOG);
	ret = mmdvfs_vcp_ipi_send(FUNC_SET_LOG, LOG_NUM, MAX_OPP, MAX_OPP);
	mtk_mmdvfs_enable_vcp(false, VCP_PWR_USR_MMDVFS_GET_VCP_LOG);

	slot = *(struct mmdvfs_ipi_data *)(u32 *)&mmdvfs_vcp_ipi_data;
	len += snprintf(
		buf + len, PAGE_SIZE - len, "mmdvfs vcp log:%#x", slot.ack);

	return len;
}

int mmdvfs_set_vcp_log(const char *val, const struct kernel_param *kp)
{
	struct mmdvfs_ipi_data slot;
	u16 log = 0;
	int ret;

	if (!mtk_is_mmdvfs_init_done()) {
		MMDVFS_DBG("mmdvfs_v3 init not ready");
		return 0;
	}

	ret = kstrtou16(val, 0, &log);
	if (ret || log >= LOG_NUM) {
		MMDVFS_ERR("failed:%d log:%hu", ret, log);
		return ret;
	}

	mtk_mmdvfs_enable_vcp(true, VCP_PWR_USR_MMDVFS_SET_VCP_LOG);
	ret = mmdvfs_vcp_ipi_send(FUNC_SET_LOG, log, MAX_OPP, MAX_OPP);
	mtk_mmdvfs_enable_vcp(false, VCP_PWR_USR_MMDVFS_SET_VCP_LOG);

	slot = *(struct mmdvfs_ipi_data *)(u32 *)&mmdvfs_vcp_ipi_data;
	MMDVFS_DBG("ret:%d slot:%#x log:%hu log:%#x", ret, slot, log, slot.ack);

	return 0;
}

static struct kernel_param_ops mmdvfs_set_vcp_log_ops = {
	.get = mmdvfs_get_vcp_log,
	.set = mmdvfs_set_vcp_log,
};
module_param_cb(vcp_log, &mmdvfs_set_vcp_log_ops, NULL, 0644);
MODULE_PARM_DESC(vcp_log, "mmdvfs vcp log");

module_param(log_level, uint, 0644);
MODULE_PARM_DESC(log_level, "mmdvfs log level");

static const struct of_device_id of_match_mmdvfs_v3[] = {
	{
		.compatible = "mediatek,mtk-mmdvfs-v3",
	}, {}
};

static int mmdvfs_vcp_init_thread(void *data)
{
	static struct mtk_ipi_device *vcp_ipi_dev;
	int ret = 0, retry = 0;

	while (mtk_mmdvfs_enable_vcp(true, VCP_PWR_USR_MMDVFS_VCP_INIT)) {
		if (++retry > 100) {
			MMDVFS_ERR("vcp is not powered on yet");
			return -ETIMEDOUT;
		}
		ssleep(1);
	}

	retry = 0;
	while (!is_vcp_ready_ex(VCP_A_ID)) {
		if (++retry > 100) {
			MMDVFS_ERR("VCP_A_ID:%d not ready", VCP_A_ID);
			return -ETIMEDOUT;
		}
		usleep_range(1000, 2000);
	}

	retry = 0;
	while (!(vcp_ipi_dev = vcp_get_ipidev())) {
		if (++retry > 100) {
			MMDVFS_ERR("cannot get vcp ipidev");
			return -ETIMEDOUT;
		}
		ssleep(1);
	}

	vcp_ready_notifier.notifier_call = mmdvfs_vcp_notifier_callback;
	vcp_A_register_notify_ex(&vcp_ready_notifier);

	mmdvfs_init_done = true;
	MMDVFS_DBG("mmdvfs_init_done:%d", mmdvfs_init_done);
	return ret;
}

static int mmdvfs_ccu_init_thread(void *data)
{
	phandle handle;
	struct device_node *node = (struct device_node *) data;
	struct device_node *rproc_np = NULL;
	int ret = 0, retry = 0;

	ret = of_property_read_u32(node, "mediatek,ccu-rproc", &handle);
	if (ret < 0) {
		MMDVFS_DBG("get CCU phandle fail");
		return ret;
	}

	rproc_np = of_find_node_by_phandle(handle);
	if (rproc_np) {
		ccu_pdev = of_find_device_by_node(rproc_np);
		if (!ccu_pdev) {
			MMDVFS_ERR("find ccu rproc pdev fail");
			ret = -EINVAL;
			goto error_handle;
		}

		while (!(ccu_rproc = rproc_get_by_phandle(handle))) {
			if (++retry > 100) {
				MMDVFS_ERR("rproc_get_by_phandle fail");
				return -ETIMEDOUT;
			}
			ssleep(1);
		}

		of_node_put(rproc_np);
		MMDVFS_DBG("get ccu proc pdev successfully\n");
	}
	return ret;
error_handle:
	of_node_put(rproc_np);

	return ret;
}

static int mmdvfs_v3_probe(struct platform_device *pdev)
{
	const char *MMDVFS_CLK_NAMES = "mediatek,mmdvfs-clock-names";
	const char *MMDVFS_CLKS = "mediatek,mmdvfs-clocks";
	struct device_node *node = pdev->dev.of_node;
	struct clk_onecell_data *clk_data;
	struct task_struct *kthr_vcp, *kthr_ccu;
	struct device_node *larbnode;
	struct platform_device *larbdev;
	struct clk *clk;
	int i, ret;

	ret = of_property_count_strings(node, MMDVFS_CLK_NAMES);
	if (ret <= 0) {
		MMDVFS_ERR("%s invalid:%d", MMDVFS_CLK_NAMES, ret);
		return ret;
	}
	mmdvfs_clk_num = ret;

	mtk_mmdvfs_clks =
		kcalloc(mmdvfs_clk_num, sizeof(*mtk_mmdvfs_clks), GFP_KERNEL);
	if (!mtk_mmdvfs_clks) {
		MMDVFS_ERR("mtk_mmdvfs_clks without memory");
		return -ENOMEM;
	}

	clk_data = mtk_alloc_clk_data(mmdvfs_clk_num);
	if (!clk_data) {
		MMDVFS_ERR("allocate clk_data failed num:%hhu", mmdvfs_clk_num);
		return -ENOMEM;
	}

	for (i = 0; i < mmdvfs_clk_num; i++) {
		struct device_node *table, *opp = NULL;
		struct of_phandle_args spec;
		struct clk_init_data init = {};
		u8 idx = 0;

		of_property_read_string_index(
			node, MMDVFS_CLK_NAMES, i, &mtk_mmdvfs_clks[i].name);

		ret = of_parse_phandle_with_args(
			node, MMDVFS_CLKS, "#mmdvfs,clock-cells", i, &spec);
		if (ret) {
			MMDVFS_ERR("parse %s i:%d failed:%d",
				MMDVFS_CLKS, i, ret);
			return ret;
		}

		mtk_mmdvfs_clks[i].clk_id = spec.args[0];
		mtk_mmdvfs_clks[i].pwr_id = spec.args[1];
		mtk_mmdvfs_clks[i].user_id = spec.args[2];
		mtk_mmdvfs_clks[i].ipi_type = spec.args[3];
		mtk_mmdvfs_clks[i].spec_type = spec.args[4];
		table = of_find_node_by_phandle(spec.args[5]);
		of_node_put(spec.np);

		do {
			u64 freq;

			opp = of_get_next_available_child(table, opp);
			if (opp) {
				of_property_read_u64(opp, "opp-hz", &freq);
				mtk_mmdvfs_clks[i].freqs[idx] = freq;
				idx += 1;
			}
		} while (opp);
		of_node_put(table);

		mtk_mmdvfs_clks[i].opp = MAX_OPP;
		mtk_mmdvfs_clks[i].freq_num = idx;

		MMDVFS_DBG(
			"i:%d name:%s clk:%hhu pwr:%hhu user:%hhu ipi:%hhu spec:%hhu opp:%hhu freq:%hhu",
			i, mtk_mmdvfs_clks[i].name, mtk_mmdvfs_clks[i].clk_id,
			mtk_mmdvfs_clks[i].pwr_id, mtk_mmdvfs_clks[i].user_id,
			mtk_mmdvfs_clks[i].ipi_type,
			mtk_mmdvfs_clks[i].spec_type,
			mtk_mmdvfs_clks[i].opp, mtk_mmdvfs_clks[i].freq_num);

		if (!IS_ERR_OR_NULL(clk_data->clks[i]))
			continue;

		init.name = mtk_mmdvfs_clks[i].name;
		init.ops = &mtk_mmdvfs_req_ops;
		mtk_mmdvfs_clks[i].clk_hw.init = &init;

		clk = clk_register(NULL, &mtk_mmdvfs_clks[i].clk_hw);
		if (IS_ERR_OR_NULL(clk))
			MMDVFS_ERR("i:%d clk:%s register failed:%d",
				i, mtk_mmdvfs_clks[idx].name, PTR_ERR(clk));
		else
			clk_data->clks[i] = clk;
	}

	ret = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);
	if (ret) {
		MMDVFS_ERR("add clk provider failed:%d", ret);
		mtk_free_clk_data(clk_data);
		return ret;
	}

	for (i = 0; i < ARRAY_SIZE(mmdvfs_pwr_opp); i++) {
		mmdvfs_pwr_opp[i] = MAX_OPP;

		clk = of_clk_get(node, i);
		if (IS_ERR_OR_NULL(clk))
			MMDVFS_DBG("i:%d clk get failed:%d", i, PTR_ERR(clk));
		else
			mmdvfs_pwr_clk[i] = clk;
	}

	larbnode = of_parse_phandle(pdev->dev.of_node, "mediatek,larbs", 0);
	if (larbnode) {
		larbdev = of_find_device_by_node(larbnode);
		if (larbdev) {
			cam_larb_dev = &larbdev->dev;
			register_pm_notifier(&lpm_spm_suspend_pm_notifier_func);
		}
		of_node_put(larbnode);
	}

	clk_cb.clk_enable = mtk_mmdvfs_clk_enable;
	clk_cb.clk_disable = mtk_mmdvfs_clk_disable;
	mtk_clk_register_ipi_callback(&clk_cb);

	if (of_property_read_bool(node, "mmdvfs-free-run"))
		mmdvfs_free_run = true;

	for (i = 0; i < PWR_MMDVFS_NUM; i++) {
		last_vote_step[i] = -1;
		last_force_step[i] = -1;
	}

	kthr_vcp = kthread_run(mmdvfs_vcp_init_thread, NULL, "mmdvfs-vcp");
	kthr_ccu = kthread_run(mmdvfs_ccu_init_thread, node, "mmdvfs-ccu");

	return ret;
}

static struct platform_driver clk_mmdvfs_drv = {
	.probe = mmdvfs_v3_probe,
	.driver = {
		.name = "mtk-mmdvfs-v3",
		.owner = THIS_MODULE,
		.of_match_table = of_match_mmdvfs_v3,
	},
};

static int __init clk_mmdvfs_init(void)
{
	return platform_driver_register(&clk_mmdvfs_drv);
}

static void __exit clk_mmdvfs_exit(void)
{
	platform_driver_unregister(&clk_mmdvfs_drv);
}

module_init(clk_mmdvfs_init);
module_exit(clk_mmdvfs_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek MMDVFS");
MODULE_AUTHOR("MediaTek Inc.");
