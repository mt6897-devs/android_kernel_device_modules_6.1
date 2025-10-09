# SPDX-License-Identifier: GPL-2.0

obj-$(CONFIG_SCSI_UFSHCD_XIAOMI)		+= ufshcd_xiaomi-core.o
ufshcd_xiaomi-core-y				+= ufshcd.o ufs-sysfs.o ufs-mcq.o
ufshcd_xiaomi-core-$(CONFIG_DEBUG_FS)		+= ufs-debugfs.o
ufshcd_xiaomi-core-$(CONFIG_SCSI_UFS_BSG)	+= ufs_bsg.o
ufshcd_xiaomi-core-$(CONFIG_SCSI_UFS_CRYPTO)	+= ufshcd-crypto.o
ufshcd_xiaomi-core-$(CONFIG_SCSI_UFS_HPB)	+= ufshpb.o
ufshcd_xiaomi-core-$(CONFIG_SCSI_UFS_FAULT_INJECTION) += ufs-fault-injection.o
ufshcd_xiaomi-core-$(CONFIG_SCSI_UFS_HWMON)	+= ufs-hwmon.o
