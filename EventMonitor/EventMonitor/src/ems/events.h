#pragma once

#define _NUM_EVENTS 45
#define CFG_INVALID_EVENT_CODE 0


typedef enum EPEBS_EVENTS {
	_PE_INVALID_EVENT	= 0x0,
	//
	_PE_MEM_INST_RET_LOADS			= 0x010B,
	_PE_MEM_INST_RET_STORES			= 0x020B,
	_PE_MEM_INST_RET_LAT_ABOV_THS	= 0x100b,
	//
	_PE_MEM_STORE_RET_SS_LAST_LVL_DTL	= 0x010C,
	_PE_MEM_STORE_RET_DROPPED_EVTS		= 0x020C,
	//
	_PE_MEM_UNC_EVT_RET_LLC_DATA_MISS	= 0x010F,
	_PE_MEM_UNC_EVT_RET_OTH_CR_L2_HIT	= 0x020F,
	_PE_MEM_UNC_EVT_RET_OTH_CR_L2_HITM	= 0x040F,
	_PE_MEM_UNC_EVT_RET_RMT_CCHE_HIT	= 0x080F,
	_PE_MEM_UNC_EVT_RET_RMT_CCHE_HITM	= 0x100F,
	_PE_MEM_UNC_EVT_RET_LOCAL_DRAM		= 0x200F,
	_PE_MEM_UNC_EVT_RET_NON_LOCAL_DRAM	= 0x400F,
	_PE_MEM_UNC_EVT_RET_IO				= 0x800F,
	//
	_PE_INST_RET_ALL	= 0x01C0,
	_PE_INST_RET_FP		= 0x02C0,
	_PE_INST_RET_MMX	= 0x04C0,
	//
	_PE_OTHER_ASSISTS_PAGE_AD_ASSISTS	= 0x01C1,
	//
	_PE_UOPS_RET_ALL_EXECUTED	= 0x01C2,
	_PE_UOPS_RET_RET_SLOTS		= 0x02C2,
	_PE_UOPS_RET_MACRO_FUSED	= 0x04C2,
	//
	_PE_BR_INST_RET_CONDITIONAL		= 0x01C4,
	_PE_BR_INST_RET_NEAR_CALL		= 0x02C4,
	_PE_BR_INST_RET_ALL_BRANCHES	= 0x04C4,
	//
	_PE_BR_MISP_RETIRED			= 0x01C5,
	_PE_BR_MISP_NEAR_CALL		= 0x02C5,
	_PE_BR_MISP_ALL_BRANCHES	= 0x04C5,
	//
	_PE_SSEX_UOPS_RET_PACKED_SINGLE		= 0x01C7,
	_PE_SSEX_UOPS_RET_SCALAR_SINGLE		= 0x02C7,
	_PE_SSEX_UOPS_RET_PACKED_DOUBLE		= 0x04C7,
	_PE_SSEX_UOPS_RET_SCALAR_DOUBLE		= 0x08C7,
	_PE_SSEX_UOPS_RET_VECTOR_INTEGER	= 0x10C7,
	//
	_PE_ITBL_MISS_RET_ITBL_MISS	= 0x20C8,
	//
	_PE_MEM_LOAD_RET_LD_HIT_L1				= 0x01CB,
	_PE_MEM_LOAD_RET_LD_HIT_L2_MLC			= 0x02CB,
	_PE_MEM_LOAD_RET_LD_HIT_L3_LLC			= 0x04CB,
	_PE_MEM_LOAD_RET_LD_HIT_OTHER_PM_PKG_L2	= 0x08CB,
	_PE_MEM_LOAD_RET_LLC_MISS				= 0x10CB,
	_PE_MEM_LOAD_RET_DROPPED_EVENTS			= 0x20CB,
	_PE_MEM_LOAD_RET_LD_HT_LFB_BUT_MS_IN_L1	= 0x40CB,
	_PE_MEM_LOAD_RET_LD_MS_IN_LAST_LVL_DTBL	= 0x80CB,
	//
	_PE_BR_CND_MISPREDICT_BIMODAL	= 0x10EB,
	//
	_PE_FP_ASSISTS_ALL		= 0x01F7,
	_PE_FP_ASSISTS_OUTPUT	= 0x02F7,
	_PE_FP_ASSISTS_INPUT	= 0x04F7
}TEPEBS_EVENTS, *PTEPEBS_EVENTS;