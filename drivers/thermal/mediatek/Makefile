ccflags-y += \
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat \
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/sspm/v2 \
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mcupm/include \
	-I$(DEVICE_MODULES_PATH)/drivers/gpu/mediatek/gpufreq \
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/connectivity/power_throttling \
	-I$(DEVICE_MODULES_PATH)/drivers/leds
CFLAGS_md_cooling.o := -I$(srctree)/drivers/thermal
CFLAGS_soc_temp_lvts.o := -I$(srctree)/drivers/thermal
CFLAGS_thermal_trace.o := -I$(DEVICE_MODULES_PATH)/drivers/thermal/mediatek

obj-$(CONFIG_MTK_SOC_THERMAL)	+= soc_temp.o
obj-$(CONFIG_MTK_SOC_THERMAL_LVTS)	+= soc_temp_lvts.o
obj-$(CONFIG_MTK_PMIC_THERMAL) += pmic_temp.o
obj-$(CONFIG_MTK_MD_THERMAL)	+= md_cooling_all.o
md_cooling_all-y 		+= md_cooling.o
md_cooling_all-$(CONFIG_DEBUG_FS) += md_cooling_debugfs.o

obj-$(CONFIG_MTK_CHARGER_THERMAL) += charger_cooling.o
obj-$(CONFIG_MTK_BACKLIGHT_THERMAL) += backlight_cooling.o

obj-$(CONFIG_MTK_THERMAL_TRACE) += thermal_trace.o

obj-$(CONFIG_MTK_THERMAL_IPI)	+= thermal_ipi.o

obj-$(CONFIG_MTK_THERMAL_INTERFACE)	+= thermal_interface.o

obj-$(CONFIG_MTK_BOARD_THERMAL)	+= board_temp.o

obj-$(CONFIG_MTK_VTSKIN_THERMAL)	+= vtskin_temp.o

obj-$(CONFIG_MTK_WIFI_THERMAL) += wifi_cooling.o

obj-$(CONFIG_MI_THERMAL_INTERFACE)              += ../xiaomi/
