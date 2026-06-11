/*
 *  File Name: linkVariables.c
 *
 *  Created on: 1/29/2026
 *  Author: POWER2-54FD92
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
}

