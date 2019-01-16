/*****************************************************************************
 *
 * Filename:
 * ---------
 *   ccmni.c
 *
 * Project:
 * --------
 *   
 *
 * Description:
 * ------------
 *   
 *
 * Author:
 * -------
 *   
 *
 ****************************************************************************/
#include <ccci.h>
extern int  ccmni_v1_init(int md_id);
extern void ccmni_v1_exit(int md_id);
extern int  ccmni_v2_init(int md_id);
extern void ccmni_v2_exit(int md_id);
extern int  ccmni_v1_ipo_h_restore(int md_id);
extern int  ccmni_v2_ipo_h_restore(int md_id);


void *ccmni_ctl_block[MAX_MD_NUM];

static char mac_addr_table[MAX_MD_NUM*5][ETH_ALEN];
static int  mac_addr_num = 0;


int is_mac_addr_duplicate(char *mac)
{
    int i = 0, j;
    int found = 0;
    
    for(i=0; i<mac_addr_num; i++) {
        if( strcmp(mac, mac_addr_table[i]) == 0 ) {
            found = 1;
            return !0;
        }
    }
    if(!found) {
        for(j=0; j<ETH_ALEN; j++)
            mac_addr_table[i][j] = mac[j];
        mac_addr_num++;
        if(mac_addr_num >= (MAX_MD_NUM*5) )
            mac_addr_num--;
    }
    return 0;
}

int ccmni_init(int md_id)
{
    int ccmni_version = 0;
    
    if(ccci_get_sub_module_cfg(md_id, "net", (char*)&ccmni_version, sizeof(int)) != sizeof(int)) {
        CCCI_MSG_INF(md_id, "net", "[Error]get ccmni version fail\n");
        return -1;
    } else {
        CCCI_MSG_INF(md_id, "net", "ccmni driver v%d\n", ccmni_version);
    }
    
    switch(ccmni_version)
    {
        case 1:
            return ccmni_v1_init(md_id);
        case 2:
            return ccmni_v2_init(md_id);
        default:
            CCCI_MSG_INF(md_id, "net", "[Error]invalid CCMNI version: %d\n", ccmni_version);
            return -1;
    }
}


void ccmni_exit(int md_id)
{
    int ccmni_version = 0;
    
    if(ccci_get_sub_module_cfg(md_id, "net", (char*)&ccmni_version, sizeof(int)) != sizeof(int)) {
        CCCI_MSG_INF(md_id, "net", "get ccmni version fail\n");
        return;
    }
    
    switch(ccmni_version)
    {
        case 1:
            return ccmni_v1_exit(md_id);
        case 2:
            return ccmni_v2_exit(md_id);
        default:
            CCCI_MSG_INF(md_id, "net", "[Error]invalid CCMNI version: %d\n", ccmni_version);
            return;
    }
}


int ccmni_ipo_h_restore(int md_id)
{
    int ccmni_version = 0;
    
    if(ccci_get_sub_module_cfg(md_id, "net", (char*)&ccmni_version, sizeof(int)) != sizeof(int)) {
        CCCI_MSG("Get ccmni verion fail\n");
        return -1;
    }
    
    switch(ccmni_version)
    {
        case 1:
            return ccmni_v1_ipo_h_restore(md_id);
        case 2:
            return ccmni_v2_ipo_h_restore(md_id);
        default:
            CCCI_MSG("[Error]CCMNI not support version\n");
            return -1;
    }
}

