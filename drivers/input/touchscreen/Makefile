# SPDX-License-Identifier: GPL-2.0
#
# Makefile for the touchscreen drivers.
#
obj-m += tui-common.o
TOUCH_LISTS := $(subst ",,$(CONFIG_TOUCHSCREEN_MTK_TOUCH))
obj-y += $(foreach TOUCH,$(TOUCH_LISTS),$(TOUCH)/)

ifneq ($(wildcard $(srctree)/$(src)/k6985v1_64_alpha),)
obj-$(CONFIG_TOUCHSCREEN_MT6985_ALPHA_SUPPORT) += k6985v1_64_alpha/
endif

obj-$(CONFIG_TOUCHSCREEN_GOODIX_BRL_9916) += goodix_9916r/
obj-$(CONFIG_TOUCHSCREEN_FOCALTECH_FT3683G) += ft3683g/
obj-$(CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE) += xiaomi_touch/
