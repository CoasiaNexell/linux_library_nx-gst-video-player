//------------------------------------------------------------------------------
//
//	Copyright (C) 2017 Nexell Co. All Rights Reserved
//	Nexell Co. Proprietary & Confidential
//
//	NEXELL INFORMS THAT THIS CODE AND INFORMATION IS PROVIDED "AS IS" BASE
//  AND	WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING
//  BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS
//  FOR A PARTICULAR PURPOSE.
//
//	Module		:
//	File		:
//	Description	:
//	Author		:
//	Export		:
//	History		:
//
//------------------------------------------------------------------------------

#ifndef __NX_DAUDIOUTILS_H__
#define __NX_DAUDIOUTILS_H__

#include <stdint.h>

#define NX_LAUNCHER_EXEC		"/usr/bin/NxLauncher"

int64_t NX_GetSystemTick( void );

void	NX_TimeInit( void );
int32_t NX_TimeIsRepeat( int64_t iRepeatTime );

int32_t NX_PostProcessRun( void (*cbFunc)( void* ), int64_t iDelayTime, void *pObj );
void	NX_PostProcessCancel( void );

int32_t NX_GetConsole( char *pDevice );

int32_t NX_RunProcess( const char *pExec );
int32_t NX_KillProcess( const char *pExec );
int32_t NX_KillProcess( int32_t iPid );

int32_t NX_RearCamIsStop( void );
int32_t NX_RearCamSetStop( void );

const char* NX_DAudioUtilGetVersion();

//
//	Callback
//		int32_t cbFunc( void *pObj, int32_t iColumnNum, char **ppColumnValue, char **ppColumnName )
//
int32_t NX_SQLiteGetData( const char *pDatabase, const char *pTable, int32_t (*cbFunc)(void*, int32_t, char**, char**), void *pObj = NULL );

#endif	// __NX_DAUDIOUTILS_H__
