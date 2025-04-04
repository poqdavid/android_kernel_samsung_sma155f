/******************************************************************************
 *
 * This file is provided under a dual license.  When you use or
 * distribute this software, you may choose to be licensed under
 * version 2 of the GNU General Public License ("GPLv2 License")
 * or BSD License.
 *
 * GPLv2 License
 *
 * Copyright(C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 *
 * BSD LICENSE
 *
 * Copyright(C) 2016 MediaTek Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *****************************************************************************/
/*
 * Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/
 *							include/mgmt/assoc.h#1
 */

/*! \file  assoc.h
 *    \brief This file contains the ASSOC REQ/RESP of
 *	   IEEE 802.11 family for MediaTek 802.11 Wireless LAN Adapters.
 */


#ifndef _ASSOC_H
#define _ASSOC_H

/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 *******************************************************************************
 */

/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
 */

/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */

/*******************************************************************************
 *                         D A T A   T Y P E S
 *******************************************************************************
 */

/*******************************************************************************
 *                            P U B L I C   D A T A
 *******************************************************************************
 */

/*******************************************************************************
 *                           P R I V A T E   D A T A
 *******************************************************************************
 */

/*******************************************************************************
 *                                 M A C R O S
 *******************************************************************************
 */

/*******************************************************************************
 *                  F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */
/*----------------------------------------------------------------------------*/
/* Routines in assoc.c                                                        */
/*----------------------------------------------------------------------------*/
uint32_t assocSendReAssocReqFrame(IN struct ADAPTER
				*prAdapter, IN struct STA_RECORD *prStaRec);

uint32_t assocCheckTxReAssocReqFrame(IN struct ADAPTER
				*prAdapter, IN struct MSDU_INFO *prMsduInfo);

uint32_t assocCheckTxReAssocRespFrame(IN struct ADAPTER
				*prAdapter, IN struct MSDU_INFO *prMsduInfo);

uint32_t
assocCheckRxReAssocRspFrameStatus(IN struct ADAPTER
				*prAdapter, IN struct SW_RFB *prSwRfb,
				OUT uint16_t *pu2StatusCode);

uint32_t assocSendDisAssocFrame(IN struct ADAPTER
				*prAdapter, IN struct STA_RECORD *prStaRec,
				IN uint16_t u2ReasonCode);

uint32_t
assocProcessRxDisassocFrame(IN struct ADAPTER *prAdapter,
				IN struct SW_RFB *prSwRfb,
				IN uint8_t aucBSSID[],
				OUT uint16_t *pu2ReasonCode);

uint32_t assocProcessRxAssocReqFrame(IN struct ADAPTER
				*prAdapter, IN struct SW_RFB *prSwRfb,
				OUT uint16_t *pu2StatusCode);

uint32_t assocSendReAssocRespFrame(IN struct ADAPTER
				*prAdapter, IN struct STA_RECORD *prStaRec);

struct MSDU_INFO *assocComposeReAssocRespFrame(IN struct ADAPTER *prAdapter,
				IN struct STA_RECORD *prStaRec);

uint16_t assocBuildCapabilityInfo(IN struct ADAPTER
				*prAdapter, IN struct STA_RECORD *prStaRec);

uint16_t assoc_get_nonwfa_vend_ie_len(struct ADAPTER *prAdapter,
	uint8_t ucBssIndex);

void assoc_build_nonwfa_vend_ie(struct ADAPTER *prAdapter,
	struct MSDU_INFO *prMsduInfo);

void assocGenerateMDIE(IN struct ADAPTER *prAdapter,
		       IN OUT struct MSDU_INFO *prMsduInfo);

uint32_t assocCalculateConnIELen(struct ADAPTER *prAdapter, uint8_t ucBssIdx,
			     struct STA_RECORD *prStaRec);

void assocGenerateConnIE(struct ADAPTER *prAdapter,
			   struct MSDU_INFO *prMsduInfo);

#if CFG_SUPPORT_ASSURANCE

uint32_t assocCalculateRoamReasonLen(struct ADAPTER *prAdapter,
		uint8_t ucBssIdx, struct STA_RECORD *prStaRec);

void assocGenerateRoamReason(struct ADAPTER *prAdapter,
	struct MSDU_INFO *prMsduInfo);

#endif
#endif /* _ASSOC_H */
