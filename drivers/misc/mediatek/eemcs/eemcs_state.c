#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/mutex.h>

#include "eemcs_state.h"
#include "eemcs_debug.h"
#include "eemcs_boot_trace.h"

#include "lte_main.h"


static volatile KAL_UINT32 eemcs_state = EEMCS_INVALID;
static KAL_UINT8 eemcs_state_name[EEMCS_STATE_MAX][32] = {
	[EEMCS_INVALID]="EEMCS_INVALID",
    [EEMCS_GATE]="EEMCS_GATE",
    [EEMCS_INIT]="EEMCS_INIT",
    [EEMCS_XBOOT]="EEMCS_XBOOT",
    [EEMCS_MOLY_HS_P1]="EEMCS_MOLY_HS_P1",
    [EEMCS_MOLY_HS_P2]="EEMCS_MOLY_HS_P2",
    [EEMCS_BOOTING_DONE]="EEMCS_BOOTING_DONE",
    [EEMCS_EXCEPTION]="EEMCS_EXCEPTION"
};


/*
 * @brief EEMCS device state change callback list init.
 * @param
 *     None.
 * @return None.
 */

static struct {
    struct list_head list;
    struct mutex mutex;
} eemcs_stat_callback_head;

void eemcs_state_callback_init(){
    DBGLOG(INIT, DBG, "%s", FUNC_NAME);
    INIT_LIST_HEAD(&eemcs_stat_callback_head.list);
    mutex_init(&eemcs_stat_callback_head.mutex);
}

kal_bool eemcs_state_callback(EEMCS_STATE state){
    struct list_head *next=NULL;
    EEMCS_STATE_CALLBACK_T *node;
    DBGLOG(BOOT, DBG, "%s: state=%d", FUNC_NAME, state);
    mutex_lock(&eemcs_stat_callback_head.mutex);
    list_for_each(next, &eemcs_stat_callback_head.list) {
        node = (EEMCS_STATE_CALLBACK_T *) list_entry(next, EEMCS_STATE_CALLBACK_T, list);
        node->callback(state);
        }
    mutex_unlock(&eemcs_stat_callback_head.mutex);
    return 0;
}

kal_bool eemcs_state_callback_register(EEMCS_STATE_CALLBACK_T *node){
    struct list_head *next=NULL;
    EEMCS_STATE_CALLBACK_T *list_node;
    
    DBGLOG(INIT, DBG, "%s: node=%p", FUNC_NAME, node);
    mutex_lock(&eemcs_stat_callback_head.mutex);
    /* To check if node is already in list or not, otherwise list will be cycle*/
    list_for_each(next, &eemcs_stat_callback_head.list) {
        list_node = (EEMCS_STATE_CALLBACK_T *) list_entry(next, EEMCS_STATE_CALLBACK_T, list);
        if (list_node == node){
            DBGLOG(INIT, ERR, "EEMCS_STATE_CALLBACK %#X has registed!", (unsigned int)node);        
            mutex_unlock(&eemcs_stat_callback_head.mutex);        
            return KAL_FAIL;
        }
    }
    list_add(&node->list, &eemcs_stat_callback_head.list);
    mutex_unlock(&eemcs_stat_callback_head.mutex);
    return 0;
}

kal_bool eemcs_state_callback_unregister(EEMCS_STATE_CALLBACK_T *node){
    DBGLOG(INIT, DBG, "%s: node=%p", FUNC_NAME, node);
    mutex_lock(&eemcs_stat_callback_head.mutex);
    list_del(&node->list);
    mutex_unlock(&eemcs_stat_callback_head.mutex);
    return 0;
}

/*
 * @brief Get EEMCS device state.
 * @param
 *     None.
 * @return Current EEMCS device state.
 */
KAL_UINT32 check_device_state(void)
{
    return eemcs_state;
}

/*
 * @brief Check if EEMCS driver is ready.
 * @param
 *     None.
 * @return 1 indicates success. Othrewise 0.
 */
kal_bool eemcs_device_ready(void)
{
#if defined(__EEMCS_XBOOT_SUPPORT__)
    if ((check_device_state() == EEMCS_BOOTING_DONE) || (check_device_state() == EEMCS_EXCEPTION))
        return 1;
	else
		return 0;
#else // !__EEMCS_XBOOT_SUPPORT__
    return 1;
#endif // __EEMCS_XBOOT_SUPPORT__
}
/* EEMCS_EXCEPTION */
#define CHANGE_STATE_ERR(old_state, new_state) \
    do { \
        DBGLOG(BOOT, ERR, "eemcs state change fail: %s->%s", \
        	eemcs_state_name[old_state], eemcs_state_name[new_state]); \
    } while (0)

/*
 * @brief Change the state of EEMCS driver.
 * @param
 *     state [in] New state.
 * @return true indicates success. Othrewise false.
 */
kal_bool change_device_state(KAL_UINT32 state)
{
	DBGLOG(BOOT, STA, "%s(%d) -> %s(%d)", eemcs_state_name[eemcs_state], eemcs_state, \
		eemcs_state_name[state], state);

    if(EEMCS_GATE == state){
        eemcs_state = EEMCS_GATE;
        eemcs_state_callback(eemcs_state);
        eemcs_boot_trace_state(state);
        return true;
    }
	
    switch (eemcs_state)
    {
        case EEMCS_INVALID:
        {
            if (state == EEMCS_GATE) {
                eemcs_state = EEMCS_GATE;
            } else {
                CHANGE_STATE_ERR(eemcs_state, state);
            }
        }
        break;

        case EEMCS_GATE:
        {
            if (state == EEMCS_INIT) {
                eemcs_state = EEMCS_INIT;
            } 
            else if (state == EEMCS_XBOOT) { //add for handle GATE while xboot fail retry
                eemcs_state = EEMCS_XBOOT;
		    }
            else {
                CHANGE_STATE_ERR(eemcs_state, state);
            }
        }
        break;
        
        case EEMCS_INIT:
        {
            if (state == EEMCS_XBOOT) {
                eemcs_state = EEMCS_XBOOT;
		    } else {
                CHANGE_STATE_ERR(eemcs_state, state);
            }
        }
        break;
        
        case EEMCS_XBOOT:
        {
            if (state == EEMCS_MOLY_HS_P1) {
                eemcs_state = EEMCS_MOLY_HS_P1;
                //Enable WDT Interrupt Mask, shall make sure md WDT setting is ready
                mtlte_enable_WDT_flow();
            } else if (state == EEMCS_XBOOT) {
                eemcs_state = EEMCS_XBOOT;
                DBGLOG(BOOT, STA, "RST!! BROM SDIO no response EEMCS_XBOOT -> EEMCS_XBOOT");
            } else {
                CHANGE_STATE_ERR(eemcs_state, state);
            }
        }
        break;
        
        case EEMCS_MOLY_HS_P1:
        {
            if (state == EEMCS_MOLY_HS_P2) {
                eemcs_state = EEMCS_MOLY_HS_P2;
            } 
            else if (state == EEMCS_EXCEPTION) {
                eemcs_state = EEMCS_EXCEPTION;
            }
            else if (state == EEMCS_XBOOT) {
                eemcs_state = EEMCS_XBOOT;
                DBGLOG(BOOT, STA, "RST!! EEMCS_MOLY_HS_P1 no response EEMCS_MOLY_HS_P1 -> EEMCS_XBOOT");
            }
            else {
                CHANGE_STATE_ERR(eemcs_state, state);
            }
        }    
        break;

        case EEMCS_MOLY_HS_P2:
        {
            if (state == EEMCS_BOOTING_DONE) {
                eemcs_state = EEMCS_BOOTING_DONE;
            } 
            else if (state == EEMCS_EXCEPTION) {
                eemcs_state = EEMCS_EXCEPTION;
            }
            else if (state == EEMCS_XBOOT) {
                eemcs_state = EEMCS_XBOOT;
                DBGLOG(BOOT, STA, "RST!! EEMCS_MOLY_HS_P2 no response EEMCS_MOLY_HS_P2 -> EEMCS_XBOOT");
            }
            else {
                CHANGE_STATE_ERR(eemcs_state, state);
            }
        }    
        break;

		case EEMCS_BOOTING_DONE:
        {
			if ((EEMCS_EXCEPTION != state) && (EEMCS_GATE != state)){
            	CHANGE_STATE_ERR(eemcs_state, state);
			}
            eemcs_state = state;
        }
        break;		    

        case EEMCS_EXCEPTION:
        {
            eemcs_state = state;
        }
        break;

        default:
            return false;
    }
    eemcs_state_callback(eemcs_state);
    eemcs_boot_trace_state(state);

    return true;
}
bool eemcs_on_reset(void){
    return ((EEMCS_GATE == eemcs_state) ? true:false);
}
