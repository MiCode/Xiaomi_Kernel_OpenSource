/*
 * micron_cld.c
 *
 *  Created on: 2022-3-16
 *      Author: sofine
 */
#include "mi_cld.h"
#include "../mi-ufshcd.h"
#include "../mi_ufs_common_ops.h"
#define QUERY_FLAG_IDN_CLD_ENABLE 0x13
#define QUERY_ATTR_IDN_CLD_LEVEL 0x31
#define QUERY_ATTR_IDN_DEFRAG 0x32

enum MICRON_DEFRAG {
	DEFRAG_IDEL = 0x0,
	DEFRAG_OPERATION_ONGOING = 0x01,
	DEFRAG_OPERATION_STOPPED = 0x02,
	DEFRAG_OPERATION_COMPLETED = 0x03,
};

int micron_get_frag_level(struct ufscld_dev *cld, int *frag_level)
{
	struct ufs_hba *hba = cld->hba;
	int ret = 0, attr = -1;
	ret = mi_ufs_query_attr(hba, UPIU_QUERY_OPCODE_READ_ATTR, (enum attr_idn)QUERY_ATTR_IDN_CLD_LEVEL, 0, 0, &attr);
	if (ret)
		return ret;
	if (attr == 0) {
		*frag_level = CLD_LEV_CLEAN;
	} else if (attr == 1) {
		*frag_level = CLD_LEV_CRITICAL;
	} else {
		pr_info("micron cld unknown level %d\n", attr);
		ret = -1;
		return ret;
	}
	return 0;
}

int micron_cld_set_trigger(struct ufscld_dev *cld, u32 trigger)
{
	struct ufs_hba *hba = cld->hba;

	if (trigger)
		return mi_ufs_query_flag(hba, UPIU_QUERY_OPCODE_SET_FLAG, (enum flag_idn)QUERY_FLAG_IDN_CLD_ENABLE, 0, 0, NULL);
	else
		return mi_ufs_query_flag(hba, UPIU_QUERY_OPCODE_CLEAR_FLAG, (enum flag_idn)QUERY_FLAG_IDN_CLD_ENABLE, 0, 0, NULL);
}

int micron_cld_get_trigger(struct ufscld_dev *cld, u32 *trigger)
{
	struct ufs_hba *hba = cld->hba;

	return mi_ufs_query_flag(hba, UPIU_QUERY_OPCODE_READ_FLAG, (enum flag_idn)QUERY_FLAG_IDN_CLD_ENABLE, 0, 0, (bool *)trigger);
}

static int micron_cld_get_defragprogress(struct ufscld_dev *cld, int *defragprogress)
{
	struct ufs_hba *hba = cld->hba;
	int ret = 0;
	*defragprogress = 0;

	ret = mi_ufs_query_attr(hba, UPIU_QUERY_OPCODE_READ_ATTR, (enum attr_idn)QUERY_ATTR_IDN_DEFRAG, 0, 0, defragprogress);

	return 0;
}

int micron_cld_operation_status(struct ufscld_dev *cld, int *op_status)
{
	int ret = 0;
	int defragprogress = 0;

	ret = micron_cld_get_defragprogress(cld, &defragprogress);
	if (ret)
		ERR_MSG("get defragprogress failed ret=%d\n", ret);

	if (defragprogress == DEFRAG_OPERATION_COMPLETED || defragprogress == DEFRAG_OPERATION_STOPPED
            || defragprogress == DEFRAG_IDEL) {
		*op_status = CLD_STATUS_IDEL;
	} else if (defragprogress == DEFRAG_OPERATION_ONGOING){
		*op_status = CLD_STATUS_PROGRESSING;
	} else {
		*op_status = CLD_STATUS_NA;
	}

	return 0;
}

struct ufscld_ops micron_cld_ops = {
		.cld_get_frag_level = micron_get_frag_level,
		.cld_set_trigger = micron_cld_set_trigger,
		.cld_get_trigger = micron_cld_get_trigger,
		.cld_operation_status = micron_cld_operation_status
};
