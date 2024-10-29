/*
 * Copyright (C) 2021 XiaoMi, Inc.
 *               2022 The LineageOS Project
 *
 * SPDX-License-Identifier: GPL-2.0
 */

#include <linux/module.h>

#include "hwid.h"

#define HW_MAJOR_VERSION_SHIFT      16
#define HW_MINOR_VERSION_SHIFT      0
#define HW_COUNTRY_VERSION_SHIFT    20
#define HW_BUILD_VERSION_SHIFT      16
#define HW_MAJOR_VERSION_MASK       0xFFFF0000
#define HW_MINOR_VERSION_MASK       0x0000FFFF
#define HW_COUNTRY_VERSION_MASK     0xFFF00000
#define HW_BUILD_VERSION_MASK       0x000F0000

static uint project;
module_param(project, uint, 0444);

static char project_name[16];
module_param_string(project_name, project_name, sizeof(project_name), 0444);

static uint hwid_value;
module_param(hwid_value, uint, 0444);

const char *product_name_get(void)
{
	return project_name;
}
EXPORT_SYMBOL(product_name_get);

uint32_t get_hw_version_platform(void)
{
	return project;
}
EXPORT_SYMBOL(get_hw_version_platform);

uint32_t get_hw_id_value(void)
{
	return hwid_value;
}
EXPORT_SYMBOL(get_hw_id_value);

uint32_t get_hw_country_version(void)
{
	return (hwid_value & HW_COUNTRY_VERSION_MASK) >> HW_COUNTRY_VERSION_SHIFT;
}
EXPORT_SYMBOL(get_hw_country_version);

uint32_t get_hw_version_major(void)
{
	return (hwid_value & HW_MAJOR_VERSION_MASK) >> HW_MAJOR_VERSION_SHIFT;
}
EXPORT_SYMBOL(get_hw_version_major);

uint32_t get_hw_version_minor(void)
{
	return (hwid_value & HW_MINOR_VERSION_MASK) >> HW_MINOR_VERSION_SHIFT;
}
EXPORT_SYMBOL(get_hw_version_minor);

uint32_t get_hw_version_build(void)
{
	return (hwid_value & HW_BUILD_VERSION_MASK) >> HW_BUILD_VERSION_SHIFT;
}
EXPORT_SYMBOL(get_hw_version_build);

MODULE_AUTHOR("weixiaotian1@xiaomi.com");
MODULE_LICENSE("GPL v2");
