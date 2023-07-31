/*
 * wdc_cld.c
 *
 *  Created on: 2022-3-16
 *      Author: sofine
 */
#include "mi_cld.h"
#include "../mi-ufshcd.h"
#include "../mi_ufs_common_ops.h"
#define QUERY_FLAG_IDN_CLD_ENABLE 0xFF
#define QUERY_ATTR_IDN_CLD_LEVEL 0xFF
#define QUERY_ATTR_IDN_DEFRAG 0xFE

enum WDC_DEFRAG {
	DEFRAG_OPERATION_ONGOING = 0x02,
	DEFRAG_OPERATION_COMPLETED = 0x03,
	DEFRAG_OPERATION_STOPPED = 0x04,
};

int wdc_get_frag_level(struct ufscld_dev *cld, int *frag_level)
{
	struct ufs_hba *hba = cld->hba;
	int ret = 0, attr = -1;
	ret = mi_ufs_query_attr(hba, UPIU_QUERY_OPCODE_READ_ATTR, (enum attr_idn)QUERY_ATTR_IDN_CLD_LEVEL, 0, 0, &attr);
	if (ret)
		return ret;

	attr = (attr & 0x80) >> 7;

	if (attr == 0) {
		*frag_level = CLD_LEV_CLEAN;
	} else if (attr == 1) {
		*frag_level = CLD_LEV_CRITICAL;
	} else {
		pr_info("wdc cld unknown level %d\n", attr);
		ret = -1;
		return ret;
	}
	return 0;
}

int wdc_cld_set_trigger(struct ufscld_dev *cld, u32 trigger)
{
	struct ufs_hba *hba = cld->hba;

	if (trigger)
		return mi_ufs_query_flag(hba, UPIU_QUERY_OPCODE_SET_FLAG, (enum flag_idn)QUERY_FLAG_IDN_CLD_ENABLE, 0, 0, NULL);
	else
		return mi_ufs_query_flag(hba, UPIU_QUERY_OPCODE_CLEAR_FLAG, (enum flag_idn)QUERY_FLAG_IDN_CLD_ENABLE, 0, 0, NULL);
}

int wdc_cld_get_trigger(struct ufscld_dev *cld, u32 *trigger)
{
	struct ufs_hba *hba = cld->hba;

	return mi_ufs_query_flag(hba, UPIU_QUERY_OPCODE_READ_FLAG, (enum flag_idn)QUERY_FLAG_IDN_CLD_ENABLE, 0, 0, (bool *)trigger);
}

static int wdc_cld_get_defragprogress(struct ufscld_dev *cld, int *defragprogress)
{
	struct ufs_hba *hba = cld->hba;
	int ret = 0;
	*defragprogress = 0;

	ret = mi_ufs_query_attr(hba, UPIU_QUERY_OPCODE_READ_ATTR, (enum attr_idn)QUERY_ATTR_IDN_DEFRAG, 0, 0, defragprogress);

	return 0;
}

int wdc_cld_operation_status(struct ufscld_dev *cld, int *op_status)
{
	int ret = 0;
	int defragprogress = 0;

	ret = wdc_cld_get_defragprogress(cld, &defragprogress);
	if (ret)
		ERR_MSG("get defragprogress failed ret=%d\n", ret);

	if (defragprogress == DEFRAG_OPERATION_COMPLETED || defragprogress == DEFRAG_OPERATION_STOPPED) {
		*op_status = CLD_STATUS_IDEL;
	} else if (defragprogress == DEFRAG_OPERATION_ONGOING){
		*op_status = CLD_STATUS_PROGRESSING;
	} else {
		*op_status = CLD_STATUS_NA;
	}

	return 0;
}

struct ufscld_ops wdc_cld_ops = {
		.cld_get_frag_level = wdc_get_frag_level,
		.cld_set_trigger = wdc_cld_set_trigger,
		.cld_get_trigger = wdc_cld_get_trigger,
		.cld_operation_status = wdc_cld_operation_status
};
