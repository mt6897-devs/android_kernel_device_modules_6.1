# SPDX-License-Identifier: GPL-2.0
subdir-ccflags-y += -Wformat

# common include path
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/adsp/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/adsp/include
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/base/power/include
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/slbc
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/clk/mediatek

# target object
obj-${CONFIG_MTK_AUDIODSP_SUPPORT} += adsp.o
obj-${CONFIG_MTK_ADSP_V1} += v1/
obj-${CONFIG_MTK_ADSP_V2} += v2/

adsp-objs += adsp_core.o
adsp-objs += adsp_fs.o
adsp-objs += adsp_semaphore.o
adsp-objs += adsp_logger.o
adsp-objs += adsp_feature_table.o
adsp-objs += adsp_timesync.o
adsp-objs += adsp_awake.o
adsp-objs += adsp_reserved_mem.o
adsp-objs += adsp_platform.o
adsp-objs += adsp_mbox.o
adsp-objs += adsp_platform_driver.o
adsp-objs += adsp_excep.o
adsp-objs += adsp_qos.o
adsp-objs += adsp_bus_monitor.o
