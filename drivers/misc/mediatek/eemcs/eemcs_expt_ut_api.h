#ifndef __EEMCS_EXPT_UT_API_H__
#define __EEMCS_EXPT_UT_API_H__

#ifdef _EEMCS_EXCEPTION_UT

/* UL APIs */
int exception_ut_UL_write_skb_to_swq(MTLTE_DF_TX_QUEUE_TYPE qno , struct sk_buff *skb);
int exception_ut_UL_swq_space(MTLTE_DF_TX_QUEUE_TYPE qno);
int exception_ut_register_swint_callback(MTLTE_DF_TO_DEV_CALLBACK func_ptr);
int exception_ut_unregister_swint_callback(void);
int exception_ut_UL_pkt_in_swq(MTLTE_DF_TX_QUEUE_TYPE qno);
struct sk_buff * exception_ut_UL_read_skb_from_swq(MTLTE_DF_TX_QUEUE_TYPE qno);
/* DL APIs */
struct sk_buff * exception_ut_DL_read_skb_from_swq(MTLTE_DF_RX_QUEUE_TYPE qno);
int exception_ut_DL_pkt_in_swq(MTLTE_DF_RX_QUEUE_TYPE qno);
int exception_ut_DL_pkt_handle_complete(MTLTE_DF_RX_QUEUE_TYPE qno);
int exception_ut_register_rx_callback(MTLTE_DF_RX_QUEUE_TYPE qno, MTLTE_DF_TO_DEV_CALLBACK func_ptr , unsigned int private_data);
void exception_ut_unregister_rx_callback(MTLTE_DF_RX_QUEUE_TYPE qno);
/* Exception APIs */
int exception_ut_exception_init(KAL_UINT32 nonstop_q, KAL_UINT32 except_q);
int exception_ut_register_expt_callback(EEMCS_CCCI_EX_IND func_ptr);

/* UT mode */
#define hif_ul_write_swq              exception_ut_UL_write_skb_to_swq
#define hif_ul_swq_space              exception_ut_UL_swq_space
#define hif_reg_swint_cb              exception_ut_register_swint_callback
#define hif_unreg_swint_cb            exception_ut_unregister_swint_callback
#define hif_except_init               exception_ut_exception_init
#define hif_reg_expt_cb               exception_ut_register_expt_callback
#define hif_dl_pkt_in_swq             exception_ut_DL_pkt_in_swq
#define hif_dl_read_swq               exception_ut_DL_read_skb_from_swq
#define hif_dl_pkt_handle_complete    exception_ut_DL_pkt_handle_complete
#define hif_ul_pkt_in_swq             exception_ut_UL_pkt_in_swq
#define hif_ul_read_swq               exception_ut_UL_read_skb_from_swq
#define hif_reg_rx_cb                 exception_ut_register_rx_callback
#define hif_unreg_rx_cb               exception_ut_unregister_rx_callback

void eemcs_expt_ut_trigger(unsigned int);

#endif // _EEMCS_EXCEPTION_UT

#endif // __EEMCS_EXPT_UT_API_H__