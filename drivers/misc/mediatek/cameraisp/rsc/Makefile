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

ccflags-$(CONFIG_MTK_IOMMU_V2) += -I$(DEVICE_MODULES_PATH)/drivers/iommu
ccflags-$(CONFIG_MTK_IOMMU_V2) += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat/


#obj-y += camera_rsc_dummy.c
obj-$(CONFIG_MTK_CAMERA_ISP_RSC_SUPPORT) += camera_rsc.o
camera_rsc-objs += engine_request.o
camera_rsc-objs += isp_6s/camera_rsc.o
