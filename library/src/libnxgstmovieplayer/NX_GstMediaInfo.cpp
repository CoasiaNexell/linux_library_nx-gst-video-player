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

NX_GST_RET	OpenMediaInfo(GST_MEDIA_INFO **media_handle)
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

NX_GST_ERROR  ParseMediaInfo(GST_MEDIA_INFO *media_handle, const char *filePath)
{
	FUNC_IN();
	enum NX_GST_ERROR err = NX_GST_ERROR_NONE;

	CONTAINER_TYPE type = CONTAINER_TYPE_UNKNOWN;
	start_typefind(filePath, &type);
	if (CONTAINER_TYPE_MPEGTS == type)
	{
		get_program_info(filePath, media_handle);
		MediaInfoToStr(media_handle, filePath);
		//for (int i=0; i<media_handle->n_program; i++)
		{
			get_stream_info(filePath, media_handle->program_number[1], media_handle);
			MediaInfoToStr(media_handle, filePath);
		}
		//err = StartDiscover(filePath, media_handle);
	}
	else
	{
		media_handle->n_program = 1;
		err = StartDiscover(filePath, media_handle);
	}

	FUNC_OUT();

	return err;
}

NX_GST_RET  CloseMediaInfo(GST_MEDIA_INFO *media_handle)
{
	FUNC_IN();

	g_free(media_handle);

	media_handle = NULL;

	FUNC_OUT();
}

void MediaInfoToStr(GST_MEDIA_INFO *media_info, const char*filePath)
{
	if (NULL == media_info)
	{
		NXGLOGE("media_info is NULL");
		return;
	}

	NXGLOGI("<=========== [GST_MEDIA_INFO] %s =========== ", filePath);
	NXGLOGI("container_type:%d, demux_type:%d, \n"
			"n_program(%d), current_program(%d), current_program_idx(%d)\n",
			media_info->container_type, media_info->demux_type,
			media_info->n_program, media_info->current_program,
			media_info->current_program_idx);

	for (int i=0; i<media_info->n_program; i++)
	{
		NXGLOGI("ProgramInfo[%d] - program_number[%d]:(%d), "
				"n_video(%d), n_audio(%d), n_subtitlte(%d), seekable(%d)",
				i, i, media_info->program_number[i],
				media_info->ProgramInfo[i].n_video,
				media_info->ProgramInfo[i].n_audio,
				media_info->ProgramInfo[i].n_subtitle,
				media_info->ProgramInfo[i].seekable);

		for (int v_idx=0; v_idx<media_info->ProgramInfo[i].n_video; v_idx++)
		{
			NXGLOGI("%*s [VideoInfo[%d]] \n"
					"type(%d), width(%d), height(%d), framerate_num/denom(%d/%d)\n",
					5, " ", v_idx,
					media_info->ProgramInfo[i].VideoInfo[v_idx].type,
					media_info->ProgramInfo[i].VideoInfo[v_idx].width,
					media_info->ProgramInfo[i].VideoInfo[v_idx].height,
					media_info->ProgramInfo[i].VideoInfo[v_idx].framerate_num,
					media_info->ProgramInfo[i].VideoInfo[v_idx].framerate_denom);
		}
		for (int a_idx=0; a_idx<media_info->ProgramInfo[i].n_audio; a_idx++)
		{
			NXGLOGI("%*s [AudioInfo[%d]] \n"
					"type(%d), n_channels(%d), samplerate(%d), bitrate(%d)\n",
					5, " ", a_idx,
					media_info->ProgramInfo[i].AudioInfo[a_idx].type,
					media_info->ProgramInfo[i].AudioInfo[a_idx].n_channels,
					media_info->ProgramInfo[i].AudioInfo[a_idx].samplerate,
					media_info->ProgramInfo[i].AudioInfo[a_idx].bitrate);
		}
		for (int s_idx=0; s_idx<media_info->ProgramInfo[i].n_subtitle; s_idx++)
		{
			NXGLOGI("%*s [SubtitleInfo[%d]] \n"
					"type(%d), language_code(%s)\n",
					5, " ", s_idx,
					media_info->ProgramInfo[i].SubtitleInfo[s_idx].type,
					media_info->ProgramInfo[i].SubtitleInfo[s_idx].language_code);
		}
	}

	NXGLOGI("=========== [GST_MEDIA_INFO] ===========> ");
}

