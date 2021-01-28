#! /usr/bin/python
# -*- coding: utf-8 -*-

# SPDX-License-Identifier: GPL-2.0
#
# Copyright (c) 2019 MediaTek Inc.
#

class EintData:
    _count = 0
    _debounce_enable_list = ['CUST_EINT_DEBOUNCE_DISABLE', 'CUST_EINT_DEBOUNCE_ENABLE']
    _map_table = {}
    _mode_map = {}
    _int_eint = {}
    _builtin_map = {}
    _builtin_eint_count = 0
    def __init__(self):
        self.__varName = ''
        self.__debounce_time = ''
        self.__polarity = ''
        self.__sensitive_level = ''
        self.__debounce_enable = ''

    def set_varName(self, varName):
        self.__varName = varName

    def get_varName(self):
        return self.__varName

    def set_debounceTime(self, time):
        self.__debounce_time = time

    def get_debounceTime(self):
        return self.__debounce_time

    def set_polarity(self, polarity):
        self.__polarity = polarity

    def get_polarity(self):
        return self.__polarity

    def set_sensitiveLevel(self, level):
        self.__sensitive_level = level

    def get_sensitiveLevel(self):
        return self.__sensitive_level

    def set_debounceEnable(self, enable):
        self.__debounce_enable = enable

    def get_debounceEnable(self):
        return self.__debounce_enable

    @staticmethod
    def set_mapTable(map):
        EintData._map_table = map

    @staticmethod
    def get_mapTable():
        return EintData._map_table

    @staticmethod
    def get_internalEint():
        return EintData._int_eint

    @staticmethod
    def get_modeName(gpio_num, mode_idx):
        key = 'gpio%s' %(gpio_num)

        if key in EintData._mode_map.keys():
            list =  EintData._mode_map[key]
            if mode_idx < len(list) and mode_idx >= 0:
                return list[mode_idx]

        return None

    @staticmethod
    def set_modeMap(map):
        for (key, value) in map.items():
            list = []
            for item in value:
                list.append(item[6:len(item)-1])
            map[key] = list

        EintData._mode_map = map

    @staticmethod
    def get_modeMap():
        return EintData._mode_map

    @staticmethod
    def get_gpioNum(num):
        if len(EintData._map_table):
            for (key,value) in EintData._map_table.items():
                if num == value:
                    return key

        return -1

