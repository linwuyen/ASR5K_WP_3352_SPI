/*
 *  File Name: linkVariables.c
 *
 *  Created on: 5/13/2026
 *  Author: POWER-532A86
 */

#include "ModbusCommon.h"
#include "ModbusSlave.h"


void initRegN(void *v){ 

	SCI_MODBUS *p = (SCI_MODBUS *) v;
	p->pReg->u16COMmonHEAder = 2423; 
	p->pReg->u16COMmonLENgth = 4; 
	p->pReg->u16MAChineINFOrmationOFFSet = 6; 
	p->pReg->u16USERPARameterOFFSet = 16; 
	p->pReg->u16ADVAncePARameterOFFSet = 21; 
	p->pReg->u16COMmonCHEcksum = 2470; 
	p->pReg->u16MAChineLENgth = 10; 
	p->pReg->u16COUntryCODe = 886; 
	p->pReg->u16VENderid = 2423; 
	p->pReg->u16PROductid = 1; 
	p->pReg->u16PARtid = 3; 
	p->pReg->u16MODuleid = 0; 
	p->pReg->u32SERialNUMber = 0; 
	p->pReg->u16VERsion = FW_VERSION; 
	p->pReg->u16BUIldDATe = FW_BUILDDATE; 
	p->pReg->u16USERPARameterLENgth = 5; 
	p->pReg->u16ADVAncePARameterLENgth = 6; 
	p->pReg->u16ADVAncePASsword = 0; 
}

void readRegN(void *v){ 

	SCI_MODBUS *p = (SCI_MODBUS *) v;
	p->pReg->u16MAInCONtrolSTAtus = sDrv.fgStatus; 
	p->pReg->u16MAInERROrSTAtus = sDrv.fgError; 
	p->pReg->u16MAInERROrMARk = sDrv.fgErrorMark.all; 
	p->pReg->u16MAInERROrRESult = sDrv.fgErrorResult.all; 
	p->pReg->u32HEArtbeatC28 = sDrv.u32HeartBeat; 
	p->pReg->u32HEArtbeatCLA = sCLA.u32HeartBeat; 
	p->pReg->u32FLAshid = g_hwTest.stFlash.u32ID; 
	p->pReg->u32FLAshstatus = g_hwTest.stFlash.u32Status; 
	p->pReg->u32FLAshdata = g_hwTest.stFlash.u32Data; 
	p->pReg->u16FLAshtrigger = g_hwTest.stFlash.u16Trigger; 
	p->pReg->u16FLAshrxcount = g_hwTest.stFlash.u16RxCount; 
	p->pReg->u16SDRamctrl = g_hwTest.stSdram.u16Ctrl; 
	p->pReg->u32SDRamstresspass = g_hwTest.stSdram.u32StressPass; 
	p->pReg->u32SDRamstresserr = g_hwTest.stSdram.u32StressErr; 
	p->pReg->s32SDRamfailaddr = g_hwTest.stSdram.u32FailAddr; 
	p->pReg->u32SDRamfailread = g_hwTest.stSdram.u32FailRead; 
	p->pReg->u16SPIbloop = g_hwTest.u16SPIBLOOP; 
}

void writeReg(void *v){ 

	SCI_MODBUS *p = (SCI_MODBUS *) v;
	switch(p->info.rwfrom) {
	case _muMAInCONtrolSTAtus :   sDrv.fgStatus = p->pReg->u16MAInCONtrolSTAtus; break;
	case _muMAInERROrSTAtus :   sDrv.fgError = p->pReg->u16MAInERROrSTAtus; break;
	case _muMAInERROrMARk :   sDrv.fgErrorMark.all = p->pReg->u16MAInERROrMARk; break;
	case _muMAInERROrRESult :   sDrv.fgErrorResult.all = p->pReg->u16MAInERROrRESult; break;
	case _muHEArtbeatC280 :   sDrv.u32HeartBeat = p->pReg->u32HEArtbeatC28; break;
	case _muHEArtbeatCLA0 :   sCLA.u32HeartBeat = p->pReg->u32HEArtbeatCLA; break;
	case _muFLAshid0 :   g_hwTest.stFlash.u32ID = p->pReg->u32FLAshid; break;
	case _muFLAshstatus0 :   g_hwTest.stFlash.u32Status = p->pReg->u32FLAshstatus; break;
	case _muFLAshdata0 :   g_hwTest.stFlash.u32Data = p->pReg->u32FLAshdata; break;
	case _muFLAshtrigger :   g_hwTest.stFlash.u16Trigger = p->pReg->u16FLAshtrigger; break;
	case _muFLAshrxcount :   g_hwTest.stFlash.u16RxCount = p->pReg->u16FLAshrxcount; break;
	case _muSDRamctrl :   g_hwTest.stSdram.u16Ctrl = p->pReg->u16SDRamctrl; break;
	case _muSDRamstresspass0 :   g_hwTest.stSdram.u32StressPass = p->pReg->u32SDRamstresspass; break;
	case _muSDRamstresserr0 :   g_hwTest.stSdram.u32StressErr = p->pReg->u32SDRamstresserr; break;
	case _muSDRamfailaddr0 :   g_hwTest.stSdram.u32FailAddr = p->pReg->s32SDRamfailaddr; break;
	case _muSDRamfailread0 :   g_hwTest.stSdram.u32FailRead = p->pReg->u32SDRamfailread; break;
	case _muSPIbloop :   g_hwTest.u16SPIBLOOP = p->pReg->u16SPIbloop; break;
	default:
	    break;
	}
}

void writeRegN(void *v){ 

	SCI_MODBUS *p = (SCI_MODBUS *) v;
	sDrv.fgStatus = p->pReg->u16MAInCONtrolSTAtus; 
	sDrv.fgError = p->pReg->u16MAInERROrSTAtus; 
	sDrv.fgErrorMark.all = p->pReg->u16MAInERROrMARk; 
	sDrv.fgErrorResult.all = p->pReg->u16MAInERROrRESult; 
	sDrv.u32HeartBeat = p->pReg->u32HEArtbeatC28; 
	sCLA.u32HeartBeat = p->pReg->u32HEArtbeatCLA; 
	g_hwTest.stFlash.u32ID = p->pReg->u32FLAshid; 
	g_hwTest.stFlash.u32Status = p->pReg->u32FLAshstatus; 
	g_hwTest.stFlash.u32Data = p->pReg->u32FLAshdata; 
	g_hwTest.stFlash.u16Trigger = p->pReg->u16FLAshtrigger; 
	g_hwTest.stFlash.u16RxCount = p->pReg->u16FLAshrxcount; 
	g_hwTest.stSdram.u16Ctrl = p->pReg->u16SDRamctrl; 
	g_hwTest.stSdram.u32StressPass = p->pReg->u32SDRamstresspass; 
	g_hwTest.stSdram.u32StressErr = p->pReg->u32SDRamstresserr; 
	g_hwTest.stSdram.u32FailAddr = p->pReg->s32SDRamfailaddr; 
	g_hwTest.stSdram.u32FailRead = p->pReg->u32SDRamfailread; 
	g_hwTest.u16SPIBLOOP = p->pReg->u16SPIbloop; 
}

