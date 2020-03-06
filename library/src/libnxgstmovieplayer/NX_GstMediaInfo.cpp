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
#include "NX_GstDiscover.h"
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
	NXGLOGI("START");

	enum NX_GST_ERROR err = NX_GST_ERROR_NONE;

	// Get demux type
	typefind_demux(media_handle, filePath);
	if (-1 == media_handle->demux_type) {
		err = NX_GST_ERROR_NOT_SUPPORTED_CONTENTS;
		return err;
	}

	if (media_handle->demux_type == DEMUX_TYPE_MPEGTSDEMUX)
	{
		// Get total number of programs, program number list from pat
		get_program_info(filePath, media_handle);
		for (int i; i< media_handle->n_program; i++)
		{
			int cur_program_no = media_handle->program_number[i];
			if (cur_program_no != 0) {
				// Get total number of streams in each program from dump_collection
				get_stream_simple_info(filePath, cur_program_no, media_handle);
				if (media_handle->ProgramInfo[i].n_video > 0) {
					// Get the detail stream information from pad-added signal in decodebin_pad_added_detail
					get_stream_detail_info(filePath, cur_program_no, media_handle);
				}
			}
		}
	}
	else
	{
		if (0 == media_handle->n_program) {
			if (media_handle->ProgramInfo[0].n_video > 0) {
				media_handle->n_program = 1;
			}
		}
		//start_parsing(filePath);
		err = StartDiscover(filePath, media_handle);
	}

	NXGLOGI("END");

	return err;
}

NX_GST_RET  CloseMediaInfo(GST_MEDIA_INFO *media_handle)
{
	NXGLOGI("START");

	for (int pIdx = 0; pIdx < media_handle->n_program; pIdx++)
	{
		for (int vIdx = 0; vIdx < media_handle->ProgramInfo[pIdx].n_video; vIdx++) {
			g_free(media_handle->ProgramInfo[pIdx].VideoInfo[vIdx].stream_id);
			media_handle->ProgramInfo[pIdx].VideoInfo[vIdx].stream_id = NULL;
		}
		for (int aIdx = 0; aIdx < media_handle->ProgramInfo[pIdx].n_audio; aIdx++) {
			g_free(media_handle->ProgramInfo[pIdx].AudioInfo[aIdx].stream_id);
			media_handle->ProgramInfo[pIdx].AudioInfo[aIdx].stream_id = NULL;
		}
		for (int sIdx = 0; sIdx < media_handle->ProgramInfo[pIdx].n_subtitle; sIdx++) {
			g_free(media_handle->ProgramInfo[pIdx].SubtitleInfo[sIdx].language_code);
			media_handle->ProgramInfo[pIdx].SubtitleInfo[sIdx].stream_id = NULL;
		}
	}

	g_free(media_handle);

	media_handle = NULL;

	NXGLOGI("END");
}

void MediaInfoToStr(GST_MEDIA_INFO *media_info, const char*filePath)
{
	if (NULL == media_info)
	{
		NXGLOGE("media_info is NULL");
		return;
	}

	NXGLOGI("<=========== [GST_MEDIA_INFO] =========== ");
	NXGLOGI("filePath(%s)", filePath);
	NXGLOGI("container_type(%d), demux_type(%d), n_program(%d), current_program(%d)",
			media_info->container_type, media_info->demux_type,
			media_info->n_program, media_info->current_program_no);

	if (media_info->demux_type != DEMUX_TYPE_MPEGTSDEMUX)
	{
		media_info->n_program = 1;
	}

	for (int i=0; i<media_info->n_program; i++)
	{
		NXGLOGI("ProgramInfo[%d] - program_number[%d]:%d, "
				"n_video(%d), n_audio(%d), n_subtitlte(%d), seekable(%d)",
				i, i, media_info->program_number[i],
				media_info->ProgramInfo[i].n_video,
				media_info->ProgramInfo[i].n_audio,
				media_info->ProgramInfo[i].n_subtitle,
				media_info->ProgramInfo[i].seekable);

		for (int v_idx=0; v_idx<media_info->ProgramInfo[i].n_video; v_idx++)
		{
			NXGLOGI("%*s [VideoInfo[%d]] "
					"type(%d), width(%d), height(%d), framerate_num/denom(%d/%d),"
					"stream_id(%s)",
					5, " ", v_idx,
					media_info->ProgramInfo[i].VideoInfo[v_idx].type,
					media_info->ProgramInfo[i].VideoInfo[v_idx].width,
					media_info->ProgramInfo[i].VideoInfo[v_idx].height,
					media_info->ProgramInfo[i].VideoInfo[v_idx].framerate_num,
					media_info->ProgramInfo[i].VideoInfo[v_idx].framerate_denom,
					media_info->ProgramInfo[i].VideoInfo[v_idx].stream_id);
		}
		for (int a_idx=0; a_idx<media_info->ProgramInfo[i].n_audio; a_idx++)
		{
			NXGLOGI("%*s [AudioInfo[%d]] "
					"type(%d), n_channels(%d), samplerate(%d), bitrate(%d)\n",
					"stream_id(%s)",
					5, " ", a_idx,
					media_info->ProgramInfo[i].AudioInfo[a_idx].type,
					media_info->ProgramInfo[i].AudioInfo[a_idx].n_channels,
					media_info->ProgramInfo[i].AudioInfo[a_idx].samplerate,
					media_info->ProgramInfo[i].AudioInfo[a_idx].bitrate,
					media_info->ProgramInfo[i].AudioInfo[a_idx].stream_id);
		}
		for (int s_idx=0; s_idx<media_info->ProgramInfo[i].n_subtitle; s_idx++)
		{
			NXGLOGI("%*s [SubtitleInfo[%d]] "
					"type(%d), language_code(%s), stream_id(%s)",
					5, " ", s_idx,
					media_info->ProgramInfo[i].SubtitleInfo[s_idx].type,
					media_info->ProgramInfo[i].SubtitleInfo[s_idx].language_code,
					media_info->ProgramInfo[i].SubtitleInfo[s_idx].stream_id);
		}
	}

	NXGLOGI("=========== [GST_MEDIA_INFO] ===========> ");
}

