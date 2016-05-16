/*
 *
 * Copyright (C) 2016 XiaoMi, Inc.
 *
 * @file	mstar_drv_hotknot_queue.c
 *
 * @brief   This file defines the queue structure for hotknot
 *
 *
 */

#include "mstar_drv_hotknot_queue.h"
#ifdef CONFIG_ENABLE_HOTKNOT

static u8 *_gQueue;
static u16  _gQFront;
static u16  _gQRear;
static u16  _gQSize = HOTKNOT_QUEUE_SIZE;


#define RESULT_OK					 0
#define RESULT_OVERPUSH			  -1
#define RESULT_OVERPOP			   -2


void _DebugShowQueueArray(u8 *pBuf, u16 nLen)
{
	int i;

	for (i = 0; i < nLen; i++) {
		DBG("%02X ", pBuf[i]);

		if (i%16 == 15) {
			DBG("\n");
		}
	}
	DBG("\n");
}


void CreateQueue()
{
	DBG("*** %s() ***\n", __func__);

	_gQueue = (u8 *)kmalloc(sizeof(u8)*_gQSize, GFP_KERNEL);
	_gQFront = _gQRear = 0;
}


void ClearQueue()
{
	DBG("*** %s() ***\n", __func__);

	_gQFront = _gQRear = 0;
}


int PushQueue(u8 *pBuf, u16 nLength)
{
	u16 nPushLen = nLength;

	DBG("*** %s() ***\n", __func__);


	DBG("*** Before PushQueue: _gQFront = %d, _gQRear = %d ***\n", _gQFront, _gQRear);

	if (_gQRear >= _gQFront) {
		if (nPushLen > 0 && _gQFront == 0 && _gQRear == _gQSize-1) {
			DBG("*** PushQueue: RESULT_OVERPUSH ***\n");
			return RESULT_OVERPUSH;
		}

		if (nPushLen > _gQSize-1 - (_gQRear - _gQFront)) {
			DBG("*** PushQueue: RESULT_OVERPUSH ***\n");
			return RESULT_OVERPUSH;
		}

		if (_gQRear+nPushLen <= _gQSize-1) {
			memcpy(&_gQueue[_gQRear+1], pBuf, nPushLen);
			_gQRear = _gQRear + nPushLen;
		} else {
			u16 nQTmp = (_gQSize-1) - _gQRear;
			memcpy(&_gQueue[_gQRear+1], pBuf, nQTmp);
			memcpy(_gQueue, &pBuf[nQTmp], nPushLen - nQTmp);
			_gQRear = nPushLen - nQTmp - 1;
		}
	} else {
		if (nPushLen > 0 && _gQFront == _gQRear+1) {
			DBG("*** PushQueue: RESULT_OVERPUSH ***\n");
			return RESULT_OVERPUSH;
		}

		if (nPushLen > (_gQFront - _gQRear) - 1) {
			DBG("*** PushQueue: RESULT_OVERPUSH ***\n");
			return RESULT_OVERPUSH;
		}

		memcpy(&_gQueue[_gQRear+1], pBuf, nPushLen);
		_gQRear = _gQRear + nPushLen;
	}




	DBG("*** After PushQueue: _gQFront = %d, _gQRear = %d ***\n", _gQFront, _gQRear);
	return nPushLen;
}


int PopQueue(u8 *pBuf, u16 nLength)
{
	u16 nPopLen = nLength;

	DBG("*** %s() ***\n", __func__);

	DBG("*** Before PopQueue: _gQFront = %d, _gQRear = %d ***\n", _gQFront, _gQRear);

	if (_gQRear >= _gQFront) {
		if (nPopLen > 0 && _gQRear == _gQFront) {
			DBG("*** PushQueue: RESULT_OVERPOP ***\n");
			return RESULT_OVERPOP;
		}

		if (nPopLen > _gQRear - _gQFront) {
			DBG("*** PushQueue: RESULT_OVERPOP ***\n");
			return RESULT_OVERPOP;
		}

		memcpy(pBuf, &_gQueue[_gQFront+1], nPopLen);
		_gQFront = _gQFront + nPopLen;
	} else {
		if (nPopLen > _gQSize - (_gQFront - _gQRear)) {
			DBG("*** PushQueue: RESULT_OVERPOP ***\n");
			return RESULT_OVERPOP;
		}

		if (_gQFront + nPopLen <= _gQSize-1) {
			memcpy(pBuf, &_gQueue[_gQFront+1], nPopLen);
			_gQFront = _gQFront + nPopLen;
		} else {
			u16 nQTmp = (_gQSize-1) - _gQFront;
			memcpy(pBuf, &_gQueue[_gQFront+1], nQTmp);
			memcpy(&pBuf[nQTmp], _gQueue, nPopLen - nQTmp);
			_gQFront = nPopLen - nQTmp - 1;
		}
	}

	DBG("*** After PopQueue: _gQFront = %d, _gQRear = %d ***\n", _gQFront, _gQRear);
	return nPopLen;
}


int ShowQueue(u8 *pBuf, u16 nLength)
{
	u16 nShowLen = nLength;

	DBG("*** %s() ***\n", __func__);

	if (_gQRear >= _gQFront) {
		if (nShowLen > 0 && _gQRear == _gQFront)
			return RESULT_OVERPOP;

		if (nShowLen > _gQRear - _gQFront)
			return RESULT_OVERPOP;

		memcpy(pBuf, &_gQueue[_gQFront+1], nShowLen);

	} else {
		if (nShowLen > _gQSize - (_gQFront - _gQRear))
			return RESULT_OVERPOP;

		if (_gQFront + nShowLen <= _gQSize-1)
			memcpy(pBuf, &_gQueue[_gQFront+1], nShowLen);
		else {
			u16 nQTmp = (_gQSize-1) - _gQFront;
			memcpy(pBuf, &_gQueue[_gQFront+1], nQTmp);
			memcpy(&pBuf[nQTmp], _gQueue, nShowLen - nQTmp);

		}
	}

	return nShowLen;
}


void ShowAllQueue(u8 *pBuf, u16 *pFront, u16 *pRear)
{
	DBG("*** %s() ***\n", __func__);

	memcpy(pBuf, _gQueue, HOTKNOT_QUEUE_SIZE);
	*pFront = _gQFront;
	*pRear = _gQRear;
}


void DeleteQueue()
{
	DBG("*** %s() ***\n", __func__);

	_gQFront = _gQRear = 0;
	kfree(_gQueue);
}

#endif
