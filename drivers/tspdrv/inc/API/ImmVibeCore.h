/*
** =========================================================================
** Copyright (c) 2003-2012  Immersion Corporation.  All rights reserved.
** Copyright (C) 2016 XiaoMi, Inc.
**                          Immersion Corporation Confidential and Proprietary
** =========================================================================
*/
/**
\file   ImmVibeCore.h
\brief  Defines constants, macros, and types for the \api.
*/

#ifndef _IMMVIBECORE_H
#define _IMMVIBECORE_H

#include "ImmVibeOS.h"


/** \defgroup devcaps Device Capabilities */
/*@{*/

/**
\brief  Device capability type to get the device category.

        Used with #ImmVibeGetDeviceCapabilityInt32. The return value will be one
        of #VIBE_DEVICECATEGORY_IFC, #VIBE_DEVICECATEGORY_IMPULSE,
        #VIBE_DEVICECATEGORY_VIRTUAL, #VIBE_DEVICECATEGORY_EMBEDDED,
        #VIBE_DEVICECATEGORY_TETHERED, or #VIBE_DEVICECATEGORY_IMMERSION_USB.
*/
#define VIBE_DEVCAPTYPE_DEVICE_CATEGORY             0

/**
\brief  Device capability type to get the maximum number of nested repeat bars
        supported for Timeline effects.

        Any repeat bars nested beyond this level will be played only once.

        Used with #ImmVibeGetDeviceCapabilityInt32.
*/
#define VIBE_DEVCAPTYPE_MAX_NESTED_REPEATS          1

/**
\brief  Device capability type to get the number of actuators present on the
        device.

        Used with #ImmVibeGetDeviceCapabilityInt32.
*/
#define VIBE_DEVCAPTYPE_NUM_ACTUATORS               2

/**
\brief  Device capability type to get the acutator type.

        Used with #ImmVibeGetDeviceCapabilityInt32. The return value will be one
        of #VIBE_DEVACTUATORTYPE_BLDC, #VIBE_DEVACTUATORTYPE_LRA, or
        #VIBE_DEVACTUATORTYPE_PIEZO.
*/
#define VIBE_DEVCAPTYPE_ACTUATOR_TYPE               3

/**
\brief  Device capability type to get the number of effect slots present on the
        device.

        The number of effect slots represents the maximum number of simple
        effects that may play simultaneously. If an attempt is made to play more
        than this number of effects at the same time, some of the simple effects
        will not play.

        Used with #ImmVibeGetDeviceCapabilityInt32.
*/
#define VIBE_DEVCAPTYPE_NUM_EFFECT_SLOTS            4

/**
\brief  Device capability type to get the supported effect styles.

        Used with #ImmVibeGetDeviceCapabilityInt32. The return value will be a
        bitwise ORing of #VIBE_STYLE_SMOOTH_SUPPORT, #VIBE_STYLE_STRONG_SUPPORT,
        and #VIBE_STYLE_SHARP_SUPPORT.
*/
#define VIBE_DEVCAPTYPE_SUPPORTED_STYLES            5

/**
\brief  Device capability type to get the supported effect styles.

\deprecated
        Use #VIBE_DEVCAPTYPE_SUPPORTED_STYLES instead.
*/
#define VIBE_DEVCAPTYPE_SUPPORTED_CONTROL_MODES     VIBE_DEVCAPTYPE_SUPPORTED_STYLES

/**
\brief  Device capability type to get the minimum period for Periodic effects.

        Used with #ImmVibeGetDeviceCapabilityInt32.
*/
#define VIBE_DEVCAPTYPE_MIN_PERIOD                  6

/**
\brief  Device capability type to get the maximum period for Periodic effects.

        Used with #ImmVibeGetDeviceCapabilityInt32.
*/
#define VIBE_DEVCAPTYPE_MAX_PERIOD                  7

/**
\brief  Device capability type to get the maximum finite duration in
        milliseconds for simple effects.

        Used with #ImmVibeGetDeviceCapabilityInt32.
*/
#define VIBE_DEVCAPTYPE_MAX_EFFECT_DURATION         8

/**
\brief  Device capability type to get the supported effect types.

        Used with #ImmVibeGetDeviceCapabilityInt32. The return value will be a
        bitwise ORing of #VIBE_MAGSWEEP_EFFECT_SUPPORT,
        #VIBE_PERIODIC_EFFECT_SUPPORT, #VIBE_TIMELINE_EFFECT_SUPPORT,
        #VIBE_STREAMING_EFFECT_SUPPORT, and #VIBE_WAVEFORM_EFFECT_SUPPORT.
*/
#define VIBE_DEVCAPTYPE_SUPPORTED_EFFECTS           9

/**
\brief  Device capability type to get the device name.

        Used with #ImmVibeGetDeviceCapabilityString. The maximum device name
        length will be #VIBE_MAX_DEVICE_NAME_LENGTH.
*/
#define VIBE_DEVCAPTYPE_DEVICE_NAME                 10

/**
\brief  Device capability type to get the maximum attack time or fade time in
        milliseconds for effect envelopes of simple effects.

        Used with #ImmVibeGetDeviceCapabilityInt32.
*/
#define VIBE_DEVCAPTYPE_MAX_ENVELOPE_TIME           11

/**
\brief  Device capability type to get the \api version number.

        Used with #ImmVibeGetDeviceCapabilityInt32. See \ref versioning for
        details about \api version numbers.

\since  Version 1.5. See \ref versioning for details about \api version numbers.
*/
#define VIBE_DEVCAPTYPE_APIVERSIONNUMBER            12

/**
\brief  Device capability type to get the maximum size in bytes for IVT files
        that can be played on a tethered device.

        Used with #ImmVibeGetDeviceCapabilityInt32.

\since  Version 2.0. See \ref versioning for details about \api version numbers.
*/
#define VIBE_DEVCAPTYPE_MAX_IVT_SIZE_TETHERED       13

/**
\brief  Device capability type to get the maximum size in bytes for
        IVT files that can be played on a non-tethered device.

        Used with #ImmVibeGetDeviceCapabilityInt32.

\since  Version 2.0. See \ref versioning for details about \api version numbers.

\deprecated
        As of version 3.1, the \player dynamically allocates memory for IVT
        data; therefore, this device capability is depracated but retained for
        backward compatibility. See \ref versioning for details about \api
        version numbers.
*/
#define VIBE_DEVCAPTYPE_MAX_IVT_SIZE                14

/**
\brief  Device capability type to get the \player edition level.

        Used with #ImmVibeGetDeviceCapabilityInt32. The return value will be one
        of #VIBE_EDITION_3000, #VIBE_EDITION_4000, or #VIBE_EDITION_5000.

\since  Version 3.1. See \ref versioning for details about \api version numbers.
*/
#define VIBE_DEVCAPTYPE_EDITION_LEVEL               15

/**
\brief  Device capability type to get the supported wave types.

        Used with #ImmVibeGetDeviceCapabilityInt32. The return value will be a
        bitwise ORing of #VIBE_WAVETYPE_SQUARE_SUPPORT,
        #VIBE_WAVETYPE_TRIANGLE_SUPPORT, #VIBE_WAVETYPE_SINE_SUPPORT,
        #VIBE_WAVETYPE_SAWTOOTHUP_SUPPORT, and
        #VIBE_WAVETYPE_SAWTOOTHDOWN_SUPPORT.

\since  Version 3.2. See \ref versioning for details about \api version numbers.
*/
#define VIBE_DEVCAPTYPE_SUPPORTED_WAVE_TYPES        16

/**
\brief  Device capability type to get the handset index.

        Used with #ImmVibeGetDeviceCapabilityInt32. The return value will always
        be zero on embedded devices. In Windows, where multiple tethered
        handsets may be present, the return value indicates the handset to which
        a device, or actuator belongs.

\since  Version 3.3. See \ref versioning for details about \api version numbers.
*/
#define VIBE_DEVCAPTYPE_HANDSET_INDEX               17

/*@}*/

/** \defgroup devprops Device Properties */
/*@{*/

/**
\brief  Device property type to set the OEM licence key associated with a device
        handle.

        Used with #ImmVibeSetDevicePropertyString. This property is write-only.
        It can be set to an OEM license key issued by Immersion in order to use
        higher priority levels not normally available to applications, or to set
        the master strength. On devices with API version 3.2 or lower,
        applications must always set this property in order to unlock the
        ability to play effects.

        See \ref licensing for more information. See \ref versioning for details
        about \api version numbers.

\sa     VIBE_DEVPROPTYPE_PRIORITY, VIBE_DEVPROPTYPE_MASTERSTRENGTH.
*/
#define VIBE_DEVPROPTYPE_LICENSE_KEY                0

/**
\brief  Device property type to get/set the priority of effects associated with
        a device handle.

        See \ref priority for details about priority levels.

        Used with #ImmVibeGetDevicePropertyInt32 and
        #ImmVibeSetDevicePropertyInt32.
*/
#define VIBE_DEVPROPTYPE_PRIORITY                   1

/**
\brief  Device property type to enable/disable effects associated with a device
        handle.

        When this property is set to true, the \player immediately stops any
        playing effects and ignores subsequent requests to play effects. When
        this property is false, the \playerbrief honors requests to play
        effects.

        Used with #ImmVibeGetDevicePropertyBool and
        #ImmVibeSetDevicePropertyBool.
*/
#define VIBE_DEVPROPTYPE_DISABLE_EFFECTS            2

/**
\brief  Device property type to get/set the overall strength for all effects
        associated with a device handle.

        The strength varies from #VIBE_MIN_MAGNITUDE (equivalent to mute) to
        #VIBE_MAX_MAGNITUDE (full strength). The default value is
        #VIBE_MAX_MAGNITUDE.

        The strength only applies to the device handle passed to
        #ImmVibeGetDevicePropertyInt32 or #ImmVibeSetDevicePropertyInt32, not to
        other device handles held by the same or a different application.

        Modifying the strength does not affect currently playing effects, only
        effects played or modified after calling #ImmVibeSetDevicePropertyInt32
        to use a new strength.

        Used with #ImmVibeGetDevicePropertyInt32 and
        #ImmVibeSetDevicePropertyInt32.
 */
#define VIBE_DEVPROPTYPE_STRENGTH                   3

/**
\brief  Device property type to get/set the overall master strength for all
        effects associated with all device handles.

        The Master Strength varies from #VIBE_MIN_MAGNITUDE (equivalent to mute)
        to #VIBE_MAX_MAGNITUDE (full strength). The default value is
        #VIBE_MAX_MAGNITUDE.

        The master strength applies to all effects on the device, including
        effects from other applications.

        Modifying the master strength immediately affects currently playing
        effects and subsequently played or modified effects.

        Before changing the master strength, applications must call
        #ImmVibeSetDevicePropertyString for the #VIBE_DEVPROPTYPE_LICENSE_KEY
        property to set an OEM license key, and #ImmVibeSetDevicePropertyInt32
        for the #VIBE_DEVPROPTYPE_PRIORITY property to associate the maximum OEM
        priority (#VIBE_MAX_OEM_DEVICE_PRIORITY) with the device handle. See
        \ref licensing for details about license keys, and \ref priority for
        details about priority levels.

        Used with #ImmVibeGetDevicePropertyInt32 and
        #ImmVibeSetDevicePropertyInt32.
*/
#define VIBE_DEVPROPTYPE_MASTERSTRENGTH             4

/*@}*/


/**
\defgroup devcategs Device Categories

        Device categories returned by #ImmVibeGetDeviceCapabilityInt32 for the
        #VIBE_DEVCAPTYPE_DEVICE_CATEGORY device capability type.
*/
/*@{*/

/**
\brief  IFC device category.

\deprecated
        The IFC device category is no longer supported.
*/
#define VIBE_DEVICECATEGORY_IFC                     0

/**
\brief  Impulse device category.

\deprecated
        The Impulse device category is no longer supported.
*/
#define VIBE_DEVICECATEGORY_IMPULSE                 1

/**
\brief  Virtual device category.
*/
#define VIBE_DEVICECATEGORY_VIRTUAL                 2

/**
\brief  Embedded device category.
*/
#define VIBE_DEVICECATEGORY_EMBEDDED                3

/**
\brief  Tethered device category.
*/
#define VIBE_DEVICECATEGORY_TETHERED                4

/**
\brief  Immersion USB device category.

\deprecated
        The Immersion USB device category is no longer supported.
*/
#define VIBE_DEVICECATEGORY_IMMERSION_USB           5

/**
\brief  Composite device category.
*/
#define VIBE_DEVICECATEGORY_COMPOSITE               6

/*@}*/


/**
\defgroup acttypes Actuator Types

        Actuator types returned by #ImmVibeGetDeviceCapabilityInt32 for the
        #VIBE_DEVCAPTYPE_ACTUATOR_TYPE device capability.
*/
/*@{*/

/** \brief  Eccentric Rotating Mass (ERM) actuator type. */
#define VIBE_DEVACTUATORTYPE_ERM                    0

/** \brief  Bush-Less Direct Current (BLDC) actuator type. */
#define VIBE_DEVACTUATORTYPE_BLDC                   1

/** \brief  Linear Resonant (LR) actuator type. */
#define VIBE_DEVACTUATORTYPE_LRA                    2

/** \brief  Piezo-electric actuator type. */
#define VIBE_DEVACTUATORTYPE_PIEZO                  4

/**
\brief  Piezo-electric actuator type.

\deprecated
        As of version 3.4.52, this value has been replaced by
        #VIBE_DEVACTUATORTYPE_PIEZO.
*/
#define VIBE_DEVACTUATORTYPE_PIEZO_WAVE             4

/*@}*/


/**
\defgroup effectstyles Effect Styles

        Effect styles used with #ImmVibeModifyPlayingMagSweepEffect,
        #ImmVibeModifyPlayingPeriodicEffect, #ImmVibePlayMagSweepEffect,
        and #ImmVibePlayPeriodicEffect.
*/
/*@{*/

/**
\brief  Effect style mask.

        Effect styles are 4 bits and may be bitwise ORed with other flags.
*/
#define VIBE_STYLE_MASK                             0x0F

/** \brief Smooth effect style. */
#define VIBE_STYLE_SMOOTH                           0

/** \brief Strong effect style. */
#define VIBE_STYLE_STRONG                           1

/** \brief Sharp effect style. */
#define VIBE_STYLE_SHARP                            2

/** \brief Default effect style. */
#define VIBE_DEFAULT_STYLE                          VIBE_STYLE_STRONG

/** \deprecated Use #VIBE_STYLE_SMOOTH instead. */
#define VIBE_CONTROLMODE_SMOOTH                     VIBE_STYLE_SMOOTH

/** \deprecated Use #VIBE_STYLE_STRONG instead. */
#define VIBE_CONTROLMODE_STRONG                     VIBE_STYLE_STRONG

/** \deprecated Use #VIBE_STYLE_SHARP instead. */
#define VIBE_CONTROLMODE_SHARP                      VIBE_STYLE_SHARP

/** \deprecated Use #VIBE_DEFAULT_STYLE instead. */
#define VIBE_DEFAULT_CONTROLMODE                    VIBE_DEFAULT_STYLE

/*@}*/


/**
\defgroup stylesupport Effect Style Support Flags

        Effect style support bit flags returned by
        #ImmVibeGetDeviceCapabilityInt32 for the
        #VIBE_DEVCAPTYPE_SUPPORTED_STYLES device capability.
*/
/*@{*/

/**
\brief  Effect style support mask.

        Effect style support bit flags occupy 16 bits and may be bitwise ORed
        with other flags.
*/
#define VIBE_STYLE_SUPPORT_MASK                     0x0000FFFF

/** \brief Smooth effect style support bit flag. */
#define VIBE_STYLE_SMOOTH_SUPPORT                   (1 << VIBE_STYLE_SMOOTH)

/** \brief Strong effect style support bit flag. */
#define VIBE_STYLE_STRONG_SUPPORT                   (1 << VIBE_STYLE_STRONG)

/** \brief Sharp effect style support bit flag. */
#define VIBE_STYLE_SHARP_SUPPORT                    (1 << VIBE_STYLE_SHARP)

/** \deprecated Use #VIBE_STYLE_SMOOTH_SUPPORT instead. */
#define VIBE_CONTROLMODE_SMOOTH_SUPPORT             VIBE_STYLE_SMOOTH_SUPPORT

/** \deprecated Use #VIBE_STYLE_STRONG_SUPPORT instead. */
#define VIBE_CONTROLMODE_STRONG_SUPPORT             VIBE_STYLE_STRONG_SUPPORT

/** \deprecated Use #VIBE_STYLE_SHARP_SUPPORT instead. */
#define VIBE_CONTROLMODE_SHARP_SUPPORT              VIBE_STYLE_SHARP_SUPPORT

/*@}*/


/**
\defgroup wavetypes Effect Wave Types

        Periodic effect wave types used with
        #ImmVibeModifyPlayingPeriodicEffect and #ImmVibePlayPeriodicEffect.
*/
/*@{*/

/**
\brief  Periodic effect wave type shift.

        Periodic effect wave types are 4 bits and may be bitwise ORed with other
        flags.
 */
#define VIBE_WAVETYPE_SHIFT                         4

/**
\brief  Periodic effect wave type mask.

        Periodic effect wave types are 4 bits and may be bitwise ORed with other
        flags.
 */
#define VIBE_WAVETYPE_MASK                          0xF0

/** \brief Square wave type. */
#define VIBE_WAVETYPE_SQUARE                        (1 << VIBE_WAVETYPE_SHIFT)

/** \brief Triangle wave type. */
#define VIBE_WAVETYPE_TRIANGLE                      (2 << VIBE_WAVETYPE_SHIFT)

/** \brief Sine wave type. */
#define VIBE_WAVETYPE_SINE                          (3 << VIBE_WAVETYPE_SHIFT)

/** \brief Sawtooth up wave type. */
#define VIBE_WAVETYPE_SAWTOOTHUP                    (4 << VIBE_WAVETYPE_SHIFT)

/** \brief Sawtooth down wave type. */
#define VIBE_WAVETYPE_SAWTOOTHDOWN                  (5 << VIBE_WAVETYPE_SHIFT)

/** \brief Default wave type. */
#define VIBE_DEFAULT_WAVETYPE                       VIBE_WAVETYPE_SQUARE

/*@}*/


/**
\defgroup wavetypesupport Effect Wave Type Support Flags

        Wave type support bit flags returned by #ImmVibeGetDeviceCapabilityInt32
        for the #VIBE_DEVCAPTYPE_SUPPORTED_WAVE_TYPES device capability.
*/
/*@{*/

/**
\brief  Wave type support mask.

        Effect wave type support bit flags occupy 16 bits and may be bitwise
        ORed with other flags.
 */
#define VIBE_WAVETYPE_SUPPORT_MASK                  0xFFFF0000

/** \brief Square wave type support bit flag. */
#define VIBE_WAVETYPE_SQUARE_SUPPORT                (0x10000 << (VIBE_WAVETYPE_SQUARE >> VIBE_WAVETYPE_SHIFT))

/** \brief Triangle wave type support bit flag. */
#define VIBE_WAVETYPE_TRIANGLE_SUPPORT              (0x10000 << (VIBE_WAVETYPE_TRIANGLE >> VIBE_WAVETYPE_SHIFT))

/** \brief Sine wave type support bit flag. */
#define VIBE_WAVETYPE_SINE_SUPPORT                  (0x10000 << (VIBE_WAVETYPE_SINE >> VIBE_WAVETYPE_SHIFT))

/** \brief Sawtooth up wave type support bit flag. */
#define VIBE_WAVETYPE_SAWTOOTHUP_SUPPORT            (0x10000 << (VIBE_WAVETYPE_SAWTOOTHUP >> VIBE_WAVETYPE_SHIFT))

/** \brief Sawtooth down wave type support bit flag. */
#define VIBE_WAVETYPE_SAWTOOTHDOWN_SUPPORT          (0x10000 << (VIBE_WAVETYPE_SAWTOOTHDOWN >> VIBE_WAVETYPE_SHIFT))

/*@}*/


/**
\defgroup effectypes Effect Types

        Effect types returned by #ImmVibeGetIVTEffectType.
*/
/*@{*/

/** \brief Periodic effect type. */
#define VIBE_EFFECT_TYPE_PERIODIC                   0

/** \brief MagSweep effect type. */
#define VIBE_EFFECT_TYPE_MAGSWEEP                   1

/** \brief Timeline effect type. */
#define VIBE_EFFECT_TYPE_TIMELINE                   2

/**
\brief  Streaming effect type.

\since  Version 1.5. See \ref versioning for details about \api version numbers.
*/
#define VIBE_EFFECT_TYPE_STREAMING                  3

/**
\brief  Waveform effect type.

\since  Version 3.4. See \ref versioning for details about \api version numbers.
*/
#define VIBE_EFFECT_TYPE_WAVEFORM                   4

/**
\brief Interpolated effect type.
\since Version 3.6. See \ref versioning for details about \api version numbers.
*/
#define VIBE_EFFECT_TYPE_INTERPOLATED               5

/*@}*/


/**
\defgroup effectypesupport Effect Type Support Bit Flags

        Effect type support bit flags returned by
        #ImmVibeGetDeviceCapabilityInt32 for the
        #VIBE_DEVCAPTYPE_SUPPORTED_EFFECTS device capability.
*/
/*@{*/

/** \brief Periodic effect type support bit flag. */
#define VIBE_PERIODIC_EFFECT_SUPPORT                (1 << VIBE_EFFECT_TYPE_PERIODIC)

/** \brief MagSweep effect type support bit flag. */
#define VIBE_MAGSWEEP_EFFECT_SUPPORT                (1 << VIBE_EFFECT_TYPE_MAGSWEEP)

/** \brief Timeline effect type support bit flag. */
#define VIBE_TIMELINE_EFFECT_SUPPORT                (1 << VIBE_EFFECT_TYPE_TIMELINE)

/**
\brief Streaming effect type support bit flag.

\since  Version 1.5. See \ref versioning for details about \api version numbers.
*/
#define VIBE_STREAMING_EFFECT_SUPPORT               (1 << VIBE_EFFECT_TYPE_STREAMING)

/**
\brief Waveform effect type support bit flag.

\since  Version 3.4. See \ref versioning for details about \api version numbers.
*/
#define VIBE_WAVEFORM_EFFECT_SUPPORT                (1 << VIBE_EFFECT_TYPE_WAVEFORM)

/*@}*/


/**
\defgroup editions API Edition Levels

        \api edition levels returned by #ImmVibeGetDeviceCapabilityInt32 for the
        #VIBE_DEVCAPTYPE_EDITION_LEVEL device capability.
*/
/*@{*/

/** \brief 3000 Series edition level. */
#define VIBE_EDITION_3000                           3000

/** \brief 4000 Series edition level. */
#define VIBE_EDITION_4000                           4000

/** \brief 5000 Series edition level. */
#define VIBE_EDITION_5000                           5000

/*@}*/


/**
\defgroup prioritydefs Device Priority Levels

        Device priority levels returned by #ImmVibeGetDevicePropertyInt32 or
        used with #ImmVibeSetDevicePropertyInt32 for the
        #VIBE_DEVPROPTYPE_PRIORITY device property.
*/
/*@{*/

/** \brief Minimum device priority. */
#define VIBE_MIN_DEVICE_PRIORITY                    0x0

/** \brief Maximum device priority for 3rd-party applications. */
#define VIBE_MAX_DEV_DEVICE_PRIORITY                0x7

/** \brief Minimum device priority for OEM applications. */
#define VIBE_MAX_OEM_DEVICE_PRIORITY                0xF

/**
\brief Maximum device priority.

\deprecated
        This value has been replaced by #VIBE_MAX_DEV_DEVICE_PRIORITY and
        #VIBE_MAX_OEM_DEVICE_PRIORITY to be used by third-party developers and
        device manufacturers, respectively.
*/
#define VIBE_MAX_DEVICE_PRIORITY                    VIBE_MAX_OEM_DEVICE_PRIORITY

/** \brief Default device priority. */
#define VIBE_DEVPRIORITY_DEFAULT                    0

/*@}*/


/**
\defgroup   devstates   Device States

        Device states returned by #ImmVibeGetDeviceState.
*/
/*@{*/

/** \brief Device state indicating that the device is attached to the system. */
#define VIBE_DEVICESTATE_ATTACHED                   (1 << 0)

/** \brief Device state indicating that the device is busy playing effects. */
#define VIBE_DEVICESTATE_BUSY                       (1 << 1)

/*@}*/


/**
\defgroup effectstates Effect States

        Effect states returned by #ImmVibeGetEffectState.
*/
/*@{*/

/**
\brief  Effect state indicating that the effect is not playing and not paused.
*/
#define VIBE_EFFECT_STATE_NOT_PLAYING               0

/**
\brief  Effect state indicating that the effect is playing.
*/
#define VIBE_EFFECT_STATE_PLAYING                   1

/**
\brief  Effect state indicating that the effect is paused.
*/
#define VIBE_EFFECT_STATE_PAUSED                    2

/*@}*/


/**
\defgroup ivtelemtypes IVT Element Types

        IVT element types for the \c m_nElementType member of the
        #VibeIVTElement structure.
*/
/*@{*/

/** \brief Periodic effect element of a Timeline effect. */
#define VIBE_ELEMTYPE_PERIODIC                      0

/** \brief MagSweep effect element of a Timeline effect. */
#define VIBE_ELEMTYPE_MAGSWEEP                      1

/** \brief Repeat element of a Timeline effect. */
#define VIBE_ELEMTYPE_REPEAT                        2

/**
\brief Waveform effect element of a Timeline effect.

\since Version 3.4. See \ref versioning for details about \api version numbers.
*/
#define VIBE_ELEMTYPE_WAVEFORM                      3

/**
\brief Periodic effect element of an interpolated effect.
*/
#define VIBE_KEYFTYPE_PERIODIC                      4

/**
\brief MagSweep effect element of a an interpolated effect.
*/
#define VIBE_KEYFTYPE_MAGSWEEP                      5

/*@}*/


/**
\brief  Invalid index.

        Used to initialize device and effect indices, for example.
*/
#define VIBE_INVALID_INDEX                          -1

/**
\brief  Invalid effect handle.

        Used to initialize an effect handle, for example.
*/
#define VIBE_INVALID_EFFECT_HANDLE_VALUE            -1

/**
\brief  Invalid device handle.

        Used to initialize a device handle, for example.
*/
#define VIBE_INVALID_DEVICE_HANDLE_VALUE            -1

/**
\brief  Maximum force magnitude.

        Used with #ImmVibePlayMagSweepEffect and #ImmVibePlayPeriodicEffect to
        play an effect with maximum force, for example.
*/
#define VIBE_MAX_MAGNITUDE                          10000 /**< Maximum Force Magnitude */

/** \brief  Minimum force magnitude. */
#define VIBE_MIN_MAGNITUDE                          0

/**
\brief  Infinite time.

        Used with #ImmVibePlayMagSweepEffect and #ImmVibePlayPeriodicEffect to
        play an effect of indefinite duration, for example.
*/
#define VIBE_TIME_INFINITE                          VIBE_INT32_MAX

/**
\brief Maximum interpolant value.

        Used with #ImmVibePlayIVTInterpolatedEffect,
        #ImmVibeGetIVTInterpolatedEffectDuration, and
        #ImmVibeModifyPlayingInterpolatedEffectInterpolant to
        interpolate at the last key frame.
*/
#define VIBE_MAX_INTERPOLANT                         10000

/**
\brief  Microsecond period bit flag.

        Used with #ImmVibePlayPeriodicEffect and
        #ImmVibeModifyPlayingPeriodicEffect to specify the period in
        microseconds.
*/
#define VIBE_PERIOD_RESOLUTION_MICROSECOND          0x80000000

/**
\brief  Infinite repeat count.

        Used with #ImmVibePlayIVTEffectRepeat.
*/
#define VIBE_REPEAT_COUNT_INFINITE                  255

/**
\brief  Maximum device name length.

        Device names returned by #ImmVibeGetDeviceCapabilityString for the
        #VIBE_DEVCAPTYPE_DEVICE_NAME device capability type will not exceed this
        length.
*/
#define VIBE_MAX_DEVICE_NAME_LENGTH                 64

/**
\brief  Maximum effect name length.

        Effect names returned by #ImmVibeGetIVTEffectName will not exceed this
        length.
*/
#define VIBE_MAX_EFFECT_NAME_LENGTH                 128

/**
\brief Maximum device capability string length.

       String device capabilities returned by #ImmVibeGetDeviceCapabilityString
       will not exceed this length.
*/
#define VIBE_MAX_CAPABILITY_STRING_LENGTH           64

/**
\brief Maximum device property string length.

       String device properties returned by #ImmVibeGetDevicePropertyString or
       set with #ImmVibeSetDevicePropertyString will not exceed this length.
*/
#define VIBE_MAX_PROPERTY_STRING_LENGTH             64

/**
\brief  Maximum Streaming Sample size.

        Used with #ImmVibePlayStreamingSample.
*/
#define VIBE_MAX_STREAMING_SAMPLE_SIZE              255

/**
\brief Maximum number of actuators that can be supported by a composite device.

        Used with #ImmVibeOpenCompositeDevice.
*/
#define VIBE_MAX_LOGICAL_DEVICE_COUNT               16

/** \brief Tests whether a device handle is invalid. */
#define VIBE_IS_INVALID_DEVICE_HANDLE(n)            (((n) == 0) || ((n) == VIBE_INVALID_DEVICE_HANDLE_VALUE))

/** \brief Tests whether an effect handle is invalid. */
#define VIBE_IS_INVALID_EFFECT_HANDLE(n)            (((n) == 0) || ((n) == VIBE_INVALID_EFFECT_HANDLE_VALUE))

/** \brief Tests whether a device handle is valid. */
#define VIBE_IS_VALID_DEVICE_HANDLE(n)              (((n) != 0) && ((n) != VIBE_INVALID_DEVICE_HANDLE_VALUE))

/** \brief Tests whether an effect handle is valid. */
#define VIBE_IS_VALID_EFFECT_HANDLE(n)              (((n) != 0) && ((n) != VIBE_INVALID_EFFECT_HANDLE_VALUE))


/** \defgroup retstatcodes Return Status Codes */
/*@{*/

/** \brief Tests whether a return status status code represents success. */
#define VIBE_SUCCEEDED(n)                           ((n) >= 0)

/** \brief Tests whether a return status status code represents failure. */
#define VIBE_FAILED(n)                              ((n) < 0)

/** \brief  No error. */
#define VIBE_S_SUCCESS                               0

/**
\brief  Indicates a false condition.

        This code is not returned by \api functions.
*/
#define VIBE_S_FALSE                                 0

/**
\brief  Indicates a true condition.

        This code is not returned by \api functions.
*/
#define VIBE_S_TRUE                                  1

/** \brief The effect is not playing. */
#define VIBE_W_NOT_PLAYING                           1

/**
\brief  The device handle does not have enough priority to play the effect.

        Another effect is currently playing on another higher priority device
        handle. Before playing the effect, you must call
        #ImmVibeStopPlayingEffect or #ImmVibeStopAllPlayingEffects to stop
        currently playing effects, or call #ImmVibeSetDevicePropertyInt32 to
        increase value of the #VIBE_DEVPROPTYPE_PRIORITY property.
*/
#define VIBE_W_INSUFFICIENT_PRIORITY                 2

/**
\brief  Effects playing on the device handle have been disabled.

        Call #ImmVibeSetDevicePropertyInt32 to set the
        #VIBE_DEVPROPTYPE_DISABLE_EFFECTS property to false to re-enable playing
        effects on the device handle.
*/
#define VIBE_W_EFFECTS_DISABLED                      3

/** \brief The effect is not paused. */
#define VIBE_W_NOT_PAUSED                            4

/**
\brief  The \api is already initialized.

        This status code is not returned by \api functions.
*/
#define VIBE_E_ALREADY_INITIALIZED                  -1

/**
\brief  The \api has not been initialized.

        #ImmVibeInitialize was not called or returned an error.
*/
#define VIBE_E_NOT_INITIALIZED                      -2

/** \brief One or more of the arguments are invalid. */
#define VIBE_E_INVALID_ARGUMENT                     -3

/** \brief  Generic error. */
#define VIBE_E_FAIL                                 -4

/** \brief Incorrect effect type. */
#define VIBE_E_INCOMPATIBLE_EFFECT_TYPE             -5

/** \brief Incorrect device capability type. */
#define VIBE_E_INCOMPATIBLE_CAPABILITY_TYPE         -6

/** \brief Incorrect device property type. */
#define VIBE_E_INCOMPATIBLE_PROPERTY_TYPE           -7

/**
\brief  Access to the device is locked until a valid license key is provided.

        This error may be returned only by \api version 3.2 or lower. Subsequent
        versions of the \apibrief do not require a license key. See \ref
        licensing for details about setting license keys.
*/
#define VIBE_E_DEVICE_NEEDS_LICENSE                 -8

/**
\brief  The function cannot allocate memory to complete the request.

        This happens if the device runs low in memory.

*/
#define VIBE_E_NOT_ENOUGH_MEMORY                    -9

/**
\brief  The \service is not running.

        You may need to reboot the device or reinstall the \api to restore the
        default settings.
*/
#define VIBE_E_SERVICE_NOT_RUNNING                  -10

/**
\brief  Cannot change the #VIBE_DEVPROPTYPE_MASTERSTRENGTH property because the
        device is not using an OEM license key or the priority is not set to
        #VIBE_MAX_OEM_DEVICE_PRIORITY.

        See \ref priority for details about priority levels.
*/
#define VIBE_E_INSUFFICIENT_PRIORITY                -11

/**
\brief  The \service is busy and could not complete the requested function call.
*/
#define VIBE_E_SERVICE_BUSY                         -12

/**
\brief  The function is not supported in the edition level of the \api.

        See \ref editions for details about \api editions.
*/
#define VIBE_E_NOT_SUPPORTED                        -13

/*@}*/


/**
\brief  Return status code type for \api functions.

        See \ref retstatcodes.
*/
typedef VibeInt32   VibeStatus;

/**
\brief  Contains information about a Periodic effect element within a
        #VibeIVTElement structure.

\since  Version 3.2. See \ref versioning for details about \api version numbers.

\deprecated
        The #VibeIVTPeriodic structure has been superceded by #VibeIVTPeriodic2
        structure as of version 3.3. See \ref versioning for details about \api
        version numbers.
*/
typedef struct
{
	VibeInt32       m_nDuration;
	VibeInt32       m_nMagnitude;
	VibeInt32       m_nPeriod;
	VibeInt32       m_nStyle;
	VibeInt32       m_nAttackTime;
	VibeInt32       m_nAttackLevel;
	VibeInt32       m_nFadeTime;
	VibeInt32       m_nFadeLevel;
} VibeIVTPeriodic;

/**
\brief  Contains information about a Periodic effect element within a
        #VibeIVTElement2 structure.

        The #VibeIVTPeriodic2 structure is identical to the #VibeIVTPeriodic
        structure with the addition of a member specifying an actuator index
        supporting Timeline effects targeting multiple actuators on composite
        devices.

\since  Version 3.3. See \ref versioning for details about \api version numbers.
*/
typedef struct
{
	VibeInt32       m_nDuration;
	VibeInt32       m_nMagnitude;
	VibeInt32       m_nPeriod;
	VibeInt32       m_nStyle;
	VibeInt32       m_nAttackTime;
	VibeInt32       m_nAttackLevel;
	VibeInt32       m_nFadeTime;
	VibeInt32       m_nFadeLevel;
	/* New in v3.3 */
	VibeInt32       m_nActuatorIndex;
} VibeIVTPeriodic2;

typedef struct
{
	VibeInt32       m_nDuration;
	VibeInt32       m_nMagnitude;
	VibeInt32       m_nPeriod;
	VibeInt32       m_nStyle;
	VibeInt32       m_nAttackTime;
	VibeInt32       m_nAttackLevel;
	VibeInt32       m_nFadeTime;
	VibeInt32       m_nFadeLevel;
	/* New in v3.3 */
	VibeInt32       m_nActuatorIndex;
	/* New in v3.6 */
	VibeInt32       m_nRepeatGap;
} VibeIVTLerpPeriodic;

/**
\brief  Contains information about a MagSweep effect element within a
        #VibeIVTElement structure.

\since  Version 3.2. See \ref versioning for details about \api version numbers.

\deprecated
        The #VibeIVTMagSweep structure has been superceded by #VibeIVTMagSweep2
        structure as of version 3.3. See \ref versioning for details about \api
        version numbers.
*/
typedef struct
{
	VibeInt32       m_nDuration;
	VibeInt32       m_nMagnitude;
	VibeInt32       m_nStyle;
	VibeInt32       m_nAttackTime;
	VibeInt32       m_nAttackLevel;
	VibeInt32       m_nFadeTime;
	VibeInt32       m_nFadeLevel;
} VibeIVTMagSweep;

/**
\brief  Contains information about a MagSweep effect element within a
        #VibeIVTElement2 structure.

        The #VibeIVTMagSweep2 structure is identical to the #VibeIVTMagSweep
        structure with the addition of a member specifying an actuator index
        supporting Timeline effects targeting multiple actuators on composite
        devices.

\since  Version 3.3. See \ref versioning for details about \api version numbers.
*/
typedef struct
{
	VibeInt32       m_nDuration;
	VibeInt32       m_nMagnitude;
	VibeInt32       m_nStyle;
	VibeInt32       m_nAttackTime;
	VibeInt32       m_nAttackLevel;
	VibeInt32       m_nFadeTime;
	VibeInt32       m_nFadeLevel;
	/* New in v3.3 */
	VibeInt32       m_nActuatorIndex;
} VibeIVTMagSweep2;

typedef struct
{
	VibeInt32       m_nDuration;
	VibeInt32       m_nMagnitude;
	VibeInt32       m_nStyle;
	VibeInt32       m_nAttackTime;
	VibeInt32       m_nAttackLevel;
	VibeInt32       m_nFadeTime;
	VibeInt32       m_nFadeLevel;
	/* New in v3.3 */
	VibeInt32       m_nActuatorIndex;
	/* New in v3.6 */
	VibeInt32       m_nRepeatGap;
} VibeIVTLerpMagSweep;

/**
\brief  Represents a repeat event within a Timeline effect.
*/
typedef struct
{
	VibeInt32       m_nCount;
	VibeInt32       m_nDuration;
} VibeIVTRepeat;

/**
\brief  Contains information about a Waveform effect element within a
        #VibeIVTElement3 structure.

\since  Version 3.4. See \ref versioning for details about \api version numbers.
*/
typedef struct
{
	const VibeUInt8 *m_pData;
	VibeInt32           m_nDataSize;
	VibeInt32           m_nSamplingRate;
	VibeInt32           m_nBitDepth;
	VibeInt32           m_nMagnitude;
	VibeInt32           m_nActuatorIndex;
} VibeIVTWaveform;

/**
\brief  A Timeline effect is defined by a sequence of #VibeIVTElement
        structures.

\since  Version 3.2. See \ref versioning for details about \api version numbers.

\deprecated
        The #VibeIVTElement structure has been superceded by #VibeIVTElement3
        structure as of version 3.4. See \ref versioning for details about \api
        version numbers.
*/
typedef struct
{
	VibeInt32       m_nElementType;
	VibeInt32       m_nTime;
	union {
		VibeIVTPeriodic     m_periodic;
		VibeIVTMagSweep     m_magsweep;
		VibeIVTRepeat       m_repeat;
	} TypeSpecific;
} VibeIVTElement;

/**
\brief  Like the #VibeIVTElement structure but contains #VibeIVTPeriodic2 and
        #VibeIVTMagSweep2 structures instead of #VibeIVTPeriodic and
        #VibeIVTMagSweep structures, respectively.

        The #VibeIVTElement2 structure is more general than the #VibeIVTElement
        structure and supports Timeline effects targeting multiple actuators on
        composite devices.

\since  Version 3.3. See \ref versioning for details about \api version numbers.

\deprecated
        The #VibeIVTElement2 structure has been superceded by #VibeIVTElement3
        structure as of version 3.4. See \ref versioning for details about \api
        version numbers.
*/
typedef struct
{
	VibeInt32       m_nElementType;
	VibeInt32       m_nTime;
	union {
		VibeIVTPeriodic2    m_periodic;
		VibeIVTMagSweep2    m_magsweep;
		VibeIVTRepeat       m_repeat;
	} TypeSpecific;
} VibeIVTElement2;

/**
\brief  Like the #VibeIVTElement2 structure but contains additionally a
        #VibeIVTWaveform structure and therefore supports Waveform effects.

\since  Version 3.4. See \ref versioning for details about \api version numbers.
*/
typedef struct
{
	VibeInt32       m_nElementType;
	VibeInt32       m_nTime;
	union {
		VibeIVTPeriodic2    m_periodic;
		VibeIVTMagSweep2    m_magsweep;
		VibeIVTRepeat       m_repeat;
		/* New in v3.4 */
		VibeIVTWaveform     m_waveform;
	} TypeSpecific;
} VibeIVTElement3;

#endif /* IMMVIBECORE_H */
