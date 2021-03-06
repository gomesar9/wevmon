#include "EMS.h"
#include "lockers.h"
#include "../config.h"
#include "../bfr/buffer.h"
#include "../clt/collector.h"

/*
 * void __writemsr(
 *	 unsigned long Register,
 *	 unsigned __int64 Value
 * );
 */

 /*
 Variables
 */


UINT32 get_cfg_collect_max(UINT32 core) {
	return CCFG[core].Collector_max;
}


LARGE_INTEGER get_cfg_collector_millis(UINT32 core) {
	return CCFG[core].Collector_millis;
}


VOID initialize_em() {
	// TODO: Enable only setted in config.h by "ENABLE_CORE[0-3]" definition
	for (UINT32 i=0; i < MAX_CORE_QTD; i++) {
		CCFG[i].Th_main = NULL;
		CCFG[i].Th_collector = NULL;
		CCFG[i].Core = i;
		CCFG[i].Interrupts = 0;
		CCFG[i].Flags = 0;
		CCFG[i].Collector_max = 0;
		CCFG[i].Threshold = 0;
		CCFG[i].Event_map.Event = _PE_INVALID_EVENT;
		CCFG[i].Event_map.Code = CFG_INVALID_EVENT_CODE;
		CCFG[i].Collector_millis.QuadPart = 10;
		KeInitializeSpinLock(&(CCFG[i].Lock_interrupts));
		KeInitializeSpinLock(&(CCFG[i].Lock_flags));
	}
}


BOOLEAN get_interrupts(_Out_ PUINT32 collect, UINT32 core) {
	KIRQL old;

	// Check if core is NOT active
	if (!checkFlag(F_EM_PEBS_ACTIVE, core)) {
		return FALSE;
	}
	ExAcquireSpinLock(&(CCFG[core].Lock_interrupts), &old);
	RtlCopyMemory(collect, &(CCFG[core].Interrupts), sizeof(INT32));
    // *collect = CCFG[core].Interrupts;
	CCFG[core].Interrupts = 0;
	KeReleaseSpinLock(&(CCFG[core].Lock_interrupts), old);
	return TRUE;
}

// TODO: Kick off?
// Information status
typedef enum _INFOS{
	I_START,
	I_ENABLE,
	I_CGF_SET,
	I_STOP
}E_INFO, *PE_INFO;

void to_buffer(int info){
	char msg_bfr[BFR_SIZE];

	switch (info) {
	case I_START:
		sprintf(msg_bfr, "STARTED");
		break;
	case I_ENABLE:
		sprintf(msg_bfr, "ENABLED");
		break;
	case I_CGF_SET:
		sprintf(msg_bfr, "CONFIG SET");
		break;
	case I_STOP:
		sprintf(msg_bfr, "STOPPED");
		break;
	default:
		break;
	}
	
	bfr_set(msg_bfr);
}


NTSTATUS stop_pebs(_In_ INT core) {
	NTSTATUS st;
	st = PsCreateSystemThread(&(CCFG[core].Th_main), GENERIC_ALL, NULL, NULL, NULL, StopperThread, (VOID*)core);
	
	clearFlag(F_EM_PEBS_ACTIVE, core);

	// uninstall hook after stop Thread if there are no PEBS active.
	// TODO: Check all cores
	if (!checkFlag(F_EM_PEBS_ACTIVE, core) && checkFlag(F_EM_HOOK_INSTALLED, core)) {
		unhook_handler(core);
	}
	
	CCFG[core].Interrupts = 0;

	return STATUS_SUCCESS;
}


NTSTATUS em_configure(_In_ PTEM_CMD emCmd, _Out_ PTEM_CCFG cfg) {
	NTSTATUS st = STATUS_SUCCESS;
	NTSTATUS CHANGE_ME = STATUS_FAIL_CHECK;
	CHAR dbgMsg[128];
	UINT32 core = cfg->Core;

#if EMS_DEBUG > 0 //--------------------
	sprintf(dbgMsg, "[EXC] EM_CFG detected on core %d.", core);
	debug(dbgMsg);
#endif //^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

	if (checkFlag(F_EM_PEBS_ACTIVE, cfg->Core)) {
		debug("[!EXC] CANNOT change Event while PEBS active.");
		return CHANGE_ME;
	}

	switch (emCmd->Subtype) {
	// #########################################################################
	// 0) Configure Event
	case EM_CFG_EVT:
#if EMS_DEBUG > 0 //--------------------
		sprintf(dbgMsg, "[EXC] EM_CFG_EVT: %u.", emCmd->Opt1);
		debug(dbgMsg);
#endif //^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

		cfg->Event_map.Code = emCmd->Opt1;
		if (!getPEBSEvt( &(cfg->Event_map) )) {
			return CHANGE_ME;
		}
		sprintf(dbgMsg, "Flags: %u.", CCFG[core].Flags);
		debug(dbgMsg);
		setFlag(F_EM_EVENT, core);
		sprintf(dbgMsg, "Flags: %u.", CCFG[core].Flags);
		debug(dbgMsg);
		break;

	// #########################################################################
	// 1) Configure Collect MAX
    case EM_CFG_COLLECT_MAX:
#if EMS_DEBUG > 0 //--------------------
		sprintf(dbgMsg, "[EXC] EM_CFG_COLLECT_MAX: %u.", emCmd->Opt1);
		debug(dbgMsg);
#endif //^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

		CCFG[core].Collector_max = emCmd->Opt1;

		setFlag(F_EM_COLLECT_MAX, core);
		break;

	// #########################################################################
	// 2) Configure Threshold
	case EM_CFG_THRESHOLD:
#if EMS_DEBUG > 0 //--------------------
		sprintf(dbgMsg, "[EXC] EM_CFG_THRESHOLD: %u.", emCmd->Opt1);
		debug(dbgMsg);
#endif //^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

		// Only 48bits setted
		cfg->Threshold = (ULLONG_MAX - emCmd->Opt1) & 0x0000FFFFFFFFFFFF;

		setFlag(F_EM_THRESHOLD, core);
		break;

	// #########################################################################
	// 3) Configure Collect Millis
	case EM_CFG_COLLECT_MILLI:
#if EMS_DEBUG > 0 //--------------------
		sprintf(dbgMsg, "[EXC] EM_CFG_COLLECT_MILLI: %u.", emCmd->Opt1);
		debug(dbgMsg);
#endif //^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

		cfg->Collector_millis.QuadPart = emCmd->Opt1;

		setFlag(F_EM_COLLECT_MILLI, core);
		break;
	default:
		sprintf(dbgMsg, "[EXC] %d is not a valid SUBTYPE.", emCmd->Subtype);
		debug(dbgMsg);

		return CHANGE_ME;
	}

	//to_buffer(I_CGF_SET);
	return st;
}


NTSTATUS em_start(_In_ PTEM_CCFG cfg) {
	NTSTATUS st = STATUS_SUCCESS;
	NTSTATUS CHANGE_ME = STATUS_FAIL_CHECK;
	UINT32 core = cfg->Core;
	CHAR dbgMsg[128];

	if (!checkFlag(F_EM_CONFIGURED, core)) {
		debug("[!EXC] NOT CONFIGURED");
		return CHANGE_ME;
	}

	// Install hook before start Thread
	if (!checkFlag(F_EM_HOOK_INSTALLED, core)) {
		hook_handler(core);
	}

#if EMS_DEBUG >= 0 //-------------------
	sprintf(dbgMsg, "[EXC] Activating PEBS in %dth core.", core);
	debug(dbgMsg);
#endif //^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
	if (checkFlag(F_EM_PEBS_ACTIVE, core)) {
		sprintf(dbgMsg, "[!EXC] PEBS Core %d Already ACTIVE!", core);
		debug(dbgMsg);
		return CHANGE_ME;
	}

	st = PsCreateSystemThread(
		&(cfg->Th_main),
		GENERIC_ALL,
		NULL, NULL, NULL,
		StarterThread,
		(VOID*)cfg->Core
	);

	if (NT_SUCCESS(st)) {
		cfg->Interrupts = 0;
		setFlag(F_EM_PEBS_ACTIVE, core);
		st = PsCreateSystemThread(
			&(cfg->Th_collector),
			GENERIC_ALL,
			NULL, NULL, NULL,
			start_collector,
			&(cfg->Core)
		);

		if (!NT_SUCCESS(st)) {
			debug("[!EXC] Failed to create collector thread!");
			//unhook_handler();
		}
	}

	//to_buffer(I_START);
	return st;
}


NTSTATUS em_stop(_In_ UINT32 core) {
	NTSTATUS st = STATUS_SUCCESS;
	NTSTATUS CHANGE_ME = STATUS_FAIL_CHECK;  // TODO: Return better code error
	CHAR dbgMsg[128];

	if (!checkFlag(F_EM_PEBS_ACTIVE, core)) {
		return CHANGE_ME;
	}

	sprintf(dbgMsg, "[EXC] Deactivating PEBS in core %d.", core);
	debug(dbgMsg);
	stop_pebs(core);

	return st;
}


NTSTATUS execute(_In_ PTEM_CMD emCmd) {
	NTSTATUS st = STATUS_FAIL_CHECK;;

	for (UINT32 core = 0; core < CORE_QTD; core++)
	{
		if (!(emCmd->Cores & (1u << core))) {
			continue;
		}

		if (emCmd->Type == EM_CMD_CFG) {
			st = em_configure(emCmd, &(CCFG[core]));

		} else if (emCmd->Type == EM_CMD_START) {
			st = em_start(&(CCFG[core]));

		} else if (emCmd->Type == EM_CMD_STOP) {
			st = em_stop(core);
		}

		if (st != STATUS_SUCCESS) {
			debug("Error while executing commands.");
			return st;
		}
	}
	return st;
}

/********************************************
* Interrupt Routine
*/
VOID PMI(__in struct _KINTERRUPT *Interrupt, __in PVOID ServiceContext) {
	UNREFERENCED_PARAMETER(Interrupt);
	UNREFERENCED_PARAMETER(ServiceContext);
	ULONG core;
	/* identify current core */
	core = KeGetCurrentProcessorNumber();
	//core = 0;  // TODO: Change to dynamic

	LARGE_INTEGER pa;
	UINT32* APIC;
	//LONG intPID;
	// Disable PEBS
	__writemsr(MSR_IA32_PEBS_ENABLE, DISABLE_PEBS);
	__writemsr(MSR_IA32_GLOBAL_CTRL, DISABLE_PEBS);

	//intPID = (LONG)PsGetCurrentProcessId();

	// --+-- Clear APIC flag --+--
	pa.QuadPart = PERF_COUNTER_APIC;
	/*
	NTKERNELAPI PVOID MmMapIoSpace(
		PHYSICAL_ADDRESS	PhysicalAddress,
		SIZE_T				NumberOfBytes,
		MEMORY_CACHING_TYPE	CacheType
	);
	*/
	APIC = (UINT32*)MmMapIoSpace(pa, sizeof(UINT32), MmNonCached);
	*APIC = ORIGINAL_APIC_VALUE;
	MmUnmapIoSpace(APIC, sizeof(UINT32));

	// TODO: PROCESS
#ifdef DEBUG_DEV //--------------------------------------------------------------------
	char _msg[128];
	UINT64 pmc0, perf_ctr0, perf_global_status;
	PTDS_BASE tmp;

	sprintf(_msg, "----- [PMI] %d -----", CCFG[core].Interrupts);
	debug(_msg);

	sprintf(_msg, "[PMI]IA32_PERF_GLOBAL_ST: %lld", DS_BASE->PEBS_BUFFER_BASE->IA32_PERF_GLOBAL_ST);
	debug(_msg);
#else //^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
	//char msg_bfr[BFR_SIZE];
	//sprintf(msg_bfr, "[PMI]INTERRUPT: %d\0", INTERRUPTS);
	//bfr_set(msg_bfr);
	//debug(msg_bfr);
#endif

	// Increment interrupt counter
	KIRQL old;
	ExAcquireSpinLock(&(CCFG[core].Lock_interrupts), &old);
	CCFG[core].Interrupts++;
	KeReleaseSpinLock(&(CCFG[core].Lock_interrupts), old);

	//bfr_tick();			// IO data

	// Re-enable PEBS
	if (CCFG[core].Interrupts < EM_SAFE_INTERRUPT_LIMIT) {
		//DS_BASE->PEBS_BUFFER_BASE = PEBS_BUFFER;	// Theoretically not necessary

#ifdef DEBUG_DEV //--------------------------------------------------------------------
		// --+-- DEBUG --+--
		// Misbehavior: Always pointing to PEBS_BUFFER_BASE
		sprintf(_msg, "[PMI]PEBS->PEBS_INDEX: %p", DS_BASE->PEBS_INDEX);
		debug(_msg);

		// MSR_IA32_PERFCTR0 and PMC0 should be 0 or 1 at this point
		// Supposing that DS_BASE->PEBS_CTR0_RST is working correctly, I am not sure if 
		// the reset value is putted in PMC0 before interruption..
		perf_ctr0 = __readmsr(MSR_IA32_PERFCTR0);
		sprintf(_msg, "[PMI]MSR_IA32_PERFCTR0: %llx", perf_ctr0);
		debug(_msg);

		pmc0 = __readpmc(IA32_PMC0);
		sprintf(_msg, "[PMI]PMC0: %llx", pmc0);
		debug(_msg);
#endif //^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
		/*
		* Index reset was disabled aiming see the complete PEBS sequence,
		* that is starting, threshold, and reach ABS_MAX.
		* Considering PEBS Buffer with 5 records
		* Registers:	[0 1 2 3 4]
		* Events:		 S - I T - M
		* S: Start, I: Threshold interruption, T: Threshold Pointer, M: Abs Max
		* 
		* I should be invoked with PEBS_Ovf flag setted after Register[2] is writted, because 
		*   threshold is pointing to register[3]
		* M should stop PEBS without invoke interruption. (Points to the first byte after PEBS Buffer)
		*/
		//DS_BASE->PEBS_INDEX = DS_BASE->PEBS_BUFFER_BASE;	// Reset index

		// --+-- Enable PEBS --+--
		__writemsr(MSR_IA32_PERFCTR0, CCFG[core].Threshold);
		__writemsr(MSR_IA32_PEBS_ENABLE, ENABLE_PEBS);
		__writemsr(MSR_IA32_GLOBAL_CTRL, ENABLE_PEBS);
#ifdef DEBUG_DEV //--------------------------------------------------------------------
	} else {
		// Last Interrupt
		// Ensures that read value comes from MSR_DS_AREA (discarding pointer mistakes)
		tmp = (PTDS_BASE)__readmsr(MSR_DS_AREA);

		sprintf(_msg, "[PMI]tmp->PEBS_INDEX: %p", tmp->PEBS_INDEX);
		debug(_msg);
#endif //^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
	}

#ifdef DEBUG_DEV //--------------------------------------------------------------------
	// --+-- DEBUG --+--
	perf_ctr0 = __readmsr(MSR_IA32_PERFCTR0);
	sprintf(_msg, "[PMI]MSR_IA32_PERFCTR0: %llx", perf_ctr0);
	debug(_msg);

	perf_global_status = __readmsr(MSR_IA32_GLOAL_STATUS);
	//sprintf(_msg, "[PMI]PERF_GLOBAL_STATUS: %llu", perf_global_status);
	//debug(_msg);

	if (perf_global_status & GLOBAL_STATUS_PEBS_OVF) {
		debug("[PMI]PEBS_Ovf setted. Cleaning..");
		// PEBS assists microcode already clear this after write on PEBS record
		//__writemsr(MSR_IA32_GLOBAL_OVF_CTRL, GLOBAL_STATUS_PEBS_OVF);	// Clear bit PEBS_Ovf
	}

	if (perf_global_status & GLOBAL_STATUS_OVF_PC0) {
		debug("[PMI]OVF_PC0 setted. Cleaning..");
		//__writemsr(MSR_IA32_GLOBAL_OVF_CTRL, GLOBAL_STATUS_OVF_PC0);	// Clear bit OVF_PC0 (Programmable counter 0)
	}
#endif //^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

}

/*
	Install PMI
*/
VOID hook_handler(UINT32 core) {
	NTSTATUS st;
	/*
	NTSTATUS HalSetSystemInformation(
		_In_	HAL_QUERY_INFORMATION_CLASS	InformationClass,
		_In_	ULONG						BufferSize,
		_Out_	PVOID						Buffer
	);
	*/
	CCFG[core].Perfmon_hook.Function = PMI;
	
	st = HalSetSystemInformation(
		HalProfileSourceInterruptHandler,
		sizeof(PVOID*),
		&(CCFG[core].Perfmon_hook.Pointer)
	);

	//char msg[126];
	//sprintf(msg, "HOOK_HANDLER: ST:%X. F:%p", st, perfmon_hook.Pointer);
	//debug(msg);
	setFlag(F_EM_HOOK_INSTALLED, core);
}

/*
	Uninstall PMI
*/
VOID unhook_handler(UINT32 core) {
	NTSTATUS st;
	st = HalSetSystemInformation(
		HalProfileSourceInterruptHandler,
		sizeof(PVOID*),
		&(CCFG[core].Restore_hook)
	);
	clearFlag(F_EM_HOOK_INSTALLED, core);
}


/********************************************
 * THREAD FUNCTIONS
*/
VOID thread_attach_to_core(uintptr_t id) {
	// High level magic, do not touch, do not look, just skip. What are you waiting for?
	KAFFINITY mask;
#pragma warning( disable : 4305 )
#pragma warning( disable : 4334 )
	mask = 1 << id;
	KeSetSystemAffinityThread(mask);
}

VOID fill_ds_with_buffer(PTEM_CCFG cfg) {

	cfg->DS_base->PEBS_BUFFER_BASE = cfg->PEBS_buffer;
	cfg->DS_base->PEBS_INDEX = cfg->PEBS_buffer;
#ifdef DEBUG_DEV
	cfg->DS_base->PEBS_MAXIMUM = cfg->PEBS_buffer + MAX_INTERRUPTS;
	cfg->DS_base->PEBS_THRESHOLD = cfg->PEBS_buffer + 2; // TODO: adjust
#else
	cfg->DS_base->PEBS_MAXIMUM = cfg->PEBS_buffer + 1;
	cfg->DS_base->PEBS_THRESHOLD = cfg->PEBS_buffer;	// Inactive, I think..
#endif
	cfg->DS_base->PEBS_CTR0_RST = cfg->Threshold;  // TODO: Make multi-core friendly
#ifdef NEHALEM_NEW_FIELDS
	cfg->DS_base->PEBS_CTR1_RST = cfg->Threshold;
	cfg->DS_base->PEBS_CTR2_RST = cfg->Threshold;
	cfg->DS_base->PEBS_CTR3_RST = cfg->Threshold;
#endif

#ifdef DEBUG_DEV
	char _msg[128];
	sprintf(_msg, "SIZEOF TPEBS_BUFFER: %Id", sizeof(TPEBS_BUFFER));
	debug(_msg);
	sprintf(_msg, "DS_BASE  : %p", cfg->DS_base->PEBS_BUFFER_BASE);
	debug(_msg);
	sprintf(_msg, "PEBS_TRHD: %p", cfg->DS_base->PEBS_THRESHOLD);
	debug(_msg);
	sprintf(_msg, "PEBS_CTR0_RST: %llx", cfg->DS_base->PEBS_CTR0_RST);
	debug(_msg);
#endif
}

VOID StarterThread(_In_ PVOID StartContext) {
	LARGE_INTEGER pa;
	UINT32 *APIC;
	uintptr_t core;
	// Get core information (number)
	core = (uintptr_t)StartContext;

	// Attach thread to core
	thread_attach_to_core(core);

	// Allocate structs and buffers
	ULONG tagds = TAG_PREFIX_DS_BASE | (ULONG) core;
	ULONG tagbuffer = TAG_PREFIX_PEBS_BUFFER | (ULONG) core;

	CCFG[core].DS_base = (PTDS_BASE)ExAllocatePoolWithTag(NonPagedPool, sizeof(TDS_BASE), tagds);
	CCFG[core].PEBS_buffer = (PTPEBS_BUFFER)ExAllocatePoolWithTag(NonPagedPool, CCFG[core].Collector_max * sizeof(TPEBS_BUFFER), tagbuffer);

	fill_ds_with_buffer(&(CCFG[core]));
	
	pa.QuadPart = PERF_COUNTER_APIC;
	/*
	NTKERNELAPI PVOID MmMapIoSpace(
	PHYSICAL_ADDRESS	PhysicalAddress,
	SIZE_T				NumberOfBytes,
	MEMORY_CACHING_TYPE	CacheType
	);
	*/
	APIC = (UINT32*)MmMapIoSpace(pa, sizeof(UINT32), MmNonCached);
	*APIC = ORIGINAL_APIC_VALUE;
	MmUnmapIoSpace(APIC, sizeof(UINT32));

	__writemsr(MSR_DS_AREA, (UINT_PTR)CCFG[core].DS_base);

	// --+-- Enable mechanism --+--
	// Disable PEBS to set up
	__writemsr(MSR_IA32_GLOBAL_CTRL, DISABLE_PEBS);
	
	// Set threshold (counter) and events
	__writemsr(MSR_IA32_PERFCTR0, CCFG[core].Threshold);

#ifdef DEBUG_DEV
	// IA32_PMC0 = 0
	CHAR _msg[64];
	UINT64 pmc0 = __readpmc(0);
	sprintf(_msg, "PMC0: %llx", pmc0);
	debug(_msg);
	pmc0 = __readmsr(MSR_IA32_PERFCTR0);
	sprintf(_msg, "MSR_IA32_PERFCTR0: %llx", pmc0);
	debug(_msg);
#endif
	//__writemsr(MSR_IA32_EVNTSEL0, PEBS_EVENT | EVTSEL_EN | EVTSEL_USR | EVTSEL_INT);
	__writemsr(MSR_IA32_EVNTSEL0, CCFG[core].Event_map.Event | EVTSEL_EN | EVTSEL_USR | EVTSEL_INT);

	// Enable PEBS
	__writemsr(MSR_IA32_PEBS_ENABLE, ENABLE_PEBS);
	__writemsr(MSR_IA32_GLOBAL_CTRL, ENABLE_PEBS);
#if EMS_DEBUG >= 0 //------------------------------------------------------------------
	CHAR dbgMsg[128];
	sprintf(dbgMsg, "[StarterThread] PEBS setted on core %lld", core);
	debug(dbgMsg);
#endif //------------------------------------------------------------------------------
}


VOID StopperThread(_In_ PVOID StartContext) {
	//int core;
	uintptr_t core;
	core = (uintptr_t)StartContext;
	
	// Attach to a given core
	thread_attach_to_core(core);

	// --+-- Disable PEBS for counter 0 --+--
	__writemsr(MSR_IA32_PEBS_ENABLE, DISABLE_PEBS);
	__writemsr(MSR_IA32_GLOBAL_CTRL, DISABLE_PEBS);

	// Free allocated memory
	ULONG tagds = TAG_PREFIX_DS_BASE | (ULONG) core;
	ULONG tagbuffer = TAG_PREFIX_PEBS_BUFFER | (ULONG) core;
	
	ExFreePoolWithTag(CCFG[core].PEBS_buffer, tagds);  // Buffer
	ExFreePoolWithTag(CCFG[core].DS_base, tagbuffer);  // Struct

#if EMS_DEBUG >= 0 //------------------------------------------------------------------
	CHAR dbgMsg[128];
	sprintf(dbgMsg, "[StopperThread] Thread stopped on core %lld", core);
	debug(dbgMsg);
#endif //------------------------------------------------------------------------------
}

BOOLEAN getPEBSEvt(PTPEBS_EVT_MAP evtMap) {
	CONST TEPEBS_EVENTS all_events[] = {
		_PE_INVALID_EVENT,					// 0
		//
		_PE_MEM_INST_RET_LOADS,				// 1
		_PE_MEM_INST_RET_STORES,
		_PE_MEM_INST_RET_LAT_ABOV_THS,
		//
		_PE_MEM_STORE_RET_SS_LAST_LVL_DTL,	// 4
		_PE_MEM_STORE_RET_DROPPED_EVTS,
		//
		_PE_MEM_UNC_EVT_RET_LLC_DATA_MISS,	// 6
		_PE_MEM_UNC_EVT_RET_OTH_CR_L2_HIT,
		_PE_MEM_UNC_EVT_RET_OTH_CR_L2_HITM,
		_PE_MEM_UNC_EVT_RET_RMT_CCHE_HIT,	// 9
		_PE_MEM_UNC_EVT_RET_RMT_CCHE_HITM,
		_PE_MEM_UNC_EVT_RET_LOCAL_DRAM,
		_PE_MEM_UNC_EVT_RET_NON_LOCAL_DRAM,	// 12
		_PE_MEM_UNC_EVT_RET_IO,
		//
		_PE_INST_RET_ALL,			// 14
		_PE_INST_RET_FP,
		_PE_INST_RET_MMX,
		//
		_PE_OTHER_ASSISTS_PAGE_AD_ASSISTS,	// 17
		//
		_PE_UOPS_RET_ALL_EXECUTED,	// 18
		_PE_UOPS_RET_RET_SLOTS,
		_PE_UOPS_RET_MACRO_FUSED,
		//
		_PE_BR_INST_RET_CONDITIONAL,	// 21
		_PE_BR_INST_RET_NEAR_CALL,		// 22
		_PE_BR_INST_RET_ALL_BRANCHES,	// 23
		//
		_PE_BR_MISP_RETIRED,			// 24
		_PE_BR_MISP_NEAR_CALL,
		_PE_BR_MISP_ALL_BRANCHES,
		//
		_PE_SSEX_UOPS_RET_PACKED_SINGLE,	// 27
		_PE_SSEX_UOPS_RET_SCALAR_SINGLE,
		_PE_SSEX_UOPS_RET_PACKED_DOUBLE,
		_PE_SSEX_UOPS_RET_SCALAR_DOUBLE,	// 30
		_PE_SSEX_UOPS_RET_VECTOR_INTEGER,
		//
		_PE_ITBL_MISS_RET_ITBL_MISS,		// 31
		//
		_PE_MEM_LOAD_RET_LD_HIT_L1,					// 32
		_PE_MEM_LOAD_RET_LD_HIT_L2_MLC,
		_PE_MEM_LOAD_RET_LD_HIT_L3_LLC,
		_PE_MEM_LOAD_RET_LD_HIT_OTHER_PM_PKG_L2,	// 35
		_PE_MEM_LOAD_RET_LLC_MISS,
		_PE_MEM_LOAD_RET_DROPPED_EVENTS,
		_PE_MEM_LOAD_RET_LD_HT_LFB_BUT_MS_IN_L1,	// 38
		_PE_MEM_LOAD_RET_LD_MS_IN_LAST_LVL_DTBL,
		//
		_PE_BR_CND_MISPREDICT_BIMODAL,		// 40
		//
		_PE_FP_ASSISTS_ALL,					// 41
		_PE_FP_ASSISTS_OUTPUT,
		_PE_FP_ASSISTS_INPUT
	};

	if (evtMap->Code < _NUM_EVENTS) {
		evtMap->Event = all_events[evtMap->Code];
		return TRUE;
	} else {
		evtMap->Code = CFG_INVALID_EVENT_CODE;
		evtMap->Event = _PE_INVALID_EVENT;
		return FALSE;
	}
}
