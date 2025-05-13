ccflags-$(CONFIG_TOUCHSCREEN_MTK_TOUCH) += -I$(DEVICE_MODULES_PATH)/drivers/input/touchscreen/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/input/touchscreen/xiaomi/
obj-$(CONFIG_TOUCHSCREEN_GOODIX_BRL_9916) += goodix_core.o
goodix_core-y :=\
		goodix_brl_spi.o \
		goodix_ts_core.o \
		goodix_brl_hw.o \
		goodix_cfg_bin.o \
		goodix_ts_utils.o \
		goodix_brl_fwupdate.o \
		goodix_ts_gesture.o \
		goodix_ts_inspect.o \
		goodix_ts_tools.o 
