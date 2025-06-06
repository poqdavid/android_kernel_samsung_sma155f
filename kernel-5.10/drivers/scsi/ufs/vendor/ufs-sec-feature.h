// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Specific feature
 *
 * Copyright (C) 2021 Samsung Electronics Co., Ltd.
 *
 * Authors:
 *	Storage Driver <storage.sec@samsung.com>
 */

#ifndef __UFS_SEC_FEATURE_H__
#define __UFS_SEC_FEATURE_H__

#include "ufshcd.h"
#include "ufshci.h"

#if IS_ENABLED(CONFIG_SEC_ABC)
#include <linux/sti/abc_common.h>
#endif

/* unique number */
#define UFS_UN_20_DIGITS 20
#define UFS_UN_MAX_DIGITS (UFS_UN_20_DIGITS + 1) //current max digit + 1

#define SERIAL_NUM_SIZE 7

#define UFS_S_INFO_SIZE 512
#define UFS_SHI_SIZE 256

#define HEALTH_DESC_PARAM_SEC_FLT 0x22
#define HEALTH_DESC_PARAM_KIC_FLT 0x11
#define HEALTH_DESC_PARAM_MIC_FLT 0x5
#define HEALTH_DESC_PARAM_SKH_FLT 0x5

#define UFS_WB_ISSUED_SIZE_CNT_MAX 4

/* the user LU for mtk solution is set as 2 */
#define UFS_USER_LUN 2
#define UFS_UPIU_MAX_GENERAL_LUN 8

struct ufs_vendor_dev_info {
	struct ufs_hba *hba;
	char unique_number[UFS_UN_MAX_DIGITS];
	u8 lt;
	u16 flt;
	u8 eli;
	unsigned int ic;
	char s_info[UFS_S_INFO_SIZE];
	char shi[UFS_SHI_SIZE];
};

struct ufs_sec_cmd_info {
	u8 opcode;
	u32 lba;
	int transfer_len;
	u8 lun;
};

enum ufs_sec_wb_state {
	WB_OFF = 0,
	WB_ON_READY,
	WB_OFF_READY,
	WB_ON,
	NR_WB_STATE
};

enum ufs_sec_log_str_t {
	UFS_SEC_CMD_SEND,
	UFS_SEC_CMD_COMP,
	UFS_SEC_QUERY_SEND,
	UFS_SEC_QUERY_COMP,
	UFS_SEC_NOP_SEND,
	UFS_SEC_NOP_COMP,
	UFS_SEC_TM_SEND,
	UFS_SEC_TM_COMP,
	UFS_SEC_TM_ERR,
	UFS_SEC_UIC_SEND,
	UFS_SEC_UIC_COMP,
};

static const char * const ufs_sec_log_str[] = {
	[UFS_SEC_CMD_SEND] = "scsi_send",
	[UFS_SEC_CMD_COMP] = "scsi_cmpl",
	[UFS_SEC_QUERY_SEND] = "query_send",
	[UFS_SEC_QUERY_COMP] = "query_cmpl",
	[UFS_SEC_NOP_SEND] = "nop_send",
	[UFS_SEC_NOP_COMP] = "nop_cmpl",
	[UFS_SEC_TM_SEND] = "tm_send",
	[UFS_SEC_TM_COMP] = "tm_cmpl",
	[UFS_SEC_TM_ERR] = "tm_err",
	[UFS_SEC_UIC_SEND] = "uic_send",
	[UFS_SEC_UIC_COMP] = "uic_cmpl",
};

struct ufs_sec_cmd_log_entry {
	const char *str;	/* ufs_sec_log_str */
	u8 lun;
	u8 cmd_id;
	u32 lba;
	int transfer_len;
	u8 idn;		/* used only for query idn */
	unsigned long outstanding_reqs;
	unsigned int tag;
	u64 tstamp;
};

#define UFS_SEC_CMD_LOGGING_MAX 200
#define UFS_SEC_CMD_LOGNODE_MAX 64
struct ufs_sec_cmd_log_info {
	struct ufs_sec_cmd_log_entry *entries;
	int pos;
};

struct ufs_sec_feature_info {
	struct ufs_sec_cmd_log_info *ufs_cmd_log;

	u32 last_ucmd;
	bool ucmd_complete;

	enum query_opcode last_qcmd;
	enum dev_cmd_type qcmd_type;
	bool qcmd_complete;
};

struct ufs_sec_wb_info {
	struct ufs_hba *hba;
	bool support;		/* feature support and enabled */
	bool setup_done;		/* setup is done or not */
	bool wb_off;			/* WB off or not */
	atomic_t wb_off_cnt;		/* WB off count */

	enum ufs_sec_wb_state state;	/* current state */
	unsigned long state_ts;		/* current state timestamp */

	int up_threshold_block;		/* threshold for WB on : block(4KB) count */
	int up_threshold_rqs;		/*               WB on : request count */
	int down_threshold_block;	/* threshold for WB off : block count */
	int down_threshold_rqs;		/*               WB off : request count */

	int disable_threshold_lt;	/* LT threshold that WB is not allowed */

	int on_delay;			/* WB on delay for WB_ON_READY -> WB_ON */
	int off_delay;			/* WB off delay for WB_OFF_READY -> WB_OFF */

	/* below values will be used when (wb_off == true) */
	int lp_up_threshold_block;	/* threshold for WB on : block(4KB) count */
	int lp_up_threshold_rqs;	/*               WB on : request count */
	int lp_down_threshold_block;	/* threshold for WB off : block count */
	int lp_down_threshold_rqs;	/*               WB off : request count */
	int lp_on_delay;		/* on_delay multiplier when (wb_off == true) */
	int lp_off_delay;		/* off_delay multiplier when (wb_off == true) */

	int current_block;		/* current block counts in WB_ON state */
	int current_rqs;		/* request counts in WB_ON */

	int curr_issued_min_block;		/* min. issued block count */
	int curr_issued_max_block;		/* max. issued block count */
	unsigned int curr_issued_block;	/* amount issued block count during current WB_ON session */
	/* volume count of amount issued block per WB_ON session */
	unsigned int issued_size_cnt[UFS_WB_ISSUED_SIZE_CNT_MAX];

	unsigned int total_issued_mb;	/* amount issued Write Size(MB) in all WB_ON */

	struct workqueue_struct *wb_workq;
	struct work_struct on_work;
	struct work_struct off_work;
};

void ufs_set_sec_features(struct ufs_hba *hba);
void ufs_sec_init_logging(struct device *dev);
void ufs_sec_get_health_desc(struct ufs_hba *hba);
void ufs_sec_register_vendor_hooks(void);
void ufs_sec_remove_features(struct ufs_hba *hba);
void ufs_sec_feature_config(struct ufs_hba *hba);
void ufs_sec_op_err_check(struct ufs_hba *hba, enum ufs_event_type evt, void *data);
void ufs_sec_check_hwrst_cnt(void);
void ufs_sec_print_err_info(struct ufs_hba *hba);
void ufs_sec_wb_force_off(struct ufs_hba *hba);
#endif
