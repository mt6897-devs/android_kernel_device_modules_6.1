# Makefile for the focaltech touchscreen drivers.

ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(DEVICE_MODULES_PATH)/drivers/gpu/drm/mediatek/mediatek_v2/

obj-$(CONFIG_TOUCHSCREEN_FOCALTECH_FT3683G) += focaltech_touch.o
focaltech_touch-y := focaltech_core.o \
		focaltech_ex_fun.o \
		focaltech_ex_mode.o \
		focaltech_gesture.o \
		focaltech_esdcheck.o \
		focaltech_point_report_check.o \
		focaltech_proximity.o

focaltech_touch-y += focaltech_spi_v2.o
focaltech_touch-y += focaltech_debug.o
focaltech_touch-y += focaltech_hp.o
focaltech_touch-y += focaltech_flash.o
focaltech_touch-y += focaltech_flash/focaltech_upgrade_ft3683g.o
