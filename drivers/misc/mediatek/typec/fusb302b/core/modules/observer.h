/*******************************************************************************
 * @file     observer.h
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
#ifndef MODULES_OBSERVER_H_
#define MODULES_OBSERVER_H_

#include "platform.h"

/** if MAX_OBSERVERS are not defined either at compile time or in platform.h */
#ifndef MAX_OBSERVERS
#define MAX_OBSERVERS       10
#endif ///< MAX_OBSERVERS

typedef void (*EventHandler)(FSC_U32 event, FSC_U8 portId,
                             void *usr_ctx, void *app_ctx);

/**
 * @brief register an observer.
 * @param[in] event to subscribe
 * @param[in] handler to be called
 * @param[in] context data sent to the handler
 */
FSC_BOOL register_observer(FSC_U32 event, EventHandler handler, void *context);

/**
 * @brief removes the observer. Observer stops getting notified
 * @param[in] handler handler to remove
 */
void remove_observer(EventHandler handler);

/**
 * @brief notifies all observer that are listening to the event.
 * @param[in] event that occured
 */
void notify_observers(FSC_U32 event, FSC_U8 portId, void *app_ctx);


#endif /* MODULES_OBSERVER_H_ */
