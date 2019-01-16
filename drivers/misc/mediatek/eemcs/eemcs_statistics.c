#include "eemcs_statistics.h"


EEMCS_STATISTICS *eemcs_statistics[MAX_EXT_MD_NUM];
void eemcs_update_statistics_number(int md_id, int port, int tx_rx, int type, int number){
    if (port >=CCCI_PORT_NUM_MAX){
        return;
    }

    //Update statistics
    switch (type){
        case NORMAL:
            {
                eemcs_statistics[md_id]->port[port].cnt[tx_rx] = number;
                eemcs_statistics[md_id]->port_total[port].cnt[tx_rx] = number;
                break;
            }
        case DROP:
            {
                eemcs_statistics[md_id]->port[port].drop[tx_rx] = number;
                eemcs_statistics[md_id]->port_total[port].drop[tx_rx] = number;
                break;
            }
        case QUEUE:
            {
                eemcs_statistics[md_id]->port[port].queue[tx_rx] = number;
                if (eemcs_statistics[md_id]->port_total[port].queue[tx_rx]< number){
                    eemcs_statistics[md_id]->port_total[port].queue[tx_rx]= number;   
                }
                break;
            }
    }
}

void eemcs_update_statistics(int md_id, int port, int tx_rx, int type){
    struct timeval	tv;
    
    if (port >=CCCI_PORT_NUM_MAX){
        return;
    }
    
    if (eemcs_statistics[md_id]->start == 0){
        return;
    }

    do_gettimeofday(&tv);
    //Check if need to re-cal
    if(tv.tv_sec - eemcs_statistics[md_id]->time.tv_sec > eemcs_statistics[md_id]->inteval){
        eemcs_statistics_reset();
        eemcs_statistics[md_id]->time = tv;
    }

    //Update statistics
    switch (type){
        case NORMAL:
            {
                eemcs_statistics[md_id]->port[port].cnt[tx_rx]++;
                eemcs_statistics[md_id]->port_total[port].cnt[tx_rx]++;
                break;
            }
        case DROP:
            {
                eemcs_statistics[md_id]->port[port].drop[tx_rx]++;
                eemcs_statistics[md_id]->port_total[port].drop[tx_rx]++;
                break;
            }
        default:
            {
                break;
            }
    }
}
    
void eemcs_statistics_reset(){
    int md_id = 0;
    for (md_id=0; md_id< MAX_EXT_MD_NUM; md_id++){
    	if(eemcs_statistics[md_id] != NULL){
            int i = 0;
            for (i=0; i< CCCI_PORT_NUM_MAX; i++){
    		    memset(&eemcs_statistics[md_id]->port[i], 0, sizeof(CCCI_PORT_STATISTICS));
            }
    	}
    }
    
}

int eemcs_statistics_init()
{
    int md_id = 0;
    for (md_id=0; md_id< MAX_EXT_MD_NUM; md_id++){
    	eemcs_statistics[md_id] = kmalloc(sizeof(EEMCS_STATISTICS), GFP_KERNEL);
    	if(eemcs_statistics[md_id] != NULL){
    		memset(eemcs_statistics[md_id], 0, sizeof(EEMCS_STATISTICS));
            eemcs_statistics[md_id]->inteval = 10;
            eemcs_statistics[md_id]->start = 1;
            do_gettimeofday(&eemcs_statistics[md_id]->time);
    	}
    }
	return KAL_SUCCESS;
}

void eemcs_statistics_exit()
{
    int md_id = 0;
    for (md_id=0; md_id< MAX_EXT_MD_NUM; md_id++){
    	if(eemcs_statistics[md_id] != NULL){
    		kfree(eemcs_statistics[md_id]);
    		eemcs_statistics[md_id] = NULL;
    	}
    }
	return;
}