#ifndef __SDIOAT_LIB_H__
#define __SDIOAT_LIB_H__


#ifdef MTK_TEST_LIB
#define AUTOEXT
#else 
#define AUTOEXT  extern
#endif


#define SEND_ERR_RETRY	10000
#define SEND_ERR_TIMEOUT	1000


typedef struct _attest_option{
    bool    show_dl_content;
    bool    exam_dl_content;

    bool    auto_receive_pkt;
}attest_option_t;


/*the information to re-assemble the fragment packets*/
typedef struct _recv_fragment_ctrl{
	unsigned int expected_xfer_len;
	unsigned int xfered_len;
	unsigned int xfered_pkt_idx;
	unsigned int max_frag_unit_sz;
	AT_PKT_HEADER pkt_head;
	unsigned char next_expected_char;
}recv_fragment_ctrl_t;

typedef enum _tx_basic_tst_case {
	ATCASE_UL_BASIC_SEND,
	ATCASE_UL_BASIC_SEND_RAND,
	ATCASE_UL_BASIC_MANY_QUE,
	ATCASE_DL_BASIC_RECV,
}tx_basic_tst_case_e;


typedef enum _lb_data_pattern {
	ATCASE_LB_DATA_5A,
	ATCASE_LB_DATA_A5,
	ATCASE_LB_DATA_INC,
	/*
	 *	@brief	device fragment original auto pattern pakcet into several packets
	*/
	ATCASE_LB_DATA_FRAGMENT,
	ATCASE_LB_DATA_AUTO
}lb_data_pattern_e;

typedef enum _rgpd_allowlen_tst_case {
	ATCASE_2RBD_ALLOW_LEN_CASE1,
	ATCASE_2RBD_ALLOW_LEN_CASE2,
	ATCASE_3RBD_ALLOW_LEN_CASE1,
	ATCASE_3RBD_ALLOW_LEN_CASE2,
	ATCASE_2RBD_ALLOW_LEN_STRESS,
	ATCASE_3RBD_ALLOW_LEN_SLT,
}rgpd_allowlen_tst_case_e;



struct timespec time_diff(struct timespec start , struct timespec end);


void f_calc_cs_byte(void *startingAddr_p, unsigned int lengthToCalculate, unsigned char *checksum_p);

int sdio_send_pkt(int ulq_no,int data_length, unsigned char ul_que, unsigned char dl_que);

int sdio_dl_npkt(athif_dl_tgpd_cfg_t *p_dl_cfg);

int f_compare_recv_pkt(struct sk_buff *dl_skb, unsigned int que_num);

int f_wait_recv_pkt_cnt(unsigned int expect_num , unsigned int timeout_ms);

int f_ul_rgpd_allow_len_tst(unsigned int txq_no ,athif_ul_rgpd_format_t *p_rgpd_format, unsigned int pkt_len_start, unsigned int pkt_len_end);

int sdio_dl_npkt_sp(athif_dl_tgpd_cfg_t *p_dl_cfg);

int f_small_pkt_lb(lb_data_pattern_e pattern);

int f_tx_rx_ep0_perf_lb(unsigned int loop, unsigned int offset, unsigned int pkt_md,
	unsigned int q_md, unsigned int pkt_len, perf_tst_case_e lb_md);

int f_rx_perf_tst(unsigned int loop, unsigned int offset, unsigned int pkt_num,
	unsigned int q_md, unsigned int pkt_len, perf_tst_case_e lb_md);

int f_ul_cs_err_tst(unsigned int cs_len, unsigned int is_bd);

int f_dl_cs_err_tst(unsigned int cs_len, unsigned int is_bd);

int tx_perf_hw_limit(unsigned int loop, unsigned int offset, unsigned int pkt_md,
	unsigned int q_md, unsigned int pkt_len, perf_tst_case_e lb_md);

int f_brom_pkt_lb(lb_data_pattern_e pattern, unsigned int min_size, unsigned int max_size);



/* ************************************ */
/*   Funciton below is not complete porting yet  */
/* ************************************ */
int sdio_dl_n_rand_pkt(unsigned int pkt_num , unsigned int que_num);
int sdio_dl_n_rand_stress(unsigned int pkt_num, unsigned int que_num);



#endif
