//
//	Copyright (C) 2015 Nexell Co. All Rights Reserved
//	Nexell Co. Proprietary & Confidential
//
//	NEXELL INFORMS THAT THIS CODE AND INFORMATION IS PROVIDED "AS IS" BASE
//  AND	WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING
//  BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS
//  FOR A PARTICULAR PURPOSE.
//
//	Module		: libnxgstvplayer.so
//	File		:
//	Description	:
//	Author		:
//	Export		:
//	History		:
//
//------------------------------------------------------------------------------

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gst/gst.h>
#include <string.h>

#include <gst/gst.h>
#include "NX_GstMediaInfo.h"
#include "GstDiscover.h"
#include "NX_GstLog.h"
#define LOG_TAG "[NxGstMediaInfo]"

NX_GST_RET  NX_GST_Open_MediaInfo(GST_MEDIA_INFO **ty_handle)
{
	FUNC_IN();

	GST_MEDIA_INFO *handle = NULL;
	handle = (GST_MEDIA_INFO *)g_malloc0(sizeof(GST_MEDIA_INFO));

	if (handle == NULL)
	{
		NXGLOGE("handle == NULL");
		return NX_GST_RET_ERROR;
	}

	memset(handle, 0, sizeof(GST_MEDIA_INFO));

	if (!gst_is_initialized())
    {
		gst_init(NULL, NULL);
	}

	*ty_handle = handle;

	FUNC_OUT();

	return NX_GST_RET_OK;
}

NX_GST_RET  NX_GST_Get_MediaInfo(GST_MEDIA_INFO *ty_media_handle,  const char *uri)
{
    StartDiscover(uri, &ty_media_handle);
}

NX_GST_RET  NX_GST_Close_MediaInfo(GST_MEDIA_INFO *ty_handle)
{
	FUNC_IN();

	g_free(ty_handle);
	
	ty_handle = NULL;

	FUNC_OUT();
}

