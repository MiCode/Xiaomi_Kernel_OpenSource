/*
** =============================================================================
** Copyright (c) 2003-2012  Immersion Corporation.  All rights reserved.
**                          Immersion Corporation Confidential and Proprietary.
** Copyright (C) 2015 XiaoMi, Inc.
** =============================================================================
*/
/**
\file   ImmVibe.h
\brief  Main header file for the \api.
*/

#ifndef _IMMVIBE_H
#define _IMMVIBE_H

#include "ImmVibeCore.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifndef IMMVIBEAPI
    #if defined(WIN32)
        #define IMMVIBEAPI __declspec(dllimport)
    #else
        #define IMMVIBEAPI extern
    #endif
#endif

/** \brief Pointer to the internal IVT file that contains built-in effects. */
#ifdef IMMVIBEAPI_STUB
IMMVIBEAPI VibeUInt8 *g_pVibeIVTBuiltInEffects;
#else
IMMVIBEAPI VibeUInt8 g_pVibeIVTBuiltInEffects[];
#endif
/**
\defgroup builtineffects Built-in Effects

        Used with #ImmVibePlayIVTEffect when playing effects from the IVT data
        pointed to by #g_pVibeIVTBuiltInEffects.
*/
/*@{*/

/**
\brief  Effect index for the short-on-short-off effect in the built-in IVT file.

        This effect plays a short vibration followed by a short rest, repeated
        indefinitely.
*/
#define VIBE_BUILTINEFFECT_SHORT_ON_SHORT_OFF	0

/**
\brief  Effect index for the short-on-medium-off effect in the built-in IVT
        file.

        This effect plays a short vibration followed by a medium rest, repeated
        indefinitely.
*/
#define VIBE_BUILTINEFFECT_SHORT_ON_MEDIUM_OFF	1

/**
\brief  Effect index for the short-on-long-off effect in the built-in IVT file.

        This effect plays a short vibration followed by a long rest, repeated
        indefinitely.
*/
#define VIBE_BUILTINEFFECT_SHORT_ON_LONG_OFF	2

/**
\brief  Effect index for the medium-on-short-off effect in the built-in IVT
        file.

        This effect plays a medium vibration followed by a short rest, repeated
        indefinitely.
*/
#define VIBE_BUILTINEFFECT_MEDIUM_ON_SHORT_OFF	3

/**
\brief  Effect index for the medium-on-medium-off effect in the built-in IVT
        file.

        This effect plays a medimum vibration followed by a medimum rest,
        repeated indefinitely.
*/
#define VIBE_BUILTINEFFECT_MEDIUM_ON_MEDIUM_OFF 4


/**
\brief  Effect index for the medium-on-long-off effect in the built-in IVT file.

        This effect plays a medimum vibration followed by a long rest, repeated
        indefinitely.
*/
#define VIBE_BUILTINEFFECT_MEDIUM_ON_LONG_OFF	5


/**
\brief  Effect index for the long-on-short-off effect in the built-in IVT file.

        This effect plays a long vibration followed by a short rest, repeated
        indefinitely.
*/
#define VIBE_BUILTINEFFECT_LONG_ON_SHORT_OFF	6

/**
\brief  Effect index for the long-on-medium-off effect in the built-in IVT file.

        This effect plays a long vibration followed by a medimum rest, repeated
        indefinitely.
*/
#define VIBE_BUILTINEFFECT_LONG_ON_MEDIUM_OFF	7

/**
\brief  Effect index for the long-on-long-off effect in the built-in IVT file.

        This effect plays a long vibration followed by a long rest, repeated
        indefinitely.
*/
#define VIBE_BUILTINEFFECT_LONG_ON_LONG_OFF		8

/**
\brief  Effect index for the short effect in the built-in IVT file.

        This effect plays a short vibration once.
*/
#define VIBE_BUILTINEFFECT_SHORT				9

/**
\brief  Effect index for the medimum effect in the built-in IVT file.

        This effect plays a medimum vibration once.
*/
#define VIBE_BUILTINEFFECT_MEDIUM				10

/**
\brief  Effect index for the long effect in the built-in IVT file.

        This effect plays a long vibration once.
*/
#define VIBE_BUILTINEFFECT_LONG					11

/*@}*/

/**
\brief  Current version number of the \api.

        See \ref versioning for details about \api version numbers.
*/
#define   VIBE_CURRENT_VERSION_NUMBER  0x307010A

/**
\brief  Initializes the \api.

\sync

\remark #ImmVibeInitialize must be called before all other \api functions are
        called. Every call to #ImmVibeInitialize should have a matching call to
        #ImmVibeTerminate. Normally, client applications call #ImmVibeInitialize
        during application startup, and call #ImmVibeTerminate during
        application shutdown.
\remark The \a nVersion parameter allows the \api to verify at run time that the
        client application was compiled with a version of the \apibrief that is
        binary compatible with the run-time version. Ensuring binary
        compatibility at run time is important on platforms such as Windows in
        which the \apibrief is dynamically linked with the client application.
        See \ref versioning for details about \apibrief version numbers.

\param[in]      nVersion    Version number of the \api with which the client
                            application was compiled. Normally, this parameter
                            is set to #VIBE_CURRENT_VERSION_NUMBER. See Remarks.

\retval VIBE_S_SUCCESS      No error.
\retval VIBE_E_INVALID_ARGUMENT
                            The \a nVersion parameter specifies a version number
                            that is not binary compatible with the run-time
                            version of the \api. See Remarks.
\retval VIBE_E_FAIL         Error initializing the \api.
\retval VIBE_E_NOT_ENOUGH_MEMORY
                            The function cannot allocate memory to complete the
                            request. This happens if the device runs low in
                            memory.
\retval VIBE_E_SERVICE_NOT_RUNNING
                            The \service is not running. You may need to reboot
                            the device or reinstall the \api to restore the
                            default settings.
\retval VIBE_E_SERVICE_BUSY The \service is busy and could not complete the
                            requested function call.
\if limo
\retval VIBE_E_NOT_SUPPORTED
                            The \service is not available on the target device.
\endif

\externappclients
*/
IMMVIBEAPI VibeStatus ImmVibeInitialize
(
	VibeInt32 nVersion
);

/**
\brief  Closes down the \api.

\sync

\remark #ImmVibeTerminate must be called after all other \api functions are
        called. Every call to #ImmVibeInitialize should have a matching call to
        #ImmVibeTerminate. Normally, client applications call #ImmVibeInitialize
        during application startup and call #ImmVibeTerminate during application
        shutdown.

\retval VIBE_S_SUCCESS      No error.
\retval VIBE_E_NOT_INITIALIZED
                            The \api has not been initialized.
                            #ImmVibeInitialize was not called or returned an
                            error.
\retval VIBE_E_FAIL         Error closing down the \api.
\retval VIBE_E_SERVICE_BUSY The \service is busy and could not complete the
                            requested function call.
\if limo
\retval VIBE_E_NOT_SUPPORTED
                            The \service is not available on the target device.
\endif

\externappclients
*/
IMMVIBEAPI VibeStatus ImmVibeTerminate(void);

/**
\brief  Gets the number of available devices.

\sync

\remark On platforms such as Windows that support a dynamic device
        configuration, the list of supported devices is determined during the
        call to #ImmVibeInitialize by enumerating the available device drivers.
        The list of supported devices includes attached devices but may also
        include devices that are not physically connected.
        #ImmVibeGetDeviceCount returns at least one device, a virtual generic
        device, if no physical device is present.
\remark On embedded systems, where the device configuration is static, a virtual
        generic device is not provided. The device count corresponds to the
        actual number of devices that are present on the system.

\retval Positive            Number of available devices.
\retval VIBE_E_NOT_INITIALIZED
                            The \api has not been initialized.
                            #ImmVibeInitialize was not called or returned an
                            error.
\retval VIBE_E_FAIL         Error getting the number of available devices.
\retval VIBE_E_SERVICE_BUSY The \service is busy and could not complete the
                            requested function call.
\if limo
\retval VIBE_E_NOT_SUPPORTED
                            The \service is not available on the target device.
\endif

\externappclients
*/
IMMVIBEAPI VibeStatus ImmVibeGetDeviceCount(void);

/**
\brief  Gets the status bits of a device.

\sync

\param[in]      nDeviceIndex
                            Index of the device for which to get the status
                            bits. The index of the device must be greater than
                            or equal to \c 0 and less than the number of devices
                            returned by #ImmVibeGetDeviceCount.
\param[out]     pnState     Pointer to the variable that will receive the status
                            bits of the device. See \ref devstates for a list of
                            the valid status bits.

\retval VIBE_S_SUCCESS      No error.
\retval VIBE_E_NOT_INITIALIZED
                            The \api has not been initialized.
                            #ImmVibeInitialize was not called or returned an
                            error.
\retval VIBE_E_INVALID_ARGUMENT
                            One or more of the arguments are invalid. For
                            example:
                            -   The value of the \a nDeviceIndex parameter is
                                negative.
                            -   The value of the \a nDeviceIndex parameter is
                                greater than or equal to the number of devices
                                returned by #ImmVibeGetDeviceCount.
                            -   The \a pnState parameter is \c NULL.
\retval VIBE_E_FAIL         Error getting the status bits.
\retval VIBE_E_SERVICE_BUSY The \service is busy and could not complete the
                            requested function call.
\if limo
\retval VIBE_E_NOT_SUPPORTED
                            The \service is not available on the target device.
\endif

\externappclients
*/
IMMVIBEAPI VibeStatus ImmVibeGetDeviceState
(
	VibeInt32 nDeviceIndex,
    VibeInt32 *pnState
);

/**
\brief  Gets a Boolean capability of a device.

\sync

\remark This function is provided for future use. The devices supported by the
        current version of the \api do not have Boolean capabilities.

\param[in]      nDeviceIndex
                            Index of the device for which to get a Boolean
                            capability. The index of the device must be greater
                            than or equal to \c 0 and less than the number of
                            devices returned by #ImmVibeGetDeviceCount.
\param[in]      nDevCapType Capability type of the Boolean capability to get.
                            See Remarks.
\param[out]     pbDevCapVal Pointer to the variable that will receive the
                            requested Boolean capability value.

\retval VIBE_S_SUCCESS      No error.
\retval VIBE_E_NOT_INITIALIZED
                            The \api has not been initialized.
                            #ImmVibeInitialize was not called or returned an
                            error.
\retval VIBE_E_INVALID_ARGUMENT
                            One or more of the arguments are invalid. For
                            example:
                            -   The value of the \a nDeviceIndex parameter is
                                negative.
                            -   The value of the \a nDeviceIndex parameter is
                                greater than or equal to the number of devices
                                returned by #ImmVibeGetDeviceCount.
                            -   The \a pnDevCapVal parameter is \c NULL.
\retval VIBE_E_INCOMPATIBLE_CAPABILITY_TYPE
                            The \a nDevCapType parameter specifies an invalid
                            capability type for a Boolean capability of the
                            device. See Remarks.
\retval VIBE_E_SERVICE_BUSY The \service is busy and could not complete the
                            requested function call.
\if limo
\retval VIBE_E_NOT_SUPPORTED
                            The \service is not available on the target device.
\endif

\externappclients
*/
IMMVIBEAPI VibeStatus ImmVibeGetDeviceCapabilityBool
(
    VibeInt32 nDeviceIndex,
    VibeInt32 nDevCapType,
    VibeBool *pbDevCapVal
);

/**
\brief  Gets a 32-bit integer capability of a device.

\sync

\param[in]      nDeviceIndex
                            Index of the device for which to get a 32-bit
                            integer capability. The index of the device must be
                            greater than or equal to \c 0 and less than the
                            number of devices returned by
                            #ImmVibeGetDeviceCount.
\param[in]      nDevCapType Capability type of the 32-bit integer capability to
                            get. See \ref devcaps for a list of the valid
                            capability types.
\param[out]     pnDevCapVal Pointer to the variable that will receive the
                            requested 32-bit integer capability value.

\retval VIBE_S_SUCCESS      No error.
\retval VIBE_E_NOT_INITIALIZED
                            The \api has not been initialized.
                            #ImmVibeInitialize was not called or returned an
                            error.
\retval VIBE_E_INVALID_ARGUMENT
                            One or more of the arguments are invalid. For
                            example:
                            -   The value of the \a nDeviceIndex parameter is
                                negative.
                            -   The value of the \a nDeviceIndex parameter is
                                greater than or equal to the number of devices
                                returned by #ImmVibeGetDeviceCount.
                            -   The \a pnDevCapVal parameter is \c NULL.
\retval VIBE_E_INCOMPATIBLE_CAPABILITY_TYPE
                            The \a nDevCapType parameter specifies an invalid
                            capability type for a 32-bit integer capability of
                            the device. See \ref devcaps for a list of the valid
                            capability types.
\retval VIBE_E_SERVICE_BUSY The \service is busy and could not complete the
                            requested function call.
\if limo
\retval VIBE_E_NOT_SUPPORTED
                            The \service is not available on the target device.
\endif

\externappclients
*/
IMMVIBEAPI VibeStatus ImmVibeGetDeviceCapabilityInt32
(
    VibeInt32 nDeviceIndex,
    VibeInt32 nDevCapType,
    VibeInt32 *pnDevCapVal
);

/**
\brief  Gets a string capability of a device.

\sync

\param[in]      nDeviceIndex
                            Index of the device for which to get a string
                            capability. The index of the device must be greater
                            than or equal to \c 0 and less than the number of
                            devices returned by #ImmVibeGetDeviceCount.
\param[in]      nDevCapType Capability type of the string capability to get. See
                            \ref devcaps for a list of the valid capability
                            types.
\param[in]      nSize       Size of the buffer, in bytes, pointed to by the \a
                            szDevCapVal parameter. Normally the buffer should
                            have a size greater than or equal to
                            #VIBE_MAX_CAPABILITY_STRING_LENGTH.
\param[out]     szDevCapVal Pointer to the character buffer that will receive
                            the requested string capability value. The size of
                            the buffer must be greater than or equal to the
                            value of the \a nSize parameter.

\retval VIBE_S_SUCCESS      No error.
\retval VIBE_E_NOT_INITIALIZED
                            The \api has not been initialized.
                            #ImmVibeInitialize was not called or returned an
                            error.
\retval VIBE_E_INVALID_ARGUMENT
                            One or more of the arguments are invalid. For
                            example:
                            -   The value of the \a nDeviceIndex parameter is
                                negative.
                            -   The value of the \a nDeviceIndex parameter is
                                greater than or equal to the number of devices
                                returned by #ImmVibeGetDeviceCount.
                            -   The value of the \a nSize parameter is less than
                                or equal to the length of the requested string.
                            -   The \a pnDevCapVal parameter is \c NULL.
\retval VIBE_E_INCOMPATIBLE_CAPABILITY_TYPE
                            The \a nDevCapType parameter specifies an invalid
                            capability type for a string capability of the
                            device. See \ref devcaps for a list of the valid
                            capability types.
\retval VIBE_E_SERVICE_BUSY The \service is busy and could not complete the
                            requested function call.
\if limo
\retval VIBE_E_NOT_SUPPORTED
                            The \service is not available on the target device.
\endif

\externappclients
*/
IMMVIBEAPI VibeStatus ImmVibeGetDeviceCapabilityString
(
    VibeInt32 nDeviceIndex,
    VibeInt32 nDevCapType,
    VibeInt32 nSize,
    char *szDevCapVal
);

/**
\brief  Opens a device.

\sync

\remark Every call to #ImmVibeOpenDevice or #ImmVibeOpenCompositeDevice should
        have a matching call to #ImmVibeCloseDevice for the same device.
        Normally, client applications call #ImmVibeOpenDevice or
        #ImmVibeOpenCompositeDevice during application startup and call
        #ImmVibeCloseDevice during application shutdown.
\remark On Windows, #ImmVibeOpenDevice always returns the same device
        handle when using a tethered device containing the 1.0 version of the
        \player. See \ref versioning for details about \api version numbers.


\param[in]      nDeviceIndex
                            Index of the device to open. The index must be
                            greater than or equal to \c 0 and less than the
                            number of devices returned by
                            #ImmVibeGetDeviceCount.
\param[out]     phDeviceHandle
                            Pointer to the variable that will receive a handle
                            to the open device.

\retval VIBE_S_SUCCESS      No error.
\retval VIBE_E_NOT_INITIALIZED
                            The \api has not been initialized.
                            #ImmVibeInitialize was not called or returned an
                            error.
\retval VIBE_E_INVALID_ARGUMENT
                            One or more of the arguments are invalid. For
                            example:
                            -   The value of the \a nDeviceIndex parameter is
                                negative.
                            -   The value of the \a nDeviceIndex parameter is
                                greater than or equal to the number of devices
                                returned by #ImmVibeGetDeviceCount.
                            -   The \a phDeviceHandle parameter is \c NULL.
\retval VIBE_E_FAIL         Error opening the device.
\retval VIBE_E_NOT_ENOUGH_MEMORY
                            The function cannot allocate memory to complete the
                            request. This happens if the device runs low in
                            memory.
\retval VIBE_E_SERVICE_BUSY The \service is busy and could not complete the
                            requested function call.
\if limo
\retval VIBE_E_NOT_SUPPORTED
                            The \service is not available on the target device.
\endif

\externappclients
*/
IMMVIBEAPI VibeStatus ImmVibeOpenDevice
(
    VibeInt32 nDeviceIndex,
    VibeInt32 *phDeviceHandle
);

/**
\brief  Opens and initializes a composite device that supports playing different
        effects simultaneously on different actuators.

\details
        Effects may be defined specifying a device index on which to play the
        effect. When played on a composite device, the effects are rendered on
        the actuator corresponding to the specified device index.

\sync

\remark Every call to #ImmVibeOpenDevice or #ImmVibeOpenCompositeDevice should
        have a matching call to #ImmVibeCloseDevice for the same device.
        Normally, client applications call #ImmVibeOpenDevice or
        #ImmVibeOpenCompositeDevice during application startup and call
        #ImmVibeCloseDevice during application shutdown.

\param[in]      pnDeviceIndex
                            Reserved. Set \a pnDeviceIndex to \c NULL.
\param[in]      nNumDevice  Number of actuators to use. The \a nNumDevice
                            parameter must be greater than \c 0 and less than or
                            equal to #VIBE_MAX_LOGICAL_DEVICE_COUNT. To use all
                            available actuators, set \a nNumDevice equal to the
                            number returned by #ImmVibeGetDeviceCount.
\param[out]     phDeviceHandle
                            Pointer to the variable that will receive a handle
                            to the open composite device.

\retval VIBE_S_SUCCESS      No error.
\retval VIBE_E_NOT_INITIALIZED
                            The \api has not been initialized.
                            #ImmVibeInitialize was not called or returned an
                            error.
\retval VIBE_E_INVALID_ARGUMENT
                            One or more of the arguments are invalid. For
                            example:
                            -   The value of the \a nNumDevice parameter is
                                negative, zero, or greater than
                                #VIBE_MAX_LOGICAL_DEVICE_COUNT.
                            -   The \a phDeviceHandle parameter is \c NULL.
\retval VIBE_E_FAIL         Error opening the composite device.
\retval VIBE_E_NOT_ENOUGH_MEMORY
                            The function cannot allocate memory to complete the
                            request. This happens if the device runs low in
                            memory.
\retval VIBE_E_SERVICE_BUSY The \service is busy and could not complete the
                            requested function call.
\retval VIBE_E_NOT_SUPPORTED
                            The tethered handset does not support this function
                            (Windows-only).
\if limo
\retval VIBE_E_NOT_SUPPORTED
                            The \service is not available on the target device.
\endif

\externappclients

\since  Version 3.3. See \ref versioning for details about \api version numbers.
*/
IMMVIBEAPI VibeStatus ImmVibeOpenCompositeDevice
(
    const VibeInt32* pnDeviceIndex,
    VibeUInt32 nNumDevice,
    VibeInt32 *phDeviceHandle
);

/**
\brief  Closes a device previously opened with a successful call to
        #ImmVibeOpenDevice or #ImmVibeOpenCompositeDevice.

\sync

\remark Every call to #ImmVibeOpenDevice or #ImmVibeOpenCompositeDevice should
        have a matching call to #ImmVibeCloseDevice for the same device.
        Normally, client applications call #ImmVibeOpenDevice or
        #ImmVibeOpenCompositeDevice during application startup and call
        #ImmVibeCloseDevice during application shutdown.

\param[in]      hDeviceHandle
                            Device handle to close. The handle must have been
                            obtained by a successful call to #ImmVibeOpenDevice
                            or #ImmVibeOpenCompositeDevice.

\retval VIBE_S_SUCCESS      No error.
\retval VIBE_E_NOT_INITIALIZED
                            The \api has not been initialized.
                            #ImmVibeInitialize was not called or returned an
                            error.
\retval VIBE_E_INVALID_ARGUMENT
                            The \a hDeviceHandle parameter specifies an invalid
                            device handle.
\retval VIBE_E_FAIL         Error closing the device.
\retval VIBE_E_SERVICE_BUSY The \service is busy and could not complete the
                            requested function call.
\if limo
\retval VIBE_E_NOT_SUPPORTED
                            The \service is not available on the target device.
\endif

\externappclients
*/
IMMVIBEAPI VibeStatus ImmVibeCloseDevice
(
	VibeInt32 hDeviceHandle
);

/**
\brief  Gets a Boolean property of an open device.

\sync

\param[in]      hDeviceHandle
                            Device handle for which to get a Boolean property.
                            The handle must have been obtained by calling
                            #ImmVibeOpenDevice or #ImmVibeOpenCompositeDevice.
\param[in]      nDevPropType
                            Property type of the Boolean property to get. See
                            \ref devprops for a list of the valid property
                            types.
\param[out]     pbDevPropVal
                            Pointer to the variable that will receive the
                            requested Boolean property value.

\retval VIBE_S_SUCCESS      No error.
\retval VIBE_E_NOT_INITIALIZED
                            The \api has not been initialized.
                            #ImmVibeInitialize was not called or returned an
                            error.
\retval VIBE_E_INVALID_ARGUMENT
                            One or more of the arguments are invalid. For
                            example:
                            -   The \a hDeviceHandle parameter specifies an
                                invalid device handle.
                            -   The \a pbDevPropVal parameter is \c NULL.
\retval VIBE_E_FAIL         Error getting the requested Boolean property of the
                            device.
\retval VIBE_E_INCOMPATIBLE_PROPERTY_TYPE
                            The \a nDevPropType parameter specifies an invalid
                            property type for a Boolean property of a device.
                            See \ref devprops for a list of the valid property
                            types.
\retval VIBE_E_DEVICE_NEEDS_LICENSE
                            Access to the device is locked until a valid license
                            key is provided. This error may be returned
                            only by \api version 3.2 or lower. Subsequent
                            versions of the \apibrief do not require a license
                            key. See \ref licensing for details about setting
                            license keys.
\retval VIBE_E_SERVICE_BUSY The \service is busy and could not complete the
                            requested function call.
\if limo
\retval VIBE_E_NOT_SUPPORTED
                            The \service is not available on the target device.
\endif

\externappclients

\sa     ImmVibeSetDevicePropertyBool.
*/
IMMVIBEAPI VibeStatus ImmVibeGetDevicePropertyBool
(
    VibeInt32 hDeviceHandle,
    VibeInt32 nDevPropType,
    VibeBool *pbDevPropVal
);

/**
\brief  Sets a Boolean property of an open device.

\sync

\param[in]      hDeviceHandle
                            Device handle for which to set a Boolean property.
                            The handle must have been obtained by calling
                            #ImmVibeOpenDevice or #ImmVibeOpenCompositeDevice.
\param[in]      nDevPropType
                            Property type of the Boolean property to set. See
                            \ref devprops for a list of the valid property
                            types.
\param[in]      bDevPropVal Value to set.

\retval VIBE_S_SUCCESS      No error.
\retval VIBE_E_NOT_INITIALIZED
                            The \api has not been initialized.
                            #ImmVibeInitialize was not called or returned an
                            error.
\retval VIBE_E_INVALID_ARGUMENT
                            One or more of the arguments are invalid. For
                            example:
                            -   The \a hDeviceHandle parameter specifies an
                                invalid device handle.
                            -   The value of the \a bDevPropVal parameter is
                                invalid.
\retval VIBE_E_FAIL         Error setting the property.
\retval VIBE_E_INCOMPATIBLE_PROPERTY_TYPE
                            The \a nDevPropType parameter specifies an invalid
                            property type for a Boolean property of a device.
                            See \ref devprops for a list of the valid property
                            types.
\retval VIBE_E_DEVICE_NEEDS_LICENSE
                            Access to the device is locked until a valid license
                            key is provided. This error may be returned
                            only by \api version 3.2 or lower. Subsequent
                            versions of the \apibrief do not require a license
                            key. See \ref licensing for details about setting
                            license keys.
\retval VIBE_E_SERVICE_BUSY The \service is busy and could not complete the
                            requested function call.
\if limo
\retval VIBE_E_NOT_SUPPORTED
                            The \service is not available on the target device.
\endif

\externappclients

\sa     ImmVibeGetDevicePropertyBool.
*/
IMMVIBEAPI VibeStatus ImmVibeSetDevicePropertyBool
(
    VibeInt32 hDeviceHandle,
    VibeInt32 nDevPropType,
    VibeBool bDevPropVal
);

/**
\brief  Gets a 32-bit integer property of an open device.

\sync

\param[in]      hDeviceHandle
                            Device handle for which to get a 32-bit integer
                            property. The handle must have been obtained by
                            calling #ImmVibeOpenDevice or
                            #ImmVibeOpenCompositeDevice.
\param[in]      nDevPropType
                            Property type of the 32-bit integer property to get.
                            See \ref devprops for a list of the valid property
                            types.
\param[out]     pnDevPropVal
                            Pointer to the variable that will receive the
                            requested 32-bit integer property value.

\retval VIBE_S_SUCCESS      No error.
\retval VIBE_E_NOT_INITIALIZED
                            The \api has not been initialized.
                            #ImmVibeInitialize was not called or returned an
                            error.
\retval VIBE_E_INVALID_ARGUMENT
                            One or more of the arguments are invalid. For
                            example:
                            -   The \a hDeviceHandle parameter specifies an
                                invalid device handle.
                            -   The \a pnDevPropVal parameter is \c NULL.
\retval VIBE_E_FAIL         Error getting the requested 32-bit integer property
                            of the device.
\retval VIBE_E_INCOMPATIBLE_PROPERTY_TYPE
                            The \a nDevPropType parameter specifies an invalid
                            property type for a 32-bit integer property of a
                            device. See \ref devprops for a list of the valid
                            property types.
\retval VIBE_E_DEVICE_NEEDS_LICENSE
                            Access to the device is locked until a valid license
                            key is provided. This error may be returned
                            only by \api version 3.2 or lower. Subsequent
                            versions of the \apibrief do not require a license
                            key. See \ref licensing for details about setting
                            license keys.
\retval VIBE_E_SERVICE_BUSY The \service is busy and could not complete the
                            requested function call.
\if limo
\retval VIBE_E_NOT_SUPPORTED
                            The \service is not available on the target device.
\endif

\externappclients

\sa     ImmVibeSetDevicePropertyInt32.
*/
IMMVIBEAPI VibeStatus ImmVibeGetDevicePropertyInt32
(
    VibeInt32 hDeviceHandle,
    VibeInt32 nDevPropType,
    VibeInt32 *pnDevPropVal
);

/**
\brief  Sets a 32-bit integer property of an open device.

\sync

\param[in]      hDeviceHandle
                            Device handle for which to set a 32-bit integer
                            property. The handle must have been obtained by
                            calling #ImmVibeOpenDevice or
                            #ImmVibeOpenCompositeDevice.
\param[in]      nDevPropType
                            Property type of the 32-bit integer property to set.
                            See \ref devprops for a list of the valid property
                            types.
\param[in]      nDevPropVal Value to set.

\retval VIBE_S_SUCCESS      No error.
\retval VIBE_E_NOT_INITIALIZED
                            The \api has not been initialized.
                            #ImmVibeInitialize was not called or returned an
                            error.
\retval VIBE_E_INVALID_ARGUMENT
                            One or more of the arguments are invalid. For
                            example:
                            -   The \a hDeviceHandle parameter specifies an
                                invalid device handle.
                            -   The value of the \a nDevPropVal parameter is
                                invalid.
\retval VIBE_E_FAIL         Error setting the property.
\retval VIBE_E_INCOMPATIBLE_PROPERTY_TYPE
                            The \a nDevPropType parameter specifies an invalid
                            property type for a 32-bit integer property of a
                            device. See \ref devprops for a list of the valid
                            property types.
\retval VIBE_E_DEVICE_NEEDS_LICENSE
                            Access to the device is locked until a valid license
                            key is provided. This error may be returned
                            only by \api version 3.2 or lower. Subsequent
                            versions of the \apibrief do not require a license
                            key. See \ref licensing for details about setting
                            license keys.
\retval VIBE_E_INSUFFICIENT_PRIORITY
                            Cannot change the #VIBE_DEVPROPTYPE_MASTERSTRENGTH
                            property because the device is not using an OEM
                            license key or the priority is not set to
                            #VIBE_MAX_OEM_DEVICE_PRIORITY. See \ref priority for
                            details about priority levels.
\retval VIBE_E_SERVICE_BUSY The \service is busy and could not complete the
                            requested function call.
\if limo
\retval VIBE_E_NOT_SUPPORTED
                            The \service is not available on the target device.
\endif

\externappclients

\sa     ImmVibeGetDevicePropertyInt32.
*/
IMMVIBEAPI VibeStatus ImmVibeSetDevicePropertyInt32
(
    VibeInt32 hDeviceHandle,
    VibeInt32 nDevPropType,
    VibeInt32 nDevPropVal
);

/**
\brief  Gets a string property of an open device.

\sync

\param[in]      hDeviceHandle
                            Device handle for which to get a string property.
                            The handle must have been obtained by calling
                            #ImmVibeOpenDevice or #ImmVibeOpenCompositeDevice.
\param[in]      nDevPropType
                            Property type of the string property to get. See
                            \ref devprops for a list of the valid property
                            types.
\param[in]      nSize       Size of the buffer, in bytes, pointed to by the
                            \a szDevPropVal parameter. Normally the buffer
                            should have a size greater than or equal to
                            #VIBE_MAX_PROPERTY_STRING_LENGTH.
\param[out]     szDevPropVal
                            Pointer to the variable that will receive the
                            requested string property value. The size of the
                            buffer must be greater than or equal to the value of
                            the \a nSize parameter.

\retval VIBE_S_SUCCESS      No error.
\retval VIBE_E_NOT_INITIALIZED
                            The \api has not been initialized.
                            #ImmVibeInitialize was not called or returned an
                            error.
\retval VIBE_E_INVALID_ARGUMENT
                            One or more of the arguments are invalid. For
                            example:
                            -   The \a hDeviceHandle parameter specifies an
                                invalid device handle.
                            -   The value of the \a nSize parameter is less than
                                or equal to the length of the requested string.
                            -   The \a szDevPropVal parameter is \c NULL.
\retval VIBE_E_FAIL         Error getting the requested string property of the
                            device.
\retval VIBE_E_INCOMPATIBLE_PROPERTY_TYPE
                            The \a nDevPropType parameter specifies an invalid
                            property type for a string property of a device. See
                            \ref devprops for a list of the valid property
                            types.
\retval VIBE_E_DEVICE_NEEDS_LICENSE
                            Access to the device is locked until a valid license
                            key is provided. This error may be returned
                            only by \api version 3.2 or lower. Subsequent
                            versions of the \apibrief do not require a license
                            key. See \ref licensing for details about setting
                            license keys.
\retval VIBE_E_SERVICE_BUSY The \service is busy and could not complete the
                            requested function call.
\if limo
\retval VIBE_E_NOT_SUPPORTED
                            The \service is not available on the target device.
\endif

\externappclients

\sa     ImmVibeSetDevicePropertyString.
*/
IMMVIBEAPI VibeStatus ImmVibeGetDevicePropertyString
(
    VibeInt32 hDeviceHandle,
    VibeInt32 nDevPropType,
    VibeInt32 nSize,
    char *szDevPropVal
);

/**
\brief  Sets a string property of an open device.

\sync

\param[in]      hDeviceHandle
                            Device handle for which to set a string property.
                            The handle must have been obtained by calling
                            #ImmVibeOpenDevice or #ImmVibeOpenCompositeDevice.
\param[in]      nDevPropType
                            Property type of the string property to set. See
                            \ref devprops for a list of the valid property
                            types.
\param[in]      szDevPropVal
                            Pointer to the character buffer containing the
                            string property value to set.

\retval VIBE_S_SUCCESS      No error.
\retval VIBE_E_NOT_INITIALIZED
                            The \api has not been initialized.
                            #ImmVibeInitialize was not called or returned an
                            error.
\retval VIBE_E_INVALID_ARGUMENT
                            One or more of the arguments are invalid. For
                            example:
                            -   The \a hDeviceHandle parameter specifies an
                                invalid device handle.
                            -   The \a szDevPropVal parameter is \c NULL.
                            -   The value of the \a szDevPropVal parameter is
                                invalid.
\retval VIBE_E_FAIL         Error setting the property.
\retval VIBE_E_INCOMPATIBLE_PROPERTY_TYPE
                            The \a nDevPropType parameter specifies an invalid
                            property type for a string property of a device. See
                            \ref devprops for a list of the valid property
                            types.
\retval VIBE_E_DEVICE_NEEDS_LICENSE
                            Access to the device is locked until a valid license
                            key is provided. This error may be returned
                            only by \api version 3.2 or lower. Subsequent
                            versions of the \apibrief do not require a license
                            key. See \ref licensing for details about setting
                            license keys.
\retval VIBE_E_SERVICE_BUSY The \service is busy and could not complete the
                            requested function call.
\if limo
\retval VIBE_E_NOT_SUPPORTED
                            The \service is not available on the target device.
\endif

\externappclients

\sa     ImmVibeGetDevicePropertyString.
*/
IMMVIBEAPI VibeStatus ImmVibeSetDevicePropertyString
(
    VibeInt32 hDeviceHandle,
    VibeInt32 nDevPropType,
    const char *szDevPropVal
);

/**
\brief  Gets number of effects defined in IVT data.

\sync

\param[in]      pIVT        Pointer to IVT data. See \ref ivtfiles for details
                            about IVT data. Use #g_pVibeIVTBuiltInEffects to
                            access built-in IVT effects.

\retval Positive            Number of effects.
\retval VIBE_E_NOT_INITIALIZED
                            Version 2.0.77 and earlier: The \api has not been
                            initialized. #ImmVibeInitialize was not called or
                            returned an error. See \ref versioning for details
                            about \apibrief version numbers.
\retval VIBE_E_INVALID_ARGUMENT
                            The \a pIVT parameter is \c NULL or points to
                            invalid IVT data.
\retval VIBE_E_FAIL         Error getting the number of effects.
\if limo
\retval VIBE_E_NOT_SUPPORTED
                            The \service is not available on the target device.
\endif

\externappclients
*/
IMMVIBEAPI VibeStatus ImmVibeGetIVTEffectCount
(
	const VibeUInt8 *pIVT
);

/**
\brief  Gets the name of an effect defined in IVT data.

\sync

\param[in]      pIVT        Pointer to IVT data. See \ref ivtfiles for details
                            about IVT data. Use #g_pVibeIVTBuiltInEffects to
                            access built-in IVT effects.
\param[in]      nEffectIndex
                            Index of the effect. The index of the effect must be
                            greater than or equal to \c 0 and less than the
                            number of effects returned by
                            #ImmVibeGetIVTEffectCount.
\param[in]      nSize       Size of the buffer, in bytes, pointed by the \a
                            szEffectName parameter. Normally the buffer should
                            have a size greater than or equal to
                            #VIBE_MAX_EFFECT_NAME_LENGTH.
\param[out]     szEffectName
                            Pointer to the character buffer that will receive
                            the name of the effect. The size of the buffer must
                            be greater than or equal to the value of the \a
                            nSize parameter.

\retval VIBE_S_SUCCESS      No error.
\retval VIBE_E_NOT_INITIALIZED
                            Version 2.0.77 and earlier: The \api has not been
                            initialized. #ImmVibeInitialize was not called or
                            returned an error. See \ref versioning for details
                            about \apibrief version numbers.
\retval VIBE_E_INVALID_ARGUMENT
                            One or more of the arguments are invalid. For
                            example:
                            -   The \a pIVT parameter is \c NULL or points to
                                invalid IVT data.
                            -   The value of the \a nEffectIndex parameter is
                                negative.
                            -   The value of the \a nEffectIndex parameter is
                                greater than or equal to the number of effects
                                returned by #ImmVibeGetIVTEffectCount.
                            -   The value of the \a nSize parameter is less than
                                or equal to the length of the requested string.
                            -   The \a szEffectName parameter is \c NULL.
\retval VIBE_E_FAIL         Error getting the name of the effect.
\if limo
\retval VIBE_E_NOT_SUPPORTED
                            The \service is not available on the target device.
\endif

\externappclients

\sa     ImmVibeGetIVTEffectNameU.
*/
IMMVIBEAPI VibeStatus ImmVibeGetIVTEffectName
(
    const VibeUInt8 *pIVT,
    VibeInt32 nEffectIndex,
	VibeInt32 nSize,
    char *szEffectName
);

/**
\brief  Gets the name of an effect defined in IVT data as a string of #VibeWChar
        in UCS-2 format.

\sync

\param[in]      pIVT        Pointer to IVT data. See \ref ivtfiles for details
                            about IVT data. Use #g_pVibeIVTBuiltInEffects to
                            access built-in IVT effects.
\param[in]      nEffectIndex
                            Index of the effect. The index of the effect must be
                            greater than or equal to \c 0 and less than the
                            number of effects returned by
                            #ImmVibeGetIVTEffectCount.
\param[in]      nSize       Size of the buffer, in characters, pointed by the \a
                            szEffectName parameter. Normally the buffer should
                            have a size greater than or equal to
                            #VIBE_MAX_EFFECT_NAME_LENGTH.
\param[out]     szEffectName
                            Pointer to the #VibeWChar buffer that will receive
                            the name of the effect. The size of the buffer must
                            be greater than or equal to the value of the \a
                            nSize parameter.

\retval VIBE_S_SUCCESS      No error.
\retval VIBE_E_INVALID_ARGUMENT
                            One or more of the arguments are invalid. For
                            example:
                            -   The \a pIVT parameter is \c NULL or points to
                                invalid IVT data.
                            -   The value of the \a nEffectIndex parameter is
                                negative.
                            -   The value of the \a nEffectIndex parameter is
                                greater than or equal to the number of effects
                                returned by #ImmVibeGetIVTEffectCount.
                            -   The value of the \a nSize parameter is less than
                                or equal to the length of the requested string.
                            -   The \a szEffectName parameter is \c NULL.
\retval VIBE_E_FAIL         Error getting the name of the effect.
\if limo
\retval VIBE_E_NOT_SUPPORTED
                            The \service is not available on the target device.
\endif

\externappclients

\since  Version 3.0. See \ref versioning for details about \api version numbers.

\sa     ImmVibeGetIVTEffectName.
*/
IMMVIBEAPI VibeStatus ImmVibeGetIVTEffectNameU
(
    const VibeUInt8 *pIVT,
    VibeInt32 nEffectIndex,
	VibeInt32 nSize,
    VibeWChar *szEffectName
);

/**
\brief  Gets the index of an effect defined in IVT data given the name of the
        effect.

\sync

\param[in]      pIVT        Pointer to IVT data. See \ref ivtfiles for details
                            about IVT data. Use #g_pVibeIVTBuiltInEffects to
                            access built-in IVT effects.
\param[in]      szEffectName
                            Pointer to the character buffer containing the name
                            of the effect for which to get the index.
\param[out]     pnEffectIndex
                            Pointer to the variable that will receive the index
                            of the effect.

\retval VIBE_S_SUCCESS      No error.
\retval VIBE_E_NOT_INITIALIZED
                            Version 2.0.77 and earlier: The \api has not been
                            initialized. #ImmVibeInitialize was not called or
                            returned an error. See \ref versioning for details
                            about \apibrief version numbers.
\retval VIBE_E_INVALID_ARGUMENT
                            One or more of the arguments are invalid. For
                            example:
                            -   The \a pIVT parameter is \c NULL or points to
                                invalid IVT data.
                            -   The \a szEffectName parameter is \c NULL.
                            -   The \a pnEffectIndex parameter is \c NULL.
\retval VIBE_E_FAIL         An effect with the given name was not found in the
                            IVT data.
\if limo
\retval VIBE_E_NOT_SUPPORTED
                            The \service is not available on the target device.
\endif

\externappclients

\sa     ImmVibeGetIVTEffectIndexFromNameU.
*/
IMMVIBEAPI VibeStatus ImmVibeGetIVTEffectIndexFromName
(
    const VibeUInt8 *pIVT,
    const char *szEffectName,
    VibeInt32 *pnEffectIndex
);

/**
\brief  Gets the index of an effect defined in IVT data given the name of the
        effect as a string of #VibeWChar in UCS-2 format.

\sync

\param[in]      pIVT        Pointer to IVT data. See \ref ivtfiles for details
                            about IVT data. Use #g_pVibeIVTBuiltInEffects to
                            access built-in IVT effects.
\param[in]      szEffectName
                            Pointer to the #VibeWChar buffer containing the
                            UCS-2 formatted name of the effect for which to get
                            the index.
\param[out]     pnEffectIndex
                            Pointer to the variable that will receive the index
                            of the effect.

\retval VIBE_S_SUCCESS      No error.
\retval VIBE_E_NOT_INITIALIZED
                           The \api has not been initialized. #ImmVibeInitialize
                           was not called or returned an error.
\retval VIBE_E_INVALID_ARGUMENT
                            One or more of the arguments are invalid. For
                            example:
                            -   The \a pIVT parameter is \c NULL or points to
                                invalid IVT data.
                            -   The \a szEffectName parameter is \c NULL.
                            -   The \a pnEffectIndex parameter is \c NULL.
\retval VIBE_E_FAIL         An effect with the given name was not found in the
                            IVT data.
\if limo
\retval VIBE_E_NOT_SUPPORTED
                            The \service is not available on the target device.
\endif

\externappclients

\since  Version 3.0. See \ref versioning for details about \api version numbers.

\sa     ImmVibeGetIVTEffectIndexFromName.
*/
IMMVIBEAPI VibeStatus ImmVibeGetIVTEffectIndexFromNameU
(
    const VibeUInt8 *pIVT,
    const VibeWChar *szEffectName,
    VibeInt32 *pnEffectIndex
);

/**
\brief  Plays an effect defined in IVT data.

\sync

\param[in]      hDeviceHandle
                            Device handle on which to play the effect. The
                            handle must have been obtained by calling
                            #ImmVibeOpenDevice or #ImmVibeOpenCompositeDevice.
\param[in]      pIVT        Pointer to IVT data. See \ref ivtfiles for details
                            about IVT data. Use #g_pVibeIVTBuiltInEffects to
                            access built-in IVT effects.
\param[in]      nEffectIndex
                            Index of the effect. The index of the effect must be
                            greater than or equal to \c 0 and less than the
                            number of effects returned by
                            #ImmVibeGetIVTEffectCount.
\param[out]     phEffectHandle
                            Pointer to the variable that will receive a handle
                            to the playing effect.

\retval VIBE_S_SUCCESS      No error.
\retval VIBE_W_INSUFFICIENT_PRIORITY
                            The device handle does not have enough priority to
                            play the effect. Another effect is currently playing
                            on another higher priority device handle. Before
                            playing the effect, you must call
                            #ImmVibeStopPlayingEffect or
                            #ImmVibeStopAllPlayingEffects to stop currently
                            playing effects, or call
                            #ImmVibeSetDevicePropertyInt32 to increase value of
                            the #VIBE_DEVPROPTYPE_PRIORITY property.
\retval  VIBE_W_EFFECTS_DISABLED
                            Effects playing on the device handle have been
                            disabled. Call #ImmVibeSetDevicePropertyInt32 to set
                            the #VIBE_DEVPROPTYPE_DISABLE_EFFECTS property to
                            false to re-enable playing effects on the device
                            handle.
\retval VIBE_E_NOT_INITIALIZED
                           The \api has not been initialized. #ImmVibeInitialize
                           was not called or returned an error.
\retval VIBE_E_INVALID_ARGUMENT
                            One or more of the arguments are invalid. For
                            example:
                            -   The \a hDeviceHandle parameter specifies an
                                invalid device.
                            -   The \a pIVT parameter is \c NULL or points to
                                invalid IVT data.
                            -   The value of the \a nEffectIndex parameter is
                                negative.
                            -   The value of the \a nEffectIndex parameter is
                                greater than or equal to the number of effects
                                returned by #ImmVibeGetIVTEffectCount.
                            -   The \a phEffectHandle parameter is \c NULL.
\retval VIBE_E_FAIL         Error playing the effect.
\retval VIBE_E_DEVICE_NEEDS_LICENSE
                            Access to the device is locked until a valid license
                            key is provided. This error may be returned
                            only by \api version 3.2 or lower. Subsequent
                            versions of the \apibrief do not require a license
                            key. See \ref licensing for details about setting
                            license keys.
\retval VIBE_E_NOT_ENOUGH_MEMORY
                            The function cannot allocate memory to complete the
                            request. This happens if the device runs low in
                            memory.
\retval VIBE_E_SERVICE_BUSY The \service is busy and could not complete the
                            requested function call.
\if limo
\retval VIBE_E_NOT_SUPPORTED
                            The \service is not available on the target device.
\endif

\externappclients

\sa     ImmVibePlayIVTEffectRepeat.
*/
IMMVIBEAPI VibeStatus ImmVibePlayIVTEffect
(
    VibeInt32 hDeviceHandle,
    const VibeUInt8 *pIVT,
    VibeInt32 nEffectIndex,
    VibeInt32 *phEffectHandle
);

/**
\brief  Repeatedly plays a Timeline effect defined in IVT data.

\sync

\remark The current implementation of #ImmVibePlayIVTEffectRepeat repeats only
        Timeline effects. If the given effect index refers to a simple effect,
        #ImmVibePlayIVTEffectRepeat ignores the \a nRepeat parameter and plays
        the simple effect once. In that case, #ImmVibePlayIVTEffectRepeat
        behaves like #ImmVibePlayIVTEffect. #ImmVibePlayIVTEffectRepeat does not
        return a warning when requested to repeat a simple effect.

\param[in]      hDeviceHandle
                            Device handle on which to play the effect. The
                            handle must have been obtained by calling
                            #ImmVibeOpenDevice or #ImmVibeOpenCompositeDevice.
\param[in]      pIVT        Pointer to IVT data. See \ref ivtfiles for details
                            about IVT data. Use #g_pVibeIVTBuiltInEffects to
                            access built-in IVT effects.
\param[in]      nEffectIndex
                            Index of the effect. The index of the effect must be
                            greater than or equal to \c 0 and less than the
                            number of effects returned by
                            #ImmVibeGetIVTEffectCount.
\param[in]      nRepeat     Number of times to repeat the effect. To play the
                            effect indefinitely, set \a nRepeat to
                            #VIBE_REPEAT_COUNT_INFINITE. To repeat the effect a
                            finite number of times, set \a nRepeat to a value
                            from \c 0 to
                            <code>#VIBE_REPEAT_COUNT_INFINITE&nbsp;&ndash;&nbsp;1</code>.
                            The effect can be repeated at most
                            <code>#VIBE_REPEAT_COUNT_INFINITE&nbsp;&ndash;&nbsp;1</code>
                            times. Setting \a nRepeat to \c 0 plays the effect
                            once (repeats the effect zero times) and is
                            equivalent to calling #ImmVibePlayIVTEffect. To stop
                            the effect before it has repeated the requested
                            number of times, or to stop an effect that is
                            playing indefinitely, call
                            #ImmVibeStopPlayingEffect or
                            #ImmVibeStopAllPlayingEffects.
\param[out]     phEffectHandle
                            Pointer to the variable that will receive a handle
                            to the playing effect.

\retval VIBE_S_SUCCESS      No error.
\retval VIBE_W_INSUFFICIENT_PRIORITY
                            The device handle does not have enough priority to
                            play the effect. Another effect is currently playing
                            on another higher priority device handle. Before
                            playing the effect, you must call
                            #ImmVibeStopPlayingEffect or
                            #ImmVibeStopAllPlayingEffects to stop currently
                            playing effects, or call
                            #ImmVibeSetDevicePropertyInt32 to increase value of
                            the #VIBE_DEVPROPTYPE_PRIORITY property.
\retval  VIBE_W_EFFECTS_DISABLED
                            Effects playing on the device handle have been
                            disabled. Call #ImmVibeSetDevicePropertyInt32 to set
                            the #VIBE_DEVPROPTYPE_DISABLE_EFFECTS property to
                            false to re-enable playing effects on the device
                            handle.
\retval VIBE_E_NOT_INITIALIZED
                           The \api has not been initialized. #ImmVibeInitialize
                           was not called or returned an error.
\retval VIBE_E_INVALID_ARGUMENT
                            One or more of the arguments are invalid. For
                            example:
                            -   The \a hDeviceHandle parameter specifies an
                                invalid device handle.
                            -   The \a pIVT parameter is \c NULL or points to
                                invalid IVT data.
                            -   The value of the \a nEffectIndex parameter is
                                negative.
                            -   The value of the \a nEffectIndex parameter is
                                greater than or equal to the number of effects
                                returned by #ImmVibeGetIVTEffectCount.
                            -   The value of the \a nRepeat parameter is
                                negative.
                            -   The \a phEffectHandle parameter is \c NULL.
\retval VIBE_E_FAIL         Error playing the effect.
\retval VIBE_E_DEVICE_NEEDS_LICENSE
                            Access to the device is locked until a valid license
                            key is provided. This error may be returned
                            only by \api version 3.2 or lower. Subsequent
                            versions of the \apibrief do not require a license
                            key. See \ref licensing for details about setting
                            license keys.
\retval VIBE_E_NOT_ENOUGH_MEMORY
                            The function cannot allocate memory to complete the
                            request. This happens if the device runs low in
                            memory.
\retval VIBE_E_SERVICE_BUSY The \service is busy and could not complete the
                            requested function call.
\if limo
\retval VIBE_E_NOT_SUPPORTED
                            The \service is not available on the target device.
\endif

\externappclients
*/
IMMVIBEAPI VibeStatus ImmVibePlayIVTEffectRepeat
(
    VibeInt32 hDeviceHandle,
    const VibeUInt8 *pIVT,
    VibeInt32 nEffectIndex,
    VibeUInt8 nRepeat,
    VibeInt32 *phEffectHandle
);

/**
\brief  Plays an interpolated effect defined in IVT data.

\sync

\param[in]      hDeviceHandle
                            Device handle on which to play the effect. The
                            handle must have been obtained by calling
                            #ImmVibeOpenDevice or #ImmVibeOpenCompositeDevice.
\param[in]      pIVT        Pointer to IVT data. See \ref ivtfiles for details
                            about IVT data. Use #g_pVibeIVTBuiltInEffects to
                            access built-in IVT effects.
\param[in]      nEffectIndex
                            Index of the effect. The index of the effect must be
                            greater than or equal to \c 0 and less than the
                            number of effects returned by
                            #ImmVibeGetIVTEffectCount.
\param[in]      nInterpolant
                            Initial interpolant value for the interpolated effect.
                            The interpolant value must be greater than or equal to \c 0 and
                            less than or equal to #VIBE_MAX_INTERPOLANT.
                            The interpolant value may be subsequently modified by calling
                            #ImmVibeModifyPlayingInterpolatedEffectInterpolant.
\param[out]     phEffectHandle
                            Pointer to the variable that will receive a handle
                            to the playing effect.

\retval VIBE_S_SUCCESS      No error.
\retval VIBE_W_INSUFFICIENT_PRIORITY
                            The device handle does not have enough priority to
                            play the effect. Another effect is currently playing
                            on another higher priority device handle. Before
                            playing the effect, you must call
                            #ImmVibeStopPlayingEffect or
                            #ImmVibeStopAllPlayingEffects to stop currently
                            playing effects, or call
                            #ImmVibeSetDevicePropertyInt32 to increase value of
                            the #VIBE_DEVPROPTYPE_PRIORITY property.
\retval  VIBE_W_EFFECTS_DISABLED
                            Effects playing on the device handle have been
                            disabled. Call #ImmVibeSetDevicePropertyInt32 to set
                            the #VIBE_DEVPROPTYPE_DISABLE_EFFECTS property to
                            false to re-enable playing effects on the device
                            handle.
\retval VIBE_E_NOT_INITIALIZED
                           The \api has not been initialized. #ImmVibeInitialize
                           was not called or returned an error.
\retval VIBE_E_INVALID_ARGUMENT
                            One or more of the arguments are invalid. For
                            example:
                            -   The \a hDeviceHandle parameter specifies an
                                invalid device.
                            -   The \a pIVT parameter is \c NULL or points to
                                invalid IVT data.
                            -   The value of the \a nEffectIndex parameter is
                                negative.
                            -   The value of the \a nEffectIndex parameter is
                                greater than or equal to the number of effects
                                returned by #ImmVibeGetIVTEffectCount.
                            -   The \a phEffectHandle parameter is \c NULL.
                            -   The value of the \a nInterpolant parameter is negative.
                            -   The value of the \a nInterpolant parameter is greater than
                                #VIBE_MAX_INTERPOLANT.
\retval VIBE_E_INCOMPATIBLE_EFFECT_TYPE
                            The effect specified by the \a hEffectHandle
                            parameter is not an interpolated effect.
\retval VIBE_E_FAIL         Error playing the effect.
\retval VIBE_E_DEVICE_NEEDS_LICENSE
                            Access to the device is locked until a valid license
                            key is provided. This error may be returned
                            only by \api version 3.2 or lower. Subsequent
                            versions of the \apibrief do not require a license
                            key. See \ref licensing for details about setting
                            license keys.
\retval VIBE_E_NOT_ENOUGH_MEMORY
                            The function cannot allocate memory to complete the
                            request. This happens if the device runs low in
                            memory.
\retval VIBE_E_SERVICE_BUSY The \service is busy and could not complete the
                            requested function call.
\retval VIBE_E_NOT_SUPPORTED
                            This function is only supported in the \api &ndash;
                            5000 Series. See \ref editions for details about
                            \apibrief editions.
\if limo
                            The \service is not available on the target device.
\endif

\externappclients

\since  Version 3.6. See \ref versioning for details about \api version numbers.
*/

IMMVIBEAPI VibeStatus ImmVibePlayIVTInterpolatedEffect
(
    VibeInt32 hDeviceHandle,
    const VibeUInt8 *pIVT,
    VibeInt32 nEffectIndex,
    VibeInt32 nInterpolant,
    VibeInt32 *phEffectHandle
);

/**
\brief  Stops a playing effect.

\sync

\param[in]      hDeviceHandle
                            Device handle on which to stop the effect. The
                            handle must have been obtained by calling
                            #ImmVibeOpenDevice or #ImmVibeOpenCompositeDevice,
                            and must be the same as the handle that was given
                            when the effect was played or created.
\param[in]      hEffectHandle
                            Handle to the effect to stop. The handle must have
                            been obtained by calling
                            #ImmVibePlayMagSweepEffect,
                            #ImmVibePlayPeriodicEffect,
                            #ImmVibePlayWaveformEffect,
                            #ImmVibeAppendWaveformEffect, #ImmVibePlayIVTEffect,
                            #ImmVibePlayIVTEffectRepeat, or
                            #ImmVibeCreateStreamingEffect.

\retval VIBE_S_SUCCESS      No error.
\retval VIBE_W_NOT_PLAYING  The effect is not playing. The effect may have
                            played to completion, may have been preempted by
                            playing another effect, or #ImmVibeStopPlayingEffect
                            or #ImmVibeStopAllPlayingEffects may have been
                            called to stop the effect.
\retval VIBE_E_NOT_INITIALIZED
                           The \api has not been initialized. #ImmVibeInitialize
                           was not called or returned an error.
\retval VIBE_E_INVALID_ARGUMENT
                            One or more of the arguments are invalid. For
                            example:
                            -   The \a hDeviceHandle parameter specifies an
                                invalid device handle.
                            -   The \a hEffectHandle parameter specifies an
                                invalid effect handle.
                            -   The effect specified by the \a hEffectHandle
                                parameter was not played or created on the
                                device specified by the \a hDeviceHandle
                                parameter.
\retval VIBE_E_FAIL         Error stopping the effect.
\retval VIBE_E_DEVICE_NEEDS_LICENSE
                            Access to the device is locked until a valid license
                            key is provided. This error may be returned
                            only by \api version 3.2 or lower. Subsequent
                            versions of the \apibrief do not require a license
                            key. See \ref licensing for details about setting
                            license keys.
\retval VIBE_E_SERVICE_BUSY The \service is busy and could not complete the
                            requested function call.
\if limo
\retval VIBE_E_NOT_SUPPORTED
                            The \service is not available on the target device.
\endif

\externappclients
*/
IMMVIBEAPI VibeStatus ImmVibeStopPlayingEffect
(
    VibeInt32 hDeviceHandle,
    VibeInt32 hEffectHandle
);

/**
\brief  Gets the type of an effect defined in IVT data.

\sync

\param[in]      pIVT        Pointer to IVT data. See \ref ivtfiles for details
                            about IVT data. Use #g_pVibeIVTBuiltInEffects to
                            access built-in IVT effects.
\param[in]      nEffectIndex
                            Index of the effect. The index of the effect must be
                            greater than or equal to \c 0 and less than the
                            number of effects returned by
                            #ImmVibeGetIVTEffectCount.
\param[out]     pnEffectType
                            Pointer to the variable that will receive the type
                            of the effect.

\retval VIBE_S_SUCCESS      No error.
\retval VIBE_E_NOT_INITIALIZED
                           Version 2.0.77 and earlier: The \api has not been
                           initialized. #ImmVibeInitialize was not called or
                           returned an error. See \ref versioning for details
                           about \apibrief version numbers.
\retval VIBE_E_INVALID_ARGUMENT
                            One or more of the arguments are invalid. For
                            example:
                            -   The \a pIVT parameter is \c NULL or points to
                                invalid IVT data.
                            -   The value of the \a nEffectIndex parameter is
                                negative.
                            -   The value of the \a nEffectIndex parameter is
                                greater than or equal to the number of effects
                                returned by #ImmVibeGetIVTEffectCount.
                            -   The \a pnEffectType parameter is \c NULL.
\retval VIBE_E_FAIL         Error getting the type of the effect.
\if limo
\retval VIBE_E_NOT_SUPPORTED
                            The \service is not available on the target device.
\endif

\externappclients
*/
IMMVIBEAPI VibeStatus ImmVibeGetIVTEffectType
(
    const VibeUInt8 *pIVT,
    VibeInt32 nEffectIndex,
    VibeInt32 *pnEffectType
);

/**
\brief  Gets the parameters of a MagSweep effect defined in IVT data.

\sync

\param[in]      pIVT        Pointer to IVT data. See \ref ivtfiles for details
                            about IVT data. Use #g_pVibeIVTBuiltInEffects to
                            access built-in IVT effects.
\param[in]      nEffectIndex
                            Index of the effect. The index of the effect must be
                            greater than or equal to \c 0 and less than the
                            number of effects returned by
                            #ImmVibeGetIVTEffectCount.
\param[out]     pnDuration  Pointer to the variable that will receive the
                            duration of the effect in milliseconds. If the
                            effect duration is infinite, \c *\a pnDuration is
                            set to #VIBE_TIME_INFINITE; otherwise, \c *\a
                            pnDuration is set to a value from \c 0 to the value
                            returned by #ImmVibeGetDeviceCapabilityInt32 for the
                            #VIBE_DEVCAPTYPE_MAX_EFFECT_DURATION capability
                            type, inclusive. Setting this pointer to \c NULL is
                            permitted and means that the caller is not
                            interested in retrieving this parameter.
\param[out] pnMagnitude     Pointer to the variable that will receive the
                            magnitude of the effect. The effect magnitude goes
                            from #VIBE_MIN_MAGNITUDE to #VIBE_MAX_MAGNITUDE,
                            inclusive. Setting this pointer to \c NULL is
                            permitted and means that the caller is not
                            interested in retrieving this parameter.
\param[out] pnStyle         Pointer to the variable that will receive the style
                            of the effect. See \ref styles for a list of
                            possible effect styles. Setting this pointer to \c
                            NULL is permitted and means that the caller is not
                            interested in retrieving this parameter.
\param[out] pnAttackTime    Pointer to the variable that will receive the attack
                            time of the effect in milliseconds. The attack time
                            goes from \c 0 to the value returned by
                            #ImmVibeGetDeviceCapabilityInt32 for the
                            #VIBE_DEVCAPTYPE_MAX_ENVELOPE_TIME capability type,
                            inclusive. See \ref envelopes for details about
                            effect envelopes. Setting this pointer to \c NULL is
                            permitted and means that the caller is not
                            interested in retrieving this parameter.
\param[out] pnAttackLevel   Pointer to the variable that will receive the attack
                            level of the effect. The attack level goes from
                            #VIBE_MIN_MAGNITUDE to #VIBE_MAX_MAGNITUDE,
                            inclusive. See \ref envelopes for details about
                            effect envelopes. Setting this pointer to \c NULL is
                            permitted and means that the caller is not
                            interested in retrieving this parameter.
\param[out] pnFadeTime      Pointer to the variable that will receive the fade
                            time of the effect in milliseconds. The fade time
                            goes from \c 0 to the value returned by
                            #ImmVibeGetDeviceCapabilityInt32 for the
                            #VIBE_DEVCAPTYPE_MAX_ENVELOPE_TIME capability type,
                            inclusive. See \ref envelopes for details about
                            effect envelopes. Setting this pointer to \c NULL is
                            permitted and means that the caller is not
                            interested in retrieving this parameter.
\param[out] pnFadeLevel     Pointer to the variable that will receive the fade
                            level of the effect. The fade level goes from
                            #VIBE_MIN_MAGNITUDE to #VIBE_MAX_MAGNITUDE,
                            inclusive. See \ref envelopes for details about
                            effect envelopes. Setting this pointer to \c NULL is
                            permitted and means that the caller is not
                            interested in retrieving this parameter.

\retval VIBE_S_SUCCESS      No error.
\retval VIBE_E_NOT_INITIALIZED
                           Version 2.0.77 and earlier: The \api has not been
                           initialized. #ImmVibeInitialize was not called or
                           returned an error. See \ref versioning for details
                           about \apibrief version numbers.
\retval VIBE_E_INVALID_ARGUMENT
                            One or more of the arguments are invalid. For
                            example:
                            -   The \a pIVT parameter is \c NULL or points to
                                invalid IVT data.
                            -   The value of the \a nEffectIndex parameter is
                                negative.
                            -   The value of the \a nEffectIndex parameter is
                                greater than or equal to the number of effects
                                returned by #ImmVibeGetIVTEffectCount.
\retval VIBE_E_FAIL         Error getting the parameters of the MagSweep effect.
\retval VIBE_E_INCOMPATIBLE_EFFECT_TYPE
                            The effect specified by the \a nEffectIndex
                            parameter is not a MagSweep effect.
\if limo
\retval VIBE_E_NOT_SUPPORTED
                            The \service is not available on the target device.
\endif

\externappclients
*/
IMMVIBEAPI VibeStatus ImmVibeGetIVTMagSweepEffectDefinition
(
    const VibeUInt8 *pIVT,
    VibeInt32 nEffectIndex,
    VibeInt32 *pnDuration,
    VibeInt32 *pnMagnitude,
	VibeInt32 *pnStyle,
    VibeInt32 *pnAttackTime,
    VibeInt32 *pnAttackLevel,
    VibeInt32 *pnFadeTime,
    VibeInt32 *pnFadeLevel
);

/**
\brief  Gets the parameters of a Periodic effect defined in IVT data.

\sync

\param[in]      pIVT        Pointer to IVT data. See \ref ivtfiles for details
                            about IVT data. Use #g_pVibeIVTBuiltInEffects to
                            access built-in IVT effects.
\param[in]      nEffectIndex
                            Index of the effect. The index of the effect must be
                            greater than or equal to \c 0 and less than the
                            number of effects returned by
                            #ImmVibeGetIVTEffectCount.
\param[out]     pnDuration  Pointer to the variable that will receive the
                            duration of the effect in milliseconds. If the
                            effect duration is infinite, \c *\a pnDuration is
                            set to #VIBE_TIME_INFINITE; otherwise, \c *\a
                            pnDuration is set to a value from \c 0 to the value
                            returned by #ImmVibeGetDeviceCapabilityInt32 for the
                            #VIBE_DEVCAPTYPE_MAX_EFFECT_DURATION capability
                            type, inclusive. Setting this pointer to \c NULL is
                            permitted and means that the caller is not
                            interested in retrieving this parameter.
\param[out] pnMagnitude     Pointer to the variable that will receive the
                            magnitude of the effect. The effect magnitude goes
                            from #VIBE_MIN_MAGNITUDE to #VIBE_MAX_MAGNITUDE,
                            inclusive. Setting this pointer to \c NULL is
                            permitted and means that the caller is not
                            interested in retrieving this parameter.
\param[out] pnPeriod        Pointer to the variable that will receive the period
                            of the effect in milliseconds. The effect period
                            goes from the value returned by
                            #ImmVibeGetDeviceCapabilityInt32 for the
                            #VIBE_DEVCAPTYPE_MIN_PERIOD capability type to the
                            value returned by #ImmVibeGetDeviceCapabilityInt32
                            for the #VIBE_DEVCAPTYPE_MAX_PERIOD capability type,
                            inclusive. Setting this pointer to \c NULL is
                            permitted and means that the caller is not
                            interested in retrieving this parameter.
\param[out] pnStyleAndWaveType
                            Pointer to the variable that will receive the style
                            and wave type of the effect. See \ref styles for a
                            list of possible effect styles, and \ref wavetypes
                            for a list of possible wave types. Setting this
                            pointer to \c NULL is permitted and means that the
                            caller is not interested in retrieving this
                            parameter.
\param[out] pnAttackTime    Pointer to the variable that will receive the attack
                            time of the effect in milliseconds. The attack time
                            goes from \c 0 to the value returned by
                            #ImmVibeGetDeviceCapabilityInt32 for the
                            #VIBE_DEVCAPTYPE_MAX_ENVELOPE_TIME capability type,
                            inclusive. See \ref envelopes for details about
                            effect envelopes. Setting this pointer to \c NULL is
                            permitted and means that the caller is not
                            interested in retrieving this parameter.
\param[out] pnAttackLevel   Pointer to the variable that will receive the attack
                            level of the effect. The attack level goes from
                            #VIBE_MIN_MAGNITUDE to #VIBE_MAX_MAGNITUDE,
                            inclusive. See \ref envelopes for details about
                            effect envelopes. Setting this pointer to \c NULL is
                            permitted and means that the caller is not
                            interested in retrieving this parameter.
\param[out] pnFadeTime      Pointer to the variable that will receive the fade
                            time of the effect in milliseconds. The fade time
                            goes from \c 0 to the value returned by
                            #ImmVibeGetDeviceCapabilityInt32 for the
                            #VIBE_DEVCAPTYPE_MAX_ENVELOPE_TIME capability type,
                            inclusive. See \ref envelopes for details about
                            effect envelopes. Setting this pointer to \c NULL is
                            permitted and means that the caller is not
                            interested in retrieving this parameter.
\param[out] pnFadeLevel     Pointer to the variable that will receive the fade
                            level of the effect. The fade level goes from
                            #VIBE_MIN_MAGNITUDE to #VIBE_MAX_MAGNITUDE,
                            inclusive. See \ref envelopes for details about
                            effect envelopes. Setting this pointer to \c NULL is
                            permitted and means that the caller is not
                            interested in retrieving this parameter.

\retval VIBE_S_SUCCESS      No error.
\retval VIBE_E_NOT_INITIALIZED
                           Version 2.0.77 and earlier: The \api has not been
                           initialized. #ImmVibeInitialize was not called or
                           returned an error. See \ref versioning for details
                           about \apibrief version numbers.
\retval VIBE_E_INVALID_ARGUMENT
                            One or more of the arguments are invalid. For
                            example:
                            -   The \a pIVT parameter is \c NULL or points to
                                invalid IVT data.
                            -   The value of the \a nEffectIndex parameter is
                                negative.
                            -   The value of the \a nEffectIndex parameter is
                                greater than or equal to the number of effects
                                returned by #ImmVibeGetIVTEffectCount.
\retval VIBE_E_FAIL         Error getting the parameters of the Periodic effect.
\retval VIBE_E_INCOMPATIBLE_EFFECT_TYPE
                            The effect specified by the \a nEffectIndex
                            parameter is not a Periodic effect.
\if limo
\retval VIBE_E_NOT_SUPPORTED
                            The \service is not available on the target device.
\endif

\externappclients
*/
IMMVIBEAPI VibeStatus ImmVibeGetIVTPeriodicEffectDefinition
(
    const VibeUInt8 *pIVT,
    VibeInt32 nEffectIndex,
    VibeInt32 *pnDuration,
    VibeInt32 *pnMagnitude,
    VibeInt32 *pnPeriod,
    VibeInt32 *pnStyleAndWaveType,
    VibeInt32 *pnAttackTime,
    VibeInt32 *pnAttackLevel,
    VibeInt32 *pnFadeTime,
    VibeInt32 *pnFadeLevel
);

/**
\brief  Gets the duration of an effect defined in IVT data.

\sync

\param[in]      pIVT        Pointer to IVT data. See \ref ivtfiles for details
                            about IVT data. Use #g_pVibeIVTBuiltInEffects to
                            access built-in IVT effects.
\param[in]      nEffectIndex
                            Index of the effect. The index of the effect must be
                            greater than or equal to \c 0 and less than the
                            number of effects returned by
                            #ImmVibeGetIVTEffectCount.
\param[out]     pnEffectDuration
                            Pointer to the variable that will receive the
                            duration of the effect. If the duration is infinite,
                            \c *\a pnEffectDuration is set to
                            #VIBE_TIME_INFINITE.

\retval VIBE_S_SUCCESS      No error.
\retval VIBE_E_NOT_INITIALIZED
                           Version 2.0.77 and earlier: The \api has not been
                           initialized. #ImmVibeInitialize was not called or
                           returned an error. See \ref versioning for details
                           about \apibrief version numbers.
\retval VIBE_E_INVALID_ARGUMENT
                            One or more of the arguments are invalid. For
                            example:
                            -   The \a pIVT parameter is \c NULL or points to
                                invalid IVT data.
                            -   The value of the \a nEffectIndex parameter is
                                negative.
                            -   The value of the \a nEffectIndex parameter is
                                greater than or equal to the number of effects
                                returned by #ImmVibeGetIVTEffectCount.
                            -   The \a pnEffectDuration parameter is \c NULL.
\retval VIBE_E_FAIL         Error getting the duration of the effect.
\if limo
\retval VIBE_E_NOT_SUPPORTED
                            The \service is not available on the target device.
\endif

\externappclients

\since  Version 1.5. See \ref versioning for details about \api version numbers.
*/
IMMVIBEAPI VibeStatus ImmVibeGetIVTEffectDuration
(
    const VibeUInt8 *pIVT,
    VibeInt32 nEffectIndex,
    VibeInt32 *pnEffectDuration
);

/**
\brief  Plays a MagSweep effect given the parameters defining the effect.

\sync

\param[in]      hDeviceHandle
                            Device handle on which to play the effect. The
                            handle must have been obtained by calling
                            #ImmVibeOpenDevice or #ImmVibeOpenCompositeDevice.
\param[in]      nDuration   Duration of the effect in milliseconds. To specify
                            an infinite duration, set this parameter to
                            #VIBE_TIME_INFINITE. For a finite duration, the
                            effect duration is clamped to a value from \c 0 to
                            the value returned by
                            #ImmVibeGetDeviceCapabilityInt32 for the
                            #VIBE_DEVCAPTYPE_MAX_EFFECT_DURATION capability
                            type, inclusive.
\param[in]      nMagnitude  Magnitude of the effect. The effect magnitude is
                            clamped to a value from #VIBE_MIN_MAGNITUDE to
                            #VIBE_MAX_MAGNITUDE, inclusive.
\param[in]      nStyle      Style of the effect. See \ref styles for a list of
                            valid effect styles.
\param[in]      nAttackTime Attack time of the effect in milliseconds. The
                            attack time is clamped to a value from \c 0 to the
                            value returned by #ImmVibeGetDeviceCapabilityInt32
                            for the #VIBE_DEVCAPTYPE_MAX_ENVELOPE_TIME
                            capability type, inclusive. See \ref envelopes for
                            details about effect envelopes.
\param[in]      nAttackLevel
                            Attack level of the effect. The attack level is
                            clamped to a value from #VIBE_MIN_MAGNITUDE to
                            #VIBE_MAX_MAGNITUDE, inclusive. See \ref envelopes
                            for details about effect envelopes.
\param[in]      nFadeTime   Fade time of the effect in milliseconds. The fade
                            time is clamped to a value from \c 0 to the value
                            returned by #ImmVibeGetDeviceCapabilityInt32 for the
                            #VIBE_DEVCAPTYPE_MAX_ENVELOPE_TIME capability type,
                            inclusive. See \ref envelopes for details about
                            effect envelopes.
\param[in]      nFadeLevel  Fade level of the effect. The fade level is clamped
                            to a value from #VIBE_MIN_MAGNITUDE to
                            #VIBE_MAX_MAGNITUDE, inclusive. See \ref envelopes
                            for details about effect envelopes.
\param[out]     phEffectHandle
                            Pointer to the variable that will receive a handle
                            to the playing effect.

\retval VIBE_S_SUCCESS      No error.
\retval VIBE_W_NOT_PLAYING  No more effect slots are available to play the
                            effect. Before playing the effect, you must call
                            #ImmVibeStopPlayingEffect or
                            #ImmVibeStopAllPlayingEffects to stop currently
                            playing effects and free an effect slot.
\retval VIBE_W_INSUFFICIENT_PRIORITY
                            The device handle does not have enough priority to
                            play the effect. Another effect is currently playing
                            on another higher priority device handle. Before
                            playing the effect, you must call
                            #ImmVibeStopPlayingEffect or
                            #ImmVibeStopAllPlayingEffects to stop currently
                            playing effects, or call
                            #ImmVibeSetDevicePropertyInt32 to increase value of
                            the #VIBE_DEVPROPTYPE_PRIORITY property.
\retval  VIBE_W_EFFECTS_DISABLED
                            Effects playing on the device handle have been
                            disabled. Call #ImmVibeSetDevicePropertyInt32 to set
                            the #VIBE_DEVPROPTYPE_DISABLE_EFFECTS property to
                            false to re-enable playing effects on the device
                            handle.
\retval VIBE_E_NOT_INITIALIZED
                           The \api has not been initialized. #ImmVibeInitialize
                           was not called or returned an error.
\retval VIBE_E_INVALID_ARGUMENT
                            One or more of the arguments are invalid. For
                            example:
                            -   The \a hDeviceHandle parameter specifies an
                                invalid device handle.
                            -   The value of one or more of the MagSweep effect
                                parameters is invalid.
                            -   The \a phEffectHandle parameter is \c NULL.
\retval VIBE_E_FAIL         Error playing the effect.
\retval VIBE_E_DEVICE_NEEDS_LICENSE
                            Access to the device is locked until a valid license
                            key is provided. This error may be returned
                            only by \api version 3.2 or lower. Subsequent
                            versions of the \apibrief do not require a license
                            key. See \ref licensing for details about setting
                            license keys.
\retval VIBE_E_SERVICE_BUSY The \service is busy and could not complete the
                            requested function call.
\if limo
\retval VIBE_E_NOT_SUPPORTED
                            The \service is not available on the target device.
\endif

\externappclients
*/
IMMVIBEAPI VibeStatus ImmVibePlayMagSweepEffect
(
    VibeInt32 hDeviceHandle,
    VibeInt32 nDuration,
    VibeInt32 nMagnitude,
    VibeInt32 nStyle,
    VibeInt32 nAttackTime,
    VibeInt32 nAttackLevel,
    VibeInt32 nFadeTime,
    VibeInt32 nFadeLevel,
    VibeInt32 *phEffectHandle
);

/**
\brief  Plays a Periodic effect given the parameters defining the effect.

\sync

\param[in]      hDeviceHandle
                            Device handle on which to play the effect. The
                            handle must have been obtained by calling
                            #ImmVibeOpenDevice or #ImmVibeOpenCompositeDevice.
\param[in]      nDuration   Duration of the effect in milliseconds. To specify
                            an infinite duration, set this parameter to
                            #VIBE_TIME_INFINITE. For a finite duration, the
                            effect duration is clamped to a value from \c 0 to
                            the value returned by
                            #ImmVibeGetDeviceCapabilityInt32 for the
                            #VIBE_DEVCAPTYPE_MAX_EFFECT_DURATION capability
                            type, inclusive.
\param[in]      nMagnitude  Magnitude of the effect. The effect magnitude is
                            clamped to a value from #VIBE_MIN_MAGNITUDE to
                            #VIBE_MAX_MAGNITUDE, inclusive.
\param[in]      nPeriod     Period of the effect in milliseconds or
                            microseconds. If the most significant bit is \c 0,
                            the period is in milliseconds. If the most
                            significant bit is \c 1, the period is in
                            microseconds. For code clarity, you can OR the value
                            #VIBE_PERIOD_RESOLUTION_MICROSECOND with the period
                            when the period is in microseconds. The effect
                            period (in milliseconds) should go from the value
                            returned by #ImmVibeGetDeviceCapabilityInt32 for the
                            #VIBE_DEVCAPTYPE_MIN_PERIOD capability type to the
                            value returned by #ImmVibeGetDeviceCapabilityInt32
                            for the #VIBE_DEVCAPTYPE_MAX_PERIOD capability type,
                            inclusive.
\param[in]      nStyleAndWaveType
                            Style and wave type of the effect. See \ref styles
                            for a list of valid effect styles, and \ref
                            wavetypes for a list of valid wave types.
\param[in]      nAttackTime Attack time of the effect in milliseconds. The
                            attack time is clamped to a value from \c 0 to the
                            value returned by #ImmVibeGetDeviceCapabilityInt32
                            for the #VIBE_DEVCAPTYPE_MAX_ENVELOPE_TIME
                            capability type, inclusive. See \ref envelopes for
                            details about effect envelopes.
\param[in]      nAttackLevel
                            Attack level of the effect. The attack level is
                            clamped to a value from #VIBE_MIN_MAGNITUDE to
                            #VIBE_MAX_MAGNITUDE, inclusive. See \ref envelopes
                            for details about effect envelopes.
\param[in]      nFadeTime   Fade time of the effect in milliseconds. The fade
                            time is clamped to a value from \c 0 to the value
                            returned by #ImmVibeGetDeviceCapabilityInt32 for the
                            #VIBE_DEVCAPTYPE_MAX_ENVELOPE_TIME capability type,
                            inclusive. See \ref envelopes for details about
                            effect envelopes.
\param[in]      nFadeLevel  Fade level of the effect. The fade level is clamped
                            to a value from #VIBE_MIN_MAGNITUDE to
                            #VIBE_MAX_MAGNITUDE, inclusive. See \ref envelopes
                            for details about effect envelopes.
\param[out]     phEffectHandle
                            Pointer to the variable that will receive a handle
                            to the playing effect.

\retval VIBE_S_SUCCESS      No error.
\retval VIBE_W_NOT_PLAYING  No more effect slots are available to play the
                            effect. Before playing the effect, you must call
                            #ImmVibeStopPlayingEffect or
                            #ImmVibeStopAllPlayingEffects to stop currently
                            playing effects and free an effect slot.
\retval VIBE_W_INSUFFICIENT_PRIORITY
                            The device handle does not have enough priority to
                            play the effect. Another effect is currently playing
                            on another higher priority device handle. Before
                            playing the effect, you must call
                            #ImmVibeStopPlayingEffect or
                            #ImmVibeStopAllPlayingEffects to stop currently
                            playing effects, or call
                            #ImmVibeSetDevicePropertyInt32 to increase value of
                            the #VIBE_DEVPROPTYPE_PRIORITY property.
\retval  VIBE_W_EFFECTS_DISABLED
                            Effects playing on the device handle have been
                            disabled. Call #ImmVibeSetDevicePropertyInt32 to set
                            the #VIBE_DEVPROPTYPE_DISABLE_EFFECTS property to
                            false to re-enable playing effects on the device
                            handle.
\retval VIBE_E_NOT_INITIALIZED
                           The \api has not been initialized. #ImmVibeInitialize
                           was not called or returned an error.
\retval VIBE_E_INVALID_ARGUMENT
                            One or more of the arguments are invalid. For
                            example:
                            -   The \a hDeviceHandle parameter specifies an
                                invalid device handle.
                            -   The value of one or more of the Periodic effect
                                parameters is invalid.
                            -   The \a phEffectHandle parameter is \c NULL.
\retval VIBE_E_FAIL         Error playing the effect.
\retval VIBE_E_DEVICE_NEEDS_LICENSE
                            Access to the device is locked until a valid license
                            key is provided. This error may be returned
                            only by \api version 3.2 or lower. Subsequent
                            versions of the \apibrief do not require a license
                            key. See \ref licensing for details about setting
                            license keys.
\retval VIBE_E_SERVICE_BUSY The \service is busy and could not complete the
                            requested function call.
\if limo
\retval VIBE_E_NOT_SUPPORTED
                            The \service is not available on the target device.
\endif

\externappclients
*/
IMMVIBEAPI VibeStatus ImmVibePlayPeriodicEffect
(
    VibeInt32 hDeviceHandle,
    VibeInt32 nDuration,
    VibeInt32 nMagnitude,
    VibeInt32 nPeriod,
    VibeInt32 nStyleAndWaveType,
    VibeInt32 nAttackTime,
    VibeInt32 nAttackLevel,
    VibeInt32 nFadeTime,
    VibeInt32 nFadeLevel,
    VibeInt32 *phEffectHandle
);

/**
\brief  Plays a Waveform effect given the parameters defining the effect.

\sync

\param[in]      hDeviceHandle
                            Device handle on which to play the effect. The
                            handle must have been obtained by calling
                            #ImmVibeOpenDevice or #ImmVibeOpenCompositeDevice.
\param[in]      pData       Pointer to the PCM data defining the actuator drive
                            signal for the Waveform effect. The data is
                            formatted in the same way as the PCM data in a WAV
                            file with the exception that only 8-bit or 16-bit
                            mono (not stereo) samples are supported. This
                            parameter must not be \c NULL.
\param[in]      nDataSize   Size of the PCM data in bytes. The size must be
                            greater than zero. The maximum supported size is 16
                            megabytes, or less if limited by platform
                            constraints.
\param[in]      nSamplingRate
                            Sampling rate of PCM data in Hertz (number of
                            samples per second).
\param[in]      nBitDepth   Bit depth of PCM data, or number of bits per sample.
                            The only supported values are 8 or 16; that is, the
                            \player supports only 8-bit or 16-bit PCM data.
\param[in]      nMagnitude  Magnitude of the effect. The effect magnitude is
                            clamped to a value from #VIBE_MIN_MAGNITUDE to
                            #VIBE_MAX_MAGNITUDE, inclusive. When set to
                            #VIBE_MAX_MAGNITUDE, the PCM data samples are not
                            attenuated. For example: an 8-bit sample having a
                            value of 192 will output a drive signal that is
                            approximately 50% of maximum:
                            (192&nbsp;&ndash;&nbsp;128)&nbsp;/&nbsp;127&nbsp;=&nbsp;50%.
                            Magnitude values less than #VIBE_MAX_MAGNITUDE serve
                            to attenuate the PCM data samples.
\param[out]     phEffectHandle
                            Pointer to the variable that will receive a handle
                            to the playing effect.

\retval VIBE_S_SUCCESS      No error.
\retval VIBE_W_NOT_PLAYING  No more effect slots are available to play the
                            effect. Before playing the effect, you must call
                            #ImmVibeStopPlayingEffect or
                            #ImmVibeStopAllPlayingEffects to stop currently
                            playing effects and free an effect slot.
\retval VIBE_W_INSUFFICIENT_PRIORITY
                            The device handle does not have enough priority to
                            play the effect. Another effect is currently playing
                            on another higher priority device handle. Before
                            playing the effect, you must call
                            #ImmVibeStopPlayingEffect or
                            #ImmVibeStopAllPlayingEffects to stop currently
                            playing effects, or call
                            #ImmVibeSetDevicePropertyInt32 to increase value of
                            the #VIBE_DEVPROPTYPE_PRIORITY property.
\retval  VIBE_W_EFFECTS_DISABLED
                            Effects playing on the device handle have been
                            disabled. Call #ImmVibeSetDevicePropertyInt32 to set
                            the #VIBE_DEVPROPTYPE_DISABLE_EFFECTS property to
                            false to re-enable playing effects on the device
                            handle.
\retval VIBE_E_NOT_INITIALIZED
                           The \api has not been initialized. #ImmVibeInitialize
                           was not called or returned an error.
\retval VIBE_E_INVALID_ARGUMENT
                            One or more of the arguments are invalid. For
                            example:
                            -   The \a hDeviceHandle parameter specifies an
                                invalid device handle.
                            -   The value of one or more of the Waveform effect
                                parameters is invalid.
                            -   The \a phEffectHandle parameter is \c NULL.
\retval VIBE_E_FAIL         Error playing the effect.
\retval VIBE_E_NOT_ENOUGH_MEMORY
                            The \api cannot allocate memory to complete the
                            request. This happens if the device runs low in
                            memory.
\retval VIBE_E_SERVICE_BUSY The \service is busy and could not complete the
                            requested function call.
\retval VIBE_E_NOT_SUPPORTED
                            This function is only supported in the \api &ndash;
                            5000 Series. See \ref editions for details about
                            \apibrief editions.
\if limo
\retval VIBE_E_NOT_SUPPORTED
                            The \service is not available on the target device.
\endif

\externappclients

\since  Version 3.4. See \ref versioning for details about \api version numbers.
*/
IMMVIBEAPI VibeStatus ImmVibePlayWaveformEffect
(
    VibeInt32 hDeviceHandle,
    const VibeUInt8 *pData,
    VibeInt32 nDataSize,
    VibeInt32 nSamplingRate,
    VibeInt32 nBitDepth,
    VibeInt32 nMagnitude,
    VibeInt32 *phEffectHandle
);

/**
\brief  Appends PCM data to a playing Waveform effect.

\details
        If the Waveform effect specified by the \a effectHandle parameter is no
        longer playing, this function starts playing a new Waveform effect and is
        equivalent to #ImmVibePlayWaveformEffect.

\sync

\param[in]      hDeviceHandle
                            Handle to the device associated to the effect. The
                            handle to the device must have been obtained from
                            #ImmVibeOpenDevice or #ImmVibepenCompositeDevice.
\param[in]      hEffectHandle
                            Handle to the playing Waveform effect to which to
                            append PCM data. The handle to the effect must have
                            been obtained from #ImmVibePlayWaveformEffect to
                            play the Waveform effect, or from a previous call to
                            #ImmVibeAppendWaveformEffect to append PCM data to
                            the playing Waveform effect. If this parameter
                            represents an invalid effect handle, this function
                            starts playing a new Waveform effect and is
                            equivalent to #ImmVibePlayWaveformEffect.
\param[in]      pData       Pointer to the PCM data to append. This data defines
                            the actuator drive signal for the Waveform effect
                            and is formatted in the same way as the PCM data in
                            a WAV file with the exception that only 8-bit and
                            16-bit mono (not stereo) samples are supported. This
                            parameter must not be \c NULL.
\param[in]      nDataSize   Size of the PCM data to append in bytes. The size
                            must be greater than zero. The maximum supported
                            size is 16 megabytes, or less if limited by platform
                            constraints.
\param[in]      nSamplingRate
                            Sampling rate of PCM data in Hertz (number of
                            samples per second). This value must be the same as
                            the value that was passed to
                            #ImmVibePlayWaveformEffect to play the Waveform
                            effect.
\param[in]      nBitDepth   Bit depth of PCM data, or number of bits per sample.
                            The only supported values are 8 or 16; that is, the
                            \player supports only 8-bit or 16-bit PCM data. This
                            value must be the same as the value that was passed
                            to #ImmVibePlayWaveformEffect to play the Waveform
                            effect.
\param[in]      nMagnitude  Magnitude of the effect. The effect magnitude is
                            clamped to a value from #VIBE_MIN_MAGNITUDE to
                            #VIBE_MAX_MAGNITUDE, inclusive. When set to
                            #VIBE_MAX_MAGNITUDE, the PCM data samples are not
                            attenuated. For example: an 8-bit sample having a
                            value of 192 will output a drive signal that is
                            approximately 50% of maximum:
                            (192&nbsp;&ndash;&nbsp;128)&nbsp;/&nbsp;127&nbsp;=&nbsp;50%.
                            Magnitude values less than #VIBE_MAX_MAGNITUDE serve
                            to attenuate the PCM data samples. There may be a
                            performance penalty when specifying a different
                            magnitude from the value that was passed to
                            #ImmVibePlayWaveformEffect to play the Waveform
                            effect.
\param[out]     phEffectHandle
                            Pointer to the variable that will receive a handle
                            to the playing effect. This value may be different
                            from the \a effectHandle parameter and must be the
                            value used in subsequent calls to this function to
                            append more PCM data to the Waveform effect, and
                            also in subsequent calls to
                            #ImmVibePausePlayingEffect or
                            #ImmVibeStopPlayingEffect.
\retval VIBE_S_SUCCESS      No error.
\retval VIBE_W_NOT_PLAYING  No more effect slots are available to play the
                            effect. Before playing the effect, you must call
                            #ImmVibeStopPlayingEffect or
                            #ImmVibeStopAllPlayingEffects to stop currently
                            playing effects and free an effect slot.
\retval VIBE_W_INSUFFICIENT_PRIORITY
                            The device handle does not have enough priority to
                            play the effect. Another effect is currently playing
                            on another higher priority device handle. Before
                            playing the effect, you must call
                            #ImmVibeStopPlayingEffect or
                            #ImmVibeStopAllPlayingEffects to stop currently
                            playing effects, or call
                            #ImmVibeSetDevicePropertyInt32 to increase value of
                            the #VIBE_DEVPROPTYPE_PRIORITY property.
\retval  VIBE_W_EFFECTS_DISABLED
                            Effects playing on the device handle have been
                            disabled. Call #ImmVibeSetDevicePropertyInt32 to set
                            the #VIBE_DEVPROPTYPE_DISABLE_EFFECTS property to
                            false to re-enable playing effects on the device
                            handle.
\retval VIBE_E_NOT_INITIALIZED
                           The \api has not been initialized. #ImmVibeInitialize
                           was not called or returned an error.
\retval VIBE_E_INVALID_ARGUMENT
                            One or more of the arguments are invalid. For
                            example:
                            -   The \a hDeviceHandle parameter specifies an
                                invalid device handle.
                            -   The value of one or more of the Waveform effect
                                parameters is invalid.
                            -   The \a phEffectHandle parameter is \c NULL.
                            -   The value of the \a sampleRate or \a
                                bitDepth</code> parameter does not match the
                                value that was passed to
                                #ImmVibePlayWaveformEffect to play the Waveform
                                effect.
\retval VIBE_E_FAIL         Error playing the effect.
\retval VIBE_E_INCOMPATIBLE_EFFECT_TYPE
                            The effect specified by the \a hEffectHandle
                            parameter is not a Waveform effect.
\retval VIBE_E_NOT_ENOUGH_MEMORY
                            The \api cannot allocate memory to complete the
                            request. This happens if the device runs low in
                            memory.
\retval VIBE_E_SERVICE_BUSY The \service is busy and could not complete the
                            requested function call.
\retval VIBE_E_NOT_SUPPORTED
                            This function is only supported in the \api &ndash;
                            5000 Series. See \ref editions for details about
                            \apibrief editions.
\if limo
\retval VIBE_E_NOT_SUPPORTED
                            The \service is not available on the target device.
\endif

\externappclients

\since  Version 3.5. See \ref versioning for details about \api version numbers.
*/
IMMVIBEAPI VibeStatus ImmVibeAppendWaveformEffect
(
    VibeInt32 hDeviceHandle,
    VibeInt32 hEffectHandle,
    const VibeUInt8 *pData,
    VibeInt32 nDataSize,
    VibeInt32 nSamplingRate,
    VibeInt32 nBitDepth,
    VibeInt32 nMagnitude,
    VibeInt32 *phEffectHandle
);

/**
\brief  Modifies the interpolant value for a playing interpolated effect.

\sync

\param[in]      hDeviceHandle
                            Device handle on which to modify the playing effect.
                            The handle must have been obtained by calling
                            #ImmVibeOpenDevice or #ImmVibeOpenCompositeDevice,
                            and must be the same as the handle that was given
                            when the effect was played.
\param[in]      hEffectHandle
                            Handle to the Periodic effect to modify. The handle
                            must have been obtained by calling
                            #ImmVibePlayPeriodicEffect or #ImmVibePlayIVTEffect
                            to play the Periodic effect.
\param[in]      nInterpolant
                            Interpolant value for the interpolated effect.
                            The interpolant value must be greater than or equal to \c 0
                            and less than or equal to #VIBE_MAX_INTERPOLANT
\retval VIBE_S_SUCCESS      No error.
\retval VIBE_W_NOT_PLAYING  The effect specified by the \a hEffectHandle
                            parameter is not playing. The effect may have played
                            to completion, may have been preempted by playing
                            another effect, or #ImmVibeStopPlayingEffect or
                            #ImmVibeStopAllPlayingEffects may have been called
                            to stop the effect.
\retval VIBE_E_NOT_INITIALIZED
                           The \api has not been initialized. #ImmVibeInitialize
                           was not called or returned an error.
\retval VIBE_E_INVALID_ARGUMENT
                            One or more of the arguments are invalid. For
                            example:
                            -   The \a hDeviceHandle parameter specifies an
                                invalid device handle.
                            -   The \a hEffectHandle parameter specifies an
                                invalid effect handle.
                            -   The effect specified by the \a hEffectHandle
                                parameter was not played on the device specified
                                by the \a hDeviceHandle parameter.
                                The value of the \a nInterpolant parameter is negative.
                                The value of the \a nInterpolant parameter is greater than
                                #VIBE_MAX_INTERPOLANT.
\retval VIBE_E_FAIL         Error modifying the effect.
\retval VIBE_E_INCOMPATIBLE_EFFECT_TYPE
                            The effect specified by the \a hEffectHandle
                            parameter is not an interpolated effect.
\retval VIBE_E_DEVICE_NEEDS_LICENSE
                            Access to the device is locked until a valid license
                            key is provided. This error may be returned
                            only by \api version 3.2 or lower. Subsequent
                            versions of the \apibrief do not require a license
                            key. See \ref licensing for details about setting
                            license keys.
\retval VIBE_E_SERVICE_BUSY The \service is busy and could not complete the
                            requested function call.
\retval VIBE_E_NOT_SUPPORTED
                            This function is only supported in the \api &ndash;
                            5000 Series. See \ref editions for details about
                            \apibrief editions.
\if limo
                            The \service is not available on the target device.
\endif

\externappclients

\since  Version 3.6. See \ref versioning for details about \api version numbers.
*/
IMMVIBEAPI VibeStatus ImmVibeModifyPlayingInterpolatedEffectInterpolant
(
    VibeInt32 hDeviceHandle,
    VibeInt32 hEffectHandle,
    VibeInt32 nInterpolant
);

/**
\brief  Modifies a playing MagSweep effect.

\sync

\param[in]      hDeviceHandle
                            Device handle on which to modify the playing effect.
                            The handle must have been obtained by calling
                            #ImmVibeOpenDevice or #ImmVibeOpenCompositeDevice,
                            and must be the same as the handle that was given
                            when the effect was played.
\param[in]      hEffectHandle
                            Handle to the MagSweep effect to modify. The handle
                            must have been obtained by calling
                            #ImmVibePlayMagSweepEffect or #ImmVibePlayIVTEffect
                            to play the effect.
\param[in]      nDuration   Duration of the effect in milliseconds. To specify
                            an infinite duration, set this parameter to
                            #VIBE_TIME_INFINITE. For a finite duration, the
                            effect duration is clamped to a value from \c 0 to
                            the value returned by
                            #ImmVibeGetDeviceCapabilityInt32 for the
                            #VIBE_DEVCAPTYPE_MAX_EFFECT_DURATION capability
                            type, inclusive.
\param[in]      nMagnitude  Magnitude of the effect. The effect magnitude is
                            clamped to a value from #VIBE_MIN_MAGNITUDE to
                            #VIBE_MAX_MAGNITUDE, inclusive.
\param[in]      nStyle      Style of the effect. See \ref styles for a list of
                            valid effect styles.
\param[in]      nAttackTime Attack time of the effect in milliseconds. The
                            attack time is clamped to a value from \c 0 to the
                            value returned by #ImmVibeGetDeviceCapabilityInt32
                            for the #VIBE_DEVCAPTYPE_MAX_ENVELOPE_TIME
                            capability type, inclusive. See \ref envelopes for
                            details about effect envelopes.
\param[in]      nAttackLevel
                            Attack level of the effect. The attack level is
                            clamped to a value from #VIBE_MIN_MAGNITUDE to
                            #VIBE_MAX_MAGNITUDE, inclusive. See \ref envelopes
                            for details about effect envelopes.
\param[in]      nFadeTime   Fade time of the effect in milliseconds. The fade
                            time is clamped to a value from \c 0 to the value
                            returned by #ImmVibeGetDeviceCapabilityInt32 for the
                            #VIBE_DEVCAPTYPE_MAX_ENVELOPE_TIME capability type,
                            inclusive. See \ref envelopes for details about
                            effect envelopes.
\param[in]      nFadeLevel  Fade level of the effect. The fade level is clamped
                            to a value from #VIBE_MIN_MAGNITUDE to
                            #VIBE_MAX_MAGNITUDE, inclusive. See \ref envelopes
                            for details about effect envelopes.

\retval VIBE_S_SUCCESS      No error.
\retval VIBE_W_NOT_PLAYING  The effect specified by the \a hEffectHandle
                            parameter is not playing. The effect may have played
                            to completion, may have been preempted by playing
                            another effect, or #ImmVibeStopPlayingEffect or
                            #ImmVibeStopAllPlayingEffects may have been called
                            to stop the effect.
\retval VIBE_E_NOT_INITIALIZED
                           The \api has not been initialized. #ImmVibeInitialize
                           was not called or returned an error.
\retval VIBE_E_INVALID_ARGUMENT
                            One or more of the arguments are invalid. For
                            example:
                            -   The \a hDeviceHandle parameter specifies an
                                invalid device handle.
                            -   The \a hEffectHandle parameter specifies an
                                invalid effect handle.
                            -   The effect specified by the \a hEffectHandle
                                parameter was not played on the device specified
                                by the \a hDeviceHandle parameter.
                            -   The value of one or more of the MagSweep effect
                                parameters is invalid.
\retval VIBE_E_FAIL         Error modifying the effect.
\retval VIBE_E_INCOMPATIBLE_EFFECT_TYPE
                            The effect specified by the \a hEffectHandle
                            parameter is not a MagSweep effect.
\retval VIBE_E_DEVICE_NEEDS_LICENSE
                            Access to the device is locked until a valid license
                            key is provided. This error may be returned
                            only by \api version 3.2 or lower. Subsequent
                            versions of the \apibrief do not require a license
                            key. See \ref licensing for details about setting
                            license keys.
\retval VIBE_E_SERVICE_BUSY The \service is busy and could not complete the
                            requested function call.
\if limo
\retval VIBE_E_NOT_SUPPORTED
                            The \service is not available on the target device.
\endif

\externappclients
*/
IMMVIBEAPI VibeStatus ImmVibeModifyPlayingMagSweepEffect
(
    VibeInt32 hDeviceHandle,
    VibeInt32 hEffectHandle,
    VibeInt32 nDuration,
    VibeInt32 nMagnitude,
    VibeInt32 nStyle,
    VibeInt32 nAttackTime,
    VibeInt32 nAttackLevel,
    VibeInt32 nFadeTime,
    VibeInt32 nFadeLevel
);

/**
\brief  Modifies a playing Periodic effect.

\sync

\param[in]      hDeviceHandle
                            Device handle on which to modify the playing effect.
                            The handle must have been obtained by calling
                            #ImmVibeOpenDevice or #ImmVibeOpenCompositeDevice,
                            and must be the same as the handle that was given
                            when the effect was played.
\param[in]      hEffectHandle
                            Handle to the Periodic effect to modify. The handle
                            must have been obtained by calling
                            #ImmVibePlayPeriodicEffect or #ImmVibePlayIVTEffect
                            to play the Periodic effect.
\param[in]      nDuration   Duration of the effect in milliseconds. To specify
                            an infinite duration, set this parameter to
                            #VIBE_TIME_INFINITE. For a finite duration, the
                            effect duration is clamped to a value from \c 0 to
                            the value returned by
                            #ImmVibeGetDeviceCapabilityInt32 for the
                            #VIBE_DEVCAPTYPE_MAX_EFFECT_DURATION capability
                            type, inclusive.
\param[in]      nMagnitude  Magnitude of the effect. The effect magnitude is
                            clamped to a value from #VIBE_MIN_MAGNITUDE to
                            #VIBE_MAX_MAGNITUDE, inclusive.
\param[in]      nPeriod     Period of the effect in milliseconds or
                            microseconds. If the most significant bit is \c 0,
                            the period is in milliseconds. If the most
                            significant bit is \c 1, the period is in
                            microseconds. For code clarity, you can OR the value
                            #VIBE_PERIOD_RESOLUTION_MICROSECOND with the period
                            when the period is in microseconds. The effect
                            period (in milliseconds) should go from the value
                            returned by #ImmVibeGetDeviceCapabilityInt32 for the
                            #VIBE_DEVCAPTYPE_MIN_PERIOD capability type to the
                            value returned by #ImmVibeGetDeviceCapabilityInt32
                            for the #VIBE_DEVCAPTYPE_MAX_PERIOD capability type,
                            inclusive.
\param[in]      nStyleAndWaveType
                            Style and wave type of the effect. See \ref styles
                            for a list of valid effect styles, and \ref
                            wavetypes for a list of valid wave types.
\param[in]      nAttackTime Attack time of the effect in milliseconds. The
                            attack time is clamped to a value from \c 0 to the
                            value returned by #ImmVibeGetDeviceCapabilityInt32
                            for the #VIBE_DEVCAPTYPE_MAX_ENVELOPE_TIME
                            capability type, inclusive. See \ref envelopes for
                            details about effect envelopes.
\param[in]      nAttackLevel
                            Attack level of the effect. The attack level is
                            clamped to a value from #VIBE_MIN_MAGNITUDE to
                            #VIBE_MAX_MAGNITUDE, inclusive. See \ref envelopes
                            for details about effect envelopes.
\param[in]      nFadeTime   Fade time of the effect in milliseconds. The fade
                            time is clamped to a value from \c 0 to the value
                            returned by #ImmVibeGetDeviceCapabilityInt32 for the
                            #VIBE_DEVCAPTYPE_MAX_ENVELOPE_TIME capability type,
                            inclusive. See \ref envelopes for details about
                            effect envelopes.
\param[in]      nFadeLevel  Fade level of the effect. The fade level is clamped
                            to a value from #VIBE_MIN_MAGNITUDE to
                            #VIBE_MAX_MAGNITUDE, inclusive. See \ref envelopes
                            for details about effect envelopes.

\retval VIBE_S_SUCCESS      No error.
\retval VIBE_W_NOT_PLAYING  The effect specified by the \a hEffectHandle
                            parameter is not playing. The effect may have played
                            to completion, may have been preempted by playing
                            another effect, or #ImmVibeStopPlayingEffect or
                            #ImmVibeStopAllPlayingEffects may have been called
                            to stop the effect.
\retval VIBE_E_NOT_INITIALIZED
                           The \api has not been initialized. #ImmVibeInitialize
                           was not called or returned an error.
\retval VIBE_E_INVALID_ARGUMENT
                            One or more of the arguments are invalid. For
                            example:
                            -   The \a hDeviceHandle parameter specifies an
                                invalid device handle.
                            -   The \a hEffectHandle parameter specifies an
                                invalid effect handle.
                            -   The effect specified by the \a hEffectHandle
                                parameter was not played on the device specified
                                by the \a hDeviceHandle parameter.
                            -   The value of one or more of the Periodic effect
                                parameters is invalid.
\retval VIBE_E_FAIL         Error modifying the effect.
\retval VIBE_E_INCOMPATIBLE_EFFECT_TYPE
                            The effect specified by the \a hEffectHandle
                            parameter is not a Periodic effect.
\retval VIBE_E_DEVICE_NEEDS_LICENSE
                            Access to the device is locked until a valid license
                            key is provided. This error may be returned
                            only by \api version 3.2 or lower. Subsequent
                            versions of the \apibrief do not require a license
                            key. See \ref licensing for details about setting
                            license keys.
\retval VIBE_E_SERVICE_BUSY The \service is busy and could not complete the
                            requested function call.
\if limo
\retval VIBE_E_NOT_SUPPORTED
                            The \service is not available on the target device.
\endif

\externappclients
*/
IMMVIBEAPI VibeStatus ImmVibeModifyPlayingPeriodicEffect
(
    VibeInt32 hDeviceHandle,
    VibeInt32 hEffectHandle,
    VibeInt32 nDuration,
    VibeInt32 nMagnitude,
    VibeInt32 nPeriod,
    VibeInt32 nStyleAndWaveType,
    VibeInt32 nAttackTime,
    VibeInt32 nAttackLevel,
    VibeInt32 nFadeTime,
    VibeInt32 nFadeLevel
);

/**
\brief  Stops all playing and paused effects on a device.

\sync

\param[in]      hDeviceHandle
                            Device handle for which to stop all effects. The
                            handle must have been obtained by calling
                            #ImmVibeOpenDevice or #ImmVibeOpenCompositeDevice.

\retval VIBE_S_SUCCESS      No error.
\retval VIBE_E_NOT_INITIALIZED
                           The \api has not been initialized. #ImmVibeInitialize
                           was not called or returned an error.
\retval VIBE_E_INVALID_ARGUMENT
                            The \a hDeviceHandle parameter specifies an invalid
                            device handle.
\retval VIBE_E_FAIL         Error stopping all effects.
\retval VIBE_E_DEVICE_NEEDS_LICENSE
                            Access to the device is locked until a valid license
                            key is provided. This error may be returned
                            only by \api version 3.2 or lower. Subsequent
                            versions of the \apibrief do not require a license
                            key. See \ref licensing for details about setting
                            license keys.
\retval VIBE_E_SERVICE_BUSY The \service is busy and could not complete the
                            requested function call.
\if limo
\retval VIBE_E_NOT_SUPPORTED
                            The \service is not available on the target device.
\endif

\externappclients
*/
IMMVIBEAPI VibeStatus ImmVibeStopAllPlayingEffects
(
	VibeInt32 hDeviceHandle
);

/**
\brief  Saves an IVT file to persistent storage.

        See \ref ivtfiles for details about IVT data.

\sync

\param[in]      pIVT        Pointer to IVT data. See \ref ivtfiles for details
                            about IVT data. Use #g_pVibeIVTBuiltInEffects to
                            access built-in IVT effects.
\param[in]      szPathname  Pointer to the character buffer containing the path
                            name of the file to save. The IVT extension is not
                            required to save the file.

\retval VIBE_S_SUCCESS      No error.
\retval VIBE_E_NOT_INITIALIZED
                           The \api has not been initialized. #ImmVibeInitialize
                           was not called or returned an error.
\retval VIBE_E_INVALID_ARGUMENT
                            One or more of the arguments are invalid. For
                            example:
                            -   The \a pIVT parameter is \c NULL or points to
                                invalid IVT data.
                            -   The \a szPathName parameter is \c NULL or points
                                to an invalid path name.
\retval VIBE_E_FAIL         Error saving the IVT file.
\retval VIBE_E_NOT_ENOUGH_MEMORY
                            The \api cannot allocate memory to complete the
                            request. This happens if the device runs low in
                            memory.
\retval VIBE_E_SERVICE_BUSY The \service is busy and could not complete the
                            requested function call.
\if limo
\retval VIBE_E_NOT_SUPPORTED
                            The \service is not available on the target device.
\endif

\externappclients
*/
IMMVIBEAPI VibeStatus ImmVibeSaveIVTFile
(
    const VibeUInt8 *pIVT,
    const char *szPathname
);

/**
\brief  Removes an IVT file from persistent storage.

        See \ref ivtfiles for details about IVT data.

\sync

\param[in]      szPathname  Pointer to the character buffer containing the path
                            name of the file to remove.

\retval VIBE_S_SUCCESS      No error.
\retval VIBE_E_NOT_INITIALIZED
                           The \api has not been initialized. #ImmVibeInitialize
                           was not called or returned an error.
\retval VIBE_E_INVALID_ARGUMENT
                            The \a szPathName parameter is \c NULL or points to
                            an invalid path name.
\retval VIBE_E_FAIL         Error removing the IVT file.
\retval VIBE_E_SERVICE_BUSY The \service is busy and could not complete the
                            requested function call.
\if limo
\retval VIBE_E_NOT_SUPPORTED
                            The \service is not available on the target device.
\endif

\externappclients
*/
IMMVIBEAPI VibeStatus ImmVibeDeleteIVTFile
(
    const char *szPathname
);

/**
\brief  Creates a Streaming effect.

\sync

\remark Applications call #ImmVibeCreateStreamingEffect to create a new
        Streaming effect and get a handle to the new effect. Applications
        should use that effect handle to play Streaming Samples by calling
        #ImmVibePlayStreamingSample or #ImmVibePlayStreamingSampleWithOffset.

\param[in]  hDeviceHandle   Device handle on which to create the Streaming
                            effect. The handle must have been obtained by
                            calling #ImmVibeOpenDevice or
                            #ImmVibeOpenCompositeDevice.
\param[out] phEffectHandle  Pointer to the variable that will receive a handle
                            to the Streaming effect.

\retval VIBE_S_SUCCESS      No error.
\retval VIBE_E_NOT_INITIALIZED
                           The \api has not been initialized. #ImmVibeInitialize
                           was not called or returned an error.
\retval VIBE_E_INVALID_ARGUMENT
                            One or more of the arguments are invalid. For
                            example:
                            -   The \a hDeviceHandle parameter specifies an
                                invalid device handle.
                            -   The \a phEffectHandle parameter is \c NULL.
\retval VIBE_E_FAIL         Error creating the Streaming effect.
\retval VIBE_E_DEVICE_NEEDS_LICENSE
                            Access to the device is locked until a valid license
                            key is provided. This error may be returned
                            only by \api version 3.2 or lower. Subsequent
                            versions of the \apibrief do not require a license
                            key. See \ref licensing for details about setting
                            license keys.
\retval VIBE_E_NOT_ENOUGH_MEMORY
                            The \api cannot allocate memory to complete the
                            request. This happens if the device runs low in
                            memory.
\retval VIBE_E_SERVICE_BUSY The \service is busy and could not complete the
                            requested function call.
\if limo
\retval VIBE_E_NOT_SUPPORTED
                            The \service is not available on the target device.
\endif

\externappclients

\since  Version 1.5. See \ref versioning for details about \api version numbers.
*/
IMMVIBEAPI VibeStatus ImmVibeCreateStreamingEffect
(
    VibeInt32 hDeviceHandle,
    VibeInt32 *phEffectHandle
);

/**
\brief  Destroys a Streaming effect.

\sync

\param[in]  hDeviceHandle   Device handle on which to destroy the Streaming
                            effect. The handle must have been obtained by
                            calling #ImmVibeOpenDevice or
                            #ImmVibeOpenCompositeDevice, and must be the same as
                            the handle that was given when the effect was
                            created.
\param[in]  hEffectHandle   Handle to the Streaming effect to destroy. The handle
                            must have been obtained by calling
                            #ImmVibeCreateStreamingEffect.

\retval VIBE_S_SUCCESS      No error.
\retval VIBE_E_NOT_INITIALIZED
                           The \api has not been initialized. #ImmVibeInitialize
                           was not called or returned an error.
\retval VIBE_E_INVALID_ARGUMENT
                            One or more of the arguments are invalid. For
                            example:
                            -   The \a hDeviceHandle parameter specifies an
                                invalid device handle.
                            -   The \a hEffectHandle parameter specifies an
                                invalid Streaming effect handle.
                            -   The effect specified by the \a hEffectHandle
                                parameter was not created on the device
                                specified by the \a hDeviceHandle parameter.
\retval VIBE_E_FAIL         Error destroying the Streaming effect.
\retval VIBE_E_DEVICE_NEEDS_LICENSE
                            Access to the device is locked until a valid license
                            key is provided. This error may be returned
                            only by \api version 3.2 or lower. Subsequent
                            versions of the \apibrief do not require a license
                            key. See \ref licensing for details about setting
                            license keys.
\retval VIBE_E_SERVICE_BUSY The \service is busy and could not complete the
                            requested function call.
\if limo
\retval VIBE_E_NOT_SUPPORTED
                            The \service is not available on the target device.
\endif

\externappclients

\since  Version 1.5. See \ref versioning for details about \api version numbers.
*/
IMMVIBEAPI VibeStatus ImmVibeDestroyStreamingEffect
(
    VibeInt32 hDeviceHandle,
    VibeInt32 hEffectHandle
);

/**
\brief  Plays a Streaming Sample.

\sync

\param[in]  hDeviceHandle   Device handle on which to play the Streaming Sample.
                            The handle must have been obtained by calling
                            #ImmVibeOpenDevice or #ImmVibeOpenCompositeDevice,
                            and must be the same as the handle that was given
                            when the effect was created.
\param[in]  hEffectHandle   Handle to the Streaming effect for which to play the
                            Streaming Sample. The handle must have been obtained
                            by calling #ImmVibeCreateStreamingEffect.
\param[in]  pStreamingSample
                            Pointer to the buffer containing the Streaming
                            Sample data to play.
\param[in]  nSize           Size of the buffer, in bytes, pointed to by the
                            \a pStreamingSample parameter. Normally, the buffer
                            should have a size less than or equal to
                            #VIBE_MAX_STREAMING_SAMPLE_SIZE.

\retval VIBE_S_SUCCESS      No error.
\retval VIBE_W_INSUFFICIENT_PRIORITY
                            The device handle does not have enough priority to
                            play the effect. Another effect is currently playing
                            on another higher priority device handle. Before
                            playing the effect, you must call
                            #ImmVibeStopPlayingEffect or
                            #ImmVibeStopAllPlayingEffects to stop currently
                            playing effects, or call
                            #ImmVibeSetDevicePropertyInt32 to increase value of
                            the #VIBE_DEVPROPTYPE_PRIORITY property.
\retval VIBE_E_NOT_INITIALIZED
                           The \api has not been initialized. #ImmVibeInitialize
                           was not called or returned an error.
\retval VIBE_E_INVALID_ARGUMENT
                            One or more of the arguments are invalid. For
                            example:
                            -   The \a hDeviceHandle parameter specifies an
                                invalid device handle.
                            -   The \a hEffectHandle parameter specifies an
                                invalid Streaming effect handle.
                            -   The effect specified by the \a hEffectHandle
                                parameter was not created on the device
                                specified by the \a hDeviceHandle parameter.
                            -   The \a pStreamingSample parameter is \c NULL or
                                points to invalid Streaming Sample data.
                            -   The value of the \a nSize parameter is less than
                                \c 0 or greater than
                                #VIBE_MAX_STREAMING_SAMPLE_SIZE.
\retval VIBE_E_FAIL         Error playing the Streaming Sample.
\retval VIBE_E_DEVICE_NEEDS_LICENSE
                            Access to the device is locked until a valid license
                            key is provided. This error may be returned
                            only by \api version 3.2 or lower. Subsequent
                            versions of the \apibrief do not require a license
                            key. See \ref licensing for details about setting
                            license keys.
\retval VIBE_E_SERVICE_BUSY The \service is busy and could not complete the
                            requested function call.
\if limo
\retval VIBE_E_NOT_SUPPORTED
                            The \service is not available on the target device.
\endif

\externappclients

\since  Version 1.5. See \ref versioning for details about \api version numbers.

\sa     ImmVibePlayStreamingSampleWithOffset.
*/
IMMVIBEAPI VibeStatus ImmVibePlayStreamingSample
(
    VibeInt32 hDeviceHandle,
    VibeInt32 hEffectHandle,
    const VibeUInt8 *pStreamingSample,
    VibeInt32 nSize
);

/**
\brief  Plays a Streaming Sample Sample with a time offset.

\sync

\param[in]  hDeviceHandle   Device handle on which to play the Streaming Sample.
                            The handle must have been obtained by calling
                            #ImmVibeOpenDevice or #ImmVibeOpenCompositeDevice,
                            and must be the same as the handle that was given
                            when the effect was created.
\param[in]  hEffectHandle   Handle to the Streaming effect for which to play the
                            Streaming Sample. The handle must have been obtained
                            by calling #ImmVibeCreateStreamingEffect.
\param[in]  pStreamingSample
                            Pointer to the buffer containing the Streaming
                            Sample data to play.
\param[in]  nSize           Size of the buffer, in bytes, pointed to by the
                            \a pStreamingSample parameter. Normally, the buffer
                            should have a size less than or equal to
                            #VIBE_MAX_STREAMING_SAMPLE_SIZE.
\param[in]  nOffsetTime     Time offset for playing the Streaming Sample. For \a
                            nOffsetTime values that are greater than \c 0,
                            playback is delayed for \a nOffsetTime in
                            milliseconds. For \a nOffsetTime values that are
                            less than \c 0, sample playback begins in offset
                            time in milliseconds into the current sample.

\retval VIBE_S_SUCCESS      No error.
\retval VIBE_W_INSUFFICIENT_PRIORITY
                            The device handle does not have enough priority to
                            play the effect. Another effect is currently playing
                            on another higher priority device handle. Before
                            playing the effect, you must call
                            #ImmVibeStopPlayingEffect or
                            #ImmVibeStopAllPlayingEffects to stop currently
                            playing effects, or call
                            #ImmVibeSetDevicePropertyInt32 to increase value of
                            the #VIBE_DEVPROPTYPE_PRIORITY property.
\retval VIBE_E_NOT_INITIALIZED
                           The \api has not been initialized. #ImmVibeInitialize
                           was not called or returned an error.
\retval VIBE_E_INVALID_ARGUMENT
                            One or more of the arguments are invalid. For
                            example:
                            -   The \a hDeviceHandle parameter specifies an
                                invalid device handle.
                            -   The \a hEffectHandle parameter specifies an
                                invalid Streaming effect handle.
                            -   The effect specified by the \a hEffectHandle
                                parameter was not created on the device
                                specified by the \a hDeviceHandle parameter.
                            -   The \a pStreamingSample parameter is \c NULL or
                                points to invalid Streaming Sample data.
                            -   The value of the \a nSize parameter is less than
                                \c 0 or greater than
                                #VIBE_MAX_STREAMING_SAMPLE_SIZE.
\retval VIBE_E_FAIL         Error playing the Streaming Sample with offset.
\retval VIBE_E_DEVICE_NEEDS_LICENSE
                            Access to the device is locked until a valid license
                            key is provided. This error may be returned
                            only by \api version 3.2 or lower. Subsequent
                            versions of the \apibrief do not require a license
                            key. See \ref licensing for details about setting
                            license keys.
\retval VIBE_E_SERVICE_BUSY The \service is busy and could not complete the
                            requested function call.
\if limo
\retval VIBE_E_NOT_SUPPORTED
                            The \service is not available on the target device.
\endif

\externappclients

\since  Version 1.5. See \ref versioning for details about \api version numbers.

\sa     ImmVibePlayStreamingSample.
*/
IMMVIBEAPI VibeStatus ImmVibePlayStreamingSampleWithOffset
(
    VibeInt32 hDeviceHandle,
    VibeInt32 hEffectHandle,
    const VibeUInt8 *pStreamingSample,
    VibeInt32 nSize,
    VibeInt32 nOffsetTime
);

/**
\brief  Pauses a playing effect.

\sync

\param[in]  hDeviceHandle   Device handle on which to pause the playing effect.
                            The handle must have been obtained by calling
                            #ImmVibeOpenDevice or #ImmVibeOpenCompositeDevice,
                            and must be the same as the handle that was given
                            when the effect was played or created.
\param[in]  hEffectHandle   Handle to the effect to pause. The handle must have
                            been obtained by calling #ImmVibePlayMagSweepEffect,
                            #ImmVibePlayPeriodicEffect,
                            #ImmVibePlayWaveformEffect,
                            #ImmVibeAppendWaveformEffect, #ImmVibePlayIVTEffect,
                            #ImmVibePlayIVTEffectRepeat, or
                            #ImmVibeCreateStreamingEffect.

\retval VIBE_S_SUCCESS      No error.
\retval VIBE_W_NOT_PLAYING  The effect specified by the \a hEffectHandle
                            parameter is not playing. The effect may have played
                            to completion, may have been preempted by playing
                            another effect, or #ImmVibeStopPlayingEffect or
                            #ImmVibeStopAllPlayingEffects may have been called
                            to stop the effect.
\retval VIBE_E_NOT_INITIALIZED
                           The \api has not been initialized. #ImmVibeInitialize
                           was not called or returned an error.
\retval VIBE_E_INVALID_ARGUMENT
                            One or more of the arguments are invalid. For
                            example:
                            -   The \a hDeviceHandle parameter specifies an
                                invalid device handle.
                            -   The \a hEffectHandle parameter specifies an
                                invalid effect handle.
                            -   The effect specified by the \a hEffectHandle
                                parameter was not played or created on the
                                device specified by the \a hDeviceHandle
                                parameter.
\retval VIBE_E_FAIL         Error pausing the effect.
\retval VIBE_E_DEVICE_NEEDS_LICENSE
                            Access to the device is locked until a valid license
                            key is provided. This error may be returned
                            only by \api version 3.2 or lower. Subsequent
                            versions of the \apibrief do not require a license
                            key. See \ref licensing for details about setting
                            license keys.
\retval VIBE_E_NOT_ENOUGH_MEMORY
                            The \api cannot allocate memory to complete the
                            request. This happens if the device runs low in
                            memory.
\retval VIBE_E_SERVICE_BUSY The \service is busy and could not complete the
                            requested function call.
\if limo
\retval VIBE_E_NOT_SUPPORTED
                            The \service is not available on the target device.
\endif

\externappclients

\since  Version 1.5. See \ref versioning for details about \api version numbers.

\sa     ImmVibeResumePausedEffect.
*/
IMMVIBEAPI VibeStatus ImmVibePausePlayingEffect
(
    VibeInt32 hDeviceHandle,
    VibeInt32 hEffectHandle
);

/**
\brief  Resumes a paused effect from the point where the effect was paused.

\sync

\param[in]  hDeviceHandle   Device handle on which to resume the paused effect.
                            The handle must have been obtained by calling
                            #ImmVibeOpenDevice or #ImmVibeOpenCompositeDevice,
                            and must be the same as the handle that was given
                            when the effect was played or created.
\param[in]  hEffectHandle   Handle to the effect to resume. The handle must have
                            been obtained by calling #ImmVibePlayMagSweepEffect,
                            #ImmVibePlayPeriodicEffect,
                            #ImmVibePlayWaveformEffect,
                            #ImmVibeAppendWaveformEffect, #ImmVibePlayIVTEffect,
                            #ImmVibePlayIVTEffectRepeat, or
                            #ImmVibeCreateStreamingEffect.

\retval VIBE_S_SUCCESS      No error.
\retval VIBE_W_NOT_PLAYING  No more effect slots are available to resume the
                            MagSweep, Periodic, or Waveform effect. Because the
                            effect could not be resumed, the \player stops the
                            effect to keep the effect from consuming resources
                            indefinitely in a paused state.
\retval VIBE_W_INSUFFICIENT_PRIORITY
                            The device handle does not have enough priority to
                            play the effect. Another effect is currently playing
                            on another higher priority device handle. Before
                            playing the effect, you must call
                            #ImmVibeStopPlayingEffect or
                            #ImmVibeStopAllPlayingEffects to stop currently
                            playing effects, or call
                            #ImmVibeSetDevicePropertyInt32 to increase value of
                            the #VIBE_DEVPROPTYPE_PRIORITY property.
\retval VIBE_W_NOT_PAUSED   The effect is not paused.
\retval VIBE_E_NOT_INITIALIZED
                           The \api has not been initialized. #ImmVibeInitialize
                           was not called or returned an error.
\retval VIBE_E_INVALID_ARGUMENT
                            One or more of the arguments are invalid. For
                            example:
                            -   The \a hDeviceHandle parameter specifies an
                                invalid device handle.
                            -   The \a hEffectHandle parameter specifies an
                                invalid effect handle.
                            -   The effect specified by the \a hEffectHandle
                                parameter was not played or created on the
                                device specified by the \a hDeviceHandle
                                parameter.
\retval VIBE_E_FAIL         Error resuming the effect.
\retval VIBE_E_DEVICE_NEEDS_LICENSE
                            Access to the device is locked until a valid license
                            key is provided. This error may be returned
                            only by \api version 3.2 or lower. Subsequent
                            versions of the \apibrief do not require a license
                            key. See \ref licensing for details about setting
                            license keys.
\retval VIBE_E_SERVICE_BUSY The \service is busy and could not complete the
                            requested function call.
\if limo
\retval VIBE_E_NOT_SUPPORTED
                            The \service is not available on the target device.
\endif

\externappclients

\since  Version 1.5. See \ref versioning for details about \api version numbers.

\sa     ImmVibePausePlayingEffect.
*/
IMMVIBEAPI VibeStatus ImmVibeResumePausedEffect
(
    VibeInt32 hDeviceHandle,
    VibeInt32 hEffectHandle
);

/**
\brief  Retrieves the status of an effect (playing, not playing, or paused).

\sync

\param[in]  hDeviceHandle   Device handle on which to get the effect status. The
                            handle must have been obtained by calling
                            #ImmVibeOpenDevice or #ImmVibeOpenCompositeDevice,
                            and must be the same as the handle that was given
                            when the effect was played or created.
\param[in]  hEffectHandle   Handle to the effect for which to get the status.
                            The handle must have been obtained by calling
                            #ImmVibePlayMagSweepEffect,
                            #ImmVibePlayPeriodicEffect,
                            #ImmVibePlayWaveformEffect,
                            #ImmVibeAppendWaveformEffect, #ImmVibePlayIVTEffect,
                            #ImmVibePlayIVTEffectRepeat, or
                            #ImmVibeCreateStreamingEffect.
\param[out] pnEffectState   Pointer to the variable that will receive the status
                            of the effect. See \ref effectstates for a list of
                            valid effect states.

\retval VIBE_S_SUCCESS      No error.
\retval VIBE_E_NOT_INITIALIZED
                           The \api has not been initialized. #ImmVibeInitialize
                           was not called or returned an error.
\retval VIBE_E_INVALID_ARGUMENT
                            One or more of the arguments are invalid. For
                            example:
                            -   The \a hDeviceHandle parameter specifies an
                                invalid device handle.
                            -   The \a hEffectHandle parameter specifies an
                                invalid effect handle.
                            -   The effect specified by the \a hEffectHandle
                                parameter was not played or created on the
                                device specified by the \a hDeviceHandle
                                parameter.
                            -   The \a pnEffectState parameter is \c NULL.
\retval VIBE_E_DEVICE_NEEDS_LICENSE
                            Access to the device is locked until a valid license
                            key is provided. This error may be returned
                            only by \api version 3.2 or lower. Subsequent
                            versions of the \apibrief do not require a license
                            key. See \ref licensing for details about setting
                            license keys.
\retval VIBE_E_SERVICE_BUSY The \service is busy and could not complete the
                            requested function call.
\if limo
\retval VIBE_E_NOT_SUPPORTED
                            The \service is not available on the target device.
\endif

\externappclients

\since  Version 1.5. See \ref versioning for details about \api version numbers.
*/
IMMVIBEAPI VibeStatus ImmVibeGetEffectState
(
    VibeInt32 hDeviceHandle,
    VibeInt32 hEffectHandle,
    VibeInt32 *pnEffectState
);

/**
\brief  Gets the size of IVT data.

\sync

\remark Call this function to determine the size of the IVT data within the
        buffer. This function may be called at any time, but is usually used to
        determine the number of bytes to write to an IVT file after effect
        editing is complete.

\param[in]      pIVT        Pointer to IVT data. This may be a pointer to a
                            buffer previously initialized by
                            #ImmVibeInitializeIVTBuffer, or it may be a
                            pointer to memory containing the contents of an IVT
                            file.
\param[in]      nSize       Size of the buffer pointed to by \a pIVT.

\retval Positive            Size of the IVT data, in bytes. The size will always
                            be less than or equal to the size of the buffer.
\retval VIBE_E_INVALID_ARGUMENT
                            One or more of the arguments are invalid. For
                            example:
                            -   The \a pIVT parameter is \c NULL or points to
                                invalid IVT data.
                            -   The size of the buffer is too small.
\retval VIBE_E_NOT_SUPPORTED
\if limo
                            One of the following conditions applies:
                            -   This function is not supported in the \api
                                &ndash; 3000 Series. See \ref editions for
                                details about \apibrief editions.
                            -   The \service is not available on the target
                                device.
\else
                            This function is not supported in the \api &ndash;
                            3000 Series. See \ref editions for details about
                            \apibrief editions.
\endif

\externappclients

\since  Version 3.2. See \ref versioning for details about \api version numbers.
*/
IMMVIBEAPI VibeStatus ImmVibeGetIVTSize(
    const VibeUInt8 *pIVT,
    VibeUInt32 nSize
);

/**
\brief  Initializes an IVT buffer.

\sync

\remark Any data currently in the buffer will be destroyed.

\param[out]     pIVT        Pointer to a buffer to initialize.
\param[in]      nSize       Size of the buffer pointed to by \a pIVT.

\retval VIBE_S_SUCCESS      No error.
\retval VIBE_E_INVALID_ARGUMENT
                            One or more of the arguments are invalid. For
                            example:
                            -   The \a pIVT parameter is \c NULL.
                            -   The size of the buffer is too small. The buffer
                                size must be at least 8 bytes. Considerably
                                more will be needed to accomodate elements in
                                the IVT buffer. The actual buffer size needed
                                depends on the number, types, and complexity of
                                elements that will be inserted in the buffer.
\retval VIBE_E_NOT_SUPPORTED
\if limo
                            One of the following conditions applies:
                            -   This function is not supported in the \api
                                &ndash; 3000 Series. See \ref editions for
                                details about \apibrief editions.
                            -   The \service is not available on the target
                                device.
\else
                            This function is not supported in the \api &ndash;
                            3000 Series. See \ref editions for details about
                            \apibrief editions.
\endif

\externappclients

\since  Version 3.2. See \ref versioning for details about \api version numbers.
*/
IMMVIBEAPI VibeStatus ImmVibeInitializeIVTBuffer(
    VibeUInt8 *pIVT,
    VibeUInt32 nSize
);

/**
\brief  Inserts an element into a Timeline effect in an IVT buffer.

        The element may be a Periodic effect, a MagSweep effect, or a Repeat
        event targeting a single default actuator on a device.

\sync

\remark The element will be added to the Timeline effect contained in the IVT
        file in chronological order. If the IVT data does not contain a Timeline
        effect, pass \c 0 for \a nTimelineIndex and one will be created to house
        this element.

\param[in]      pIVT        Pointer to IVT data. This may be a pointer to a
                            buffer previously initialized by
                            #ImmVibeInitializeIVTBuffer, or it may be a
                            pointer to memory containing the contents of an IVT
                            file.
\param[in]      nSize       Size of the buffer pointed to by \a pIVT.
\param[in]      nTimelineIndex
                            Index of the Timeline effect in which to insert the
                            element. If the IVT data only contains one Timeline
                            effect, pass \c 0 for this parameter. If the IVT
                            data does not contain a Timeline effect, pass \c 0
                            for this parameter and the \api will create a
                            Timeline effect to house this element.
\param[in]      pElement    Pointer to an #VibeIVTElement structure containing
                            the parameters of a Periodic effect, MagSweep
                            effect, or Repeat event to insert into the Timeline
                            effect.

\retval Positive            Index within the Timeline effect at which the
                            element is inserted.
\retval VIBE_E_INVALID_ARGUMENT
                            One or more of the arguments are invalid. For
                            example:
                            -   The \a pIVT parameter is \c NULL or points to
                                invalid IVT data.
                            -   The size of the buffer is too small.
                            -   The \a nTimelineIndex parameter is not the index
                                of a Timeline effect.
                            -   The \a pElement parameter is \c NULL.
\retval VIBE_E_FAIL         Error inserting the element.
\retval VIBE_E_NOT_SUPPORTED
\if limo
                            One of the following conditions applies:
                            -   This function is not supported in the \api
                                &ndash; 3000 Series. See \ref editions for
                                details about \apibrief editions.
                            -   The \service is not available on the target
                                device.
\else
                            This function is not supported in the \api &ndash;
                            3000 Series. See \ref editions for details about
                            \apibrief editions.
\endif

\externappclients

\since  Version 3.2. See \ref versioning for details about \api version numbers.

\deprecated This function has been superceded by #ImmVibeInsertIVTElement3 as of
            version 3.4. See \ref versioning for details about \api version
            numbers.
*/
IMMVIBEAPI VibeStatus ImmVibeInsertIVTElement(
    VibeUInt8 *pIVT,
    VibeUInt32 nSize,
    VibeUInt32 nTimelineIndex,
    const VibeIVTElement *pElement
);

/**
\brief  Inserts an element into a Timeline effect in an IVT buffer.

        The element may be a Periodic effect, or MagSweep effect, or a Repeat
        event targeting a particular actuator on a composite device.

\sync

\remark The element will be added to the Timeline effect contained in the IVT
        file in chronological order. If the IVT data does not contain a Timeline
        effect, pass \c 0 for \a nTimelineIndex and one will be created to house
        this element.
\remark This function supercedes #ImmVibeInsertIVTElement and adds support for
        elements targeting a particular actuator on a composite device.

\param[in]      pIVT        Pointer to IVT data. This may be a pointer to a
                            buffer previously initialized by
                            #ImmVibeInitializeIVTBuffer, or it may be a
                            pointer to memory containing the contents of an IVT
                            file.
\param[in]      nSize       Size of the buffer pointed to by \a pIVT.
\param[in]      nTimelineIndex
                            Index of the Timeline effect in which to insert the
                            element. If the IVT data only contains one Timeline
                            effect, pass \c 0 for this parameter. If the IVT
                            data does not contain a Timeline effect, pass \c 0
                            for this parameter and the \api will create a
                            Timeline effect to house this element.
\param[in]      pElement    Pointer to an #VibeIVTElement2 structure containing
                            the parameters of a Periodic effect, MagSweep
                            effect, or Repeat event to insert into the Timeline
                            effect.

\retval Positive            Index within the Timeline effect at which the
                            element is inserted.
\retval VIBE_E_INVALID_ARGUMENT
                            One or more of the arguments are invalid. For
                            example:
                            -   The \a pIVT parameter is \c NULL or points to
                                invalid IVT data.
                            -   The size of the buffer is too small.
                            -   The \a nTimelineIndex parameter is not the index
                                of a Timeline effect.
                            -   The \a pElement parameter is \c NULL.
\retval VIBE_E_FAIL         Error inserting the element.
\retval VIBE_E_NOT_SUPPORTED
\if limo
                            One of the following conditions applies:
                            -   This function is not supported in the \api
                                &ndash; 3000 Series. See \ref editions for
                                details about \apibrief editions.
                            -   The \service is not available on the target
                                device.
\else
                            This function is not supported in the \api &ndash;
                            3000 Series. See \ref editions for details about
                            \apibrief editions.
\endif

\externappclients

\since  Version 3.3. See \ref versioning for details about \api version numbers.

\deprecated This function has been superceded by #ImmVibeInsertIVTElement3 as of
            version 3.4. See \ref versioning for details about \api version
            numbers.
*/
IMMVIBEAPI VibeStatus ImmVibeInsertIVTElement2(
    VibeUInt8 *pIVT,
    VibeUInt32 nSize,
    VibeUInt32 nTimelineIndex,
    const VibeIVTElement2 *pElement
);

/**
\brief  Inserts an element into a Timeline effect in an IVT buffer.

        The element may be a Periodic effect, a MagSweep effect, a Waveform
        effect, or a Repeat event targeting a particular actuator on a composite
        device.

\sync

\remark The element will be added to the Timeline effect contained in the IVT
        file in chronological order. If the IVT data does not contain a Timeline
        effect, pass \c 0 for \a nTimelineIndex and one will be created to house
        this element.

\remark This function supercedes #ImmVibeInsertIVTElement2 and adds support for
        Waveform effects.

\param[in]      pIVT        Pointer to IVT data. This may be a pointer to a
                            buffer previously initialized by
                            #ImmVibeInitializeIVTBuffer, or it may be a
                            pointer to memory containing the contents of an IVT
                            file.
\param[in]      nSize       Size of the buffer pointed to by \a pIVT.
\param[in]      nTimelineIndex
                            Index of the Timeline effect in which to insert the
                            element. If the IVT data only contains one Timeline
                            effect, pass \c 0 for this parameter. If the IVT
                            data does not contain a Timeline effect, pass \c 0
                            for this parameter and the \api will create a
                            Timeline effect to house this element.
\param[in]      pElement    Pointer to an #VibeIVTElement3 structure containing
                            the parameters of a Periodic effect, MagSweep
                            effect, Waveform effect, or Repeat event to insert
                            into the Timeline effect.

\retval Positive            Index within the Timeline effect at which the
                            element is inserted.
\retval VIBE_E_INVALID_ARGUMENT
                            One or more of the arguments are invalid. For
                            example:
                            -   The \a pIVT parameter is \c NULL or points to
                                invalid IVT data.
                            -   The size of the buffer is too small.
                            -   The \a nTimelineIndex parameter is not the index
                                of a Timeline effect.
                            -   The \a pElement parameter is \c NULL.
\retval VIBE_E_FAIL         Error inserting the element.
\retval VIBE_E_NOT_SUPPORTED
\if limo
                            One of the following conditions applies:
                            -   This function is not supported in the \api
                                &ndash; 3000 Series. See \ref editions for
                                details about \apibrief editions.
                            -   The \service is not available on the target
                                device.
\else
                            This function is not supported in the \api &ndash;
                            3000 Series. See \ref editions for details about
                            \apibrief editions.
\endif

\externappclients

\since  Version 3.4. See \ref versioning for details about \api version numbers.
*/
IMMVIBEAPI VibeStatus ImmVibeInsertIVTElement3(
    VibeUInt8 *pIVT,
    VibeUInt32 nSize,
    VibeUInt32 nTimelineIndex,
    const VibeIVTElement3 *pElement
);

/**
\brief  Retrieves the parameters of an element of a Timeline effect in an IVT
        buffer.

        The element may be a Periodic effect, a MagSweep effect, or a Repeat
        event targeting a single default actuator on a device.

\sync

\param[in]      pIVT        Pointer to IVT data. This may be a pointer to a
                            buffer previously initialized by
                            #ImmVibeInitializeIVTBuffer, or it may be a
                            pointer to memory containing the contents of an IVT
                            file.
\param[in]      nSize       Size of the buffer pointed to by \a pIVT.
\param[in]      nTimelineIndex
                            Index of the Timeline effect from which to read the
                            element. If the IVT data only contains one Timeline
                            effect, pass \c 0 for this parameter.
\param[in]      nElementIndex
                            Index of the element to retrieve.
\param[out]     pElement    Pointer to an #VibeIVTElement structure to receive
                            the parameters of a Periodic effect, MagSweep
                            effect, or Repeat event.

\retval VIBE_S_SUCCESS      No error.
\retval VIBE_E_INVALID_ARGUMENT
                            One or more of the arguments are invalid. For
                            example:
                            -   The \a pIVT parameter is \c NULL or points to
                                invalid IVT data.
                            -   The size of the buffer is too small.
                            -   The \a nTimelineIndex parameter is not the index
                                of a Timeline effect.
                            -   The \a nElementIndex parameter is not a valid
                                index of an element in the Timeline effect.
                            -   The \a pElement parameter is \c NULL.
\retval VIBE_E_NOT_SUPPORTED
\if limo
                            One of the following conditions applies:
                            -   This function is not supported in the \api
                                &ndash; 3000 Series. See \ref editions for
                                details about \apibrief editions.
                            -   The \service is not available on the target
                                device.
\else
                            This function is not supported in the \api &ndash;
                            3000 Series. See \ref editions for details about
                            \apibrief editions.
\endif

\externappclients

\since  Version 3.2. See \ref versioning for details about \api version numbers.

\deprecated This function has been superceded by #ImmVibeReadIVTElement3 as of
            version 3.4. See \ref versioning for details about \api version
            numbers.
*/
IMMVIBEAPI VibeStatus ImmVibeReadIVTElement(
    const VibeUInt8 *pIVT,
    VibeUInt32 nSize,
    VibeUInt32 nTimelineIndex,
    VibeUInt32 nElementIndex,
    VibeIVTElement *pElement
);

/**
\brief  Retrieves the parameters of an element of a Timeline effect in an IVT
        buffer.

        The element may be a Periodic effect, or MagSweep effect, or a Repeat
        event targeting a particular actuator on a composite device.

\sync

\remark This function supercedes #ImmVibeReadIVTElement and adds support for
        elements targeting a particular actuator on a composite device.

\param[in]      pIVT        Pointer to IVT data. This may be a pointer to a
                            buffer previously initialized by
                            #ImmVibeInitializeIVTBuffer, or it may be a
                            pointer to memory containing the contents of an IVT
                            file.
\param[in]      nSize       Size of the buffer pointed to by \a pIVT.
\param[in]      nTimelineIndex
                            Index of the Timeline effect from which to read the
                            element. If the IVT data only contains one Timeline
                            effect, pass \c 0 for this parameter.
\param[in]      nElementIndex
                            Index of the element to retrieve.
\param[out]     pElement    Pointer to an #VibeIVTElement2 structure to receive
                            the parameters of a Periodic effect, MagSweep
                            effect, or Repeat event.

\retval VIBE_S_SUCCESS      No error.
\retval VIBE_E_INVALID_ARGUMENT
                            One or more of the arguments are invalid. For
                            example:
                            -   The \a pIVT parameter is \c NULL or points to
                                invalid IVT data.
                            -   The size of the buffer is too small.
                            -   The \a nTimelineIndex parameter is not the index
                                of a Timeline effect.
                            -   The \a nElementIndex parameter is not a valid
                                index of an element in the Timeline effect.
                            -   The \a pElement parameter is \c NULL.
\retval VIBE_E_NOT_SUPPORTED
\if limo
                            One of the following conditions applies:
                            -   This function is not supported in the \api
                                &ndash; 3000 Series. See \ref editions for
                                details about \apibrief editions.
                            -   The \service is not available on the target
                                device.
\else
                            This function is not supported in the \api &ndash;
                            3000 Series. See \ref editions for details about
                            \apibrief editions.
\endif

\externappclients

\since  Version 3.3. See \ref versioning for details about \api version numbers.

\deprecated This function has been superceded by #ImmVibeReadIVTElement3 as of
            version 3.4. See \ref versioning for details about \api version
            numbers.
*/
IMMVIBEAPI VibeStatus ImmVibeReadIVTElement2(
    const VibeUInt8 *pIVT,
    VibeUInt32 nSize,
    VibeUInt32 nTimelineIndex,
    VibeUInt32 nElementIndex,
    VibeIVTElement2 *pElement
);

/**
\brief  Retrieves the parameters of an element of a Timeline effect in an IVT
        buffer.

        The element may be a Periodic effect, a MagSweep effect, a Waveform
        effect, or a Repeat event targeting a particular actuator on a composite
        device.

\sync

\remark This function supercedes #ImmVibeReadIVTElement2 and adds support for
        Waveform effects.

\param[in]      pIVT        Pointer to IVT data. This may be a pointer to a
                            buffer previously initialized by
                            #ImmVibeInitializeIVTBuffer, or it may be a
                            pointer to memory containing the contents of an IVT
                            file.
\param[in]      nSize       Size of the buffer pointed to by \a pIVT.
\param[in]      nTimelineIndex
                            Index of the Timeline effect from which to read the
                            element. If the IVT data only contains one Timeline
                            effect, pass \c 0 for this parameter.
\param[in]      nElementIndex
                            Index of the element to retrieve.
\param[out]     pElement    Pointer to an #VibeIVTElement3 structure to receive
                            the parameters of a Periodic effect, MagSweep
                            effect, Waveform effect, or Repeat event.

\retval VIBE_S_SUCCESS      No error.
\retval VIBE_E_INVALID_ARGUMENT
                            One or more of the arguments are invalid. For
                            example:
                            -   The \a pIVT parameter is \c NULL or points to
                                invalid IVT data.
                            -   The size of the buffer is too small.
                            -   The \a nTimelineIndex parameter is not the index
                                of a Timeline effect.
                            -   The \a nElementIndex parameter is not a valid
                                index of an element in the Timeline effect.
                            -   The \a pElement parameter is \c NULL.
\retval VIBE_E_NOT_SUPPORTED
\if limo
                            One of the following conditions applies:
                            -   This function is not supported in the \api
                                &ndash; 3000 Series. See \ref editions for
                                details about \apibrief editions.
                            -   The \service is not available on the target
                                device.
\else
                            This function is not supported in the \api &ndash;
                            3000 Series. See \ref editions for details about
                            \apibrief editions.
\endif

\externappclients

\since  Version 3.4. See \ref versioning for details about \api version numbers.
*/
IMMVIBEAPI VibeStatus ImmVibeReadIVTElement3(
    const VibeUInt8 *pIVT,
    VibeUInt32 nSize,
    VibeUInt32 nTimelineIndex,
    VibeUInt32 nElementIndex,
    VibeIVTElement3 *pElement
);

/**
\brief  Removes the element at the given index from a Timeline effect in an IVT
        buffer.

\remark If the Basis effect referenced by this element of the Timeline effect
        is not referenced by any other element, the Basis effect will also be
        removed.

\remark If the element being removed is the only element in a Timeline effect,
        the Timeline effect will also be removed.

\remark Start-times of subsequent elements will not be modified.

\sync

\param[in]      pIVT        Pointer to IVT data. This may be a pointer to a
                            buffer previously initialized by
                            #ImmVibeInitializeIVTBuffer, or it may be a
                            pointer to memory containing the contents of an IVT
                            file.
\param[in]      nSize       Size of the buffer pointed to by \a pIVT.
\param[in]      nTimelineIndex
                            Index of the Timeline effect from which to remove
                            the element. If the IVT data only contains one
                            Timeline effect, pass \c 0 for this parameter.
\param[in]      nElementIndex
                            Index of the element to remove.

\retval VIBE_S_SUCCESS      No error.
\retval VIBE_E_INVALID_ARGUMENT
                            One or more of the arguments are invalid. For
                            example:
                            -   The \a pIVT parameter is \c NULL or points to
                                invalid IVT data.
                            -   The size of the buffer is too small.
                            -   The \a nTimelineIndex parameter is not the index
                                of a Timeline effect.
                            -   The \a nElementIndex parameter is not a valid
                                index of an element in the Timeline effect.
                            -   The \a pElement parameter is \c NULL.
\retval VIBE_E_NOT_SUPPORTED
\if limo
                            One of the following conditions applies:
                            -   This function is not supported in the \api
                                &ndash; 3000 Series. See \ref editions for
                                details about \apibrief editions.
                            -   The \service is not available on the target
                                device.
\else
                            This function is not supported in the \api &ndash;
                            3000 Series. See \ref editions for details about
                            \apibrief editions.
\endif

\externappclients

\since  Version 3.2. See \ref versioning for details about \api version numbers.
*/
IMMVIBEAPI VibeStatus ImmVibeRemoveIVTElement(
    VibeUInt8 *pIVT,
    VibeUInt32 nSize,
    VibeUInt32 nTimelineIndex,
    VibeUInt32 nElementIndex
);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _IMMVIBE_H */
