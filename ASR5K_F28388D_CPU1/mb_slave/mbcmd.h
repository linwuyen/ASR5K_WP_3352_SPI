/*
 *  File Name: mbcmd.h
 *
 *  Created on: 1/29/2026
 *  Author: POWER2-54FD92
 */

#ifndef MBCMD_H_
#define MBCMD_H_

typedef enum {
	_g0_MODE = 0,
    _END_OF_MODE
} ID_MODE;


enum {
	_muCOMmonHEAder = 0,                    // #0  T_U16                
	_muCOMmonLENgth = 1,                    // #1  T_U16                
	_muMAChineINFOrmationOFFSet = 2,        // #2  T_U16                
	_muUSERPARameterOFFSet = 3,             // #3  T_U16                
	_muADVAncePARameterOFFSet = 4,          // #4  T_U16                
	_muCOMmonCHEcksum = 5,                  // #5  T_U16                
	_muMAChineLENgth = 6,                   // #6  T_U16                
	_muCOUntryCODe = 7,                     // #7  T_U16                
	_muVENderid = 8,                        // #8  T_U16                
	_muPROductid = 9,                       // #9  T_U16                // 1: PBL
	_muPARtid = 10,                         // #10  T_U16               // Type: 00:Invalid, 01:Test, 10:Valid, 11:Normal.
	_muMODuleid = 11,                       // #11  T_U16               // Module: 0:Invalid, 3:System, 7:Buck, 10:DAB, 13:PFC. 
	_muSERialNUMber0 = 12,                  // #12  T_U32               
	_muSERialNUMber1 = 13,                  // #13  T_U32               
	_muVERsion = 14,                        // #14  T_U16               
	_muBUIldDATe = 15,                      // #15  T_U16               
	_muUSERPARameterLENgth = 16,            // #16  T_U16               
	_muMAInCONtrolSTAtus = 17,              // #17  T_U16               
	_muMAInERROrSTAtus = 18,                // #18  T_U16               
	_muMAInERROrMARk = 19,                  // #19  T_U16               
	_muMAInERROrRESult = 20,                // #20  T_U16               
	_muREServedid = 21,                     // #21  T_U16               
	_muADVAncePARameterLENgth = 22,         // #22  T_U16               
	_muADVAncePASsword = 23,                // #23  T_U16               
	_muHEArtbeatC280 = 24,                  // #24  T_U32               
	_muHEArtbeatC281 = 25,                  // #25  T_U32               
	_muHEArtbeatCLA0 = 26,                  // #26  T_U32               
	_muHEArtbeatCLA1 = 27,                  // #27  T_U32               
    _size_of_mbslave_id
};

typedef union { 
    uint16_t u16MbusData[_size_of_mbslave_id];
    struct { 
		uint16_t u16COMmonHEAder;
		uint16_t u16COMmonLENgth;
		uint16_t u16MAChineINFOrmationOFFSet;
		uint16_t u16USERPARameterOFFSet;
		uint16_t u16ADVAncePARameterOFFSet;
		uint16_t u16COMmonCHEcksum;
		uint16_t u16MAChineLENgth;
		uint16_t u16COUntryCODe;
		uint16_t u16VENderid;
		uint16_t u16PROductid;
		uint16_t u16PARtid;
		uint16_t u16MODuleid;
		uint32_t u32SERialNUMber;
		uint16_t u16VERsion;
		uint16_t u16BUIldDATe;
		uint16_t u16USERPARameterLENgth;
		uint16_t u16MAInCONtrolSTAtus;
		uint16_t u16MAInERROrSTAtus;
		uint16_t u16MAInERROrMARk;
		uint16_t u16MAInERROrRESult;
		uint16_t u16REServedid;
		uint16_t u16ADVAncePARameterLENgth;
		uint16_t u16ADVAncePASsword;
		uint32_t u32HEArtbeatC28;
		uint32_t u32HEArtbeatCLA;
    }; 
} REG_MBUSDATA;
extern REG_MBUSDATA regMbusData;
extern int chkValidAddress(uint16_t addr);
extern uint16_t getModbusData(uint16_t addr);
extern uint16_t setModbusData(uint16_t addr, uint16_t data);




#endif /* MBCMD_H_ */

