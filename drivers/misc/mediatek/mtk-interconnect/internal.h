/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Interconnect framework internal structs
 *
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __DRIVERS_MTK_INTERCONNECT_INTERNAL_H
#define __DRIVERS_MTK_INTERCONNECT_INTERNAL_H

/**
 * struct icc_req - constraints that are attached to each node
 * @req_node: entry in list of requests for the particular @node
 * @node: the interconnect node to which this constraint applies
 * @dev: reference to the device that sets the constraints
 * @tag: path tag (optional)
 * @avg_bw: an integer describing the average bandwidth in kBps
 * @peak_bw: an integer describing the peak bandwidth in kBps
 */
struct icc_req {
	struct hlist_node req_node;
	struct icc_node *node;
	struct device *dev;
	u32 tag;
	u32 avg_bw;
	u32 peak_bw;
};

/**
 * struct icc_path - interconnect path structure
 * @name: a string name of the path (useful for ftrace)
 * @num_nodes: number of hops (nodes)
 * @reqs: array of the requests applicable to this path of nodes
 */
struct icc_path {
	const char *name;
	u32 old_avg_bw;
	u32 old_peak_bw;
	bool is_write;
	size_t num_nodes;
	struct icc_req reqs[];
};

#endif
