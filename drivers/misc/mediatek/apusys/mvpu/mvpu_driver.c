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

struct mutex mvpu_pool_lock;

#ifdef MVPU_SECURITY
static int mvpu_validation(void *hnd)
{
	int ret = 0;
	struct mvpu_request *mvpu_req;
	struct apusys_cmd_valid_handle *cmd_hnd;
	void *session;
	struct apusys_cmdbuf *cmdbuf;

	void *kreg_kva;
	uint32_t knl_num = 0;

	uint32_t batch_name_hash;
	uint32_t buf_num = 0;
	uint32_t rp_num = 0;
	uint32_t kerarg_num = 0;
	uint32_t sec_level = 0;
	uint32_t buf_int_check_pass = 1;

	uint32_t i = 0;

	uint32_t *sec_chk_addr = NULL;
	uint32_t *sec_buf_size = NULL;
	uint32_t *sec_buf_attr = NULL;

	uint32_t *target_buf_old_base = NULL;
	uint32_t *target_buf_old_offset = NULL;
	uint32_t *target_buf_new_base = NULL;
	uint32_t *target_buf_new_offset = NULL;

	uint32_t *kerarg_buf_id = NULL;
	uint32_t *kerarg_offset = NULL;
	uint32_t *kerarg_size = NULL;

#ifdef MVPU_SEC_BUF_INFO_COPY
	void *sec_chk_addr_kva;
	void *sec_buf_size_kva;
	void *sec_buf_attr_kva;
#endif
#ifdef MVPU_SEC_OLD_ADDR_INFO_COPY
	void *tgtbuf_old_base_kva;
	void *tgtbuf_old_ofst_kva;
#endif
#ifdef MVPU_SEC_NEW_ADDR_INFO_COPY
	void *tgtbuf_new_base_kva;
	void *tgtbuf_new_ofst_kva;
#endif
#ifdef MVPU_SEC_KERARG_INFO_COPY
	void *kerarg_buf_id_kva;
	void *kerarg_offset_kva;
	void *kerarg_size_kva;
#endif

	uint32_t *target_buf_old_map = NULL;
	uint32_t *target_buf_new_map = NULL;
	uint32_t *rp_skip_buf = NULL;

	bool algo_in_img = false;
	uint32_t ker_bin_offset = 0;
	//uint32_t ker_size = 0;
	uint32_t ker_bin_num = 0;
	uint32_t *ker_bin_each_iova;

	uint32_t session_id = -1;
	uint32_t hash_id = -1;
	bool algo_in_pool = false;

	uint32_t buf_cmd_cnt = 0;
	uint32_t buf_cmd_next = 0x0;

	uint32_t buf_ker_cnt = 0x0;
	uint32_t buf_ker_start_long = 0x0;
	uint32_t buf_ker_end_long = 0x0;

	bool buf_get_start = false;
	bool buf_get_end = false;
	bool buf_get_cnt_done = false;

	bool kerarg_pool = false;

	cmd_hnd = hnd;

	if (cmd_hnd->session == NULL) {
		pr_info("[MVPU][Sec] [ERROR] APUSYS_CMD_VALIDATE: session is NULL\n");
		ret = -1;
		goto END;
	} else {
		session = cmd_hnd->session;
		if (mvpu_loglvl_drv >= APUSYS_MVPU_LOG_DBG)
			pr_info("[MVPU][Sec] get session: 0x%llx\n", (uint64_t)session);
	}

	if (cmd_hnd->num_cmdbufs < MVPU_MIN_CMDBUF_NUM) {
		pr_info("[MVPU][Sec] [ERROR] %s get wrong num_cmdbufs: %d\n",
				__func__, cmd_hnd->num_cmdbufs);
		ret = -1;
		goto END;
	}

	cmdbuf = cmd_hnd->cmdbufs;

	if (cmdbuf[MVPU_CMD_INFO_IDX].size == sizeof(struct mvpu_request)) {
		kerarg_pool = true;
	} else if (cmdbuf[MVPU_CMD_INFO_IDX].size != MVPU_CMD_LITE_SIZE) {
		pr_info("[MVPU][Sec] [ERROR] get wrong cmdbuf size: 0x%x, should be 0x%x\n",
				cmdbuf[MVPU_CMD_INFO_IDX].size,
				sizeof(struct mvpu_request));
		ret = -1;
		goto END;
	}

	mvpu_req = (struct mvpu_request *)cmdbuf[MVPU_CMD_INFO_IDX].kva;
	kreg_kva = cmdbuf[MVPU_CMD_KREG_BASE_IDX].kva;

	batch_name_hash = mvpu_req->batch_name_hash;

	//get buf infos
	buf_num = mvpu_req->buf_num;

	//get replace infos
	rp_num = mvpu_req->rp_num;

	//get kernel ard infos
	//kerarg_num = mvpu_req->kerarg_num;
	kerarg_num = mvpu_req->mpu_seg[0];

	//get security level
	sec_level = mvpu_req->mpu_seg[1];

	if (mvpu_loglvl_drv >= APUSYS_MVPU_LOG_DBG) {
		pr_info("[MVPU][Sec] DRV set batch: 0x%08x\n", batch_name_hash);
		pr_info("[MVPU][Sec] sec_level %d\n", sec_level);
		pr_info("[MVPU][Sec] buf_num %d\n", buf_num);
		pr_info("[MVPU][Sec] rp_num %d\n", rp_num);
		pr_info("[MVPU][Sec] kerarg_num %d\n", kerarg_num);
	}

	if (mvpu_loglvl_drv >= APUSYS_MVPU_LOG_ALL) {
		pr_info("[MVPU][SEC] [MPU] Engine settings\n");
		pr_info("[MVPU][SEC] [MPU] mpu_num = %3d\n", mvpu_req->mpu_num);
		for (i = 0; i < MVPU_MPU_SEGMENT_NUMS; i++)
			pr_info("[MVPU][SEC] [MPU] eng mpu_reg[%3d] = 0x%08x\n",
						i, mvpu_req->mpu_seg[i]);
	}

	if (mem_use_iova(mvpu_req->pmu_buff)) {
		if (apusys_mem_validate_by_cmd(session, cmd_hnd->cmd,
				(uint64_t)mvpu_req->pmu_buff, mvpu_req->buff_size) != 0) {
			pr_info("[MVPU][Sec] pmu_buff 0x%llx integrity checked FAIL\n",
						mvpu_req->pmu_buff);
			ret = -EINVAL;
			goto END;
		}
	}

	if ((buf_num == 0 || rp_num == 0) ||
		(kerarg_pool == false && kerarg_num != 0) ||
		sec_level != SEC_LVL_PROTECT) {
		if (mvpu_loglvl_drv >= APUSYS_MVPU_LOG_DBG) {
			pr_info("[MVPU][Sec] check flow\n");
			if (kerarg_pool == false && kerarg_num != 0)
				pr_info("[MVPU][Sec] update kerarg without pool\n");
		}

		knl_num = mvpu_req->header.reg_bundle_setting_0.s.kreg_kernel_num;
		ret = check_batch_flow(session, cmd_hnd->cmd, sec_level, kreg_kva, knl_num);

		if (ret != 0)
			pr_info("[MVPU][Sec] integrity checked FAIL\n");

		goto END;
	}

#ifdef MVPU_SEC_BUF_INFO_COPY
	sec_chk_addr = kzalloc(buf_num*sizeof(uint32_t), GFP_KERNEL);
	if (!sec_chk_addr) {
		ret = -ENOMEM;
		goto END;
	}

#ifdef MVPU_SEC_CHECK_INFO
	// check buffer integrity
	if (apusys_mem_validate_by_cmd(session, cmd_hnd->cmd,
			(uint64_t)mvpu_req->sec_chk_addr, buf_num*sizeof(uint32_t)) != 0) {
		pr_info("[MVPU][Sec] sec_chk_addr 0x%llx integrity checked FAIL\n",
					mvpu_req->sec_chk_addr);
		ret = -EINVAL;
		goto END;
	}
#endif

	sec_chk_addr_kva =
		apusys_mem_query_kva_by_sess(session, mvpu_req->sec_chk_addr);

	if (sec_chk_addr_kva != NULL)
		memcpy(sec_chk_addr, sec_chk_addr_kva, buf_num*sizeof(uint32_t));
#else
	sec_chk_addr =
		(uint32_t *)apusys_mem_query_kva_by_sess(session, mvpu_req->sec_chk_addr);
#endif

#ifdef MVPU_SEC_BUF_INFO_COPY
	sec_buf_size = kzalloc(buf_num*sizeof(uint32_t), GFP_KERNEL);
	if (!sec_buf_size) {
		ret = -ENOMEM;
		goto END;
	}

#ifdef MVPU_SEC_CHECK_INFO
	// check buffer integrity
	if (apusys_mem_validate_by_cmd(session, cmd_hnd->cmd,
			(uint64_t)mvpu_req->sec_buf_size, buf_num*sizeof(uint32_t)) != 0) {
		pr_info("[MVPU][Sec] sec_buf_size integrity checked FAIL\n");
		ret = -EINVAL;
		goto END;
	}
#endif

	sec_buf_size_kva =
		apusys_mem_query_kva_by_sess(session, mvpu_req->sec_buf_size);

	if (sec_buf_size_kva != NULL)
		memcpy(sec_buf_size, sec_buf_size_kva, buf_num*sizeof(uint32_t));
#else
	sec_buf_size =
		(uint32_t *)apusys_mem_query_kva_by_sess(session, mvpu_req->sec_buf_size);
#endif

#ifdef MVPU_SEC_BUF_INFO_COPY
	sec_buf_attr = kzalloc(buf_num*sizeof(uint32_t), GFP_KERNEL);
	if (!sec_buf_attr) {
		ret = -ENOMEM;
		goto END;
	}

#ifdef MVPU_SEC_CHECK_INFO
	// check buffer integrity
	if (apusys_mem_validate_by_cmd(session, cmd_hnd->cmd,
			(uint64_t)mvpu_req->sec_buf_attr, buf_num*sizeof(uint32_t)) != 0) {
		pr_info("[MVPU][Sec] sec_buf_attr integrity checked FAIL\n");
		ret = -EINVAL;
		goto END;
	}
#endif

	sec_buf_attr_kva =
		apusys_mem_query_kva_by_sess(session, mvpu_req->sec_buf_attr);

	if (sec_buf_attr_kva != NULL)
		memcpy(sec_buf_attr, sec_buf_attr_kva, buf_num*sizeof(uint32_t));
#else
	sec_buf_attr =
		(uint32_t *)apusys_mem_query_kva_by_sess(session, mvpu_req->sec_buf_attr);
#endif

	//integrity check
	for (i = 0; i < buf_num; i++) {
		if (mvpu_loglvl_drv >= APUSYS_MVPU_LOG_DBG) {
			pr_info("[MVPU][Sec] buf[%3d]: addr 0x%08x, attr: %d, size: 0x%08x\n",
				i, sec_chk_addr[i],
				sec_buf_attr[i],
				sec_buf_size[i]);
		}

		if (sec_chk_addr[i] == 0) {
			buf_cmd_cnt++;
			buf_get_cnt_done = true;
			if (buf_get_end == false) {
				buf_ker_end_long = i - 1;
				buf_get_end = true;
			}
			continue;
		}

		if (buf_cmd_cnt == MVPU_MIN_CMDBUF_NUM && buf_cmd_next == 0x0)
			buf_cmd_next = i;

		if (sec_buf_attr[i] != BUF_RINGBUFFER) {
			if (buf_get_cnt_done == false)
				buf_ker_cnt++;

			if (buf_get_start == false) {
				buf_ker_start_long = i;
				buf_get_start = true;
			}
		}

		// check buffer integrity
		if (apusys_mem_validate_by_cmd(cmd_hnd->session, cmd_hnd->cmd,
				(uint64_t)sec_chk_addr[i], sec_buf_size[i]) != 0) {
			pr_info("[MVPU][Sec] buf[%3d]: 0x%08x integrity checked FAIL\n",
						i, sec_chk_addr[i]);
			buf_int_check_pass = 0;
		} else {
			if (mvpu_loglvl_drv >= APUSYS_MVPU_LOG_DBG)
				pr_info("[MVPU][Sec] buf[%3d]: 0x%08x integrity checked PASS\n",
						i, sec_chk_addr[i]);
		}
	}

	if (buf_int_check_pass == 0) {
		pr_info("[MVPU][Sec] [ERROR] integrity checked FAIL\n");
		ret = -1;
		goto END;
	}

	//get image infos: kernel.bin
	algo_in_img = get_ker_info(batch_name_hash, &ker_bin_offset, &ker_bin_num);
	if (algo_in_img) {
		ker_bin_each_iova = kcalloc(ker_bin_num, sizeof(uint32_t), GFP_KERNEL);
		if (!ker_bin_each_iova) {
			ret = -ENOMEM;
			goto END;
		}

		set_ker_iova(ker_bin_offset, ker_bin_num, ker_bin_each_iova);
	}

	if (kerarg_num != 0) {
#ifdef MVPU_SEC_KERARG_INFO_COPY
		kerarg_buf_id = kzalloc(kerarg_num*sizeof(uint32_t), GFP_KERNEL);
		if (!kerarg_buf_id) {
			ret = -ENOMEM;
			goto END;
		}

#ifdef MVPU_SEC_CHECK_INFO
		// check buffer integrity
		if (apusys_mem_validate_by_cmd(session, cmd_hnd->cmd,
				(uint64_t)mvpu_req->kerarg_buf_id,
				kerarg_num*sizeof(uint32_t)) != 0) {
			pr_info("[MVPU][Sec] kerarg_buf_id integrity checked FAIL\n");
			ret = -EINVAL;
			goto END;
		}
#endif

		kerarg_buf_id_kva =
			apusys_mem_query_kva_by_sess(session, mvpu_req->kerarg_buf_id);

		if (kerarg_buf_id_kva != NULL)
			memcpy(kerarg_buf_id, kerarg_buf_id_kva, kerarg_num*sizeof(uint32_t));
#else
		kerarg_buf_id =
			(uint32_t *)apusys_mem_query_kva_by_sess(session, mvpu_req->kerarg_buf_id);
#endif

#ifdef MVPU_SEC_KERARG_INFO_COPY
		kerarg_offset = kzalloc(kerarg_num*sizeof(uint32_t), GFP_KERNEL);
		if (!kerarg_offset) {
			ret = -ENOMEM;
			goto END;
		}

#ifdef MVPU_SEC_CHECK_INFO
		// check buffer integrity
		if (apusys_mem_validate_by_cmd(session, cmd_hnd->cmd,
				(uint64_t)mvpu_req->kerarg_offset,
				kerarg_num*sizeof(uint32_t)) != 0) {
			pr_info("[MVPU][Sec] kerarg_offset integrity checked FAIL\n");
			ret = -EINVAL;
			goto END;
		}
#endif

		kerarg_offset_kva =
			apusys_mem_query_kva_by_sess(session, mvpu_req->kerarg_offset);

		if (kerarg_offset_kva != NULL)
			memcpy(kerarg_offset, kerarg_offset_kva, kerarg_num*sizeof(uint32_t));
#else
		kerarg_offset =
			(uint32_t *)apusys_mem_query_kva_by_sess(session, mvpu_req->kerarg_offset);
#endif

#ifdef MVPU_SEC_KERARG_INFO_COPY
		kerarg_size = kzalloc(kerarg_num*sizeof(uint32_t), GFP_KERNEL);
		if (!kerarg_size) {
			ret = -ENOMEM;
			goto END;
		}

#ifdef MVPU_SEC_CHECK_INFO
		// check buffer integrity
		if (apusys_mem_validate_by_cmd(session, cmd_hnd->cmd,
				(uint64_t)mvpu_req->kerarg_size,
				kerarg_num*sizeof(uint32_t)) != 0) {
			pr_info("[MVPU][Sec] kerarg_size integrity checked FAIL\n");
			ret = -EINVAL;
			goto END;
		}
#endif

		kerarg_size_kva =
			apusys_mem_query_kva_by_sess(session, mvpu_req->kerarg_size);

		if (kerarg_size_kva != NULL)
			memcpy(kerarg_size, kerarg_size_kva, kerarg_num*sizeof(uint32_t));
#else
		kerarg_size =
			(uint32_t *)apusys_mem_query_kva_by_sess(session, mvpu_req->kerarg_size);
#endif
	}

	mutex_lock(&mvpu_pool_lock);

	algo_in_pool = get_hash_info(session,
						batch_name_hash,
						&session_id,
						&hash_id,
						buf_num,
						rp_num,
						sec_buf_size);


	if (algo_in_pool == true)
		goto REPLACE_MEM;

#ifdef MVPU_SEC_OLD_ADDR_INFO_COPY
	target_buf_old_base = kzalloc(rp_num*sizeof(uint32_t), GFP_KERNEL);
	if (!target_buf_old_base) {
		ret = -ENOMEM;
		goto VALIDATE_END;
	}

#ifdef MVPU_SEC_CHECK_INFO
	// check buffer integrity
	if (apusys_mem_validate_by_cmd(session, cmd_hnd->cmd,
			(uint64_t)mvpu_req->target_buf_old_base, rp_num*sizeof(uint32_t)) != 0) {
		pr_info("[MVPU][Sec] target_buf_old_base integrity checked FAIL\n");
		ret = -EINVAL;
		goto VALIDATE_END;
	}
#endif

	tgtbuf_old_base_kva =
		apusys_mem_query_kva_by_sess(session, mvpu_req->target_buf_old_base);

	if (tgtbuf_old_base_kva != NULL)
		memcpy(target_buf_old_base, tgtbuf_old_base_kva, rp_num*sizeof(uint32_t));
#else
	target_buf_old_base =
		(uint32_t *)apusys_mem_query_kva_by_sess(session, mvpu_req->target_buf_old_base);
#endif

#ifdef MVPU_SEC_OLD_ADDR_INFO_COPY
	target_buf_old_offset = kzalloc(rp_num*sizeof(uint32_t), GFP_KERNEL);
	if (!target_buf_old_offset) {
		ret = -ENOMEM;
		goto VALIDATE_END;
	}

#ifdef MVPU_SEC_CHECK_INFO
	// check buffer integrity
	if (apusys_mem_validate_by_cmd(session, cmd_hnd->cmd,
			(uint64_t)mvpu_req->target_buf_old_offset, rp_num*sizeof(uint32_t)) != 0) {
		pr_info("[MVPU][Sec] target_buf_old_offset integrity checked FAIL\n");
		ret = -EINVAL;
		goto VALIDATE_END;
	}
#endif

	tgtbuf_old_ofst_kva =
		apusys_mem_query_kva_by_sess(session, mvpu_req->target_buf_old_offset);

	if (tgtbuf_old_ofst_kva != NULL)
		memcpy(target_buf_old_offset, tgtbuf_old_ofst_kva, rp_num*sizeof(uint32_t));
#else
	target_buf_old_offset =
		(uint32_t *)apusys_mem_query_kva_by_sess(session, mvpu_req->target_buf_old_offset);
#endif

#ifdef MVPU_SEC_NEW_ADDR_INFO_COPY
	target_buf_new_base = kzalloc(rp_num*sizeof(uint32_t), GFP_KERNEL);
	if (!target_buf_new_base) {
		ret = -ENOMEM;
		goto VALIDATE_END;
	}

#ifdef MVPU_SEC_CHECK_INFO
	// check buffer integrity
	if (apusys_mem_validate_by_cmd(session, cmd_hnd->cmd,
			(uint64_t)mvpu_req->target_buf_new_base, rp_num*sizeof(uint32_t)) != 0) {
		pr_info("[MVPU][Sec] target_buf_new_base integrity checked FAIL\n");
		ret = -EINVAL;
		goto VALIDATE_END;
	}
#endif

	tgtbuf_new_base_kva =
		apusys_mem_query_kva_by_sess(session, mvpu_req->target_buf_new_base);

	if (tgtbuf_new_base_kva != NULL)
		memcpy(target_buf_new_base, tgtbuf_new_base_kva, rp_num*sizeof(uint32_t));
#else
	target_buf_new_base =
		(uint32_t *)apusys_mem_query_kva_by_sess(session, mvpu_req->target_buf_new_base);
#endif

#ifdef MVPU_SEC_NEW_ADDR_INFO_COPY
	target_buf_new_offset = kzalloc(rp_num*sizeof(uint32_t), GFP_KERNEL);
	if (!target_buf_new_offset) {
		ret = -ENOMEM;
		goto VALIDATE_END;
	}

#ifdef MVPU_SEC_CHECK_INFO
	// check buffer integrity
	if (apusys_mem_validate_by_cmd(session, cmd_hnd->cmd,
			(uint64_t)mvpu_req->target_buf_new_offset, rp_num*sizeof(uint32_t)) != 0) {
		pr_info("[MVPU][Sec] target_buf_new_offset integrity checked FAIL\n");
		ret = -EINVAL;
		goto VALIDATE_END;
	}
#endif

	tgtbuf_new_ofst_kva =
		apusys_mem_query_kva_by_sess(session, mvpu_req->target_buf_new_offset);

	if (tgtbuf_new_ofst_kva != NULL)
		memcpy(target_buf_new_offset, tgtbuf_new_ofst_kva, rp_num*sizeof(uint32_t));
#else
	target_buf_new_offset =
		(uint32_t *)apusys_mem_query_kva_by_sess(session, mvpu_req->target_buf_new_offset);
#endif

	target_buf_old_map = kzalloc(rp_num*sizeof(uint32_t), GFP_KERNEL);
	if (!target_buf_old_map) {
		ret = -ENOMEM;
		goto VALIDATE_END;
	}

	target_buf_new_map = kzalloc(rp_num*sizeof(uint32_t), GFP_KERNEL);
	if (!target_buf_new_map) {
		ret = -ENOMEM;
		goto VALIDATE_END;
	}

	//map rp_info to buf_id
	map_base_buf_id(buf_num,
					sec_chk_addr,
					sec_buf_attr,
					rp_num,
					target_buf_old_map,
					target_buf_old_base,
					target_buf_new_map,
					target_buf_new_base,
					buf_cmd_next,
					buf_ker_cnt,
					buf_ker_start_long,
					buf_ker_end_long);

	//mem pool: session/hash
	if (session_id != -1) {
		if (hash_id == -1) {
			hash_id = get_avail_hash_id(session_id);
#ifndef MVPU_SEC_USE_OLDEST_HASH_ID
			if (hash_id == -1) {
				pr_info("[MVPU][SEC] [ERROR] out of hash pool\n");
				ret = -1;
				goto VALIDATE_END;
			}
#endif

			clear_hash(session_id, hash_id);

			ret = update_hash_pool(session,
						algo_in_img,
						session_id,
						hash_id,
						batch_name_hash,
						buf_num,
						sec_chk_addr,
						sec_buf_size,
						sec_buf_attr);

			if (ret != 0) {
				pr_info("[MVPU][SEC] hash error: ret = 0x%08x\n",
							ret);
				goto VALIDATE_END;
			}
		}
	} else {
		session_id = get_avail_session_id();
#ifndef MVPU_SEC_USE_OLDEST_SESSION_ID
		if (session_id == -1) {
			pr_info("[MVPU][SEC] [ERROR] out of session pool\n");
			ret = -1;
			goto VALIDATE_END;
		}
#endif

		//clear_session_id(session_id);
		update_session_id(session_id, session);

		hash_id = get_avail_hash_id(session_id);
#ifndef MVPU_SEC_USE_OLDEST_HASH_ID
		if (hash_id == -1) {
			pr_info("[MVPU][SEC] [ERROR] out of hash pool\n");
			ret = -1;
			goto VALIDATE_END;
		}
#endif

		clear_hash(session_id, hash_id);

		ret = update_hash_pool(session,
						algo_in_img,
						session_id,
						hash_id,
						batch_name_hash,
						buf_num,
						sec_chk_addr,
						sec_buf_size,
						sec_buf_attr);

		if (ret != 0) {
			pr_info("[MVPU][SEC] hash error: ret = 0x%08x\n",
						ret);
			goto VALIDATE_END;
		}
	}

REPLACE_MEM:

	rp_skip_buf = kzalloc(buf_num*sizeof(uint32_t), GFP_KERNEL);
	if (!rp_skip_buf) {
		ret = -ENOMEM;
		goto VALIDATE_END;
	}

	if (algo_in_pool == true) {
		set_rp_skip_buf(session_id,
						hash_id,
						buf_num,
						sec_chk_addr,
						sec_buf_attr,
						rp_skip_buf);
	}


	ret = update_new_base_addr(algo_in_img,
							algo_in_pool,
							session_id,
							hash_id,
							sec_chk_addr,
							sec_buf_attr,
							rp_skip_buf,
							rp_num,
							target_buf_new_map,
							target_buf_new_base,
							ker_bin_num,
							ker_bin_each_iova,
							kreg_kva);

	if (ret != 0)
		goto VALIDATE_END;

	if (algo_in_pool == false) {
		ret = save_hash_info(session_id,
						hash_id,
						buf_num,
						rp_num,
						sec_chk_addr,
						sec_buf_size,
						target_buf_old_base,
						target_buf_old_offset,
						target_buf_new_base,
						target_buf_new_offset,
						target_buf_old_map,
						target_buf_new_map);
	}

	ret = replace_mem(session_id, hash_id,
					sec_buf_attr,
					algo_in_pool,
					rp_skip_buf,
					rp_num,
					target_buf_old_map,
					target_buf_old_base,
					target_buf_old_offset,
					target_buf_new_map,
					target_buf_new_base,
					target_buf_new_offset,
					kreg_kva);

	if (kerarg_num != 0 && algo_in_pool == true) {
		ret = replace_kerarg(session,
					session_id, hash_id,
					kerarg_num,
					sec_chk_addr,
					kerarg_buf_id,
					kerarg_offset,
					kerarg_size);
	}

VALIDATE_END:
	mutex_unlock(&mvpu_pool_lock);

	if (ret != 0)
		goto END;

	ret = update_mpu(mvpu_req, session_id, hash_id,
					sec_chk_addr, sec_buf_size, sec_buf_attr);

	if (mvpu_req->mpu_num != 0 && mvpu_loglvl_drv >= APUSYS_MVPU_LOG_DBG) {
		pr_info("[MVPU][SEC] [MPU] mpu_num = %3d\n", mvpu_req->mpu_num);
		for (i = 0; i < MVPU_MPU_SEGMENT_NUMS; i++)
			pr_info("[MVPU][SEC] [MPU] drv mpu_reg[%3d] = 0x%08x\n",
						i, mvpu_req->mpu_seg[i]);
	}

END:

#ifdef MVPU_SEC_BUF_INFO_COPY
	if (sec_chk_addr != NULL) {
		kfree(sec_chk_addr);
		sec_chk_addr = NULL;
	}

	if (sec_buf_size != NULL) {
		kfree(sec_buf_size);
		sec_buf_size = NULL;
	}

	if (sec_buf_attr != NULL) {
		kfree(sec_buf_attr);
		sec_buf_attr = NULL;
	}
#endif

#ifdef MVPU_SEC_OLD_ADDR_INFO_COPY
	if (target_buf_old_base != NULL) {
		kfree(target_buf_old_base);
		target_buf_old_base = NULL;
	}

	if (target_buf_old_offset != NULL) {
		kfree(target_buf_old_offset);
		target_buf_old_offset = NULL;
	}
#endif

#ifdef MVPU_SEC_NEW_ADDR_INFO_COPY
	if (target_buf_new_base != NULL) {
		kfree(target_buf_new_base);
		target_buf_new_base = NULL;
	}

	if (target_buf_new_offset != NULL) {
		kfree(target_buf_new_offset);
		target_buf_new_offset = NULL;
	}
#endif

	if (target_buf_old_map != NULL) {
		kfree(target_buf_old_map);
		target_buf_old_map = NULL;
	}

	if (target_buf_new_map != NULL) {
		kfree(target_buf_new_map);
		target_buf_new_map = NULL;
	}

	if (rp_skip_buf != NULL) {
		kfree(rp_skip_buf);
		rp_skip_buf = NULL;
	}

	return ret;
}
#endif

static int apusys_mvpu_handler_lite(int type, void *hnd, struct apusys_device *dev)
{
	int ret = 0;

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

	if (mvpu_loglvl_drv >= APUSYS_MVPU_LOG_DBG)
		pr_info("[MVPU][Sec] APU CMD type %d\n", type);

	switch (type) {
	case APUSYS_CMD_VALIDATE:
#ifdef MVPU_SECURITY
		ret = mvpu_validation(hnd);
#else
		ret = 0;
#endif
		break;
	case APUSYS_CMD_SESSION_CREATE:
		mvpu_loglvl_drv = get_mvpu_log_lvl();
		set_sec_log_lvl(mvpu_loglvl_drv);
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
	default:
		pr_info("%s get wrong cmd type: %d\n", __func__, type);
		ret = -1;
		break;
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

	mvpu_loglvl_drv = 0;

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

	pr_info("%s +\n", __func__);

	return 0;
}

void mvpu_exit(void)
{
	pr_info("%s +\n", __func__);
	mvpu_ipi_deinit();
	mvpu_sysfs_exit();
	mvpu_drv_exit();
}
