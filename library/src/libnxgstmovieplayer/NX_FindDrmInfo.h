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
//	Module		: 
//	File		: 
//	Description	: 
//	Author		: 
//	Export		: 
//	History		: 
//
//------------------------------------------------------------------------------

#ifndef __NX_FINDDRMID_H__
#define __NX_FINDDRMID_H__

#include <stdint.h>
#include "NX_GstMovie.h"

#ifdef __cplusplus

int32_t FindPlaneForDisplay(int32_t crtcIdx,
        	                int32_t findRgb,
            	            int32_t layerIdx,
                	        MP_DRM_PLANE_INFO *pDrmPlaneInfo);


#endif	// __cplusplus

#endif	// __NX_FINDDRMID_H__