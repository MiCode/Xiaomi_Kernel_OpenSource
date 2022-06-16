// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/device.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/delay.h>
#include <linux/slab.h>

#include <linux/of_device.h>
#include <linux/io.h>
#include <linux/sched/clock.h>
#include <linux/dma-mapping.h>

#include "apusys_power.h"
#include "apusys_device.h"
#include "apu_config.h"

#include "mvpu_plat_device.h"
#include "mvpu_sysfs.h"
#include "mvpu_ipi.h"
#include "mvpu_cmd_data.h"
#include "mvpu_driver.h"
#include "mvpu_sec.h"

#define SEC_DISABLE_CHK

struct mutex mvpu_pool_lock;

static int apusys_mvpu_handler_lite(int type, void *hnd, struct apusys_device *dev)
{
	int ret = 0;
	//struct mvpu_dev *mvpu_info;
	mvpu_request_t *mvpu_req;
	//struct apusys_cmd_handle *cmd_hnd;
	struct apusys_cmd_valid_handle *cmd_hnd;
	struct apusys_cmdbuf *cmdbuf;
	//uint64_t kreg_base_iova = 0;
	//struct apusys_power_hnd *pwr_cmd_hnd;

#ifdef MVPU_SECURITY
	uint32_t batch_name_hash;
	uint32_t buf_num;
	uint32_t rp_num;
	void *kreg_kva;

	uint32_t i = 0;

	uint32_t *sec_chk_addr = NULL;
	uint32_t *sec_buf_size = NULL;
	uint32_t *mem_is_kernel = NULL;

	uint32_t *target_buf_old_base = NULL;
	uint32_t *target_buf_old_offset = NULL;
	uint32_t *target_buf_new_base = NULL;
	uint32_t *target_buf_new_offset = NULL;

	uint32_t *target_buf_old_map = NULL;
	uint32_t *target_buf_new_map = NULL;

	bool algo_in_img = false;
	uint32_t ker_bin_offset = 0;
	//uint32_t ker_size = 0;
	uint32_t ker_bin_num = 0;
	uint32_t *ker_bin_each_iova = NULL;

	uint32_t session_id = -1;
	uint32_t hash_id = -1;
#endif

#ifdef MVPU_SEC_DEBUG
	pr_info("%s cmd type %d\n", __func__, type);
#endif

	if (!dev)
		return -1;

	if (!hnd) {
		pr_info("%s get hnd fail\n", __func__);
		return -1;
	}

	if (dev->dev_type != APUSYS_DEVICE_MVPU) {
		pr_info("%s get wrong dev_type: %d\n", __func__, dev->dev_type);
		return -1;
	}

	switch (type) {
	case APUSYS_CMD_VALIDATE:
		cmd_hnd = hnd;

		if (cmd_hnd->session == NULL) {
			pr_info("[MVPU][Sec] APUSYS_CMD_VALIDATE error: cmd_hnd->session is NULL\n");
			ret = -1;
			break;
		}

		if (cmd_hnd->num_cmdbufs < MVPU_MIN_CMDBUF_NUM) {
			pr_info("%s get wrong num_cmdbufs: %d\n", __func__, cmd_hnd->num_cmdbufs);
			ret = -1;
			break;
		}

		cmdbuf = cmd_hnd->cmdbufs;

		if (cmdbuf[MVPU_CMD_INFO_IDX].size < sizeof(mvpu_request_t)) {
			pr_info("%s get wrong cmdbuf size: 0x%x, should be 0x%x\n",
				__func__, cmdbuf[MVPU_CMD_INFO_IDX].size,
				sizeof(mvpu_request_t));
			ret = -1;
			break;
		}

		mvpu_req = (mvpu_request_t *)cmdbuf[MVPU_CMD_INFO_IDX].kva;
		kreg_kva = cmdbuf[MVPU_CMD_KREG_BASE_IDX].kva;

#ifdef MVPU_SECURITY
		batch_name_hash = mvpu_req->batch_name_hash;

		//get buf infos
		buf_num = mvpu_req->buf_num;

		//get replace infos
		rp_num = mvpu_req->rp_num;

#ifdef MVPU_SEC_DEBUG
		pr_info("[MVPU][Sec] DRV get batch: 0x%08x\n", batch_name_hash);
		pr_info("[MVPU][Sec] buf_num %d\n", buf_num);
		pr_info("[MVPU][Sec] rp_num %d\n", rp_num);
#endif

		//FIXME: processor wrong infos
		if (buf_num == 0 || rp_num == 0) {
			pr_info("[MVPU][Sec] WARNING: wrong buf infos: buf_num %d, rp_num %d\n",
						buf_num, rp_num);
			pr_info("[MVPU][Sec] WARNING: BYPASS secure flow\n");
			ret = 0;
			goto END;
		}

		sec_chk_addr = kzalloc(buf_num*sizeof(uint32_t), GFP_KERNEL);
		if (!sec_chk_addr) {
			ret = -ENOMEM;
			goto END;
		}

		if (copy_from_user(sec_chk_addr, (void __user *)mvpu_req->sec_chk_addr, buf_num*sizeof(uint32_t))) {
			pr_info("[MVPU][Sec] copy sec_chk_addr fail 0x%llx\n", mvpu_req->sec_chk_addr);
			ret = -EFAULT;
			goto END;
		} else {
#ifdef MVPU_SEC_DEBUG
			pr_info("[MVPU][Sec] copy sec_chk_addr success 0x%llx, sec_chk_addr 0x%lx\n",
				mvpu_req->sec_chk_addr, sec_chk_addr[0]);
#endif
		}

		sec_buf_size = kzalloc(buf_num*sizeof(uint32_t), GFP_KERNEL);
		if (!sec_buf_size) {
			ret = -ENOMEM;
			goto END;
		}

		if (copy_from_user(sec_buf_size, (void __user *)mvpu_req->sec_buf_size, buf_num*sizeof(uint32_t))) {
			pr_info("[MVPU][Sec] copy sec_buf_size fail 0x%llx\n", mvpu_req->sec_buf_size);
			ret = -EFAULT;
			goto END;
		} else {
#ifdef MVPU_SEC_DEBUG
			pr_info("[MVPU][Sec] copy sec_buf_size success 0x%llx, sec_buf_size 0x%lx\n",
					mvpu_req->sec_buf_size,
					sec_buf_size[0]);
#endif
		}

		mem_is_kernel = kzalloc(buf_num*sizeof(uint32_t), GFP_KERNEL);
		if (!mem_is_kernel) {
			ret = -ENOMEM;
			goto END;
		}

		if (copy_from_user(mem_is_kernel, (void __user *)mvpu_req->mem_is_kernel, buf_num*sizeof(uint32_t))) {
			pr_info("[MVPU][Sec] copy mem_is_kernel fail 0x%llx\n", mvpu_req->mem_is_kernel);
			ret = -EFAULT;
			goto END;
		} else {
#ifdef MVPU_SEC_DEBUG
			for (i = 0; i < buf_num; i++)
				pr_info("[MVPU][Sec] copy mem_is_kernel success 0x%llx, mem_is_kernel[%d]: %d\n",
						mvpu_req->mem_is_kernel,
						i, mem_is_kernel[i]);
#endif
		}

		//integrity check
		for (i = 0; i < buf_num; i++) {
#ifdef MVPU_SEC_DEBUG
			pr_info("[MVPU][Sec] DRV check cnt %3d, addr 0x%08x, buf_attr: %d, buf_size: 0x%08x\n",
					i, sec_chk_addr[i],
					mem_is_kernel[i],
					sec_buf_size[i]);
#endif
			if (sec_chk_addr[i] == 0)
				continue;

			// check buffer integrity
			if (apusys_mem_get_by_iova(cmd_hnd->session, (uint64_t)sec_chk_addr[i]) != 0) {
				pr_info("[MVPU][Sec] buf[%d]: 0x%08x integrity checked FAIL\n",
							i, sec_chk_addr[i]);
				ret = -1;
				goto END;
			} else {
				pr_info("[MVPU][Sec] buf[%d]: 0x%08x integrity checked PASS\n",
							i, sec_chk_addr[i]);
			}
		}

		target_buf_old_base = kzalloc(rp_num*sizeof(uint32_t), GFP_KERNEL);
		if (!target_buf_old_base) {
			ret = -ENOMEM;
			goto END;
		}

		if (copy_from_user(target_buf_old_base, (void __user *)mvpu_req->target_buf_old_base, rp_num*sizeof(uint32_t))) {
			ret = -EFAULT;
			goto END;
		}

		target_buf_old_offset = kzalloc(rp_num*sizeof(uint32_t), GFP_KERNEL);
		if (!target_buf_old_offset) {
			ret = -ENOMEM;
			goto END;
		}

		if (copy_from_user(target_buf_old_offset, (void __user *)mvpu_req->target_buf_old_offset, rp_num*sizeof(uint32_t))) {
			ret = -EFAULT;
			goto END;
		}

		target_buf_new_base = kzalloc(rp_num*sizeof(uint32_t), GFP_KERNEL);
		if (!target_buf_new_base) {
			ret = -ENOMEM;
			goto END;
		}

		if (copy_from_user(target_buf_new_base, (void __user *)mvpu_req->target_buf_new_base, rp_num*sizeof(uint32_t))) {
			ret = -EFAULT;
			goto END;
		}

		target_buf_new_offset = kzalloc(rp_num*sizeof(uint32_t), GFP_KERNEL);
		if (!target_buf_new_offset) {
			ret = -ENOMEM;
			goto END;
		}

		if (copy_from_user(target_buf_new_offset, (void __user *)mvpu_req->target_buf_new_offset, rp_num*sizeof(uint32_t))) {
			ret = -EFAULT;
			goto END;
		}

		target_buf_old_map = kzalloc(rp_num*sizeof(uint32_t), GFP_KERNEL);
		if (!target_buf_old_map) {
			ret = -ENOMEM;
			goto END;
		}

		target_buf_new_map = kzalloc(rp_num*sizeof(uint32_t), GFP_KERNEL);
		if (!target_buf_new_map) {
			ret = -ENOMEM;
			goto END;
		}

		//map rp_info to buf_id
		map_base_buf_id(buf_num, sec_chk_addr, rp_num, target_buf_old_map, target_buf_old_base);
		map_base_buf_id(buf_num, sec_chk_addr, rp_num, target_buf_new_map, target_buf_new_base);

		//get image infos: kernel.bin
		//algo_in_img = get_ker_info(batch_name_hash, &ker_bin_offset, &ker_size, &ker_bin_num);
		algo_in_img = get_ker_info(batch_name_hash, &ker_bin_offset, &ker_bin_num);
		if (algo_in_img) {
			ker_bin_each_iova = kcalloc(ker_bin_num, sizeof(uint32_t), GFP_KERNEL);
			if (!ker_bin_each_iova) {
				ret = -ENOMEM;
				goto END;
			}

			set_ker_iova(ker_bin_offset, ker_bin_num, ker_bin_each_iova);
		}

		mutex_lock(&mvpu_pool_lock);
		//mem pool: session/hash
		session_id = get_saved_session_id(cmd_hnd->session);

		if (session_id != -1) {
			hash_id = get_saved_hash_id(session_id, batch_name_hash);

			if (hash_id == -1) {
				hash_id = get_avail_hash_id(session_id);
				clear_hash(session_id, hash_id);

				ret = update_hash_pool(cmd_hnd->session, algo_in_img,
							session_id, hash_id, batch_name_hash,
							buf_num, sec_chk_addr,
							sec_buf_size, mem_is_kernel);

				if (ret != 0) {
					pr_info("[MVPU][SEC] hash error: ret = 0x%08x\n",
								ret);
					goto VALIDATE_END;
				}
			}
		} else {
			session_id = get_avail_session_id();

			//clear_session_id(session_id);
			update_session_id(session_id, cmd_hnd->session);

			hash_id = get_avail_hash_id(session_id);
			clear_hash(session_id, hash_id);

			ret = update_hash_pool(cmd_hnd->session, algo_in_img,
						session_id, hash_id, batch_name_hash,
						buf_num, sec_chk_addr, sec_buf_size, mem_is_kernel);
			if (ret != 0) {
				pr_info("[MVPU][SEC] hash error: ret = 0x%08x\n",
							ret);
				goto VALIDATE_END;
			}
		}

		ret = update_new_base_addr(algo_in_img, session_id, hash_id,
						sec_chk_addr, mem_is_kernel,
						rp_num, target_buf_new_map, target_buf_new_base,
						ker_bin_num, ker_bin_each_iova, kreg_kva);

		if (ret != 0)
			goto VALIDATE_END;

		ret = replace_mem(session_id, hash_id, rp_num,
						target_buf_old_map,
						target_buf_old_base, target_buf_old_offset,
						target_buf_new_base, target_buf_new_offset,
						kreg_kva);
VALIDATE_END:
		mutex_unlock(&mvpu_pool_lock);

		break;
	case APUSYS_CMD_SESSION_CREATE:
		ret = 0;
		break;
	case APUSYS_CMD_SESSION_DELETE:
		if (hnd == NULL) {
			pr_info("[MVPU][Sec] APUSYS_CMD_SESSION_DELETE error: hnd is NULL\n");
			ret = -1;
		} else {
			mutex_lock(&mvpu_pool_lock);
			clear_session(hnd);
			mutex_unlock(&mvpu_pool_lock);
			ret = 0;
		}
		break;
#endif //MVPU_SECURITY
	default:
		pr_info("%s get wrong cmd type: %d\n", __func__, type);
		ret = -1;
		break;
	}

END:
	if (sec_chk_addr != NULL)
		kfree(sec_chk_addr);

	if (sec_buf_size != NULL)
		kfree(sec_buf_size);

	if (mem_is_kernel != NULL)
		kfree(mem_is_kernel);

	if (target_buf_old_base != NULL)
		kfree(target_buf_old_base);

	if (target_buf_old_offset != NULL)
		kfree(target_buf_old_offset);

	if (target_buf_new_base != NULL)
		kfree(target_buf_new_base);

	if (target_buf_new_offset != NULL)
		kfree(target_buf_new_offset);

	if (target_buf_old_map != NULL)
		kfree(target_buf_old_map);

	if (target_buf_new_map != NULL)
		kfree(target_buf_new_map);

	if (ker_bin_each_iova != NULL) {
		kfree(ker_bin_each_iova);
		ker_bin_each_iova = NULL;
	}

	return ret;
}

static int mvpu_apusys_register(struct platform_device *pdev)
{
	int ret = 0;

	struct device *dev = &pdev->dev;

	mvpu_adev.dev_type = APUSYS_DEVICE_MVPU;
	mvpu_adev.preempt_type = APUSYS_PREEMPT_NONE;
	mvpu_adev.preempt_level = 0;
	mvpu_adev.send_cmd = apusys_mvpu_handler_lite;
	mvpu_adev.idx = 0;

	ret = apusys_register_device(&mvpu_adev);
	if (ret) {
		dev_notice(dev,
			"Failed to register apusys (%d)\n", ret);
		ret = -EPROBE_DEFER;
	}

	return ret;
}

static int mvpu_suspend(struct platform_device *pdev, pm_message_t mesg)
{
	return 0;
}

static int mvpu_resume(struct platform_device *pdev)
{
	return 0;
}

static int mvpu_probe(struct platform_device *pdev)
{
	int ret = 0;

	struct device *dev = &pdev->dev;

	dev_info(dev, "mvpu probe start\n");
	ret = mvpu_plat_info_init(pdev);
	if (!ret) {
		dev_info(dev, "(f:%s/l:%d) mvpu get plat info pass\n",
						__func__, __LINE__);
	} else {
		dev_info(dev, "(f:%s/l:%d) mvpu get plat info fail\n",
						__func__, __LINE__);
		return ret;
	}

	ret = mvpu_apusys_register(pdev);
	if (!ret) {
		dev_info(dev, "(f:%s/l:%d) register apusys device success\n",
						__func__, __LINE__);
	} else {
		dev_info(dev, "(f:%s/l:%d) set probe defer, wait longer\n",
						__func__, __LINE__);
		return ret;
	}

	mutex_init(&mvpu_pool_lock);

	/* Initialize platform to allocate mvpu devices first. */
	ret = mvpu_plat_init(pdev);
	if (!ret) {
		dev_info(dev, "(f:%s/l:%d) mvpu register power device pass\n",
						__func__, __LINE__);
	} else {
		dev_info(dev, "(f:%s/l:%d) register mvpu power fail\n",
						__func__, __LINE__);
		return ret;
	}

	ret = mvpu_sec_init(dev);
	if (!ret) {
		dev_info(dev, "(f:%s/l:%d) mvpu sec pass\n",
						__func__, __LINE__);
	} else {
		dev_info(dev, "(f:%s/l:%d) mvpu sec fail\n",
						__func__, __LINE__);
		return ret;
	}

	ret = mvpu_load_img(dev);
	if (!ret) {
		dev_info(dev, "(f:%s/l:%d) mvpu load mvpu_algo.img pass\n",
						__func__, __LINE__);
	} else {
		dev_info(dev, "(f:%s/l:%d) mvpu load mvpu_algo.img fail\n",
						__func__, __LINE__);
		return ret;
	}

	dev_info(dev, "%s probe pass\n", __func__);

	return ret;
}

static int mvpu_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	dev_info(dev, "%s remove pass\n", __func__);
	return 0;
}

static struct platform_driver mvpu_driver = {
	.probe = mvpu_probe,
	.remove = mvpu_remove,
	.suspend = mvpu_suspend,
	.resume = mvpu_resume,
	.driver = {
		.name = "mtk_mvpu",
		.owner = THIS_MODULE,
	}
};

static int mvpu_drv_init(void)
{
	int ret;

	mvpu_driver.driver.of_match_table = mvpu_plat_get_device();

	ret = platform_driver_register(&mvpu_driver);
	if (ret != 0) {
		pr_info("mvpu, register platform driver fail\n");
		return ret;
	}
	pr_info("mvpu, register platform driver pass\n");
	return 0;
}

static void mvpu_drv_exit(void)
{
	platform_driver_unregister(&mvpu_driver);
}

int mvpu_init(void)
{
	/* Register platform driver after debugfs initialization */
	if (!mvpu_drv_init()) {
		mvpu_sysfs_init();
		mvpu_ipi_init();
	} else {
		return -ENODEV;
	}

	pr_info("%s() done\n", __func__);

	return 0;
}

void mvpu_exit(void)
{
	pr_info("%s()!!\n", __func__);
	//mvpu_sysfs_exit();
	mvpu_drv_exit();
}
