
#include <linux/skbuff.h>
#include <linux/time.h>
#include <linux/jiffies.h>




void f_calc_cs_byte(void *startingAddr_p, unsigned int lengthToCalculate, unsigned char *checksum_p)
{
	unsigned int i;
	unsigned char *cp;
	
	*checksum_p = 0;
	cp = (unsigned char *) startingAddr_p;
	
	for (i=0; i<lengthToCalculate; i++, cp++) {
		*checksum_p += *cp;
	}
}


int sdio_send_pkt(int ulq_no,int data_length, unsigned char ul_que, unsigned char dl_que)
{

	int ret, i;
    struct sk_buff *skb = NULL; 
	PAT_PKT_HEADER pAtHeader = NULL;
	unsigned char rand_seed = 0, bak_seed = 0;
	unsigned char *buf;
    unsigned char cksm = 0;
    unsigned int timeout =0;

    ret = RET_SUCCESS;
    buf = buff_kmemory_ulpkt_data;

    while(mtlte_df_UL_swq_space(ulq_no)==0){
        KAL_SLEEP_USEC(1) ;
        timeout++;
        if(timeout > 10000){
            KAL_DBGPRINT(KAL, DBG_ERROR,("%s : send pkt timeout becaucse no que space!\n", KAL_FUNC_NAME));
		    return RET_FAIL ;	
        }
    }
        
    //if (mtlte_df_UL_swq_space(ulq_no)==0){
    //    KAL_DBGPRINT(KAL, DBG_WARN,("%s : UL Queue %d has no space.\n", KAL_FUNC_NAME, ulq_no));
	//	return RET_FAIL;		
	//}
	
	if ((skb = dev_alloc_skb(data_length))==NULL){
		KAL_DBGPRINT(KAL, DBG_ERROR,("%s : allocate skb failed\n", KAL_FUNC_NAME));
		return RET_FAIL ;	
	}


	switch (send_pattern) {
		case ATCASE_LB_DATA_5A :
			memset(buf, 0x5a , data_length);			
			break;
		case ATCASE_LB_DATA_A5:
			memset(buf, 0xa5 , data_length);			
			break;
		case ATCASE_LB_DATA_INC:
			get_random_bytes(&rand_seed , 1);
			for (i = 0 ; i < data_length ; i ++) {
				buf[i] = rand_seed++;
			}
			break;

		case ATCASE_LB_DATA_AUTO :
        default:
			// fill packet payload
		 	pAtHeader = (PAT_PKT_HEADER)buf;
			memset(pAtHeader, 0 , sizeof(AT_PKT_HEADER));

			get_random_bytes(&rand_seed , 1);
			bak_seed = rand_seed;
            KAL_DBGPRINT(KAL, DBG_TRACE,("rand_seed = %d..\n", rand_seed));
			pAtHeader->RndSeed = rand_seed;
			pAtHeader->SrcQID = ul_que & 0xf;  
			pAtHeader->DstQID = dl_que & 0xf;
			pAtHeader->SeqNo = 0;
			if (data_length < sizeof(AT_PKT_HEADER)) {
				data_length = sizeof(AT_PKT_HEADER); 
			}
			pAtHeader->PktLen = data_length;
			
			f_calc_cs_byte(pAtHeader, sizeof(AT_PKT_HEADER), &cksm);
			pAtHeader->Checksum = ~cksm;

			 // fill payload, don't fill memory lenght larger than URB buffer
			for (i = 0 ; i < (data_length - sizeof(AT_PKT_HEADER)) ; i ++) {
				pAtHeader->Data[i] = rand_seed++;
			}
			break;

	}

    /* fill the data content */
	memcpy(skb_put(skb, data_length), buf, data_length);
	
	/* always reply we have free space or add ccci_write_space_check */
	ret = mtlte_df_UL_write_skb_to_swq(ulq_no, skb);
	
	if(ret != KAL_SUCCESS){
        return RET_FAIL;
	}
	return ret;
}


int sdio_dl_npkt(athif_dl_tgpd_cfg_t *p_dl_cfg)
{
	int ret = RET_SUCCESS;
	athif_cmd_t cmd;
	unsigned int recv_cnt = 0, timeout = 0, i=0;
    unsigned int old_recv_cnt = 0;
    //athif_dl_tgpd_cfg_t *struct_dl_cfg;

    struct sk_buff *result_ptr = NULL;
    attest_option_t back_test_option;
    
    back_test_option = sdio_test_option;
    sdio_test_option.auto_receive_pkt = false;
    
	memcpy(cmd.buf , p_dl_cfg , sizeof(athif_dl_tgpd_cfg_t));
	//struct_dl_cfg = (athif_dl_tgpd_cfg_t *)cmd.buf;
    //struct_dl_cfg->q_num = p_dl_cfg->q_num;
    //struct_dl_cfg->gpd_num = p_dl_cfg->gpd_num;
    //struct_dl_cfg->tgpd_format = p_dl_cfg->tgpd_format;
    
	cmd.cmd = ATHIF_CMD_DL_SEND_N;
	cmd.len = sizeof(athif_dl_tgpd_cfg_t);
	
    mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    recv_cnt = 0;

#if 1	
	/*wait receiving packets*/
	while(recv_cnt != p_dl_cfg->gpd_num) {

        result_ptr = mtlte_df_DL_read_skb_from_swq(p_dl_cfg->q_num);
        if( result_ptr != NULL ) {
            recv_cnt++;
            mtlte_df_DL_pkt_handle_complete(p_dl_cfg->q_num);

            KAL_DBGPRINT(KAL, DBG_TRACE,("[INFO] : receive pkt from RxQ %d .\n", p_dl_cfg->q_num));

            if(true == sdio_test_option.show_dl_content){
                KAL_DBGPRINT(KAL, DBG_ERROR,("Content : "));
                for(i=0; i<result_ptr->len; i++){
                    KAL_DBGPRINT(KAL, DBG_ERROR,("%x ", *(result_ptr->data+i) ));
                }
                KAL_DBGPRINT(KAL, DBG_ERROR,(" \n"));
            }
            
            if(true == sdio_test_option.exam_dl_content){
                if ( RET_FAIL == f_compare_recv_pkt(result_ptr, p_dl_cfg->q_num) ){
                        KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] data compare error at que=%d  gpd_num=%d !!! \n", \
                            KAL_FUNC_NAME, p_dl_cfg->q_num, p_dl_cfg->gpd_num)) ;
                        KAL_DBGPRINT(KAL, DBG_ERROR,("           buf_len=%d, ext_len=%d, bd_num=%d !!! \n", \
                             p_dl_cfg->tgpd_format.tgpd_buf_len, p_dl_cfg->tgpd_format.tgpd_ext_len, p_dl_cfg->tgpd_format.tbd_num)) ;
                        return RET_FAIL;
                }
            }
        }

        dev_kfree_skb(result_ptr); 

       KAL_SLEEP_USEC(1);
		if(recv_cnt != old_recv_cnt){ timeout ==0; }
        else{ timeout++; }
        
		if (timeout > 10000) { // 1sec no any packet 
		    KAL_DBGPRINT(KAL, DBG_ERROR,("Timeout at receiving packet, received packet now = %d \n", recv_cnt ));
			ret = RET_FAIL;
			break;
		}
        old_recv_cnt = recv_cnt;
	}

    if( RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT)){
        KAL_DBGPRINT(KAL, DBG_ERROR,("[ERR] Device response test fail!!! \n"));
        return RET_FAIL;
    }

#endif
    sdio_test_option.auto_receive_pkt = back_test_option.auto_receive_pkt;
	return ret;
}

int sdio_dl_n_rand_pkt(unsigned int pkt_num , unsigned int que_num)
{
	int ret = RET_SUCCESS;
	athif_cmd_t cmd;
	//athif_status_t status;
	unsigned int recv_cnt = 0, timeout = 0, old_pkt_cnt = 0;
	athif_basic_set_t tst_cfg;

	tst_cfg.gpd_num = pkt_num;
	tst_cfg.q_num = que_num;
	memcpy(cmd.buf , &tst_cfg, sizeof(athif_basic_set_t));
	cmd.cmd = ATHIF_CMD_DL_SEND_RAND_N;
	cmd.len = sizeof(athif_basic_set_t);
	recv_cnt = tst_cfg.gpd_num;

    sdio_test_option.auto_receive_pkt = true;
    sdio_test_option.exam_dl_content = true;
    recv_total_pkt_cnt = 0;

#if 1
    mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, sizeof(athif_basic_set_t), dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);



	/*wait receiving packets*/
	while (recv_total_pkt_cnt < recv_cnt) {
		KAL_SLEEP_MSEC(1);
        if(recv_total_pkt_cnt != old_pkt_cnt){ timeout == 0; }
        else{ timeout++; }
        
		if (recv_th_rslt == RET_FAIL) {
            KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] receive pkt fail at %d pkt !!! \n",KAL_FUNC_NAME, recv_total_pkt_cnt)) ;
            ret = RET_FAIL;
			break;
		}
		if (timeout > 1000) { //1sec no any packet
			ret = RET_FAIL;
            KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] total receive pkt =%d But expect pkt =%d !!! \n",KAL_FUNC_NAME, recv_total_pkt_cnt, recv_cnt)) ;
			break;
		}
        old_pkt_cnt = recv_total_pkt_cnt;
	}

	/*some command shout not get cmd_ack*/
	if( RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT)){
        KAL_DBGPRINT(KAL, DBG_ERROR,("[ERR] Device response test fail!!! \n"));
        return RET_FAIL;
    }	
#endif 

    sdio_test_option.auto_receive_pkt = false;
    sdio_test_option.exam_dl_content = false;

	return ret;
}

int sdio_dl_n_rand_stress(unsigned int pkt_num, unsigned int que_num)
{
	int ret = RET_SUCCESS;
	athif_cmd_t cmd;
	//athif_status_t status;
	unsigned int recv_cnt = 0, timeout = 0;
    unsigned int old_pkt_cnt =0;

	memcpy(cmd.buf , &pkt_num, sizeof(unsigned int));
	/*ep_md = n, test ep 1~n*/
	cmd.buf[4] = que_num;
	cmd.cmd = ATHIF_CMD_DL_SEND_RAND_STRESS;
    // TODO: cmd.buf[5]~[8] is used by ming, please check it if this test case failed.
	cmd.len = 1 + sizeof(unsigned int);
	recv_cnt = pkt_num;

    sdio_test_option.auto_receive_pkt = true;
    sdio_test_option.exam_dl_content = true;
    //sdio_test_option.show_dl_content = true;
	recv_total_pkt_cnt = 0;
    
#if 1
	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, (1 + sizeof(unsigned int)), dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);


	/*wait receiving packets*/
	while (recv_total_pkt_cnt < recv_cnt) {
		KAL_SLEEP_MSEC(10);
		if(recv_total_pkt_cnt != old_pkt_cnt){ timeout == 0; }
        else{ timeout++; }
        
		if (recv_th_rslt == RET_FAIL) {
			ret = RET_FAIL;
            KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] receive pkt fail at %d pkt !!! \n",KAL_FUNC_NAME, recv_total_pkt_cnt)) ;
			break;
		}
		if (timeout > 5000) { //5sec
			ret = RET_FAIL;
            KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] total receive pkt =%d But expect pkt =%d !!! \n",KAL_FUNC_NAME, recv_total_pkt_cnt, recv_cnt)) ;
			break;
		}
        old_pkt_cnt = recv_total_pkt_cnt;
	}

	/*some command shout not get cmd_ack*/
	if( RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT)){
        KAL_DBGPRINT(KAL, DBG_ERROR,("[ERR] Device response test fail!!! \n"));
        return RET_FAIL;
    }	
#endif

    sdio_test_option.auto_receive_pkt = false;
    sdio_test_option.exam_dl_content = false;
    //sdio_test_option.show_dl_content = false;

	return ret;
}


int sdio_dl_npkt_sp(athif_dl_tgpd_cfg_t *p_dl_cfg)
{
	int ret = RET_SUCCESS;
	athif_cmd_t cmd;
	unsigned int recv_cnt = 0, timeout = 0, i=0;
    unsigned int old_recv_cnt = 0;
    //athif_dl_tgpd_cfg_t *struct_dl_cfg;

    struct sk_buff *result_ptr = NULL;

	memcpy(cmd.buf , p_dl_cfg , sizeof(athif_dl_tgpd_cfg_t));
	//struct_dl_cfg = (athif_dl_tgpd_cfg_t *)cmd.buf;
    //struct_dl_cfg->q_num = p_dl_cfg->q_num;
    //struct_dl_cfg->gpd_num = p_dl_cfg->gpd_num;
    //struct_dl_cfg->tgpd_format = p_dl_cfg->tgpd_format;
    
	cmd.cmd = SDIO_AT_DL_SEND_SP;
	cmd.len = sizeof(athif_dl_tgpd_cfg_t);
	
    mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    recv_cnt = 0;

#if 1	
	/*wait receiving packets*/
	while(recv_cnt != p_dl_cfg->gpd_num) {

        result_ptr = mtlte_df_DL_read_skb_from_swq(p_dl_cfg->q_num);
        if( result_ptr != NULL ) {
            recv_cnt++;
            mtlte_df_DL_pkt_handle_complete(p_dl_cfg->q_num);

            KAL_DBGPRINT(KAL, DBG_TRACE,("[INFO] : receive pkt from RxQ %d .\n", p_dl_cfg->q_num));

            if(true == sdio_test_option.show_dl_content){
                KAL_DBGPRINT(KAL, DBG_ERROR,("Content : "));
                for(i=0; i<result_ptr->len; i++){
                    KAL_DBGPRINT(KAL, DBG_ERROR,("%x ", *(result_ptr->data+i) ));
                }
                KAL_DBGPRINT(KAL, DBG_ERROR,(" \n"));
            }
            
            if(true == sdio_test_option.exam_dl_content){
                if ( RET_FAIL == f_compare_recv_pkt(result_ptr, p_dl_cfg->q_num) ){
                        KAL_DBGPRINT(KAL, DBG_ERROR,("[%s]:[ERR] data compare error at que=%d  expect gpd_num=%d, now is %d!!! \n", \
                            KAL_FUNC_NAME, p_dl_cfg->q_num, p_dl_cfg->gpd_num, recv_cnt)) ;
                        KAL_DBGPRINT(KAL, DBG_ERROR,("           buf_len=%d, ext_len=%d, bd_num=%d !!! \n", \
                             p_dl_cfg->tgpd_format.tgpd_buf_len, p_dl_cfg->tgpd_format.tgpd_ext_len, p_dl_cfg->tgpd_format.tbd_num)) ;
                        return RET_FAIL;
                }
            }
        }

        dev_kfree_skb(result_ptr); 

        KAL_SLEEP_MSEC(1);
		if(recv_cnt != old_recv_cnt){ timeout ==0; }
        else{ timeout++; }
        
		if (timeout > 1000) { // 1sec no any packet 
		    KAL_DBGPRINT(KAL, DBG_ERROR,("Timeout at receiving packet, received packet now = %d \n", recv_cnt ));
			ret = RET_FAIL;
			break;
		}
        old_recv_cnt = recv_cnt;
	}

    if( RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT)){
        KAL_DBGPRINT(KAL, DBG_ERROR,("[ERR] Device response test fail!!! \n"));
        return RET_FAIL;
    }

#endif

	return ret;
}


int f_compare_auto_pattern(struct sk_buff *dl_skb, unsigned int que_num)
{
	int ret = RET_SUCCESS;
	unsigned int pkt_len = 0, idx = 0;
	PAT_PKT_HEADER pAtHeader;
	unsigned char cksm = 0 ,*buf , data_char = 0;


	pAtHeader = (PAT_PKT_HEADER)(dl_skb->data);
	buf = dl_skb->data;
	pkt_len = dl_skb->len;

	if (pkt_len == 0) {
		KAL_DBGPRINT(KAL, DBG_ERROR,("[ERR] Zero Pkt received que_num=%d , pkt_len=%d!!\n", que_num , pkt_len));
	}

	if (pkt_len < sizeof(AT_PKT_HEADER)) {
		KAL_DBGPRINT(KAL, DBG_ERROR,("[ERR] que_num=%d , pkt_len=%d length less than header!!\n", que_num , pkt_len));		
		return RET_SUCCESS;
	}

	if (pkt_len != pAtHeader->PktLen) {
		KAL_DBGPRINT(KAL, DBG_ERROR,("[ERR] dl_skb=%p ,que_num=%d , pkt_len=%d bytes, expect %d bytes!!\n",dl_skb ,que_num , pkt_len,pAtHeader->PktLen));		
		return RET_FAIL;
	}

	/*Packet count here TODO*/
	
	/*check loopback que-to-que mapping here TODO*/

	/*check loopback packet sequence here TODO*/

	/*compare payload header check sum*/
	f_calc_cs_byte(buf, sizeof(AT_PKT_HEADER), &cksm);
	if (cksm != 0xff) {
		KAL_DBGPRINT(KAL, DBG_ERROR,("[ERR] que_num=%d , pkt_len=%d checksum error!!\n", que_num , pkt_len));
		return RET_FAIL;
	}

	data_char = pAtHeader->RndSeed;
	for (idx = sizeof(AT_PKT_HEADER) ; idx < pkt_len ; idx ++, data_char ++) {
		if (buf[idx] != data_char) {
			KAL_DBGPRINT(KAL, DBG_ERROR,("[ERR] que_num=%d , pkt_len=%d data mismatch !!\n" , que_num , pkt_len));			
			KAL_DBGPRINT(KAL, DBG_ERROR,("[ERR] (pkt_len=%d, pos=%d , expect=0x%02x, read=0x%x)!!\n" ,pkt_len ,idx ,data_char ,buf[idx]));			
			return RET_FAIL;
		}
	}	

	if (ret == RET_SUCCESS) {
		KAL_DBGPRINT(KAL, DBG_CRIT,("[WARN] que_num=%d , pkt_len=%d compare success!!\n", que_num , pkt_len));
	}

	return ret;
}

#define BPS_GPD_ADDR_TAG 0
int f_compare_fragment_pattern(struct sk_buff *dl_skb, unsigned int que_num)
{
	int ret = RET_SUCCESS;
	unsigned int pkt_len = 0, idx = 0 , que_idx = 0;
	PAT_PKT_HEADER pAtHeader;
	unsigned char cksm = 0 ,*buf , data_char = 0;
	recv_fragment_ctrl_t *p_frag_ctrl = NULL;
	//struct usb_endpoint_descriptor *p_ep_desc = NULL;
	bool first_gpd_of_pkt = false;


	que_idx = que_num;
	p_frag_ctrl = &recv_frag_ctrl[que_idx];

	buf = dl_skb->data;
	pkt_len = dl_skb->len;

	KAL_DBGPRINT(KAL, DBG_TRACE,("[TRACE][%s:%d] recv one packet que_num=%d, len=%d!!\n",__FUNCTION__,__LINE__,que_num,pkt_len));


	/*start packet of this fragment transfer, assume 1st packet contain whole packet header*/
    if (p_frag_ctrl->xfered_len == 0) {		
	    pAtHeader = (PAT_PKT_HEADER)dl_skb->data;
	    memcpy(&p_frag_ctrl->pkt_head, pAtHeader , sizeof(AT_PKT_HEADER));		
	    p_frag_ctrl->expected_xfer_len = pAtHeader->PktLen;
	    p_frag_ctrl->xfered_len = dl_skb->len;
	    p_frag_ctrl->xfered_pkt_idx = 1;
	    /*compare payload header check sum*/
	    f_calc_cs_byte(buf, sizeof(AT_PKT_HEADER), &cksm);
	    if (cksm != 0xff) {
		    KAL_DBGPRINT(KAL, DBG_WARN,("[WARN] que_num=%d , pkt_len=%d checksum error!!\n", que_num , pkt_len));
		    /*set xfered_len as 0 to start another auto-test transfer*/
		    p_frag_ctrl->xfered_len = 0;
		    return RET_FAIL;
	    }
	    KAL_DBGPRINT(KAL, DBG_TRACE,("[TRACE][%s:%d] auto-test transfer start!!\n",__FUNCTION__,__LINE__));
		KAL_DBGPRINT(KAL, DBG_TRACE,("[TRACE][%s:%d] expect_len=%d, cur_len=%d, cur_pkt_cnt=%d!!\n",__FUNCTION__,__LINE__
							,p_frag_ctrl->expected_xfer_len,p_frag_ctrl->xfered_len, p_frag_ctrl->xfered_pkt_idx));

		data_char = pAtHeader->RndSeed;
		for (idx = sizeof(AT_PKT_HEADER) ; idx < pkt_len ; idx ++, data_char ++) {
#if BPS_GPD_ADDR_TAG
			if (idx < (sizeof(AT_PKT_HEADER) + 4)) {
				continue;
			}
#endif		
			if (buf[idx] != data_char) {
				KAL_DBGPRINT(KAL, DBG_WARN,("[WARN] que_num=%d , pkt_len=%d data mismatch !!\n" , que_num , pkt_len));			
				KAL_DBGPRINT(KAL, DBG_WARN,("[WARN] (pkt_len=%d, pos=%d , expect=0x%02x, read=0x%x)!!\n" ,pkt_len ,idx ,data_char ,buf[idx]));			
				/*set xfered_len as 0 to start another auto-test transfer*/
				p_frag_ctrl->xfered_len = 0;
				return RET_FAIL;
			}
		}	
		/*store next fragment packet start pattern*/
		p_frag_ctrl->next_expected_char = data_char;
        first_gpd_of_pkt = true;
        
	} else {
		KAL_DBGPRINT(KAL, DBG_TRACE,("[TRACE][%s:%d] auto-test in progress!!\n",__FUNCTION__,__LINE__));
		KAL_DBGPRINT(KAL, DBG_TRACE,("[TRACE][%s:%d] expect_len=%d, cur_len=%d, cur_pkt_cnt=%d!!\n",__FUNCTION__,__LINE__
							,p_frag_ctrl->expected_xfer_len,p_frag_ctrl->xfered_len, p_frag_ctrl->xfered_pkt_idx));
		p_frag_ctrl->xfered_len += dl_skb->len;
		p_frag_ctrl->xfered_pkt_idx ++;

		data_char = p_frag_ctrl->next_expected_char;
		for (idx = 0 ; idx < pkt_len ; idx ++, data_char ++) {
			if (buf[idx] != data_char) {
				KAL_DBGPRINT(KAL, DBG_WARN,("[WARN] que_num=%d , pkt_len=%d data mismatch !!\n" , que_num , pkt_len));			
				KAL_DBGPRINT(KAL, DBG_WARN,("[WARN] (pkt_len=%d, pos=%d , expect=0x%02x, read=0x%x)!!\n" ,pkt_len ,idx ,data_char ,buf[idx]));			
				/*set xfered_len as 0 to start another auto-test transfer*/
				p_frag_ctrl->xfered_len = 0;
				return RET_FAIL;
			}
		}	
		/*store next fragment packet start pattern*/
		p_frag_ctrl->next_expected_char = data_char;
	}


	if (pkt_len == 0) {
		KAL_DBGPRINT(KAL, DBG_ERROR,("[ERR][%s:%d] Zero Pkt received que_num=%d , pkt_len=%d!!\n",__FUNCTION__,__LINE__, que_num , pkt_len));
        return RET_FAIL;
	}

	/*means latest fragment packet of this auto-test transfer*/
	if (p_frag_ctrl->xfered_len == p_frag_ctrl->expected_xfer_len) {
		KAL_DBGPRINT(KAL, DBG_TRACE,( "[TRACE][%s:%d] auto-test end!!\n",__FUNCTION__,__LINE__));
		KAL_DBGPRINT(KAL, DBG_TRACE,("[TRACE][%s:%d] expect_len=%d, cur_len=%d, cur_pkt_cnt=%d!!\n",__FUNCTION__,__LINE__
							,p_frag_ctrl->expected_xfer_len,p_frag_ctrl->xfered_len, p_frag_ctrl->xfered_pkt_idx));

		if (p_frag_ctrl->xfered_len < sizeof(AT_PKT_HEADER)) {
			KAL_DBGPRINT(KAL, DBG_WARN,("[WARN][%s:%d] que_num=%d , pkt_len=%d length less than header!!\n",__FUNCTION__,__LINE__, que_num , pkt_len));		
			ret = RET_SUCCESS;
		}
		/*set xfered_len as 0 to start another auto-test transfer*/
		p_frag_ctrl->xfered_len = 0;
		/*count xfer count after aggregation*/
		recv_total_pkt_cnt_agg ++;
        
	} 
    /* else if(true == first_gpd_of_pkt){

        // TODO: Remove this [else if] when Tx header can be auto removed by HW
        if (dl_skb->len != (recv_frag_ctrl[que_idx].max_frag_unit_sz - 4) ) {
			KAL_DBGPRINT(KAL, DBG_ERROR,("[ERR][%s:%d] fragment length error , recv_len=%d, expected allow_len=(%d-4) (Because Tx Header)!!\n",
										__FUNCTION__,__LINE__,dl_skb->len,recv_frag_ctrl[que_idx].max_frag_unit_sz));
			ret = RET_FAIL;
		}
        first_gpd_of_pkt = false;
        
	} */ 
    else { /*if not the end of the fragment transfer, the usb transfer size should be == rgpd/bd allow length*/
		if (dl_skb->len != recv_frag_ctrl[que_idx].max_frag_unit_sz) {
			KAL_DBGPRINT(KAL, DBG_ERROR,("[ERR][%s:%d] fragment length error , recv_len=%d, expected allow_len=%d!!\n",
										__FUNCTION__,__LINE__,dl_skb->len,recv_frag_ctrl[que_idx].max_frag_unit_sz));
			ret = RET_FAIL;
		}
	}

	if (ret == RET_SUCCESS) {
		KAL_DBGPRINT(KAL, DBG_WARN,("[WARN] que_num=%d , pkt_len=%d compare success!!\n", que_num , pkt_len));
	}

	return ret;
}

int f_compare_5a_pattern(struct sk_buff *dl_skb, unsigned int que_num)
{
	int ret = RET_SUCCESS;
	unsigned int pkt_len = 0, idx = 0;
	unsigned char *buf;


	buf = dl_skb->data;
	pkt_len = dl_skb->len;

	if (pkt_len == 0) {
		KAL_DBGPRINT(KAL, DBG_WARN,("[WARN] Zero Pkt received que_num=%d , pkt_len=%d!!\n", que_num , pkt_len));
	}

	for (idx = 0 ; idx < pkt_len ; idx ++) {
		if (buf[idx] != 0x5a) {
			KAL_DBGPRINT(KAL, DBG_WARN,("[WARN] que_num=%d , pkt_len=%d data mismatch !!\n" , que_num , pkt_len));			
			KAL_DBGPRINT(KAL, DBG_WARN,("[WARN] (pkt_len=%d, pos=%d , expect=0x5a, read=0x%x)!!\n" ,pkt_len ,idx ,buf[idx]));			
			return RET_FAIL;
		}
	}	

	return ret;
}

int f_compare_a5_pattern(struct sk_buff *dl_skb, unsigned int que_num)
{
	int ret = RET_SUCCESS;
	unsigned int pkt_len = 0, idx = 0;
	unsigned char *buf;


	buf = dl_skb->data;
	pkt_len = dl_skb->len;

	if (pkt_len == 0) {
		KAL_DBGPRINT(KAL, DBG_WARN,("[WARN] Zero Pkt received que_num=%d , pkt_len=%d!!\n", que_num , pkt_len));
	}

	for (idx = 0 ; idx < pkt_len ; idx ++) {
		if (buf[idx] != 0xa5) {
			KAL_DBGPRINT(KAL, DBG_WARN,("[WARN] que_num=%d , pkt_len=%d data mismatch !!\n" , que_num , pkt_len));			
			KAL_DBGPRINT(KAL, DBG_WARN,("[WARN] (pkt_len=%d, pos=%d , expect=0xa5, read=0x%x)!!\n" ,pkt_len ,idx ,buf[idx]));			
			return RET_FAIL;
		}
	}	

	return ret;
}

int f_compare_inc_pattern(struct sk_buff *dl_skb, unsigned int que_num)
{
	int ret = RET_SUCCESS;
	unsigned int pkt_len = 0, idx = 0;
	unsigned char *buf , data_char = 0;


	buf = dl_skb->data;
	pkt_len = dl_skb->len;

	if (pkt_len == 0) {
		KAL_DBGPRINT(KAL, DBG_WARN,("[WARN] Zero Pkt received que_num=%d , pkt_len=%d!!\n", que_num , pkt_len) );
	}

	/*ATCASE_LB_DATA_INC increase from 1st byte char*/
	data_char = buf[0];
	for (idx = 0 ; idx < pkt_len ; idx ++, data_char++) {
		if (buf[idx] != data_char) {
			KAL_DBGPRINT(KAL, DBG_WARN,("[WARN] que_num=%d , pkt_len=%d data mismatch !!\n" , que_num , pkt_len));			
			KAL_DBGPRINT(KAL, DBG_WARN,("[WARN] (pkt_len=%d, pos=%d , expect=0x%x, read=0x%x)!!\n" ,pkt_len , data_char ,idx ,buf[idx]));			
			return RET_FAIL;
		}
	}	

	return ret;
}

int f_compare_recv_pkt(struct sk_buff *dl_skb, unsigned int que_num)
{
	int ret = RET_SUCCESS;
#if 1
	switch (cmp_pattern) {
		case ATCASE_LB_DATA_5A :
			ret = f_compare_5a_pattern(dl_skb, que_num);
			break;
		case ATCASE_LB_DATA_A5 :
			ret = f_compare_a5_pattern(dl_skb, que_num);
			break;
		case ATCASE_LB_DATA_INC :
			ret = f_compare_inc_pattern(dl_skb, que_num);
			break;
		case ATCASE_LB_DATA_FRAGMENT:
			ret = f_compare_fragment_pattern(dl_skb, que_num);
			break;
		case ATCASE_LB_DATA_AUTO :
		default :
			ret = f_compare_auto_pattern(dl_skb, que_num);
			break;
	}
#endif
	return ret;
}

int f_wait_recv_pkt_cnt(unsigned int expect_num , unsigned int timeout_ms)
{
	int ret = RET_SUCCESS;
	unsigned int idx = 0, msg_delay = 0 , cur_pkt_num = 0;;

	for (idx = 0 ; idx < timeout_ms ; idx ++) {
		if (cmp_pattern == ATCASE_LB_DATA_FRAGMENT) {
			if (expect_num <= recv_total_pkt_cnt_agg) {
				break;
			} else {
				cur_pkt_num = recv_total_pkt_cnt_agg; 
			}
		} else {
			if (expect_num <= recv_total_pkt_cnt) {
				break;
			} else {
				cur_pkt_num = recv_total_pkt_cnt; 
			}
		}
		if (recv_th_rslt != RET_SUCCESS) {
			KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s:%d] f_wait_recv_pkt_cnt compare fail\n", __FUNCTION__, __LINE__));		
			return RET_FAIL;
		}
		KAL_SLEEP_MSEC(1) ;
		msg_delay ++;
		if (msg_delay > 1000) {
			msg_delay = 0;
			KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s:%d] f_wait_recv_pkt_cnt waiting for %d ms, expect=%d pkts , cur=%d pkts \n",
														__FUNCTION__, __LINE__, idx, expect_num,cur_pkt_num ));
		}
	}

	if (idx >= timeout_ms) {
		KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s:%d] f_wait_recv_pkt_cnt timeout\n", __FUNCTION__, __LINE__));		
		ret = RET_FAIL;
	}

	return ret;
}



int f_ul_rgpd_allow_len_tst(unsigned int txq_no ,athif_ul_rgpd_format_t *p_rgpd_format, unsigned int pkt_len_start, unsigned int pkt_len_end)
{
	int ret = RET_SUCCESS;
	athif_cmd_t cmd;
	//athif_status_t status;
	athif_ul_rgpd_tst_cfg_t *p_rgpd_cfg;
	unsigned int pktSize = 0;
	unsigned int q_num = 0 , pkt_cnt = 0;
	//int send_err_timeout = SEND_ERR_TIMEOUT, send_err_retry = SEND_ERR_RETRY;
	unsigned int total_allow_len = 0, idx = 0;


    recv_th_rslt = RET_SUCCESS;
	recv_total_pkt_cnt = 0;
    recv_total_pkt_cnt_agg = 0;
    recv_total_bytes_cnt = 0;

	cmp_pattern = ATCASE_LB_DATA_AUTO;

	/*pause reload rgpd flow*/
	cmd.cmd = ATHIF_CMD_PAUSE_RGPD_RL;
	cmd.buf[0] = 1; // 1 : pause , 0 : resume
	cmd.len = 1;

	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, 1, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    at_mtlte_hif_sdio_clear_tx_count();

	/*prepare RGPD*/
	if (pkt_len_start < sizeof(AT_PKT_HEADER)) {
		pkt_len_start = sizeof(AT_PKT_HEADER);
	}
	if (pkt_len_end < sizeof(AT_PKT_HEADER)) {
		pkt_len_end = sizeof(AT_PKT_HEADER);
	}

	/*calculate the whole rgpd allow length*/
	total_allow_len = 0;
	if (p_rgpd_format->rbd_num) {
		for (idx = 0 ; idx < p_rgpd_format->rbd_num ; idx ++) {
			if (p_rgpd_format->rbd_allow_len[idx] > 0) {
				total_allow_len += p_rgpd_format->rbd_allow_len[idx];
			}
		}
	} else {
		total_allow_len = p_rgpd_format->rgpd_allow_len;
	}
	if (total_allow_len == 0) {
		KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s : %d] RGPD allow length configure err\n",__FUNCTION__ ,__LINE__));		
		return RET_FAIL;
	}

	/*calculate expected pkt count for specific allow length*/
	pkt_cnt = 0;
	for (pktSize = pkt_len_start ; pktSize <= pkt_len_end ; pktSize ++) {
		pkt_cnt += ((pktSize) / total_allow_len);
		if ( (pktSize) % total_allow_len) {
			pkt_cnt ++;
		}
	}

	q_num = txq_no;
	cmd.cmd = ATHIF_CMD_PREPARE_RGPD;
	p_rgpd_cfg = (athif_ul_rgpd_tst_cfg_t *)cmd.buf;
	p_rgpd_cfg->q_num = q_num;
	/*must add one more gpd for queue initial tail*/
	p_rgpd_cfg->gpd_num = pkt_cnt + 1;
	memcpy(&p_rgpd_cfg->rgpd_format, p_rgpd_format , sizeof(athif_ul_rgpd_format_t));
	cmd.len = sizeof(athif_ul_rgpd_tst_cfg_t);
    
	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

    /*send and compare loopback pkt to check if correct*/
#if 1
	for (pktSize = pkt_len_start ; pktSize <= pkt_len_end ; pktSize ++) {

        ret = sdio_send_pkt(txq_no, pktSize , txq_no, 0);
		if (ret != RET_SUCCESS) {
			break;
		}
		if (recv_th_rslt != RET_SUCCESS) {
			KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] recv thread report fail\n", __FUNCTION__));
			ret = RET_FAIL;
			break;
		}			
	}

	if (ret == RET_SUCCESS) {
		/*wait loopback data*/
		ret = f_wait_recv_pkt_cnt(pkt_cnt, 10000);
		if (ret != RET_SUCCESS) {
			KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] f_wait_recv_pkt_cnt timeout\n", __FUNCTION__));
		}
		if (recv_th_rslt != RET_SUCCESS) {
			KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] recv thread report fail\n", __FUNCTION__));
			ret = RET_FAIL;					
		}
		recv_th_rslt = RET_SUCCESS;
		recv_total_pkt_cnt = 0;
        recv_total_pkt_cnt_agg = 0;
        recv_total_bytes_cnt = 0;
	}

    
	cmp_pattern = ATCASE_LB_DATA_AUTO;

	if (ret != RET_SUCCESS) {
		return ret;
	}

    at_mtlte_hif_sdio_clear_tx_count();
    
	/*resume reload rgpd flow*/
	cmd.cmd = ATHIF_CMD_PAUSE_RGPD_RL;
	cmd.buf[0] = 0; // 1 : pause , 0 : resume
	cmd.len = 1;

	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
	
#endif
	return ret;
}




/*
 *	loopback length 1~100 bytes for each queue with specific data pattern
*/
int f_small_pkt_lb(lb_data_pattern_e pattern)
{
	int ret = RET_SUCCESS;
	struct timespec start_t , end_t, diff_t;
	athif_cmd_t	cmd;
	athif_status_t status;
	unsigned int i = 0 ;
	int send_err_timeout = SEND_ERR_TIMEOUT, send_err_retry = SEND_ERR_RETRY;
    unsigned int rand_num = 0,pktSize = 0, q_random_mod = 0,packetnum=0;
    unsigned char que_no = 0;
	lb_data_pattern_e org_send_pattern = 0, org_cmp_pattern = 0;
    unsigned int min_size, max_size;

	recv_th_rslt = RET_SUCCESS;
	recv_total_pkt_cnt = 0;

	if (pattern > ATCASE_LB_DATA_INC) {
		return RET_FAIL;
	}

	/*backup pattern mode*/
	org_send_pattern = send_pattern;
	org_cmp_pattern = cmp_pattern;
	send_pattern = pattern;	
	cmp_pattern = pattern;

    min_size = 1;
    max_size = 101;
    
	for (que_no = 0 ; que_no < HIF_MAX_ULQ_NUM ; que_no ++) {
		for (pktSize = min_size ; pktSize < max_size ; pktSize ++) {

                ret = sdio_send_pkt(que_no, pktSize , que_no, 0);

			    if (ret != RET_SUCCESS) {
				    break;
			    }
			if (recv_th_rslt != RET_SUCCESS) {
				KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] recv thread report fail\n", __FUNCTION__));
				ret = RET_FAIL;
				break;
			}			
		}
		if (ret == RET_SUCCESS) {
			/*wait loopback data*/			
			ret = f_wait_recv_pkt_cnt(max_size-min_size , 10000);
			if (ret != RET_SUCCESS) {
				KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] f_wait_recv_pkt_cnt timeout\n", __FUNCTION__));
				break;
			}
			if (recv_th_rslt != RET_SUCCESS) {
				KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] recv thread report fail\n", __FUNCTION__));
				ret = RET_FAIL;					
				break;
			}
			recv_th_rslt = RET_SUCCESS;
			recv_total_pkt_cnt = 0;

		} else {
			break;
		}
	}

	/*restore pattern mode*/
	send_pattern = org_send_pattern;
	cmp_pattern = org_cmp_pattern;
	
	return ret;
}


struct timespec time_diff(struct timespec start , struct timespec end)
{
	struct timespec temp;
	if ((end.tv_nsec-start.tv_nsec)<0) {
		temp.tv_sec = end.tv_sec-start.tv_sec-1;
		temp.tv_nsec = 1000000000+end.tv_nsec-start.tv_nsec;
	} else {
		temp.tv_sec = end.tv_sec-start.tv_sec;
		temp.tv_nsec = end.tv_nsec-start.tv_nsec;
	}
	return temp;
}



int f_tx_rx_ep0_perf_lb(unsigned int loop, unsigned int offset, unsigned int pkt_md,
	unsigned int q_md, unsigned int pkt_len, perf_tst_case_e lb_md)
{
	int ret = RET_SUCCESS;
	struct timespec start_t , end_t, diff_t;
	athif_cmd_t	cmd;
	athif_status_t status;
	unsigned int chk_payload = 0, ep0_tst = 0, i = 0 ;
	int send_err_timeout = SEND_ERR_TIMEOUT, send_err_retry = SEND_ERR_RETRY;
    unsigned int ep0_delay_cnt = 0 , ep0_delay_th = 100 , rand_num = 0,pktSize = 0, q_random_mod = 0,packetnum=0;
    unsigned char tx_ep = 0;
    unsigned long long transferdata=0,performance = 0;
    unsigned long long diff_ms = 0 ;

	if (lb_md == ATCASE_PERF_TXRX) {
		chk_payload = 1;
	}

	/*perpare ep0 buffer first*/
	for (i = 0 ; i < 1024 ; i++) {
		//cmd.buf[i] = (EP0_TST_BUF_SEED + i) & 0xff;
		if (i % 2) {
			cmd.buf[i] = 0x5a;
		} else {
			cmd.buf[i] = 0xa5;			
		}
	}

	recv_th_rslt = RET_SUCCESS;
	recv_total_pkt_cnt = 0;
	recv_total_pkt_cnt_agg = 0;

    sdio_test_option.auto_receive_pkt = true;
	
	while (loop) {
        if(packetnum == 0){
			jiffies_to_timespec(jiffies , &start_t);
		}
		if ((chk_payload) && (recv_th_rslt != RET_SUCCESS)) {
			ret = recv_th_rslt;
			break;
		}
		switch (q_md) {
			case 0 : //all out ep random
				q_random_mod = HIF_MAX_ULQ_NUM;
				break;
			case 1 : //random queue 0~2 
				q_random_mod = 3;
				break;
			case 2 : //random queue 0~1 
				q_random_mod = 2;
				break;
			case 3 : //random queue 0 
				q_random_mod = 1;
				break;
			default :
				q_random_mod = HIF_MAX_ULQ_NUM;
				break;					
		}
		get_random_bytes(&rand_num, sizeof(rand_num));
		tx_ep = rand_num % q_random_mod;


		switch (pkt_md) {
			case 0 : //random pktSize = random(2048)
				get_random_bytes(&rand_num, sizeof(rand_num));
				pktSize =1 + rand_num %MAX_UL_PKT_SIZE;
				break;
			case 1 : //random pktSize = random(pkt_len)
				get_random_bytes(&rand_num, sizeof(rand_num));
				pktSize =1 + rand_num%pkt_len;
				break;
			case 2 : //pkt_len specific 
				pktSize =pkt_len;
				break;
			default :
				get_random_bytes(&rand_num, sizeof(rand_num));
				pktSize =1 + rand_num %MAX_UL_PKT_SIZE;
				break;						
		}
		if (pktSize < sizeof(AT_PKT_HEADER)){
			 pktSize = sizeof(AT_PKT_HEADER);
		}

		if (pktSize > MAX_UL_PKT_SIZE) {
			pktSize = MAX_UL_PKT_SIZE;
			KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] pktSize error len=%d \n", __FUNCTION__,pktSize));
		}
		if (pktSize == 0) {
			pktSize = 100;
			KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] pktSize error len=%d \n", __FUNCTION__,pktSize));
		}
		
		transferdata+=pktSize;

		
		ret = sdio_send_pkt(tx_ep, pktSize , tx_ep, 0);

		if (ret != RET_SUCCESS) {
            KAL_DBGPRINT(KAL, DBG_ERROR,("%s : sending error at pkt num = %d! \n", KAL_FUNC_NAME, packetnum));
			break;
		}
		if (recv_th_rslt != RET_SUCCESS) {
			KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] recv thread report fail\n", __FUNCTION__));
			ret = RET_FAIL;
			break;
		}
		packetnum ++;

		if (packetnum > 100000) {
			if (chk_payload) {
				ret = f_wait_recv_pkt_cnt(packetnum , 100000);
				if (ret != RET_SUCCESS) {
					KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] f_wait_recv_pkt_cnt timeout\n", __FUNCTION__));
					break;
				}
				if (recv_th_rslt != RET_SUCCESS) {
					KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] recv thread report fail\n", __FUNCTION__));
					ret = RET_FAIL;					
					break;
				}
				if (cmp_pattern == ATCASE_LB_DATA_FRAGMENT) {
					if (packetnum != recv_total_pkt_cnt_agg) {
						KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] recv fragment pattern pkt number mismatch expect=%d, recv=%d\n", __FUNCTION__,packetnum , recv_total_pkt_cnt_agg));
						ret = RET_FAIL;					
						break;
					}
				} else {
					if (packetnum != recv_total_pkt_cnt) {
						KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] recv auto pattern pkt number mismatch expect=%d, recv=%d\n", __FUNCTION__,packetnum , recv_total_pkt_cnt));
						ret = RET_FAIL;					
						break;
					}
				}
			}
			/*transfer done without error, calc performance*/
			jiffies_to_timespec(jiffies , &end_t);
			diff_t = time_diff(start_t, end_t);
			diff_ms = (1000 * diff_t.tv_sec) ;
			diff_ms += (diff_t.tv_nsec / 1000000);
 			performance = ((unsigned int)transferdata / (unsigned int)diff_ms);

			KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] performance = %d KBPS\n", __FUNCTION__, performance ));
			KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] transfered data=%u\n", __FUNCTION__, transferdata));
			KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] diff_ms=%u\n", __FUNCTION__, diff_ms));

			recv_total_pkt_cnt = 0;
			recv_total_pkt_cnt_agg = 0;
			recv_th_rslt = RET_SUCCESS;
			packetnum = 0;
			transferdata = 0;

			loop --;
		}
		if (recv_th_rslt != RET_SUCCESS) {
			ret = RET_FAIL;
			break;
		}
	
	}

    sdio_test_option.auto_receive_pkt = false;
	return ret;
}


int f_rx_perf_tst(unsigned int loop, unsigned int offset, unsigned int pkt_num,
	unsigned int q_md, unsigned int pkt_len, perf_tst_case_e lb_md)
{
	int ret = RET_SUCCESS;
	athif_cmd_t cmd;
	athif_status_t status;
	unsigned int tst_q_num = 0, i = 0;
	athif_dl_perf_cfg_t *p_dl_perf_cfg;
	struct timespec start_t , end_t, diff_t;
    unsigned long long transferdata=0,performance = 0;
    unsigned long long diff_ms = 0 ;

    sdio_test_option.auto_receive_pkt = true;	
    sdio_test_option.exam_dl_content = false;	
	recv_total_pkt_cnt = 0;
	recv_total_bytes_cnt = 0;
	recv_th_rslt = RET_SUCCESS;

	tst_q_num = q_md ;
	p_dl_perf_cfg = cmd.buf;
	memset(p_dl_perf_cfg , 0 , sizeof(athif_dl_perf_cfg_t));
	for (i = 0 ; i < tst_q_num ; i++) { //start queue 0~q_md dl test
		p_dl_perf_cfg->txq_cfg[i].que_en = true;
		p_dl_perf_cfg->txq_cfg[i].gpd_type = ATHIF_GPD_GENERIC;
		p_dl_perf_cfg->txq_cfg[i].bd_num = 0;
		p_dl_perf_cfg->txq_cfg[i].q_num = i;
		p_dl_perf_cfg->txq_cfg[i].gpd_num = 100;
		p_dl_perf_cfg->txq_cfg[i].gpd_len = pkt_len;
		p_dl_perf_cfg->txq_cfg[i].pkt_cnt = pkt_num;
	}
	cmd.cmd = ATHIF_CMD_DL_PERF;
	cmd.len = sizeof(athif_dl_perf_cfg_t);
	
	while (loop){ 
        
		recv_total_pkt_cnt = 0;
		recv_total_bytes_cnt = 0;
		recv_th_rslt = RET_SUCCESS;
        for (i = 0 ; i < tst_q_num ; i ++) {
			que_recv_pkt_cnt[i] = 0;
        }
		mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
        mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
        
		jiffies_to_timespec(jiffies , &start_t);
		while (1) {
	        for (i = 0 ; i < tst_q_num ; i ++) {
	        	if (que_recv_pkt_cnt[i] < p_dl_perf_cfg->txq_cfg[i].pkt_cnt) {
					break;
	        	}
	        }				
	        if (i >= tst_q_num) { //all dl queue transfer done
				if (recv_th_rslt != RET_SUCCESS) {
					ret = RET_FAIL;
					break;
				}
				jiffies_to_timespec(jiffies , &end_t);
				diff_t = time_diff(start_t, end_t);
				diff_ms = (1000 * diff_t.tv_sec) ;
				diff_ms += (diff_t.tv_nsec / 1000000);
	 			performance = ((unsigned int)recv_total_bytes_cnt / (unsigned int)diff_ms);
				KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] performance = %d KBPS\n", __FUNCTION__, performance ));
				KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] transfered data=%u\n", __FUNCTION__, recv_total_bytes_cnt));
				KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] diff_ms=%u\n", __FUNCTION__, diff_ms));

	        	/*reset the profile variable*/
				recv_total_pkt_cnt = 0;
				recv_total_bytes_cnt = 0;
				recv_th_rslt = RET_SUCCESS;
		        for (i = 0 ; i < tst_q_num ; i ++) {
		        	que_recv_pkt_cnt[i] = 0;
		        }	
				if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}	
				loop --;
				break;	        	

	        }
	        /*if fail just return*/
	        if (recv_th_rslt != RET_SUCCESS) {
				ret = RET_FAIL;
				break;
	        }
	        if (ret != RET_SUCCESS) {
	        	break;
	        }

			/*check timeout*/
			jiffies_to_timespec(jiffies , &end_t);
			diff_t = time_diff(start_t, end_t);
			diff_ms = (1000 * diff_t.tv_sec) ;
			diff_ms += (diff_t.tv_nsec / 1000000);
			if (diff_ms > 120*1000) {
				KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] wait recv timeout %d ms , recv_cnt=%d\n", __FUNCTION__,diff_ms, recv_total_pkt_cnt));
				ret = RET_FAIL;
				break;
			}
			 KAL_SLEEP_MSEC(0);
		}
		if (ret != RET_SUCCESS) {
			KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] fail , loop=%d, recv_cnt=%d\n", __FUNCTION__,loop , recv_total_pkt_cnt));
			break;
		}
				
	}
	sdio_test_option.auto_receive_pkt = false;
	return ret;
}

int f_ul_cs_err_tst(unsigned int cs_len, unsigned int is_bd)
{
	unsigned int ret = RET_SUCCESS;
	unsigned int org_cs_len = 0, q_num = 0, valid_pkt_cnt = 0, expect_free_cnt = 0,idx = 0;
	unsigned int cs_err_position = 0;
	athif_mem_tst_cfg_t *p_mem_rw_cfg;
	unsigned int is_cs16 = 0, orig_HWFCR = 0, new_HWFCR = 0;
	athif_gpd_cs_cfg_t *p_rgpd_cfg;
	athif_local_rgpd_rslt_t *p_rgpd_rslt;
    athif_cmd_t cmd;
	athif_status_t status;
    hifsdio_isr_status_t *device_int_st;



    p_mem_rw_cfg = cmd.buf;
	p_mem_rw_cfg->mem_addr = (unsigned int)ORG_SDIO_HWFCR;
	p_mem_rw_cfg->len = 4;
    mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, ATHIF_CMD_READ_MEM, (char *)p_mem_rw_cfg, sizeof(athif_mem_tst_cfg_t), dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
    orig_HWFCR = (*(unsigned int *)athif_result_save_t->buf);
	
    /*configure checksum mode and chechsum enable disable*/
    new_HWFCR = orig_HWFCR | ORG_SDIO_TRX_DESC_CHKSUM_EN;
    if (cs_len == 16) {
		is_cs16 = 1;
		new_HWFCR = new_HWFCR & (~ORG_SDIO_TRX_DESC_CHKSUM_12);
	} else {
		is_cs16 = 0;
		new_HWFCR = new_HWFCR | ORG_SDIO_TRX_DESC_CHKSUM_12;
	}

	/*set new checksum enable configure*/
    p_mem_rw_cfg = cmd.buf;
	p_mem_rw_cfg->mem_addr = (unsigned int)ORG_SDIO_HWFCR;
	p_mem_rw_cfg->len = 4;
    *(unsigned int*)p_mem_rw_cfg->mem_val = new_HWFCR;
    cmd.len = sizeof(athif_mem_tst_cfg_t)+4;
    mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, ATHIF_CMD_WRITE_MEM, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}



	for (q_num = 0 ; q_num < HIF_MAX_ULQ_NUM ; q_num ++) {

        /*pause reload rgpd flow*/
	    cmd.cmd = ATHIF_CMD_PAUSE_RESUME_DATAFLOW;
	    cmd.buf[0] = 1; // 1 : pause , 0 : resume
	    cmd.len = 1;
	    mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
        mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
        if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
    
		/*clear qmu interrupt info*/
        cmd.cmd = SDIO_AT_DL_INT_TEST_SWITCH;
	    cmd.buf[0] = 1; // 1 : test mode , 0 : normal mode
	    cmd.len = 1;
	    mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
        mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
        if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

        p_rgpd_cfg = cmd.buf;
	    valid_pkt_cnt = 20;
	    cs_err_position = 10; //means 10GPD correct
		memset(p_rgpd_cfg , 0 , sizeof(athif_gpd_cs_cfg_t));	
		/*prepare 20 RGPD/BD with error checksum, include bypass RGPD*/
		p_rgpd_cfg->gpd_num = valid_pkt_cnt + 1; //plus 1 for the hwo=0 gpd
		expect_free_cnt = p_rgpd_cfg->gpd_num;
		p_rgpd_cfg->q_num = q_num;
		for(idx = 0 ; idx < p_rgpd_cfg->gpd_num ; idx ++) {
			if (is_cs16) {
				p_rgpd_cfg->ioc_bps_valid[idx] = (1<<2);
			} else {
				p_rgpd_cfg->ioc_bps_valid[idx] = 0;
			}
		}
		/*set error checksum GPD*/
		p_rgpd_cfg->ioc_bps_valid[cs_err_position] |= (is_bd ? (1<<4) : (1<<3));
		
		cmd.cmd = ATHIF_CMD_PREPARE_CS_TST_RGPD;
		cmd.len = sizeof(athif_gpd_cs_cfg_t) + p_rgpd_cfg->gpd_num;
		mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
        mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
        if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}


		for (idx = 0 ; idx < cs_err_position ; idx ++) {
			ret = sdio_send_pkt(q_num, 60 , q_num, 0); //set small than smallest MPS
			if (ret != RET_SUCCESS) {
				break;
			}
		}

		/*sleep to wait the device recv the urbs*/
		msleep(100);
		
		/*no error interrupt assert here and the error interrupt would be asserted at next packet*/
		cmd.cmd = SDIO_AT_READ_INT_STATUS;
	    cmd.buf[0] = 0; 
	    cmd.len = 1;
	    mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
        mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
        if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;} 
		else { //compare expected interrupt information
			device_int_st = (hifsdio_isr_status_t *)athif_result_save_t->buf;
            
			if ((device_int_st->UL0_INTR_Status & 0xFFFFFF00) != 0) {
				KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s:%d] cs err intr cmp fail, is_bd=%d, cs_len=%d ,q_num=%d, ul_int=%x !\n"
													,__FUNCTION__,__LINE__,is_bd, cs_len, q_num, device_int_st->UL0_INTR_Status));
				ret = RET_FAIL;
				return ret;
			} else {
				KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s:%d] cs err intr cmp success, is_bd=%d, cs_len=%d ,q_num=%d !\n"
													,__FUNCTION__,__LINE__,is_bd, cs_len, q_num));
			}
		}		

		ret = sdio_send_pkt(q_num, 60 , q_num, 0); //set small than smallest MPS
		if (ret != RET_SUCCESS) {
			return ret;
		}

		/*get qmu interrupt info and expect no error interrupt*/
		cmd.cmd = SDIO_AT_READ_INT_STATUS;
	    cmd.buf[0] = 0; 
	    cmd.len = 1;
	    mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
        mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
        if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;} 
        else { //compare expected interrupt information
			device_int_st = (hifsdio_isr_status_t *)athif_result_save_t->buf;
            
			/*the rx/rx err interrupt info is bit-map*/
			if ((device_int_st->UL0_INTR_Status & 0xFFFFFF00) != ORG_SDIO_TXQ_CHKSUM_ERR(q_num)){
				KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s:%d] cs err intr cmp fail, is_bd=%d, cs_len=%d ,q_num=%d, ul_int=%x !\n"
													,__FUNCTION__,__LINE__,is_bd, cs_len, q_num, device_int_st->UL0_INTR_Status));
				ret = RET_FAIL;
				return ret;
			} else {
				KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s:%d] cs err intr cmp success, is_bd=%d, cs_len=%d ,q_num=%d !\n"
													,__FUNCTION__,__LINE__,is_bd, cs_len, q_num));
			}
		}		

        /*
		cmd.cmd = ATHIF_CMD_GET_LOCAL_RGPD_RSLT;
		*(unsigned int*)cmd.buf = q_num;
		cmd.len = 4;
		mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
        mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
        if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;} 
		else { //compare expected interrupt information
			p_rgpd_rslt = (athif_local_rgpd_rslt_t *)athif_result_save_t->buf;
			//don't care fail count, because some GPD HWO=1
			if ((p_rgpd_rslt->correct_cnt != cs_err_position) || (p_rgpd_rslt->free_cnt != expect_free_cnt)) {
				KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s:%d] q_num=%d, local_rgpd_result fail !\n",__FUNCTION__,__LINE__, q_num));
				KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s:%d] fail_cnt=%d, correct_cnt=%d, free_cnt=%d !\n",
							__FUNCTION__,__LINE__, p_rgpd_rslt->fail_cnt, p_rgpd_rslt->correct_cnt, p_rgpd_rslt->free_cnt));
				ret = RET_FAIL;			
				return ret;
			}
		}*/

        cmd.cmd = SDIO_AT_RESET_UL_QUEUE;
	    cmd.buf[0] = 0xFF; 
	    cmd.len = 1;
	    mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
        mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
        if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

        /*resume reload rgpd flow*/
	    cmd.cmd = ATHIF_CMD_PAUSE_RESUME_DATAFLOW;
	    cmd.buf[0] = 0; // 1 : pause , 0 : resume
	    cmd.len = 1;
	    mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
        mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
        if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

	}
    
    cmd.cmd = SDIO_AT_DL_INT_TEST_SWITCH;
	cmd.buf[0] = 0; // 1 : test mode , 0 : normal mode
	cmd.len = 1;
	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
    
	/*restore original checksum enable configure*/
    
    p_mem_rw_cfg = cmd.buf;
    p_mem_rw_cfg->mem_addr = (unsigned int)ORG_SDIO_HWFCR;
    p_mem_rw_cfg->len = 4;
    *(unsigned int*)p_mem_rw_cfg->mem_val = orig_HWFCR;
    cmd.len = sizeof(athif_mem_tst_cfg_t)+4;
    mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, ATHIF_CMD_WRITE_MEM, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}


	return ret;
}


int f_dl_cs_err_tst(unsigned int cs_len, unsigned int is_bd)
{
	unsigned int ret = RET_SUCCESS;
	unsigned int org_cs_len = 0, q_num = 0, valid_pkt_cnt = 0, expect_free_cnt = 0,idx = 0;
	unsigned int cs_err_position = 0, expected_tx_err_intr = 0;
	athif_mem_tst_cfg_t *p_mem_rw_cfg;
	unsigned int is_cs16 = 0, orig_HWFCR = 0, new_HWFCR = 0;
	athif_gpd_cs_cfg_t *p_tgpd_cfg;
	athif_local_tgpd_rslt_t *p_tgpd_rslt;
	int send_err_timeout = SEND_ERR_TIMEOUT, send_err_retry = SEND_ERR_RETRY;
    athif_cmd_t cmd;
	athif_status_t status;
    hifsdio_isr_status_t *device_int_st;
    struct sk_buff *result_ptr = NULL;
	


	p_mem_rw_cfg = cmd.buf;
	p_mem_rw_cfg->mem_addr = (unsigned int)ORG_SDIO_HWFCR;
	p_mem_rw_cfg->len = 4;
    mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, ATHIF_CMD_READ_MEM, (char *)p_mem_rw_cfg, sizeof(athif_mem_tst_cfg_t), dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
    orig_HWFCR = (*(unsigned int *)athif_result_save_t->buf);
	
    /*configure checksum mode and chechsum enable disable*/
    new_HWFCR = orig_HWFCR | ORG_SDIO_TRX_DESC_CHKSUM_EN;
    if (cs_len == 16) {
		is_cs16 = 1;
		new_HWFCR = new_HWFCR & (~ORG_SDIO_TRX_DESC_CHKSUM_12);
	} else {
		is_cs16 = 0;
		new_HWFCR = new_HWFCR | ORG_SDIO_TRX_DESC_CHKSUM_12;
	}

	/*set new checksum enable configure*/
    p_mem_rw_cfg = cmd.buf;
	p_mem_rw_cfg->mem_addr = (unsigned int)ORG_SDIO_HWFCR;
	p_mem_rw_cfg->len = 4;
    *(unsigned int*)p_mem_rw_cfg->mem_val = new_HWFCR;
    cmd.len = sizeof(athif_mem_tst_cfg_t)+4;
    mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, ATHIF_CMD_WRITE_MEM, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
    

	
	for (q_num = 0 ; q_num < HIF_MAX_DLQ_NUM ; q_num ++) {

        /*pause reload rgpd flow*/
	    cmd.cmd = ATHIF_CMD_PAUSE_RESUME_DATAFLOW;
	    cmd.buf[0] = 1; // 1 : pause , 0 : resume
	    cmd.len = 1;
	    mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
        mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
        if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

		/*clear qmu interrupt info*/
        cmd.cmd = SDIO_AT_DL_INT_TEST_SWITCH;
	    cmd.buf[0] = 1; // 1 : test mode , 0 : normal mode
	    cmd.len = 1;
	    mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
        mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
        if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

        p_tgpd_cfg = (athif_gpd_cs_cfg_t *)cmd.buf;
		memset(p_tgpd_cfg , 0 , sizeof(athif_gpd_cs_cfg_t));
	    valid_pkt_cnt = 10;
	    cs_err_position = 5; //means 10GPD correct
	
		/*prepare 20 RGPD/BD with error checksum, include bypass RGPD*/
		p_tgpd_cfg->gpd_num = valid_pkt_cnt + 1; //plus 1 for the hwo=0 gpd
		expect_free_cnt = p_tgpd_cfg->gpd_num;
		p_tgpd_cfg->q_num = q_num;

		for(idx = 0 ; idx < p_tgpd_cfg->gpd_num ; idx ++) {
			if (is_cs16) {
				p_tgpd_cfg->ioc_bps_valid[idx] = (1<<2);
			} else {
				p_tgpd_cfg->ioc_bps_valid[idx] = 0;
			}
		}
		/*set error checksum GPD*/
		p_tgpd_cfg->ioc_bps_valid[cs_err_position] |= (is_bd ? (1<<4) : (1<<3));
		
		recv_th_rslt = RET_SUCCESS;
		recv_total_pkt_cnt = 0;
        sdio_test_option.auto_receive_pkt = true;
        
		cmd.cmd = ATHIF_CMD_PREPARE_CS_TST_TGPD;
		cmd.len = sizeof(athif_gpd_cs_cfg_t) + p_tgpd_cfg->gpd_num;	

		mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
        mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
        if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}


		/*wait expected received pkt count*/
		if (ret = f_wait_recv_pkt_cnt(cs_err_position , 100000) == RET_FAIL) {
			KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s:%d] wait recv pkt cnt timeout, expect %d pkts !\n",__FUNCTION__,__LINE__, valid_pkt_cnt));
			return ret;
		} else {
			if (recv_th_rslt != RET_SUCCESS) {
				KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s:%d] recv packet payload compare fail !\n",__FUNCTION__,__LINE__));
				return RET_FAIL;
			}
		}
        /*
		cmd.cmd = ATHIF_CMD_GET_LOCAL_TGPD_RSLT;
		*(unsigned int*)cmd.buf = q_num;
		cmd.len = 4;
		mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
        mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
        if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
        else { //compare expected interrupt information
			p_tgpd_rslt = athif_result_save_t->buf;
			if ((p_tgpd_rslt->sent_cnt != cs_err_position) || (p_tgpd_rslt->free_cnt != expect_free_cnt)) {
				KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s:%d] q_num=%d, local_rgpd_result fail !\n",__FUNCTION__,__LINE__, q_num));
				ret = RET_FAIL;			
				return ret;
			}
		}
		*/
        cmd.cmd = SDIO_AT_READ_INT_STATUS;
	    cmd.buf[0] = 0; 
	    cmd.len = 1;
	    mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
        mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
        if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;} 
        else { //compare expected interrupt information
			device_int_st = (hifsdio_isr_status_t *)athif_result_save_t->buf;
            
			if (device_int_st->DL0_INTR_Status & 0x00FF0000 != ORG_SDIO_RXQ_CHKSUM_ERR(q_num)) {
				KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s:%d] checksum test fail, cs_len=%d, is_bd=%d ,q_num=%d ,dl0_int=%x !\n"
													,__FUNCTION__,__LINE__, cs_len, is_bd, q_num, device_int_st->DL0_INTR_Status));
				ret = RET_FAIL;
				return ret;
			} else {
				KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s:%d] checksum test success, cs_len=%d, is_bd=%d ,q_num=%d !\n"
													,__FUNCTION__,__LINE__, cs_len, is_bd, q_num));
			}
		}
        cmd.cmd = SDIO_AT_RESET_DL_QUEUE;
        cmd.buf[0] = 0xCE;    //checksum error test, directly stop DL queue
	    cmd.buf[1] = q_num; 
	    cmd.len = 2;
	    mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
        mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
        if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}

        sdio_test_option.auto_receive_pkt = false;

        /*resume reload rgpd flow*/
	    cmd.cmd = ATHIF_CMD_PAUSE_RESUME_DATAFLOW;
	    cmd.buf[0] = 0; // 1 : pause , 0 : resume
	    cmd.len = 1;
	    mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
        mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
        if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}



        result_ptr = mtlte_df_DL_read_skb_from_swq(q_num);
        while(result_ptr != NULL){
            dev_kfree_skb(result_ptr);
            result_ptr = mtlte_df_DL_read_skb_from_swq(q_num);
        }

	}

   
    
	cmd.cmd = SDIO_AT_DL_INT_TEST_SWITCH;
	cmd.buf[0] = 0; // 1 : test mode , 0 : normal mode
	cmd.len = 1;
	mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, cmd.cmd, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
    
	/*restore original checksum enable configure*/
    
    p_mem_rw_cfg = cmd.buf;
    p_mem_rw_cfg->mem_addr = (unsigned int)ORG_SDIO_HWFCR;
    p_mem_rw_cfg->len = 4;
    *(unsigned int*)p_mem_rw_cfg->mem_val = orig_HWFCR;
    cmd.len = sizeof(athif_mem_tst_cfg_t)+4;
    mtlte_dev_test_config_atcmd(ATHIF_CMD_SET_SIG, ATHIF_CMD_WRITE_MEM, cmd.buf, cmd.len, dev_test_athif_cmd_t);
    mtlte_dev_test_sent_atcmd(dev_test_athif_cmd_t);
    if(RET_FAIL == mtlte_dev_test_check_cmd_ack(athif_result_save_t, WAIT_TIMEOUT) ){return RET_FAIL;}
    

	return ret;
}


#define TEST_ALIGN_TO_DWORD(_value)				(((_value) + 0x3) & ~0x3)
#define test2_rx_tail_len (4+4+4+2+2+2+2+2*(test_rx_pkt_cnt_q0+test_rx_pkt_cnt_q1+test_rx_pkt_cnt_q2+test_rx_pkt_cnt_q3)+4+4)


sdio_tx_queue_info tx_queue_info_test[HIF_MAX_ULQ_NUM] = {
	{TXQ_Q0,	SDIO_IP_WTDR1},
	{TXQ_Q1,	SDIO_IP_WTDR1},
	{TXQ_Q2,	SDIO_IP_WTDR1},
	{TXQ_Q3,	SDIO_IP_WTDR1},
	{TXQ_Q4,	SDIO_IP_WTDR1},
    {TXQ_Q5,	SDIO_IP_WTDR1},
    {TXQ_Q6,	SDIO_IP_WTDR1},
};

int tx_perf_hw_limit(unsigned int loop, unsigned int offset, unsigned int pkt_md,
	unsigned int q_md, unsigned int pkt_len, perf_tst_case_e lb_md)
{
	int ret = RET_SUCCESS;
	struct timespec start_t , end_t, diff_t;
	athif_cmd_t	cmd;
	athif_status_t status;
	unsigned int chk_payload = 0, ep0_tst = 0, i = 0, pkt_no = 0;
	int send_err_timeout = SEND_ERR_TIMEOUT, send_err_retry = SEND_ERR_RETRY;
    unsigned int ep0_delay_cnt = 0 , ep0_delay_th = 100 , rand_num = 0,pktSize = 0, q_random_mod = 0,packetnum=0;
    unsigned char tx_ep = 0;
    unsigned long long transferdata=0,performance = 0;
    unsigned long long diff_ms = 0 ;
    unsigned char *buf_pt;
    PAT_PKT_HEADER pAtHeader = NULL;
	unsigned char rand_seed = 0, bak_seed = 0;
    unsigned char cksm = 0;
    unsigned int  pkt_size_record[HIF_MAX_ULQ_NUM][64];
    KAL_UINT32  Tx_avail_GPD[HIF_MAX_ULQ_NUM];
    sdio_whisr_enhance *test_whisr;
    unsigned int pkt_no_thistime=0, pkt_len_thistime=0;
    sdio_tx_sdu_header *tx_header_temp ;
    unsigned int  timeout;
    


	recv_th_rslt = RET_SUCCESS;
	recv_total_pkt_cnt = 0;
	recv_total_pkt_cnt_agg = 0;

    KAL_ALLOCATE_PHYSICAL_MEM(buff_kmemory_hwlimit, 458752);

    KAL_ZERO_MEM(buff_kmemory_hwlimit, 458752) ;
    buf_pt = buff_kmemory_hwlimit;

    // pre-fill the packet content for UL HW limit speed test.
    for(tx_ep=0; tx_ep<HIF_MAX_ULQ_NUM; tx_ep++){
        buf_pt = buff_kmemory_hwlimit + (tx_ep*65536) ;
        
        for(pkt_no=0; pkt_no<30; pkt_no++){
            switch (pkt_md) {
	        		case 0 : //random pktSize = random(2048)
	        			get_random_bytes(&rand_num, sizeof(rand_num));
	        			pktSize =1 + rand_num %MAX_UL_PKT_SIZE;
	        			break;
	        		case 1 : //random pktSize = random(pkt_len)
	        			get_random_bytes(&rand_num, sizeof(rand_num));
	        			pktSize =1 + rand_num%pkt_len;
	        			break;
	        		case 2 : //pkt_len specific 
	        			pktSize =pkt_len;
	        			break;
	        		default :
	        			get_random_bytes(&rand_num, sizeof(rand_num));
	        			pktSize =1 + rand_num %MAX_UL_PKT_SIZE;
	        			break;						
	        }
            
            if (pktSize < sizeof(AT_PKT_HEADER)){
	        	 //pktSize = sizeof(AT_PKT_HEADER);
                 pktSize = 20;
	        }
        
	        if (pktSize > MAX_UL_PKT_SIZE) {
	        	pktSize = MAX_UL_PKT_SIZE;
	        	KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] pktSize error len=%d \n", __FUNCTION__,pktSize));
	        }
	        if (pktSize == 0) {
	        	pktSize = 100;
	        	KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] pktSize error len=%d \n", __FUNCTION__,pktSize));
	        }
    
            tx_header_temp = (sdio_tx_sdu_header *)buf_pt;
            tx_header_temp->u.bits.length = pktSize+MT_LTE_TX_HEADER_LENGTH;
            tx_header_temp->u.bits.tx_type = tx_ep ;
            buf_pt = buf_pt + MT_LTE_TX_HEADER_LENGTH;

        
            switch (send_pattern) {
	    	case ATCASE_LB_DATA_5A :
	    		memset(buf_pt, 0x5a , pktSize);			
	    		break;
	    	case ATCASE_LB_DATA_A5:
	    		memset(buf_pt, 0xa5 , pktSize);			
	    		break;
	    	case ATCASE_LB_DATA_INC:
	    		get_random_bytes(&rand_seed , 1);
	    		for (i = 0 ; i < pktSize ; i++) {
	    			buf_pt[i] = rand_seed++;
	    		}
	    		break;
    
	    	case ATCASE_LB_DATA_AUTO :
            default:
			// fill packet payload
	        	 	pAtHeader = (PAT_PKT_HEADER)buf_pt;
		    	memset(pAtHeader, 0 , sizeof(AT_PKT_HEADER));
    
		    	get_random_bytes(&rand_seed , 1);
		    	bak_seed = rand_seed;
                KAL_DBGPRINT(KAL, DBG_TRACE,("rand_seed = %d..\n", rand_seed));
		    	pAtHeader->RndSeed = rand_seed;
		    	pAtHeader->SrcQID =  0xf;  
		    	pAtHeader->DstQID =  0xf;
		    	pAtHeader->SeqNo = 0;
    
		    	pAtHeader->PktLen = pktSize;
		    	
		    	f_calc_cs_byte(pAtHeader, sizeof(AT_PKT_HEADER), &cksm);
		    	pAtHeader->Checksum = ~cksm;
    
		    	 // fill payload, don't fill memory lenght larger than URB buffer
			for (i = 0 ; i < (pktSize - sizeof(AT_PKT_HEADER)) ; i ++) {
	    	    		pAtHeader->Data[i] = rand_seed++;
	    		}
	    		break;

	        }

            buf_pt = (unsigned char *)( (unsigned int)buf_pt + TEST_ALIGN_TO_DWORD(pktSize) );
            pkt_size_record[tx_ep][pkt_no] = pktSize;
        }
    }


    sdio_test_option.auto_receive_pkt = true;
    for (i = 0 ; i<HIF_MAX_ULQ_NUM ; i++) {
        Tx_avail_GPD[i] = 0;
    }
    
    at_mtlte_hif_sdio_get_tx_count((KAL_UINT32 *)Tx_avail_GPD);

    KAL_DBGPRINT(KAL, DBG_ERROR,("[TEST] start transfer packet !! \r\n")) ;


    // start transfer Tx packet.
	while (loop) {
        if(packetnum == 0){
			jiffies_to_timespec(jiffies , &start_t);
		}

		switch (q_md) {
			case 0 : //all out ep random
				q_random_mod = HIF_MAX_ULQ_NUM;
				break;
			case 1 : //random queue 0~2 
				q_random_mod = 3;
				break;
			case 2 : //random queue 0~1 
				q_random_mod = 2;
				break;
			case 3 : //random queue 0 
				q_random_mod = 1;
				break;
			default :
				q_random_mod = HIF_MAX_ULQ_NUM;
				break;					
		}
		get_random_bytes(&rand_num, sizeof(rand_num));
		tx_ep = rand_num % q_random_mod;

        //KAL_DBGPRINT(KAL, DBG_ERROR,("[TEST] send pkt to txq %d this time !! \r\n", tx_ep)) ;
        
        timeout = 0;
        while(Tx_avail_GPD[tx_ep] < 30){

            for(i=0; i<HIF_MAX_ULQ_NUM; i++){
                if(Tx_avail_GPD[i] > 30){
                    tx_ep = i;
                    break;
                }
            }
            if(i!=HIF_MAX_ULQ_NUM) {break;}

            if(1 == test_rx_tail_change){
                sdio_func1_rd(SDIO_IP_WHISR, buff_kmemory_ulpkt_data, test2_rx_tail_len) ;
            }else{
                sdio_func1_rd(SDIO_IP_WHISR, buff_kmemory_ulpkt_data, MT_LTE_RX_TAILOR_LENGTH) ;
            }       
            
            test_whisr = (sdio_whisr_enhance *)buff_kmemory_ulpkt_data;
            
            Tx_avail_GPD[0] += test_whisr->whtsr0.u.bits.tq0_cnt;
            Tx_avail_GPD[1] += test_whisr->whtsr0.u.bits.tq1_cnt ; 
	        Tx_avail_GPD[2] += test_whisr->whtsr0.u.bits.tq2_cnt ; 
	        Tx_avail_GPD[3] += test_whisr->whtsr0.u.bits.tq3_cnt ; 
	        Tx_avail_GPD[4] += test_whisr->whtsr1.u.bits.tq4_cnt ;
            Tx_avail_GPD[5] += test_whisr->whtsr1.u.bits.tq5_cnt ;
            Tx_avail_GPD[6] += test_whisr->whtsr1.u.bits.tq6_cnt ;


            timeout++;
            if (timeout > 100){
                KAL_DBGPRINT(KAL, DBG_ERROR,("[TEST] wait tx empty packet timeout !! \r\n")) ;
                return RET_FAIL;
            }
            KAL_SLEEP_MSEC(1) ;
        }

        pkt_no_thistime = 30;
        
        buf_pt = buff_kmemory_hwlimit;
        pkt_len_thistime = 0;
        for(i = 0 ; i<pkt_no_thistime ; i++) {  
            
            pkt_len_thistime += TEST_ALIGN_TO_DWORD(pkt_size_record[tx_ep][i] + MT_LTE_TX_HEADER_LENGTH);
            transferdata += pkt_size_record[tx_ep][i];
        }

        KAL_ZERO_MEM((buf_pt + pkt_len_thistime),MT_LTE_TX_ZERO_PADDING_LEN) ;
        
        if(sdio_func1_wr(tx_queue_info_test[tx_ep].port_address, (unsigned char *)(buff_kmemory_hwlimit+tx_ep*65536), ALIGN_TO_BLOCK_SIZE(pkt_len_thistime))){
			KAL_DBGPRINT(KAL, DBG_ERROR,("[ERR] Tx transfer fail in tx_hw_limit test !! \r\n")) ;
            return RET_FAIL;
		}

        Tx_avail_GPD[tx_ep] -= pkt_no_thistime;

		if (recv_th_rslt != RET_SUCCESS) {
			KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] recv thread report fail\n", __FUNCTION__));
			ret = RET_FAIL;
			break;
		}
		packetnum += pkt_no_thistime;

		if (packetnum > 100000) {
			
			/*transfer done without error, calc performance*/
			jiffies_to_timespec(jiffies , &end_t);
			diff_t = time_diff(start_t, end_t);
			diff_ms = (1000 * diff_t.tv_sec) ;
			diff_ms += (diff_t.tv_nsec / 1000000);
 			performance = ((unsigned int)transferdata / (unsigned int)diff_ms);

			KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] performance = %d KBPS\n", __FUNCTION__, performance ));
			KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] transfered data=%u\n", __FUNCTION__, transferdata));
			KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] diff_ms=%u\n", __FUNCTION__, diff_ms));

			recv_total_pkt_cnt = 0;
			recv_total_pkt_cnt_agg = 0;
			recv_th_rslt = RET_SUCCESS;
			packetnum = 0;
			transferdata = 0;

			loop --;
		}
		if (recv_th_rslt != RET_SUCCESS) {
			ret = RET_FAIL;
			break;
		}
	
	}

    KAL_FREE_PHYSICAL_MEM(buff_kmemory_hwlimit);

    sdio_test_option.auto_receive_pkt = false;
	return ret;
}


int f_brom_pkt_lb(lb_data_pattern_e pattern, unsigned int min_size, unsigned int max_size)
{
	int ret = RET_SUCCESS;
	struct timespec start_t , end_t, diff_t;
	athif_cmd_t	cmd;
	athif_status_t status;
	unsigned int i = 0 ;
	int send_err_timeout = SEND_ERR_TIMEOUT, send_err_retry = SEND_ERR_RETRY;
    unsigned int rand_num = 0,pktSize = 0, q_random_mod = 0,packetnum=0;
    unsigned char que_no = 0;
	lb_data_pattern_e org_send_pattern = 0, org_cmp_pattern = 0;

	recv_th_rslt = RET_SUCCESS;
	recv_total_pkt_cnt = 0;

	/*backup pattern mode*/
	org_send_pattern = send_pattern;
	org_cmp_pattern = cmp_pattern;
	send_pattern = pattern;	
	cmp_pattern = pattern;

    que_no = 0;

	for (pktSize = min_size ; pktSize < max_size ; pktSize ++) {

            ret = sdio_send_pkt(que_no, pktSize , que_no, 0);

		    if (ret != RET_SUCCESS) {
			    break;
		    }
		if (recv_th_rslt != RET_SUCCESS) {
			KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] recv thread report fail\n", __FUNCTION__));
			ret = RET_FAIL;
			break;
		}			
	}
    
	if (ret == RET_SUCCESS) {
		/*wait loopback data*/			
		ret = f_wait_recv_pkt_cnt(max_size-min_size , 10000);
		if (ret != RET_SUCCESS) {
			KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] f_wait_recv_pkt_cnt timeout\n", __FUNCTION__));
		}
		if (recv_th_rslt != RET_SUCCESS) {
			KAL_DBGPRINT(KAL, DBG_ERROR, ("[%s] recv thread report fail\n", __FUNCTION__));
			ret = RET_FAIL;					
		}
		recv_th_rslt = RET_SUCCESS;
		recv_total_pkt_cnt = 0;
	} 


	/*restore pattern mode*/
	send_pattern = org_send_pattern;
	cmp_pattern = org_cmp_pattern;
	
	return ret;
}


