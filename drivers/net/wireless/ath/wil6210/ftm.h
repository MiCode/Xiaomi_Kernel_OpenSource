/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef __WIL6210_FTM_H__
#define __WIL6210_FTM_H__

/**
 * NOTE: The authoritative place for definition of QCA_NL80211_VENDOR_ID,
 * vendor subcmd definitions prefixed with QCA_NL80211_VENDOR_SUBCMD, and
 * qca_wlan_vendor_attr is open source file src/common/qca-vendor.h in
 * git://w1.fi/srv/git/hostap.git; the values here are just a copy of that
 */

/**
 * enum qca_vendor_attr_loc - attributes for FTM and AOA commands
 *
 * @QCA_WLAN_VENDOR_ATTR_FTM_SESSION_COOKIE: Session cookie, specified in
 *  %QCA_NL80211_VENDOR_SUBCMD_FTM_START_SESSION. It will be provided by driver
 *  events and can be used to identify events targeted for this session.
 * @QCA_WLAN_VENDOR_ATTR_LOC_CAPA: Nested attribute containing extra
 *  FTM/AOA capabilities, returned by %QCA_NL80211_VENDOR_SUBCMD_LOC_GET_CAPA.
 *  see %enum qca_wlan_vendor_attr_loc_capa.
 * @QCA_WLAN_VENDOR_ATTR_FTM_MEAS_PEERS: array of nested attributes
 *  containing information about each peer in measurement session
 *  request. See %enum qca_wlan_vendor_attr_peer_info for supported
 *  attributes for each peer
 * @QCA_WLAN_VENDOR_ATTR_FTM_PEER_RESULTS: nested attribute containing
 *  measurement results for a peer. reported by the
 *  %QCA_NL80211_VENDOR_SUBCMD_FTM_MEAS_RESULT event.
 *  See %enum qca_wlan_vendor_attr_peer_result for list of supported
 *  attributes.
 * @QCA_WLAN_VENDOR_ATTR_FTM_RESPONDER_ENABLE: flag attribute for
 *  enabling or disabling responder functionality.
 * @QCA_WLAN_VENDOR_ATTR_FTM_LCI: used in the
 *  %QCA_NL80211_VENDOR_SUBCMD_FTM_CFG_RESPONDER command in order to
 *  specify the LCI report that will be sent by the responder during
 *  a measurement exchange. The format is defined in IEEE P802.11-REVmc/D5.0,
 *  9.4.2.22.10
 * @QCA_WLAN_VENDOR_ATTR_FTM_LCR: provided with the
 *  %QCA_NL80211_VENDOR_SUBCMD_FTM_CFG_RESPONDER command in order to
 *  specify the location civic report that will be sent by the responder during
 *  a measurement exchange. The format is defined in IEEE P802.11-REVmc/D5.0,
 *  9.4.2.22.13
 * @QCA_WLAN_VENDOR_ATTR_LOC_SESSION_STATUS: session/measurement completion
 *  status code, reported in %QCA_NL80211_VENDOR_SUBCMD_FTM_SESSION_DONE
 *  and %QCA_NL80211_VENDOR_SUBCMD_AOA_MEAS_RESULT
 * @QCA_WLAN_VENDOR_ATTR_FTM_INITIAL_TOKEN: initial dialog token used
 *  by responder (0 if not specified)
 * @QCA_WLAN_VENDOR_ATTR_AOA_TYPE: AOA measurement type. Requested in
 *  %QCA_NL80211_VENDOR_SUBCMD_AOA_MEAS and optionally in
 *  %QCA_NL80211_VENDOR_SUBCMD_FTM_START_SESSION if AOA measurements
 *  are needed as part of an FTM session.
 *  Reported by QCA_NL80211_VENDOR_SUBCMD_AOA_MEAS_RESULT.
 *  See enum qca_wlan_vendor_attr_aoa_type.
 * @QCA_WLAN_VENDOR_ATTR_LOC_ANTENNA_ARRAY_MASK: bit mask indicating
 *  which antenna arrays were used in location measurement.
 *  Reported in %QCA_NL80211_VENDOR_SUBCMD_FTM_MEAS_RESULT and
 *  %QCA_NL80211_VENDOR_SUBCMD_AOA_MEAS_RESULT
 * @QCA_WLAN_VENDOR_ATTR_AOA_MEAS_RESULT: AOA measurement data.
 *  Its contents depends on the AOA type and antenna array mask:
 *  %QCA_WLAN_VENDOR_ATTR_AOA_TYPE_TOP_CIR_PHASE: array of U16 values,
 *  phase of the strongest CIR path for each antenna in the measured
 *  array(s).
 *  %QCA_WLAN_VENDOR_ATTR_AOA_TYPE_TOP_CIR_PHASE_AMP: array of 2 U16
 *  values, phase and amplitude of the strongest CIR path for each
 *  antenna in the measured array(s)
 */
enum qca_wlan_vendor_attr_loc {
	/* we reuse these attributes */
	QCA_WLAN_VENDOR_ATTR_MAC_ADDR = 6,
	QCA_WLAN_VENDOR_ATTR_PAD = 13,
	QCA_WLAN_VENDOR_ATTR_FTM_SESSION_COOKIE = 14,
	QCA_WLAN_VENDOR_ATTR_LOC_CAPA = 15,
	QCA_WLAN_VENDOR_ATTR_FTM_MEAS_PEERS = 16,
	QCA_WLAN_VENDOR_ATTR_FTM_MEAS_PEER_RESULTS = 17,
	QCA_WLAN_VENDOR_ATTR_FTM_RESPONDER_ENABLE = 18,
	QCA_WLAN_VENDOR_ATTR_FTM_LCI = 19,
	QCA_WLAN_VENDOR_ATTR_FTM_LCR = 20,
	QCA_WLAN_VENDOR_ATTR_LOC_SESSION_STATUS = 21,
	QCA_WLAN_VENDOR_ATTR_FTM_INITIAL_TOKEN = 22,
	QCA_WLAN_VENDOR_ATTR_AOA_TYPE = 23,
	QCA_WLAN_VENDOR_ATTR_LOC_ANTENNA_ARRAY_MASK = 24,
	QCA_WLAN_VENDOR_ATTR_AOA_MEAS_RESULT = 25,
	/* keep last */
	QCA_WLAN_VENDOR_ATTR_LOC_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_LOC_MAX = QCA_WLAN_VENDOR_ATTR_LOC_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_loc_capa - indoor location capabilities
 *
 * @QCA_WLAN_VENDOR_ATTR_LOC_CAPA_FLAGS: various flags. See
 *  %enum qca_wlan_vendor_attr_loc_capa_flags
 * @QCA_WLAN_VENDOR_ATTR_FTM_CAPA_MAX_NUM_SESSIONS: Maximum number
 *  of measurement sessions that can run concurrently.
 *  Default is one session (no session concurrency)
 * @QCA_WLAN_VENDOR_ATTR_FTM_CAPA_MAX_NUM_PEERS: The total number of unique
 *  peers that are supported in running sessions. For example,
 *  if the value is 8 and maximum number of sessions is 2, you can
 *  have one session with 8 unique peers, or 2 sessions with 4 unique
 *  peers each, and so on.
 * @QCA_WLAN_VENDOR_ATTR_FTM_CAPA_MAX_NUM_BURSTS_EXP: Maximum number
 *  of bursts per peer, as an exponent (2^value). Default is 0,
 *  meaning no multi-burst support.
 * @QCA_WLAN_VENDOR_ATTR_FTM_CAPA_MAX_MEAS_PER_BURST: Maximum number
 *  of measurement exchanges allowed in a single burst
 * @QCA_WLAN_VENDOR_ATTR_AOA_CAPA_SUPPORTED_TYPES: Supported AOA measurement
 *  types. A bit mask (unsigned 32 bit value), each bit corresponds
 *  to an AOA type as defined by %enum qca_vendor_attr_aoa_type.
 */
enum qca_wlan_vendor_attr_loc_capa {
	QCA_WLAN_VENDOR_ATTR_LOC_CAPA_INVALID,
	QCA_WLAN_VENDOR_ATTR_LOC_CAPA_FLAGS,
	QCA_WLAN_VENDOR_ATTR_FTM_CAPA_MAX_NUM_SESSIONS,
	QCA_WLAN_VENDOR_ATTR_FTM_CAPA_MAX_NUM_PEERS,
	QCA_WLAN_VENDOR_ATTR_FTM_CAPA_MAX_NUM_BURSTS_EXP,
	QCA_WLAN_VENDOR_ATTR_FTM_CAPA_MAX_MEAS_PER_BURST,
	QCA_WLAN_VENDOR_ATTR_AOA_CAPA_SUPPORTED_TYPES,
	/* keep last */
	QCA_WLAN_VENDOR_ATTR_LOC_CAPA_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_LOC_CAPA_MAX =
		QCA_WLAN_VENDOR_ATTR_LOC_CAPA_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_loc_capa_flags: Indoor location capability flags
 *
 * @QCA_WLAN_VENDOR_ATTR_LOC_CAPA_FLAG_FTM_RESPONDER: Set if driver
 *  can be configured as an FTM responder (for example, an AP that
 *  services FTM requests). %QCA_NL80211_VENDOR_SUBCMD_FTM_CFG_RESPONDER
 *  will be supported if set.
 * @QCA_WLAN_VENDOR_ATTR_LOC_CAPA_FLAG_FTM_INITIATOR: Set if driver
 *  can run FTM sessions. %QCA_NL80211_VENDOR_SUBCMD_FTM_START_SESSION
 *  will be supported if set.
 * @QCA_WLAN_VENDOR_ATTR_LOC_CAPA_FLAG_ASAP: Set if FTM responder
 *  supports immediate (ASAP) response.
 * @QCA_WLAN_VENDOR_ATTR_LOC_CAPA_FLAG_AOA: Set if driver supports standalone
 *  AOA measurement using %QCA_NL80211_VENDOR_SUBCMD_AOA_MEAS
 * @QCA_WLAN_VENDOR_ATTR_LOC_CAPA_FLAG_AOA_IN_FTM: Set if driver supports
 *  requesting AOA measurements as part of an FTM session.
 */
enum qca_wlan_vendor_attr_loc_capa_flags {
	QCA_WLAN_VENDOR_ATTR_LOC_CAPA_FLAG_FTM_RESPONDER = 1 << 0,
	QCA_WLAN_VENDOR_ATTR_LOC_CAPA_FLAG_FTM_INITIATOR = 1 << 1,
	QCA_WLAN_VENDOR_ATTR_LOC_CAPA_FLAG_ASAP = 1 << 2,
	QCA_WLAN_VENDOR_ATTR_LOC_CAPA_FLAG_AOA = 1 << 3,
	QCA_WLAN_VENDOR_ATTR_LOC_CAPA_FLAG_AOA_IN_FTM = 1 << 4,
};

/**
 * enum qca_wlan_vendor_attr_peer_info: information about
 *  a single peer in a measurement session.
 *
 * @QCA_WLAN_VENDOR_ATTR_FTM_PEER_MAC_ADDR: The MAC address of the peer.
 * @QCA_WLAN_VENDOR_ATTR_FTM_PEER_MEAS_FLAGS: Various flags related
 *  to measurement. See %enum qca_wlan_vendor_attr_ftm_peer_meas_flags.
 * @QCA_WLAN_VENDOR_ATTR_FTM_PEER_MEAS_PARAMS: Nested attribute of
 *  FTM measurement parameters, as specified by IEEE P802.11-REVmc/D7.0,
 *  9.4.2.167. See %enum qca_wlan_vendor_attr_ftm_meas_param for
 *  list of supported attributes.
 * @QCA_WLAN_VENDOR_ATTR_FTM_PEER_SECURE_TOKEN_ID: Initial token ID for
 *  secure measurement
 * @QCA_WLAN_VENDOR_ATTR_FTM_PEER_AOA_BURST_PERIOD: Request AOA
 *  measurement every _value_ bursts. If 0 or not specified,
 *  AOA measurements will be disabled for this peer.
 */
enum qca_wlan_vendor_attr_ftm_peer_info {
	QCA_WLAN_VENDOR_ATTR_FTM_PEER_INVALID,
	QCA_WLAN_VENDOR_ATTR_FTM_PEER_MAC_ADDR,
	QCA_WLAN_VENDOR_ATTR_FTM_PEER_MEAS_FLAGS,
	QCA_WLAN_VENDOR_ATTR_FTM_PEER_MEAS_PARAMS,
	QCA_WLAN_VENDOR_ATTR_FTM_PEER_SECURE_TOKEN_ID,
	QCA_WLAN_VENDOR_ATTR_FTM_PEER_AOA_BURST_PERIOD,
	/* keep last */
	QCA_WLAN_VENDOR_ATTR_FTM_PEER_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_FTM_PEER_MAX =
		QCA_WLAN_VENDOR_ATTR_FTM_PEER_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_ftm_peer_meas_flags: Measurement request flags,
 *  per-peer
 * @QCA_WLAN_VENDOR_ATTR_FTM_PEER_MEAS_FLAG_ASAP: If set, request
 *  immediate (ASAP) response from peer
 * @QCA_WLAN_VENDOR_ATTR_FTM_PEER_MEAS_FLAG_LCI: If set, request
 *  LCI report from peer. The LCI report includes the absolute
 *  location of the peer in "official" coordinates (similar to GPS).
 *  See IEEE P802.11-REVmc/D7.0, 11.24.6.7 for more information.
 * @QCA_WLAN_VENDOR_ATTR_FTM_PEER_MEAS_FLAG_LCR: If set, request
 *  Location civic report from peer. The LCR includes the location
 *  of the peer in free-form format. See IEEE P802.11-REVmc/D7.0,
 *  11.24.6.7 for more information.
 * @QCA_WLAN_VENDOR_ATTR_FTM_PEER_MEAS_FLAG_SECURE: If set,
 *  request a secure measurement.
 *  %QCA_WLAN_VENDOR_ATTR_FTM_PEER_SECURE_TOKEN_ID must also be provided.
 */
enum qca_wlan_vendor_attr_ftm_peer_meas_flags {
	QCA_WLAN_VENDOR_ATTR_FTM_PEER_MEAS_FLAG_ASAP	= 1 << 0,
	QCA_WLAN_VENDOR_ATTR_FTM_PEER_MEAS_FLAG_LCI	= 1 << 1,
	QCA_WLAN_VENDOR_ATTR_FTM_PEER_MEAS_FLAG_LCR	= 1 << 2,
	QCA_WLAN_VENDOR_ATTR_FTM_PEER_MEAS_FLAG_SECURE  = 1 << 3,
};

/**
 * enum qca_wlan_vendor_attr_ftm_meas_param: Measurement parameters
 *
 * @QCA_WLAN_VENDOR_ATTR_FTM_PARAM_MEAS_PER_BURST: Number of measurements
 *  to perform in a single burst.
 * @QCA_WLAN_VENDOR_ATTR_FTM_PARAM_NUM_BURSTS_EXP: Number of bursts to
 *  perform, specified as an exponent (2^value)
 * @QCA_WLAN_VENDOR_ATTR_FTM_PARAM_BURST_DURATION: Duration of burst
 *  instance, as specified in IEEE P802.11-REVmc/D7.0, 9.4.2.167
 * @QCA_WLAN_VENDOR_ATTR_FTM_PARAM_BURST_PERIOD: Time between bursts,
 *  as specified in IEEE P802.11-REVmc/D7.0, 9.4.2.167. Must
 *  be larger than %QCA_WLAN_VENDOR_ATTR_FTM_PARAM_BURST_DURATION
 */
enum qca_wlan_vendor_attr_ftm_meas_param {
	QCA_WLAN_VENDOR_ATTR_FTM_PARAM_INVALID,
	QCA_WLAN_VENDOR_ATTR_FTM_PARAM_MEAS_PER_BURST,
	QCA_WLAN_VENDOR_ATTR_FTM_PARAM_NUM_BURSTS_EXP,
	QCA_WLAN_VENDOR_ATTR_FTM_PARAM_BURST_DURATION,
	QCA_WLAN_VENDOR_ATTR_FTM_PARAM_BURST_PERIOD,
	/* keep last */
	QCA_WLAN_VENDOR_ATTR_FTM_PARAM_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_FTM_PARAM_MAX =
		QCA_WLAN_VENDOR_ATTR_FTM_PARAM_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_ftm_peer_result: Per-peer results
 *
 * @QCA_WLAN_VENDOR_ATTR_FTM_PEER_RES_MAC_ADDR: MAC address of the reported
 *  peer
 * @QCA_WLAN_VENDOR_ATTR_FTM_PEER_RES_STATUS: Status of measurement
 *  request for this peer.
 *  See %enum qca_wlan_vendor_attr_ftm_peer_result_status
 * @QCA_WLAN_VENDOR_ATTR_FTM_PEER_RES_FLAGS: Various flags related
 *  to measurement results for this peer.
 *  See %enum qca_wlan_vendor_attr_ftm_peer_result_flags
 * @QCA_WLAN_VENDOR_ATTR_FTM_PEER_RES_VALUE_SECONDS: Specified when
 *  request failed and peer requested not to send an additional request
 *  for this number of seconds.
 * @QCA_WLAN_VENDOR_ATTR_FTM_PEER_RES_LCI: LCI report when received
 *  from peer. In the format specified by IEEE P802.11-REVmc/D7.0,
 *  9.4.2.22.10
 * @QCA_WLAN_VENDOR_ATTR_FTM_PEER_RES_LCR: Location civic report when
 *  received from peer.In the format specified by IEEE P802.11-REVmc/D7.0,
 *  9.4.2.22.13
 * @QCA_WLAN_VENDOR_ATTR_FTM_PEER_RES_MEAS_PARAMS: Reported when peer
 *  overridden some measurement request parameters. See
 *  enum qca_wlan_vendor_attr_ftm_meas_param.
 * @QCA_WLAN_VENDOR_ATTR_FTM_PEER_RES_AOA_MEAS: AOA measurement
 *  for this peer. Same contents as %QCA_WLAN_VENDOR_ATTR_AOA_MEAS_RESULT
 * @QCA_WLAN_VENDOR_ATTR_FTM_PEER_RES_MEAS: Array of measurement
 *  results. Each entry is a nested attribute defined
 *  by enum qca_wlan_vendor_attr_ftm_meas.
 */
enum qca_wlan_vendor_attr_ftm_peer_result {
	QCA_WLAN_VENDOR_ATTR_FTM_PEER_RES_INVALID,
	QCA_WLAN_VENDOR_ATTR_FTM_PEER_RES_MAC_ADDR,
	QCA_WLAN_VENDOR_ATTR_FTM_PEER_RES_STATUS,
	QCA_WLAN_VENDOR_ATTR_FTM_PEER_RES_FLAGS,
	QCA_WLAN_VENDOR_ATTR_FTM_PEER_RES_VALUE_SECONDS,
	QCA_WLAN_VENDOR_ATTR_FTM_PEER_RES_LCI,
	QCA_WLAN_VENDOR_ATTR_FTM_PEER_RES_LCR,
	QCA_WLAN_VENDOR_ATTR_FTM_PEER_RES_MEAS_PARAMS,
	QCA_WLAN_VENDOR_ATTR_FTM_PEER_RES_AOA_MEAS,
	QCA_WLAN_VENDOR_ATTR_FTM_PEER_RES_MEAS,
	/* keep last */
	QCA_WLAN_VENDOR_ATTR_FTM_PEER_RES_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_FTM_PEER_RES_MAX =
		QCA_WLAN_VENDOR_ATTR_FTM_PEER_RES_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_ftm_peer_result_status
 *
 * @QCA_WLAN_VENDOR_ATTR_FTM_PEER_RES_STATUS_OK: Request sent ok and results
 *  will be provided. Peer may have overridden some measurement parameters,
 *  in which case overridden parameters will be report by
 *  %QCA_WLAN_VENDOR_ATTR_FTM_PEER_RES_MEAS_PARAMS attribute
 * @QCA_WLAN_VENDOR_ATTR_FTM_PEER_RES_STATUS_INCAPABLE: Peer is incapable
 *  of performing the measurement request. No more results will be sent
 *  for this peer in this session.
 * @QCA_WLAN_VENDOR_ATTR_FTM_PEER_RES_STATUS_FAILED: Peer reported request
 *  failed, and requested not to send an additional request for number
 *  of seconds specified by %QCA_WLAN_VENDOR_ATTR_FTM_PEER_RES_VALUE_SECONDS
 *  attribute.
 * @QCA_WLAN_VENDOR_ATTR_FTM_PEER_RES_STATUS_INVALID: Request validation
 *  failed. Request was not sent over the air.
 */
enum qca_wlan_vendor_attr_ftm_peer_result_status {
	QCA_WLAN_VENDOR_ATTR_FTM_PEER_RES_STATUS_OK,
	QCA_WLAN_VENDOR_ATTR_FTM_PEER_RES_STATUS_INCAPABLE,
	QCA_WLAN_VENDOR_ATTR_FTM_PEER_RES_STATUS_FAILED,
	QCA_WLAN_VENDOR_ATTR_FTM_PEER_RES_STATUS_INVALID,
};

/**
 * enum qca_wlan_vendor_attr_ftm_peer_result_flags : Various flags
 *  for measurement result, per-peer
 *
 * @QCA_WLAN_VENDOR_ATTR_FTM_PEER_RES_FLAG_DONE: If set,
 *  measurement completed for this peer. No more results will be reported
 *  for this peer in this session.
 */
enum qca_wlan_vendor_attr_ftm_peer_result_flags {
	QCA_WLAN_VENDOR_ATTR_FTM_PEER_RES_FLAG_DONE = 1 << 0,
};

/**
 * enum qca_vendor_attr_loc_session_status: Session completion status code
 *
 * @QCA_WLAN_VENDOR_ATTR_LOC_SESSION_STATUS_OK: Session completed
 *  successfully.
 * @QCA_WLAN_VENDOR_ATTR_LOC_SESSION_STATUS_ABORTED: Session aborted
 *  by request
 * @QCA_WLAN_VENDOR_ATTR_LOC_SESSION_STATUS_INVALID: Session request
 *  was invalid and was not started
 * @QCA_WLAN_VENDOR_ATTR_LOC_SESSION_STATUS_FAILED: Session had an error
 *  and did not complete normally (for example out of resources)
 *
 */
enum qca_vendor_attr_loc_session_status {
	QCA_WLAN_VENDOR_ATTR_LOC_SESSION_STATUS_OK,
	QCA_WLAN_VENDOR_ATTR_LOC_SESSION_STATUS_ABORTED,
	QCA_WLAN_VENDOR_ATTR_LOC_SESSION_STATUS_INVALID,
	QCA_WLAN_VENDOR_ATTR_LOC_SESSION_STATUS_FAILED,
};

/**
 * enum qca_wlan_vendor_attr_ftm_meas: Single measurement data
 *
 * @QCA_WLAN_VENDOR_ATTR_FTM_MEAS_T1: Time of departure(TOD) of FTM packet as
 *  recorded by responder, in picoseconds.
 *  See IEEE P802.11-REVmc/D7.0, 11.24.6.4 for more information.
 * @QCA_WLAN_VENDOR_ATTR_FTM_MEAS_T2: Time of arrival(TOA) of FTM packet at
 *  initiator, in picoseconds.
 *  See IEEE P802.11-REVmc/D7.0, 11.24.6.4 for more information.
 * @QCA_WLAN_VENDOR_ATTR_FTM_MEAS_T3: TOD of ACK packet as recorded by
 *  initiator, in picoseconds.
 *  See IEEE P802.11-REVmc/D7.0, 11.24.6.4 for more information.
 * @QCA_WLAN_VENDOR_ATTR_FTM_MEAS_T4: TOA of ACK packet at
 *  responder, in picoseconds.
 *  See IEEE P802.11-REVmc/D7.0, 11.24.6.4 for more information.
 * @QCA_WLAN_VENDOR_ATTR_FTM_MEAS_RSSI: RSSI (signal level) as recorded
 *  during this measurement exchange. Optional and will be provided if
 *  the hardware can measure it.
 * @QCA_WLAN_VENDOR_ATTR_FTM_MEAS_TOD_ERR: TOD error reported by
 *  responder. Not always provided.
 *  See IEEE P802.11-REVmc/D7.0, 9.6.8.33 for more information.
 * @QCA_WLAN_VENDOR_ATTR_FTM_MEAS_TOA_ERR: TOA error reported by
 *  responder. Not always provided.
 *  See IEEE P802.11-REVmc/D7.0, 9.6.8.33 for more information.
 * @QCA_WLAN_VENDOR_ATTR_FTM_MEAS_INITIATOR_TOD_ERR: TOD error measured by
 *  initiator. Not always provided.
 *  See IEEE P802.11-REVmc/D7.0, 9.6.8.33 for more information.
 * @QCA_WLAN_VENDOR_ATTR_FTM_MEAS_INITIATOR_TOA_ERR: TOA error measured by
 *  initiator. Not always provided.
 *  See IEEE P802.11-REVmc/D7.0, 9.6.8.33 for more information.
 * @QCA_WLAN_VENDOR_ATTR_FTM_MEAS_PAD: Dummy attribute for padding.
 */
enum qca_wlan_vendor_attr_ftm_meas {
	QCA_WLAN_VENDOR_ATTR_FTM_MEAS_INVALID,
	QCA_WLAN_VENDOR_ATTR_FTM_MEAS_T1,
	QCA_WLAN_VENDOR_ATTR_FTM_MEAS_T2,
	QCA_WLAN_VENDOR_ATTR_FTM_MEAS_T3,
	QCA_WLAN_VENDOR_ATTR_FTM_MEAS_T4,
	QCA_WLAN_VENDOR_ATTR_FTM_MEAS_RSSI,
	QCA_WLAN_VENDOR_ATTR_FTM_MEAS_TOD_ERR,
	QCA_WLAN_VENDOR_ATTR_FTM_MEAS_TOA_ERR,
	QCA_WLAN_VENDOR_ATTR_FTM_MEAS_INITIATOR_TOD_ERR,
	QCA_WLAN_VENDOR_ATTR_FTM_MEAS_INITIATOR_TOA_ERR,
	QCA_WLAN_VENDOR_ATTR_FTM_MEAS_PAD,
	/* keep last */
	QCA_WLAN_VENDOR_ATTR_FTM_MEAS_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_FTM_MEAS_MAX =
		QCA_WLAN_VENDOR_ATTR_FTM_MEAS_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_aoa_type: AOA measurement type
 *
 * @QCA_WLAN_VENDOR_ATTR_AOA_TYPE_TOP_CIR_PHASE: Phase of the strongest
 *  CIR (channel impulse response) path for each antenna.
 * @QCA_WLAN_VENDOR_ATTR_AOA_TYPE_TOP_CIR_PHASE_AMP: Phase and amplitude
 *  of the strongest CIR path for each antenna.
 */
enum qca_wlan_vendor_attr_aoa_type {
	QCA_WLAN_VENDOR_ATTR_AOA_TYPE_TOP_CIR_PHASE,
	QCA_WLAN_VENDOR_ATTR_AOA_TYPE_TOP_CIR_PHASE_AMP,
	QCA_WLAN_VENDOR_ATTR_AOA_TYPE_MAX,
};

/* vendor event indices, used from both cfg80211.c and ftm.c */
enum qca_nl80211_vendor_events_index {
	QCA_NL80211_VENDOR_EVENT_FTM_MEAS_RESULT_INDEX,
	QCA_NL80211_VENDOR_EVENT_FTM_SESSION_DONE_INDEX,
	QCA_NL80211_VENDOR_EVENT_AOA_MEAS_RESULT_INDEX,
};

/* measurement parameters. Specified for each peer as part
 * of measurement request, or provided with measurement
 * results for peer in case peer overridden parameters
 */
struct wil_ftm_meas_params {
	u8 meas_per_burst;
	u8 num_of_bursts_exp;
	u8 burst_duration;
	u16 burst_period;
};

/* measurement request for a single peer */
struct wil_ftm_meas_peer_info {
	u8 mac_addr[ETH_ALEN];
	u8 channel;
	u32 flags; /* enum qca_wlan_vendor_attr_ftm_peer_meas_flags */
	struct wil_ftm_meas_params params;
	u8 secure_token_id;
};

/* session request, passed to wil_ftm_cfg80211_start_session */
struct wil_ftm_session_request {
	u64 session_cookie;
	u32 n_peers;
	/* keep last, variable size according to n_peers */
	struct wil_ftm_meas_peer_info peers[0];
};

/* single measurement for a peer */
struct wil_ftm_peer_meas {
	u64 t1, t2, t3, t4;
};

/* measurement results for a single peer */
struct wil_ftm_peer_meas_res {
	u8 mac_addr[ETH_ALEN];
	u32 flags; /* enum qca_wlan_vendor_attr_ftm_peer_result_flags */
	u8 status; /* enum qca_wlan_vendor_attr_ftm_peer_result_status */
	u8 value_seconds;
	bool has_params; /* true if params is valid */
	struct wil_ftm_meas_params params; /* peer overridden params */
	u8 *lci;
	u8 lci_length;
	u8 *lcr;
	u8 lcr_length;
	u32 n_meas;
	/* keep last, variable size according to n_meas */
	struct wil_ftm_peer_meas meas[0];
};

/* standalone AOA measurement request */
struct wil_aoa_meas_request {
	u8 mac_addr[ETH_ALEN];
	u32 type;
};

/* AOA measurement result */
struct wil_aoa_meas_result {
	u8 mac_addr[ETH_ALEN];
	u32 type;
	u32 antenna_array_mask;
	u32 status;
	u32 length;
	/* keep last, variable size according to length */
	u8 data[0];
};

/* private data related to FTM. Part of the wil6210_priv structure */
struct wil_ftm_priv {
	struct mutex lock; /* protects the FTM data */
	u8 session_started;
	u64 session_cookie;
	struct wil_ftm_peer_meas_res *ftm_res;
	u8 has_ftm_res;
	u32 max_ftm_meas;

	/* standalone AOA measurement */
	u8 aoa_started;
	u8 aoa_peer_mac_addr[ETH_ALEN];
	u32 aoa_type;
	struct timer_list aoa_timer;
	struct work_struct aoa_timeout_work;
};

int wil_ftm_get_capabilities(struct wiphy *wiphy, struct wireless_dev *wdev,
			     const void *data, int data_len);
int wil_ftm_start_session(struct wiphy *wiphy, struct wireless_dev *wdev,
			  const void *data, int data_len);
int wil_ftm_abort_session(struct wiphy *wiphy, struct wireless_dev *wdev,
			  const void *data, int data_len);
int wil_ftm_configure_responder(struct wiphy *wiphy, struct wireless_dev *wdev,
				const void *data, int data_len);
int wil_aoa_start_measurement(struct wiphy *wiphy, struct wireless_dev *wdev,
			      const void *data, int data_len);
int wil_aoa_abort_measurement(struct wiphy *wiphy, struct wireless_dev *wdev,
			      const void *data, int data_len);

#endif /* __WIL6210_FTM_H__ */
