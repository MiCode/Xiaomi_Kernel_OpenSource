#ifndef __MTK_INTF_MI_H
#define __MTK_INTF_MI_H


/*
#include "mtk_charger_intf.h"
#include "mtk_pe40.h"
#include "mtk_pdc.h"
*/
#define ADAPTER_CAP_MAX_NR_BQ 10
#define ADAPTER_RET_VERIFYING 6

struct pps_cap_bq {
	uint8_t selected_cap_idx;
	uint8_t nr;
	uint8_t pdp;
	uint8_t pwr_limit[ADAPTER_CAP_MAX_NR_BQ];
	int max_mv[ADAPTER_CAP_MAX_NR_BQ];
	int min_mv[ADAPTER_CAP_MAX_NR_BQ];
	int ma[ADAPTER_CAP_MAX_NR_BQ];
	int maxwatt[ADAPTER_CAP_MAX_NR_BQ];
	int minwatt[ADAPTER_CAP_MAX_NR_BQ];
	uint8_t type[ADAPTER_CAP_MAX_NR_BQ];
	int info[ADAPTER_CAP_MAX_NR_BQ];
};

int charger_is_chip_enabled(bool *en);
int charger_enable_chip(bool en);
int charger_is_enabled(bool *en);
int charger_enable_chip(bool en);
int charger_get_mivr_state(bool *in_loop);
int charger_get_mivr(u32 *uV);
int charger_set_mivr(u32 uV);
int charger_get_input_current(u32 *uA);
int charger_set_input_current(u32 uA);
int charger_set_charging_current(u32 uA);

int charger_get_ibus(u32 *ibus);
int charger_get_ibat(u32 *ibat);
int charger_set_constant_voltage(u32 uV);
int charger_enable_termination(bool en);
int charger_enable_powerpath(bool en);
int charger_dump_registers(void);
/*
int adapter_set_cap(int mV, int mA);
int adapter_set_cap_start(int mV, int mA);
int adapter_set_cap_end(int mV, int mA);
int adapter_set_cap_bq(int mV, int mA);
int adapter_set_cap_start_bq(int mV, int mA);
int adapter_set_cap_end_bq(int mV, int mA);
int adapter_get_output(int *mV, int *mA);

int adapter_get_status(struct ta_status *sta);
int adapter_is_support_pd_pps(void);

int adapter_get_cap(struct pd_cap *cap);
int adapter_is_support_pd(void);

int set_charger_manager(struct charger_manager *info);
int enable_vbus_ovp(bool en);
*/
int adapter_get_cap_bq(struct pps_cap_bq *cap);
int adapter_get_pps_cap_bq(struct pps_cap_bq *cap);
int adapter_set_cap_bq(int mV, int mA);
int adapter_set_cap_start_bq(int mV, int mA);
int adapter_set_cap_end_bq(int mV, int mA);
int adapter_is_support_pd_pps(void);
#endif /* __MTK_INTF_MI_H */
