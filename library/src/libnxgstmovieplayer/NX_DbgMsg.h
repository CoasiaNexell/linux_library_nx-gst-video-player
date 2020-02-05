//------------------------------------------------------------------------------
//
//	Copyright (C) 2015 Nexell Co. All Rights Reserved
//	Nexell Co. Proprietary & Confidential
//
//	NEXELL INFORMS THAT THIS CODE AND INFORMATION IS PROVIDED "AS IS" BASE
//  AND	WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING
//  BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS
//  FOR A PARTICULAR PURPOSE.
//
//	Module		: libnxgstmovieplayer.so
//	File		:
//	Description	:
//	Author		:
//	Export		:
//	History		:
//
//------------------------------------------------------------------------------

#ifndef	__NX_DBGMSG_H
#define	__NX_DBGMSG_H

#include <NX_Log.h>
#include <stdio.h>

#ifdef DTAG
#undef DTAG
#endif
#define DTAG	"[LibGstMov] "

// Debug Flags
#define	DBG_MSG				0
#define	VBS_MSG				1
#define	DBG_FUNCTION		1

#ifdef NDEBUG
#define	NXLOGD
#define	NXLOGV
#endif  // _NDEBUG_

#ifdef  DEBUG
#if DBG_MSG
#else
#define NXGLOGD(...)       NXLOGD(__VA_ARGS__)
#endif  // DBG_MSG
#if VBS_MSG
#define NXGLOGV(...)       NXLOGV(__VA_ARGS__)
#else
#define	NXGLOGV
#endif  // VBS_MSG

#if	DBG_FUNCTION
#define	FUNC_IN()			g_print("%s %s() In\n", DTAG, __func__)
#define	FUNC_OUT()			g_print("%s %s() Out\n", DTAG, __func__)
#else
#define	FUNC_IN()			do{}while(0)
#define	FUNC_OUT()			do{}while(0)
#endif	//	DBG_FUNCTION

#else	// DEBUG

#define	FUNC_IN()			do{}while(0)
#define	FUNC_OUT()			do{}while(0)
#endif	//	DEBUG

#endif	//	__NX_DBGMSG_H
