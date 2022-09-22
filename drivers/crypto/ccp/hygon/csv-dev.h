/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * HYGON CSV driver interface
 *
 * Copyright (C) 2024 Hygon Info Technologies Ltd.
 *
 * Author: Liyang Han <hanliyang@hygon.cn>
 */

#ifndef __CCP_HYGON_CSV_DEV_H__
#define __CCP_HYGON_CSV_DEV_H__

#include <linux/psp-sev.h>

int csv_cmd_buffer_len(int cmd);
int csv_ioctl_do_hgsc_import(struct sev_issue_cmd *argp);

#endif	/* __CCP_HYGON_CSV_DEV_H__ */
