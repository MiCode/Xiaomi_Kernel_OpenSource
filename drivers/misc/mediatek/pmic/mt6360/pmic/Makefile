ifeq ($(CONFIG_MT6360_PMIC),y)
obj-y += mt6360_pmic_regmap.o
ifeq ($(CONFIG_PISSARRO_CHARGER),y)
obj-y += mt6360_pmic_i2c_pissarro.o
else
obj-y += mt6360_pmic_i2c.o
endif
endif
