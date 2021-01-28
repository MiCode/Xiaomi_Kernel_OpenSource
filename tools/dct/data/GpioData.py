#! /usr/bin/python
# -*- coding: utf-8 -*-

# SPDX-License-Identifier: GPL-2.0
#
# Copyright (c) 2019 MediaTek Inc.
#

class GpioData:
    _count = 0
    _modNum = 8
    _specMap = {}
    _freqMap = {}
    _mapList = []
    _modeMap = {}
    _smtMap = {}
    _map_table = {}

    def __init__(self):
        self.__defMode = 0
        self.__eintMode = False
        self.__modeVec = ['0', '0', '0', '0', '0', '0', '0', '0']
        self.__inPullEn = True
        self.__inPullSelHigh = False
        self.__defDirInt = 0
        self.__defDir = 'IN'
        self.__inEn = True
        self.__outEn = False
        self.__outHigh = False
        self.__varNames = []
        self.__smtNum = -1
        self.__smtEn = False
        self.__iesEn = True
        self.__drvCur = ""


    def get_defMode(self):
        return self.__defMode

    def set_defMode(self, mode):
        self.__defMode = mode

    def get_eintMode(self):
        return self.__eintMode

    def set_eintMode(self, flag):
        self.__eintMode = flag

    def get_modeVec(self):
        return self.__modeVec

    def set_modeVec(self, vec):
        self.__modeVec = vec

    def get_inPullEn(self):
        return self.__inPullEn

    def set_inpullEn(self, flag):
        self.__inPullEn = flag

    def get_inPullSelHigh(self):
        return self.__inPullSelHigh

    def set_inpullSelHigh(self, flag):
        self.__inPullSelHigh = flag

    def get_defDir(self):
        return self.__defDir

    def set_defDir(self, dir):
        self.__defDir = dir

    def get_inEn(self):
        return self.__inEn

    def set_inEn(self, flag):
        self.__inEn = flag

    def get_outEn(self):
        return self.__outEn

    def set_outEn(self, flag):
        self.__outEn = flag

    def get_outHigh(self):
        return self.__outHigh

    def set_outHigh(self, outHigh):
        self.__outHigh = outHigh

    def get_varNames(self):
        return self.__varNames

    def set_varNames(self, names):
        self.__varNames = names

    def set_smtEn(self, flag):
        self.__smtEn = flag

    def get_smtEn(self):
        return self.__smtEn

    def get_iesEn(self):
        return self.__iesEn

    def set_iesEn(self, flag):
        self.__iesEn = flag

    def set_drvCur(self, val):
        self.__drvCur = val

    def get_drvCur(self):
        return self.__drvCur

    def set_smtNum(self, num):
        self.__smtNum = num

    def get_smtNum(self):
        return self.__smtNum

    def ge_defDirInt(self):
        if self.__defDir == 'IN':
            return 0
        else:
            return 1

    @staticmethod
    def set_eint_map_table(map_table):
        GpioData._map_table = map_table

    @staticmethod
    def get_modeName(key, idx):
        if key in GpioData._modeMap.keys():
            value = GpioData._modeMap[key]
            return value[idx]

