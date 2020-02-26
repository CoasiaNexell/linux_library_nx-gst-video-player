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
#include "NX_TypeFind.h"
#include "NX_GstLog.h"
#define LOG_TAG "[NxGstMediaInfo]"

NX_GST_RET  NX_GST_OpenMediaInfo(GST_MEDIA_INFO **media_handle)
{
	FUNC_IN();

	GST_MEDIA_INFO *handle = NULL;
	handle = (GST_MEDIA_INFO *)g_malloc0(sizeof(GST_MEDIA_INFO));
	if (handle == NULL)
	{
		return NX_GST_RET_ERROR;
	}

	memset(handle, 0, sizeof(GST_MEDIA_INFO));

	if (!gst_is_initialized())
    {
		gst_init(NULL, NULL);
	}

	*media_handle = handle;

	FUNC_OUT();

	return NX_GST_RET_OK;
}

NX_GST_ERROR  NX_GST_GetMediaInfo(GST_MEDIA_INFO *media_handle, const char *filePath)
{
	FUNC_IN();
	enum NX_GST_ERROR err = NX_GST_ERROR_NONE;

	CONTAINER_TYPE type = CONTAINER_TYPE_UNKNOWN;
	start_typefind(filePath, &type);
	if (CONTAINER_TYPE_MPEGTS == type)
	{
		NXGLOGI("start to parse ts data");
		start_ts(filePath, media_handle);
		NXGLOGI("done to parse ts data");
		err = StartDiscover(filePath, media_handle);
	}
	else
	{
		err = StartDiscover(filePath, media_handle);
	}

	FUNC_OUT();

	return err;
}

NX_GST_RET  NX_GST_CloseMediaInfo(GST_MEDIA_INFO *media_handle)
{
	FUNC_IN();

	g_free(media_handle);

	media_handle = NULL;

	FUNC_OUT();
}

