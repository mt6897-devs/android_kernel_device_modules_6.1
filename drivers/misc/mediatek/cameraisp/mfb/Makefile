#
# Copyright (C) 2015 MediaTek Inc.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#

ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mmp/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/cmdq/v3/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/staging/android/ion \
             -I$(DEVICE_MODULES_PATH)/drivers/staging/android/ion/mtk
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/smi/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/cmdq/mailbox/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/

obj-$(CONFIG_MTK_CAMERA_ISP_MFB_SUPPORT) += camera_mfb.o
camera_mfb-objs += isp_6s/engine_request.o
camera_mfb-objs += isp_6s/camera_mfb.o

ifeq ($(CONFIG_MTK_CAMERA_ISP_PLATFORM),"mt6873")
MTK_MFB_REG_VERSION := 2
endif

ifeq ($(CONFIG_MTK_CAMERA_ISP_PLATFORM),"mt6853")
MTK_MFB_REG_VERSION := 3
endif

subdir-ccflags-y += -DMTK_MFB_REG_VERSION=$(MTK_MFB_REG_VERSION)
