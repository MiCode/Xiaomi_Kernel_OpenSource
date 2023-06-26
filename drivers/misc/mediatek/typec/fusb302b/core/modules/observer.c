/*******************************************************************************
 * @file     observer.c
 * @author   USB PD Firmware Team
 *
 * Copyright 2018 ON Semiconductor. All rights reserved.
 *
 * This software and/or documentation is licensed by ON Semiconductor under
 * limited terms and conditions. The terms and conditions pertaining to the
 * software and/or documentation are available at
 * http://www.onsemi.com/site/pdf/ONSEMI_T&C.pdf
 * ("ON Semiconductor Standard Terms and Conditions of Sale, Section 8 Software").
 *
 * DO NOT USE THIS SOFTWARE AND/OR DOCUMENTATION UNLESS YOU HAVE CAREFULLY
 * READ AND YOU AGREE TO THE LIMITED TERMS AND CONDITIONS. BY USING THIS
 * SOFTWARE AND/OR DOCUMENTATION, YOU AGREE TO THE LIMITED TERMS AND CONDITIONS.
 ******************************************************************************/
#include "observer.h"

/**
 * Private structure for manipulation list of observers
 */
typedef struct
{
	FSC_U32 event;
    EventHandler event_handler;
    void *context;
} Observer_t;

/**
 * Private list of registered observers. upto MAX_OBSERVERS
 */
typedef struct
{
	FSC_U8 obs_count;
    Observer_t list[MAX_OBSERVERS];
} ObserversList_t;

static ObserversList_t observers = {0};

FSC_BOOL register_observer(FSC_U32 event, EventHandler handler, void *context)
{
    FSC_BOOL status = FALSE;
    if (observers.obs_count < MAX_OBSERVERS)
    {
        observers.list[observers.obs_count].event = event;
        observers.list[observers.obs_count].event_handler = handler;
        observers.list[observers.obs_count].context = context;
        observers.obs_count++;
        status = TRUE;
    }
    return status;
}

/**
 * It is slightly expensive to remove since all pointers have to be checked
 * and moved in array when an observer is deleted.
 */
void remove_observer(EventHandler handler)
{
	FSC_U32 i, j = 0;
    FSC_BOOL status = FALSE;

    /* Move all the observer pointers in arary and over-write the
     * one being deleted */
    for (i = 0; i < observers.obs_count; i++)
    {
        if (observers.list[i].event_handler == handler)
        {
            /* Object found */
            status = TRUE;
            continue;
        }
        observers.list[j] = observers.list[i];
        j++;
    }

    /* If observer was found and removed decrement the count */
    if (status == TRUE)
    {
        observers.obs_count--;
    }
}

void notify_observers(FSC_U32 event, FSC_U8 portId, void *app_ctx)
{
    FSC_U32 i;
    for (i = 0; i < observers.obs_count; i++)
    {
        if (observers.list[i].event & event)
        {
            observers.list[i].event_handler(event, portId,
                                            observers.list[i].context, app_ctx);
        }
    }
}

