# SPDX-License-Identifier: GPL-2.0

# scp vow include path
subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/sound/soc/mediatek/common
subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/sound/soc/mediatek/vow

ifneq ($(CONFIG_MTK_TINYSYS_SCP_SUPPORT),)
subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/scp/include
endif
subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/vow/ver02

obj-$(CONFIG_MTK_VOW_SUPPORT) += mtk-scp-vow.o
