#include <gst/gst.h>
#include <gst/mpegts/mpegts.h>

#include "NX_TypeFind.h"
#include "NX_GstLog.h"
#define LOG_TAG "[NX_TypeFind]"

typedef struct TypeFindSt {
    GMainLoop *loop;
    GstBus *bus;
    GstElement *pipeline;
    GstElement *filesrc;
    GstElement *filesrc_typefind;
	GstElement *demux;
	GstElement *decode;
	GstElement *audio_decode;
	GstElement *video_decode;
	GstElement *video_parse;
	GstElement *audio_parse;
	GstElement *video_queue;
	GstElement *audio_queue;
	GstElement *temp_queue;
	GstElement *video_typefind;
	GstElement *video_fakesink;
	GstElement *audio_typefind;
	GstElement *audio_fakesink;
	gint		codec_type;
	gint		audio_track_on;
	gint		video_track_on;

	gint		program_idx;
	gint		video_stream_idx;
	gint		audio_stream_idx;
	gint		subtitle_stream_idx;
    struct GST_MEDIA_INFO *media_info;
} TypeFindSt;

#define DUMP_DESCRIPTORS 0

static gboolean idle_exit_loop (gpointer data);

/******************************************************************************
 * Get demux type
 * filesrc <--> filesrc_typefind <--> video_fakesink
*******************************************************************************/
gint typefind_demux(struct GST_MEDIA_INFO *media_handle, const char* filePath);
static void cb_typefind_demux(GstElement *typefind, guint probability,
								GstCaps *caps, gpointer data);

/******************************************************************************
 * In case of TS file, get total program number, program number list for ts file
 * Otherwise, get total number of each stream type, the codec type of each stream in on_demux_pad_added_num
 * filesrc<-->xxxdemux<-->audio_queue<-->fakesink
 *                    <-->video_queue<-->fakesink
*******************************************************************************/
int get_stream_num_type(struct GST_MEDIA_INFO *media_handle, const char *filePath);
static void on_demux_pad_added_num(GstElement *element, GstPad *pad, gpointer data);

/******************************************************************************
 * filesrc<-->mpegpsdemux<-->video_queue<-->decodebin<-->video_typefind<-->video_fakesink
 * 						 <-->audio_queue<-->audio_fakesink
*******************************************************************************/
int find_avcodec_num_ps(struct GST_MEDIA_INFO *media_handle, const char *filePath);
static void on_demux_pad_added_num_ps(GstElement *element, GstPad *pad, gpointer data);
// Exit loop
static void cb_typefind_video_ps(GstElement *typefind, guint probability, GstCaps *caps, gpointer data);

/******************************************************************************
 * Get each stream info
 * 
*******************************************************************************/
int typefind_codec_info(struct GST_MEDIA_INFO *media_handle,
               const char *filePath, gint codec_type, gint program_num, gint track_num);
// demux<-->video_queue / demux<-->audio_queue
static void on_demux_pad_added_typefind(GstElement *element, GstPad *pad, TypeFindSt *handle);
// video_decode <--> video_typefind
static void on_video_decodebin_pad_added(GstElement *element, GstPad *pad, gpointer data);
// If it's ts file, audio_decode <--> audio_typefind
// Otherwise, audio_decode <--> audio_queue
static void on_audio_decodebin_pad_added(GstElement *element, GstPad *pad, gpointer data);
// Get video mpegversion, width, height, framerate from video_typefind
static void cb_typefind_video (GstElement *typefind, guint probability,	GstCaps *caps, gpointer data);
// Get audio mpegversion, layer, n_channels, samplerate from audio_typefind
static void cb_typefind_audio (GstElement *typefind, guint probability,	GstCaps *caps, gpointer data);

static void on_pad_added (GstElement *element, GstPad *pad, gpointer data);
static gboolean bus_callback (GstBus * bus, GstMessage * msg, TypeFindSt* handle);

static gboolean
idle_exit_loop (gpointer data)
{
    g_main_loop_quit ((GMainLoop *) data);

    /* once */
    return FALSE;
}

static void
cb_typefind_video(GstElement *typefind, guint probability,
	      				GstCaps *caps, gpointer data)
{
	TypeFindSt *handle = (TypeFindSt *)data;
	const gchar *type = NULL;

	GstStructure *structure;
	gchar *media_type = NULL;
	gint pIdx = 0, sIdx = 0;
	const gchar *mime_type;

	NXGLOGI("START");

	sIdx = handle->video_stream_idx;
	pIdx = handle->program_idx;

	structure = gst_caps_get_structure (caps, 0);
	mime_type = gst_structure_get_name(structure);

	type = gst_structure_get_name (structure);
	
	if(0 == strcmp(type, "video/mpeg"))
  	{
		// Get mpegversion
		NXGLOGI("video_type = %s", type);
		if (!gst_structure_has_field (structure, "mpegversion")) {
  			NXGLOGE("There is no mpegversion field");
  		} else {
			gint video_mpegversion;
			VIDEO_TYPE video_type = handle->media_info->ProgramInfo[pIdx].VideoInfo[sIdx].type;
			if ((structure != NULL) && (video_type == VIDEO_TYPE_MPEG_V4))
			{
				gst_structure_get_int (structure, "mpegversion", &video_mpegversion);
				if (video_mpegversion == 1) {
					handle->media_info->ProgramInfo[pIdx].VideoInfo[sIdx].type = VIDEO_TYPE_MPEG_V1;
				} else if (video_mpegversion == 2) {
					handle->media_info->ProgramInfo[pIdx].VideoInfo[sIdx].type = VIDEO_TYPE_MPEG_V2;
				}
			}
  		}
  	}

	// Get width, height
	if (!gst_structure_has_field (structure, "width") ||
		!gst_structure_has_field (structure, "height")) {
		NXGLOGE("There is no width/height field");
	} else {
		gint width = 0, height = 0;
		if (gst_structure_get_int (structure, "width", &width) &&
			gst_structure_get_int (structure, "height", &height))
		{
			handle->media_info->ProgramInfo[pIdx].VideoInfo[sIdx].width = width;
			handle->media_info->ProgramInfo[pIdx].VideoInfo[sIdx].height = height;
			NXGLOGI("width(%d), height(%d)", width, height);
		}
	}
	// Get framerate
	if (!gst_structure_has_field (structure, "framerate")) {
		NXGLOGE("There is no framerate");	
	} else {
		gint framerate_num = -1, framerate_den = -1;
		gst_structure_get_fraction (structure, "framerate", &framerate_num, &framerate_den);
		handle->media_info->ProgramInfo[pIdx].VideoInfo[sIdx].framerate_num = framerate_num;
		handle->media_info->ProgramInfo[pIdx].VideoInfo[sIdx].framerate_denom = framerate_den;
		NXGLOGI("framerate(%d/%d)", framerate_num, framerate_den);
	}

	media_type = gst_caps_to_string (caps);
	g_free(media_type);

	g_idle_add (idle_exit_loop, handle->loop);

	NXGLOGI("END");
}

static void cb_typefind_audio (GstElement *typefind,
	      					guint       probability,
	      					GstCaps    *caps,
	      					gpointer    data)
{
	GMainLoop *loop = NULL;
	TypeFindSt *handle = data;
	const gchar *type = NULL;

	GstStructure *structure;
	const GValue *value;
	const gchar *profile_type = NULL;
	gchar *media_type = NULL;
	gint pIdx = 0, sIdx = 0;

	NXGLOGI("START");

	sIdx = handle->audio_stream_idx;
	pIdx = handle->program_idx;

	loop = handle->loop;

	structure = gst_caps_get_structure (caps, 0);
	type = gst_structure_get_name (structure);

	if( 0 == strcmp(type, "audio/mpeg") )
  	{
		if (!gst_structure_has_field (structure, "mpegversion"))
		{
  			NXGLOGE("There is no mpegversion field");
  		}
		else
		{
			gint audio_mpegversion = 0;
			gst_structure_get_int (structure, "mpegversion", &audio_mpegversion);
	  		NXGLOGI("mpegversion(%d)", audio_mpegversion);

	  		if(4 == audio_mpegversion)
	  		{
	  			if (!gst_structure_has_field (structure, "profile")) {
  					NXGLOGE("There is no profile field");	
  				} else {
    				profile_type = gst_structure_get_string (structure, "profile");
	  				NXGLOGI("profile = %s", profile_type);
					if(0 == strcmp(profile_type, "lc")) {	// AAC LC
						handle->media_info->ProgramInfo[pIdx].AudioInfo[sIdx].type = AUDIO_TYPE_AAC;
					}
  				}
	  		}
			else if(1 == audio_mpegversion)
	  		{
	  			if (!gst_structure_has_field (structure, "layer")) {
  					NXGLOGI("There is no layer field");	
  				} else {
					gint layer = 0;
					gst_structure_get_int (structure, "layer", &layer);
	  				NXGLOGI("layer(%d)", layer);
					if(layer == 3) {		// MP3
						handle->media_info->ProgramInfo[pIdx].AudioInfo[sIdx].type = AUDIO_TYPE_MP3;
					}
  				}
	  		}
  		}  	
  	}

  	if (!gst_structure_has_field (structure, "rate")) {
  		NXGLOGI("There is no rate field");
  	} else {
		gint rate = 0;
		gst_structure_get_int (structure, "rate", &rate);
		handle->media_info->ProgramInfo[pIdx].AudioInfo[sIdx].samplerate = rate;
	  	NXGLOGI("rate(%d)", rate);
  	}

  	if (!gst_structure_has_field (structure, "bitrate")) {
  		NXGLOGI("There is no bitrate field");
  	} else {
		gint bitrate = 0;
		gst_structure_get_int (structure, "bitrate", &bitrate);
		handle->media_info->ProgramInfo[pIdx].AudioInfo[sIdx].bitrate = bitrate;
	  	NXGLOGI("bitrate(%d)", bitrate);
  	}

  	if (!gst_structure_has_field (structure, "channels")) {
  		NXGLOGI("There is no channels field");
  	} else {
		gint channels = 0;
		gst_structure_get_int (structure, "channels", &channels);
		handle->media_info->ProgramInfo[pIdx].AudioInfo[sIdx].n_channels = channels;
	  	NXGLOGI("channels(%d))", channels);
  	}

	media_type = gst_caps_to_string (caps);
	g_free(media_type);
	g_idle_add (idle_exit_loop, loop);

	NXGLOGI("END");
}

static void on_demux_pad_added_typefind(GstElement *element, GstPad *pad, TypeFindSt *handle)
{
	GMainLoop *loop = NULL;
	GstCaps *caps;
	GstStructure *str;
	gchar *name;
	GstPadLinkReturn rc;
	GstElement *targetqueue;
	GstPad *targetsink;
	gint i = 0, len = 0;
	gint sIdx = 0;
	gint audio_on = 0;
	gint codec_type = handle->codec_type;
	gint pIdx = handle->program_idx;

	NXGLOGI("START");

	loop = handle->loop;

	caps = gst_pad_get_current_caps(pad);
	g_assert(caps != NULL);

	name = gst_pad_get_name(pad);

	if((0 == strcasecmp(name, "private_2"))) {
		goto EXIT;
	}

	str = gst_caps_get_structure(caps, 0);
    g_assert(str != NULL);

	const char* mime_type = gst_structure_get_name(str);
	NXGLOGI("new demux pad(%s) MIME-type(%s)", name, mime_type);

	targetqueue = NULL;

	if (CODEC_TYPE_VIDEO == codec_type)
	{ 
		if (g_strrstr(mime_type, "video"))
		{
			// Set target sink as video_queue
			targetqueue = handle->video_queue;

			sIdx = handle->video_stream_idx;

			VIDEO_TYPE video_type = get_video_codec_type(mime_type);
			len = strlen(name);
			// TODO:
			//memcpy(handle->media_info->VideoInfo[tot_num].VideoPadName, name, len);
			//handle->media_info->VideoInfo[tot_num].VideoPadName[len] = 0;
			
			if(handle->media_info->ProgramInfo[pIdx].VideoInfo[sIdx].type == -1)
			{
				NXGLOGE("Not Support Video Codec");
			}
		}
	}
	else if (CODEC_TYPE_AUDIO == codec_type)
	{  
		if (g_strrstr(mime_type, "audio")) {
			gchar *type = NULL;

			// Set target sink as video_queue
			targetqueue = handle->audio_queue;

			sIdx = handle->audio_stream_idx;

			len = strlen(name);
			// TODO:
			//memcpy(handle->media_info->ProgramInfo[pIdx].AudioInfo[tot_num].AudioPadName, name, len);
			//handle->media_info->ProgramInfo[pIdx].AudioInfo[tot_num].AudioPadName[len] = 0;

			type = gst_caps_to_string (caps);

			if(handle->media_info->ProgramInfo[pIdx].AudioInfo[sIdx].type == AUDIO_TYPE_AC3_PRI) 
			{
				handle->media_info->ProgramInfo[pIdx].AudioInfo[sIdx].type = AUDIO_TYPE_AC3;
			}

			g_free (type);
			
			if(handle->media_info->ProgramInfo[pIdx].AudioInfo[sIdx].type == -1)
			{
				NXGLOGE("Not Support Audio Codec");
			}
		}
	}
	else if (CODEC_TYPE_SUBTITLE == codec_type)
	{
		// TODO:
	}

	if (g_str_has_prefix(mime_type, "video/") ||
		g_str_has_prefix(mime_type, "audio/"))
	{ 
		if (targetqueue) {
			targetsink = gst_element_get_static_pad(targetqueue, "sink");
			g_assert(targetsink != NULL);	

			rc = gst_pad_link(pad, targetsink);
			NXGLOGI("%s to link %s:%s to %s:%s",
					(GST_PAD_LINK_OK != rc) ? "Failed":"Succeed",
					GST_DEBUG_PAD_NAME(pad),
					GST_DEBUG_PAD_NAME(targetsink));

			gst_object_unref(targetsink);
		}
	}

EXIT:
	g_free(name);	
	gst_caps_unref(caps);
//	TODO:
	if( (handle->media_info->demux_type == DEMUX_TYPE_MPEGTSDEMUX)
		&& (CODEC_TYPE_AUDIO == codec_type)
		&& (audio_on == 0) 
		)
	{
		NXGLOGI("!!!!! EXIT LOOP");
		g_idle_add (idle_exit_loop, loop);
	}

	NXGLOGI("END");
}

static void on_video_decodebin_pad_added(GstElement *element, GstPad *pad, gpointer data)
{
	GstCaps *caps;
	GstStructure *str;
	gchar *name;
	GstPadLinkReturn rc;
	GstElement *targetqueue;
	GstPad *targetsink;
	TypeFindSt *handle = data; 

	NXGLOGI("START");

	caps = gst_pad_get_current_caps(pad);
	g_assert(caps != NULL);
	name = gst_pad_get_name(pad);

	str = gst_caps_get_structure(caps, 0);
	g_assert(str != NULL);

	const char* mime_type = gst_structure_get_name(str);
	NXGLOGI("new video decodebin pad(%s), mime_type(%s)", name, mime_type);
	g_free(name);
	
	targetqueue = NULL;
	if (g_strrstr(gst_structure_get_name(str), "video")) {
		// Set target sink as video_typefind
		targetqueue = handle->video_typefind;
	}

	if (targetqueue)
    {
		targetsink = gst_element_get_static_pad(targetqueue, "sink");
		if (NULL == targetsink) {
            NXGLOGE("Failed to get pad from targetqueue");
        }
		rc = gst_pad_link(pad, targetsink);
		NXGLOGI("%s to link %s:%s to %s:%s\n",
				(rc == GST_PAD_LINK_OK) ? "Succeed":"Failed",
				GST_DEBUG_PAD_NAME(pad),
				GST_DEBUG_PAD_NAME(targetsink));
		gst_object_unref(targetsink);
	}
	
	gst_caps_unref(caps);

	NXGLOGI("END");
}

static void on_audio_decodebin_pad_added(GstElement *element, GstPad *pad, gpointer data)
{
	GstCaps *caps;
	GstStructure *str;
	gchar *name;
	GstPadLinkReturn rc;
	GstElement *targetqueue;
	GstPad *targetsink;
	TypeFindSt *handle = data; 

	NXGLOGI("START");

	caps = gst_pad_get_current_caps(pad);
	g_assert(caps != NULL);
	name = gst_pad_get_name(pad);

	str = gst_caps_get_structure(caps, 0);
	g_assert(str != NULL);

	const char* mime_type = gst_structure_get_name(str);
	NXGLOGI("new video decodebin pad(%s), mime_type(%s)", name, mime_type);
	g_free(name);
	
	targetqueue = NULL;

	if (g_strrstr(gst_structure_get_name(str), "audio"))
	{
		if(handle->media_info->demux_type == DEMUX_TYPE_MPEGTSDEMUX)
			targetqueue = handle->audio_typefind;
		else
			targetqueue = handle->audio_queue;
	}

	g_free(name);

	if (targetqueue)
	{
		targetsink = gst_element_get_static_pad(targetqueue, "sink");
		if (NULL == targetsink) {
            NXGLOGE("Failed to get pad from targetqueue");
        }
		rc = gst_pad_link(pad, targetsink);
		NXGLOGI("%s to link %s:%s to %s:%s\n",
				(rc == GST_PAD_LINK_OK) ? "Succeed":"Failed",
				GST_DEBUG_PAD_NAME(pad),
				GST_DEBUG_PAD_NAME(targetsink));
		gst_object_unref(targetsink);
	}

	gst_caps_unref(caps);

	NXGLOGI("END");
}

int typefind_codec_info(struct GST_MEDIA_INFO *media_handle, const char *uri,
        gint codec_type, gint program_idx, gint track_num)
{
	TypeFindSt handle;
	
    NXGLOGI("START");

	gint ret = 0;
	gint demux_type = 0;

	handle.loop = g_main_loop_new (NULL, FALSE);
	handle.media_info = media_handle;
	demux_type = handle.media_info->demux_type;
	handle.program_idx = program_idx;
	handle.codec_type = codec_type;

	if (codec_type == CODEC_TYPE_VIDEO)
		handle.video_stream_idx = track_num;
	else if (codec_type == CODEC_TYPE_AUDIO)
		handle.audio_stream_idx = track_num;
	else if (codec_type == CODEC_TYPE_SUBTITLE)
		handle.subtitle_stream_idx = track_num;

	NXGLOGI("codec_type(%d), program_idx(%d), track_num(%d)", codec_type, program_idx, track_num);

	// create a new pipeline to hold the elements
	handle.pipeline = gst_pipeline_new ("pipe");

	handle.bus = gst_pipeline_get_bus ( (handle.pipeline));
	gst_bus_add_watch (handle.bus, bus_callback, &handle);
	gst_object_unref (handle.bus);

	// create filesrc
	handle.filesrc = gst_element_factory_make ("filesrc", "source");
	g_object_set (G_OBJECT (handle.filesrc), "location", uri, NULL);

	// create demux
	if (demux_type == DEMUX_TYPE_MPEGDEMUX) {
		if(CODEC_TYPE_AUDIO == codec_type)
		{
			if(handle.media_info->ProgramInfo[program_idx].AudioInfo[track_num].type == AUDIO_TYPE_AC3) {
				handle.demux = gst_element_factory_make ("dvddemux", "demux");
			} else {
				handle.demux = gst_element_factory_make ("mpegpsdemux", "demux");
			}
			if(handle.media_info->ProgramInfo[program_idx].AudioInfo[track_num].type == AUDIO_TYPE_MPEG) {
				handle.audio_parse = gst_element_factory_make ("mpegaudioparse", "parse_audio");
			} else if(handle.media_info->ProgramInfo[program_idx].AudioInfo[track_num].type == AUDIO_TYPE_AC3) {
				handle.audio_parse = gst_element_factory_make ("ac3parse", "parse_audio");
			}
		}
		else if (CODEC_TYPE_VIDEO == codec_type)
		{
			handle.demux = gst_element_factory_make ("mpegpsdemux", "demux");
			handle.video_parse = gst_element_factory_make ("mpegvideoparse", "parse_video");
		}
	} else if (demux_type == DEMUX_TYPE_QTDEMUX) {
		handle.demux = gst_element_factory_make ("qtdemux", "demux");
    } else if (demux_type == DEMUX_TYPE_MATROSKADEMUX) {
		handle.demux = gst_element_factory_make ("matroskademux", "demux");
	} else if (demux_type == DEMUX_TYPE_AVIDEMUX) {
		handle.demux = gst_element_factory_make ("avidemux", "demux");
	} else if (demux_type == DEMUX_TYPE_MPEGTSDEMUX) {
		handle.demux = gst_element_factory_make ("tsdemux", "demux");
        g_object_set (G_OBJECT(handle.demux), "program-number",
						handle.media_info->program_number[program_idx], NULL); 
	} else {
		NXGLOGE("Not supported demux_type(%d)", demux_type);
	}

	if (CODEC_TYPE_VIDEO == codec_type)
	{
		// create elements (decode, video_queue, video_typefind, video_fakesink)
		if (demux_type == DEMUX_TYPE_MPEGTSDEMUX) {
			handle.decode = gst_element_factory_make ("decodebin", "videodecoder");
		}
		handle.video_queue = gst_element_factory_make ("queue2", "video_queue");
		handle.video_typefind = gst_element_factory_make ("typefind", "typefinder_video");
		handle.video_fakesink = gst_element_factory_make ("fakesink", "sink_video");
		g_signal_connect (handle.video_typefind, "have-type", G_CALLBACK (cb_typefind_video), &handle);
	}
	else if (CODEC_TYPE_AUDIO == codec_type)
	{
		// create elements (decode, audio_queue, audio_typefind, audio_fakesink)
		if(demux_type == DEMUX_TYPE_MPEGTSDEMUX) {
			handle.decode = gst_element_factory_make ("decodebin", "audiodecoder");
		}
		handle.audio_queue = gst_element_factory_make ("queue2", "audio_queue");
		handle.audio_typefind = gst_element_factory_make ("typefind", "typefinder_audio");
		handle.audio_fakesink = gst_element_factory_make ("fakesink", "sink_audio");
		g_signal_connect(handle.audio_typefind, "have-type", G_CALLBACK (cb_typefind_audio), &handle);
	}
	else if (CODEC_TYPE_SUBTITLE == codec_type)
	{
		NXGLOGI("TODO: subtitle");
		return;
	}

	// demux <--> video_queue && demux <--> audio_queue
	g_signal_connect(handle.demux, "pad-added", G_CALLBACK(on_demux_pad_added_typefind), &handle);

	// Add elements to bin and link them
	if(demux_type == DEMUX_TYPE_MPEGDEMUX)
	{
		if(CODEC_TYPE_VIDEO == codec_type)
		{
			// Add video elements
			gst_bin_add_many((handle.pipeline),
                                handle.filesrc, handle.demux,													
                                handle.video_queue, handle.video_parse,
                                handle.video_typefind, handle.video_fakesink,
                                NULL);
			// Link video elements
			ret = gst_element_link(handle.filesrc, handle.demux);
			NXGLOGI("(%d) %s to link filesrc<-->demux", __LINE__, (ret == 0) ? "Failed":"Succeed");
			ret = gst_element_link_many(handle.video_queue, handle.video_parse,
                        handle.video_typefind, handle.video_fakesink, NULL);	
			NXGLOGI("(%d) %s to link video_queue<-->video_parse<-->video_typefind<-->video_fakesink",
					__LINE__, (ret == 0) ? "Failed":"Succeed");
		}
		else if (CODEC_TYPE_AUDIO == codec_type)
		{
			// Add video elements
			gst_bin_add_many((handle.pipeline),
								handle.filesrc, handle.demux,																	
								handle.audio_queue, handle.audio_parse,
                                handle.audio_typefind, handle.audio_fakesink,		
								NULL);
			// Link audio elements
			ret = gst_element_link(handle.filesrc, handle.demux);
			NXGLOGI("(%d) %s to link filesrc<-->demux",
					__LINE__, (ret == 0) ? "Failed":"Succeed");
			ret = gst_element_link_many(handle.audio_queue, handle.audio_parse,
                            handle.audio_typefind, handle.audio_fakesink, NULL);
			NXGLOGI("(%d) %s to link audio_queue<-->audio_parse<-->video_typefind<-->audio_fakesink",
					__LINE__, (ret == 0) ? "Failed":"Succeed");
		}		
	}
	else if(demux_type == DEMUX_TYPE_MPEGTSDEMUX)
	{
		if(CODEC_TYPE_VIDEO == codec_type)
		{
			// Add elements for TS Video
			gst_bin_add_many((handle.pipeline),
                            handle.filesrc, handle.demux,
                            handle.video_queue, handle.decode,
                            handle.video_typefind, handle.video_fakesink,		
                            NULL);
			// decode <--> video_typefind
			g_signal_connect(handle.decode, "pad-added", G_CALLBACK(on_video_decodebin_pad_added), &handle);

			// Link elements for TS Video
			ret = gst_element_link(handle.filesrc, handle.demux);
			NXGLOGI("(%d) %s to link filesrc<-->demux", __LINE__, (ret == 0) ? "Failed":"Succeed");
			ret = gst_element_link ( handle.video_queue, handle.decode);	
			NXGLOGI("(%d) %s to link video_queue<-->decode", __LINE__, (ret == 0) ? "Failed":"Succeed");
			ret = gst_element_link_many (handle.video_typefind, handle.video_fakesink, NULL );	
			NXGLOGI("(%d) %s to link video_typefind<-->video_fakesink", __LINE__, (ret == 0) ? "Failed":"Succeed");
		}
		else if (CODEC_TYPE_AUDIO == codec_type)
		{
			// Add elements for TS Audio
			gst_bin_add_many((handle.pipeline),
								handle.filesrc, handle.demux,																	
								handle.audio_queue, handle.decode,
								handle.audio_typefind, handle.audio_fakesink,		
								NULL);
			//	decode <--> audio_typefind
			g_signal_connect(handle.decode, "pad-added", G_CALLBACK(on_audio_decodebin_pad_added), &handle);
			// Link elements for TS Audio
			ret = gst_element_link(handle.filesrc, handle.demux);
			NXGLOGI("(%d) Failed to link filesrc<-->demux", __LINE__, (ret == 0) ? "Failed":"Succeed");
			ret = gst_element_link(handle.audio_queue, handle.decode);	
			NXGLOGI("(%d) %s to link audio_queue<-->decode", __LINE__, (ret == 0) ? "Failed":"Succeed");
			ret = gst_element_link_many(handle.audio_typefind, handle.audio_fakesink, NULL);	
			NXGLOGI("(%d) %s to link audio_typefind<-->audio_fakesink", __LINE__, (ret == 0) ? "Failed":"Succeed");
		}
		else if (CODEC_TYPE_SUBTITLE == codec_type)
		{
			NXGLOGI("TODO: subtitle");
		}
	}
	else
	{
		if(CODEC_TYPE_VIDEO == codec_type)
		{
			gst_bin_add_many((handle.pipeline),
								handle.filesrc,	handle.demux,																	
								handle.video_queue,
								handle.video_typefind, handle.video_fakesink,		
								NULL);
			//	Link video elements
			ret = gst_element_link(handle.filesrc, handle.demux);
			NXGLOGI("(%d) %s to link filesrc<-->demux", __LINE__, (ret == 0) ? "Failed":"Succeed");
			ret = gst_element_link_many(handle.video_queue, handle.video_typefind, handle.video_fakesink, NULL);	
			NXGLOGI("(%d) %s to link video_queue<-->video_typefind<-->video_fakesink",
					__LINE__, (ret == 0) ? "Failed":"Succeed");
		}
		else if (CODEC_TYPE_AUDIO == codec_type)
		{		
            gst_bin_add_many((handle.pipeline),
								handle.filesrc, handle.demux,
								handle.audio_queue,
								handle.audio_typefind, handle.audio_fakesink,		
								NULL);
			//	Link audio elements
			ret = gst_element_link(handle.filesrc, handle.demux);
			NXGLOGI("(%d) %s to link filesrc<-->demux", __LINE__, (ret == 0) ? "Failed":"Succeed");
			ret = gst_element_link_many (handle.audio_queue, handle.audio_typefind, handle.audio_fakesink, NULL);
			NXGLOGI("(%d) %s to link audio_queue<-->audio_typefind<-->audio_fakesink",
					__LINE__, (ret == 0) ? "Failed":"Succeed");
		}
		else if (CODEC_TYPE_SUBTITLE == codec_type)
		{
			// TODO:
			NXGLOGI("TODO: subtitle");
			return;
		}
	}

	gst_element_set_state ((handle.pipeline), GST_STATE_PLAYING);
	g_main_loop_run (handle.loop);

    // Unset
	gst_element_set_state ((handle.pipeline), GST_STATE_NULL);
	gst_object_unref (GST_OBJECT (handle.pipeline));

	NXGLOGI("END");

	return ret;
}

static void on_pad_added (GstElement *element,
						  GstPad     *pad,
						  gpointer    data)
{
	GstPad *sinkpad;
	TypeFindSt *handle = (TypeFindSt *)data;
	GstElement *target_queue = handle->video_queue;

	NXGLOGI("START");

	sinkpad = gst_element_get_static_pad (target_queue, "sink");
	if (!gst_pad_is_linked (sinkpad))
		gst_pad_link (pad, sinkpad);

	gst_object_unref (sinkpad);

	NXGLOGI("END");
}

static void
dump_pat (GstMpegtsSection * section, TypeFindSt *handle)
{
	GPtrArray *pat = gst_mpegts_section_get_pat (section);
	guint i, len;

	// n_program
	len = pat->len;
	handle->media_info->n_program = pat->len;
	NXGLOGI("   %d program(s):", len);

	for (i = 0; i < len; i++) {
		GstMpegtsPatProgram *patp = g_ptr_array_index (pat, i);
		handle->media_info->program_number[i] = patp->program_number;
		NXGLOGI("## n_program(%d), handle->media_info->program_number[%d] = %d",
		handle->media_info->n_program, i, handle->media_info->program_number[i]);
		// program_number
		NXGLOGI("     program_number:%6d (0x%04x), network_or_program_map_PID:0x%04x",
				patp->program_number, patp->program_number,
				patp->network_or_program_map_PID);
	}

	g_ptr_array_unref (pat);
}

#define dump_memory_content(desc, spacing) dump_memory_bytes((desc)->data + 2, (desc)->length, spacing)

static const gchar *
descriptor_name (gint val)
{
  GEnumValue *en;

  en = g_enum_get_value (G_ENUM_CLASS (g_type_class_peek
          (GST_TYPE_MPEGTS_DESCRIPTOR_TYPE)), val);
  if (en == NULL)
    /* Else try with DVB enum types */
    en = g_enum_get_value (G_ENUM_CLASS (g_type_class_peek
            (GST_TYPE_MPEGTS_DVB_DESCRIPTOR_TYPE)), val);
  if (en == NULL)
    /* Else try with ATSC enum types */
    en = g_enum_get_value (G_ENUM_CLASS (g_type_class_peek
            (GST_TYPE_MPEGTS_ATSC_DESCRIPTOR_TYPE)), val);
  if (en == NULL)
    /* Else try with ISB enum types */
    en = g_enum_get_value (G_ENUM_CLASS (g_type_class_peek
            (GST_TYPE_MPEGTS_ISDB_DESCRIPTOR_TYPE)), val);
  if (en == NULL)
    /* Else try with misc enum types */
    en = g_enum_get_value (G_ENUM_CLASS (g_type_class_peek
            (GST_TYPE_MPEGTS_MISC_DESCRIPTOR_TYPE)), val);
  if (en == NULL)
    return "UNKNOWN/PRIVATE";
  return en->value_nick;
}

static void
dump_descriptors (GPtrArray * descriptors, guint spacing)
{
  guint i;

  for (i = 0; i < descriptors->len; i++) {
    GstMpegtsDescriptor *desc = g_ptr_array_index (descriptors, i);
    NXGLOGI("%*s [descriptor 0x%02x (%s) length:%d]", spacing, "",
        desc->tag, descriptor_name (desc->tag), desc->length);
    if (DUMP_DESCRIPTORS)
      dump_memory_content (desc, spacing + 2);
    switch (desc->tag) {
      case GST_MTS_DESC_REGISTRATION:
      {
        const guint8 *data = desc->data + 2;
#define SAFE_CHAR(a) (g_ascii_isprint(a) ? a : '.')
        NXGLOGI("%*s   Registration : %c%c%c%c [%02x%02x%02x%02x]", spacing,
            "", SAFE_CHAR (data[0]), SAFE_CHAR (data[1]), SAFE_CHAR (data[2]),
            SAFE_CHAR (data[3]), data[0], data[1], data[2], data[3]);

        break;
      }
      case GST_MTS_DESC_CA:
      {
        guint16 ca_pid, ca_system_id;
        const guint8 *private_data;
        gsize private_data_size;
        if (gst_mpegts_descriptor_parse_ca (desc, &ca_system_id, &ca_pid,
                &private_data, &private_data_size)) {
          NXGLOGI("%*s   CA system id : 0x%04x", spacing, "", ca_system_id);
          NXGLOGI("%*s   CA PID       : 0x%04x", spacing, "", ca_pid);
          if (private_data_size) {
            NXGLOGI("%*s   Private Data :", spacing, "");
            dump_memory_bytes ((guint8 *) private_data, private_data_size,
                spacing + 2);
          }
        }
        break;
      }
      case GST_MTS_DESC_DVB_NETWORK_NAME:
      {
        gchar *network_name;
        if (gst_mpegts_descriptor_parse_dvb_network_name (desc, &network_name)) {
          NXGLOGI("%*s   Network Name : %s", spacing, "", network_name);
          g_free (network_name);
        }
        break;
      }
      case GST_MTS_DESC_DVB_SERVICE_LIST:
      {
        dump_dvb_service_list (desc, spacing + 2);
        break;
      }
      case GST_MTS_DESC_DVB_CABLE_DELIVERY_SYSTEM:
        dump_cable_delivery_descriptor (desc, spacing + 2);
        break;
      case GST_MTS_DESC_DVB_TERRESTRIAL_DELIVERY_SYSTEM:
        dump_terrestrial_delivery (desc, spacing + 2);
        break;
      case GST_MTS_DESC_DVB_BOUQUET_NAME:
      {
        gchar *bouquet_name;
        if (gst_mpegts_descriptor_parse_dvb_bouquet_name (desc, &bouquet_name)) {
          NXGLOGI("%*s   Bouquet Name Descriptor, bouquet_name:%s", spacing,
              "", bouquet_name);
          g_free (bouquet_name);
        }
        break;
      }
      case GST_MTS_DESC_DTG_LOGICAL_CHANNEL:
        dump_logical_channel_descriptor (desc, spacing + 2);
        break;
      case GST_MTS_DESC_DVB_SERVICE:
      {
        gchar *service_name, *provider_name;
        GstMpegtsDVBServiceType service_type;
        if (gst_mpegts_descriptor_parse_dvb_service (desc, &service_type,
                &service_name, &provider_name)) {
          NXGLOGI("%*s   Service Descriptor, type:0x%02x (%s)", spacing, "",
              service_type, enum_name (GST_TYPE_MPEGTS_DVB_SERVICE_TYPE,
                  service_type));
          NXGLOGI("%*s      service_name  : %s", spacing, "", service_name);
          NXGLOGI("%*s      provider_name : %s", spacing, "",
              provider_name);
          g_free (service_name);
          g_free (provider_name);

        }
        break;
      }
      case GST_MTS_DESC_DVB_MULTILINGUAL_BOUQUET_NAME:
      {
        dump_multiligual_bouquet_name (desc, spacing + 2);
        break;
      }
      case GST_MTS_DESC_DVB_MULTILINGUAL_NETWORK_NAME:
      {
        dump_multiligual_network_name (desc, spacing + 2);
        break;
      }
      case GST_MTS_DESC_DVB_MULTILINGUAL_SERVICE_NAME:
      {
        dump_multiligual_service_name (desc, spacing + 2);
        break;
      }
      case GST_MTS_DESC_DVB_MULTILINGUAL_COMPONENT:
      {
        dump_multiligual_component (desc, spacing + 2);
        break;
      }
      case GST_MTS_DESC_DVB_PRIVATE_DATA_SPECIFIER:
      {
        guint32 specifier;
        guint8 len = 0, *data = NULL;

        if (gst_mpegts_descriptor_parse_dvb_private_data_specifier (desc,
                &specifier, &data, &len)) {
          NXGLOGI("%*s   private_data_specifier : 0x%08x", spacing, "",
              specifier);
          if (len > 0) {
            dump_memory_bytes (data, len, spacing + 2);
            g_free (data);
          }
        }
        break;
      }
      case GST_MTS_DESC_DVB_FREQUENCY_LIST:
      {
        gboolean offset;
        GArray *list;
        if (gst_mpegts_descriptor_parse_dvb_frequency_list (desc, &offset,
                &list)) {
          guint j;
          for (j = 0; j < list->len; j++) {
            guint32 freq = g_array_index (list, guint32, j);
            NXGLOGI("%*s   Frequency : %u %s", spacing, "", freq,
                offset ? "kHz" : "Hz");
          }
          g_array_unref (list);
        }
        break;
      }
      case GST_MTS_DESC_DVB_LINKAGE:
        dump_linkage (desc, spacing + 2);
        break;
      case GST_MTS_DESC_DVB_COMPONENT:
        dump_component (desc, spacing + 2);
        break;
      case GST_MTS_DESC_DVB_STREAM_IDENTIFIER:
      {
        guint8 tag;
        if (gst_mpegts_descriptor_parse_dvb_stream_identifier (desc, &tag)) {
          NXGLOGI("%*s   Component Tag : 0x%02x", spacing, "", tag);
        }
        break;
      }
      case GST_MTS_DESC_DVB_CA_IDENTIFIER:
      {
        GArray *list;
        guint j;
        guint16 ca_id;
        if (gst_mpegts_descriptor_parse_dvb_ca_identifier (desc, &list)) {
          for (j = 0; j < list->len; j++) {
            ca_id = g_array_index (list, guint16, j);
            NXGLOGI("%*s   CA Identifier : 0x%04x", spacing, "", ca_id);
          }
          g_array_unref (list);
        }
        break;
      }
      case GST_MTS_DESC_DVB_CONTENT:
        dump_content (desc, spacing + 2);
        break;
      case GST_MTS_DESC_DVB_PARENTAL_RATING:
      {
        GPtrArray *ratings;
        guint j;

        if (gst_mpegts_descriptor_parse_dvb_parental_rating (desc, &ratings)) {
          for (j = 0; j < ratings->len; j++) {
            GstMpegtsDVBParentalRatingItem *item =
                g_ptr_array_index (ratings, j);
            NXGLOGI("%*s   country_code : %s", spacing, "",
                item->country_code);
            NXGLOGI("%*s   rating age   : %d", spacing, "", item->rating);
          }
          g_ptr_array_unref (ratings);
        }
        break;
      }
      case GST_MTS_DESC_DVB_DATA_BROADCAST:
      {
        GstMpegtsDataBroadcastDescriptor *res;

        if (gst_mpegts_descriptor_parse_dvb_data_broadcast (desc, &res)) {
          NXGLOGI("%*s   data_broadcast_id : 0x%04x", spacing, "",
              res->data_broadcast_id);
          NXGLOGI("%*s   component_tag     : 0x%02x", spacing, "",
              res->component_tag);
          if (res->length > 0) {
            NXGLOGI("%*s   selector_bytes:", spacing, "");
            dump_memory_bytes (res->selector_bytes, res->length, spacing + 2);
          }
          NXGLOGI("%*s   text              : %s", spacing, "",
              res->text ? res->text : "NULL");
          gst_mpegts_dvb_data_broadcast_descriptor_free (res);
        }
        break;
      }
      case GST_MTS_DESC_ISO_639_LANGUAGE:
        dump_iso_639_language (desc, spacing + 2);
        break;
      case GST_MTS_DESC_DVB_SHORT_EVENT:
      {
        gchar *language_code, *event_name, *text;
        if (gst_mpegts_descriptor_parse_dvb_short_event (desc, &language_code,
                &event_name, &text)) {
          NXGLOGI("%*s   Short Event, language_code:%s", spacing, "",
              language_code);
          NXGLOGI("%*s     event_name : %s", spacing, "", event_name);
          NXGLOGI("%*s     text       : %s", spacing, "", text);
          g_free (language_code);
          g_free (event_name);
          g_free (text);
        }
      }
        break;
      case GST_MTS_DESC_DVB_EXTENDED_EVENT:
      {
        dump_dvb_extended_event (desc, spacing + 2);
        break;
      }
      case GST_MTS_DESC_DVB_SUBTITLING:
      {
        gchar *lang;
        guint8 type;
        guint16 composition;
        guint16 ancillary;
        guint j;

        for (j = 0;
            gst_mpegts_descriptor_parse_dvb_subtitling_idx (desc, j, &lang,
                &type, &composition, &ancillary); j++) {
          NXGLOGI("%*s   Subtitling, language_code:%s", spacing, "", lang);
          NXGLOGI("%*s      type                : %u", spacing, "", type);
          NXGLOGI("%*s      composition page id : %u", spacing, "",
              composition);
          NXGLOGI("%*s      ancillary page id   : %u", spacing, "",
              ancillary);
          g_free (lang);
        }
      }
        break;
      case GST_MTS_DESC_DVB_TELETEXT:
      {
        GstMpegtsDVBTeletextType type;
        gchar *lang;
        guint8 magazine, page_number;
        guint j;

        for (j = 0;
            gst_mpegts_descriptor_parse_dvb_teletext_idx (desc, j, &lang, &type,
                &magazine, &page_number); j++) {
          NXGLOGI("%*s   Teletext, type:0x%02x (%s)", spacing, "", type,
              enum_name (GST_TYPE_MPEGTS_DVB_TELETEXT_TYPE, type));
          NXGLOGI("%*s      language    : %s", spacing, "", lang);
          NXGLOGI("%*s      magazine    : %u", spacing, "", magazine);
          NXGLOGI("%*s      page number : %u", spacing, "", page_number);
          g_free (lang);
        }
      }
        break;
      default:
        break;
    }
  }
}

static const gchar *
stream_type_name (gint val)
{
	GEnumValue *en;

	en = g_enum_get_value (G_ENUM_CLASS (g_type_class_peek
					(GST_TYPE_MPEGTS_STREAM_TYPE)), val);
	if (en == NULL)
		/* Else try with SCTE enum types */
		en = g_enum_get_value (G_ENUM_CLASS (g_type_class_peek
						(GST_TYPE_MPEGTS_SCTE_STREAM_TYPE)), val);
	if (en == NULL)
		return "UNKNOWN/PRIVATE";
	return en->value_nick;
}

static void
dump_pmt(GstMpegtsSection * section, TypeFindSt *handle)
{
	const GstMpegtsPMT *pmt = gst_mpegts_section_get_pmt (section);
	guint i, len;

	NXGLOGI("     program_number : 0x%04x", section->subtable_extension);
	NXGLOGI("     pcr_pid        : 0x%04x", pmt->pcr_pid);
	//dump_descriptors (pmt->descriptors, 7);
	len = pmt->streams->len;
	NXGLOGI("     %d Streams:", len);
	
	for (i = 0; i < len; i++) {
		GstMpegtsPMTStream *stream = g_ptr_array_index (pmt->streams, i);
		const gchar* stream_type = stream_type_name (stream->stream_type);
		NXGLOGI("       pid:0x%04x , stream_type:0x%02x (%s)", stream->pid,
				stream->stream_type, stream_type);

		if (g_str_has_prefix(stream_type, "video")) {
			gint v_idx = handle->media_info->ProgramInfo[i].n_video;
			if (GST_MPEGTS_STREAM_TYPE_VIDEO_MPEG1 == stream->stream_type) {
				handle->media_info->ProgramInfo[i].VideoInfo[v_idx].type = VIDEO_TYPE_MPEG_V1;
			} else if (GST_MPEGTS_STREAM_TYPE_VIDEO_MPEG1 == stream->stream_type) {
				handle->media_info->ProgramInfo[i].VideoInfo[v_idx].type = VIDEO_TYPE_MPEG_V2;
			} else if (GST_MPEGTS_STREAM_TYPE_VIDEO_H264 == stream->stream_type) {
				handle->media_info->ProgramInfo[i].VideoInfo[v_idx].type = VIDEO_TYPE_H264;
			}
			handle->media_info->ProgramInfo[i].n_video++;
		} else if (g_str_has_prefix(stream_type, "audio")) {
			gint a_idx = handle->media_info->ProgramInfo[i].n_audio;
			if (GST_MPEGTS_STREAM_TYPE_AUDIO_MPEG1 == stream->stream_type) {
				handle->media_info->ProgramInfo[i].AudioInfo[a_idx].type = AUDIO_TYPE_MPEG_V1;
			} else if(GST_MPEGTS_STREAM_TYPE_AUDIO_MPEG2 == stream->stream_type) {
				handle->media_info->ProgramInfo[i].AudioInfo[a_idx].type = AUDIO_TYPE_MPEG_V2;
			}
			handle->media_info->ProgramInfo[i].n_audio++;
		} else if (g_str_has_prefix(stream_type, "private-pes-packets")) {
			gint s_idx = handle->media_info->ProgramInfo[i].n_subtitle;
			if (GST_MPEGTS_STREAM_TYPE_PRIVATE_PES_PACKETS == stream->stream_type) {
				handle->media_info->ProgramInfo[i].SubtitleInfo[s_idx].type = AUDIO_TYPE_MPEG_V1;
			}
			handle->media_info->ProgramInfo[i].n_subtitle++;
		}
		//dump_descriptors (stream->descriptors, 9);
	}
}

static gboolean
bus_callback(GstBus * bus, GstMessage * msg, TypeFindSt* handle)
{
    NXGLOGI("Got %s msg\n", GST_MESSAGE_TYPE_NAME (msg));

    GMainLoop *loop = handle->loop;
    switch (GST_MESSAGE_TYPE (msg)) {
        case GST_MESSAGE_ERROR:{
            GError *err;
            gchar *debug;

            gst_message_parse_error(msg, &err, &debug);
            NXGLOGE("Error: %s\n", err->message);
            g_error_free (err);
            g_free (debug);

            g_main_loop_quit (loop);
            break;
        }
        case GST_MESSAGE_EOS:
            /* end-of-stream */
            g_main_loop_quit(loop);
            break;
        case GST_MESSAGE_STATE_CHANGED:
        {
            GstState old_state, new_state;
            gst_message_parse_state_changed(msg, &old_state, &new_state, NULL);
            NXGLOGI("Element '%s' changed state from  '%s' to '%s'"
                       , GST_OBJECT_NAME (msg->src)
                       , gst_element_state_get_name (old_state)
                       , gst_element_state_get_name (new_state));
            break;
        }
		case GST_MESSAGE_ELEMENT:
		{
			GstMpegtsSection *section;
			if ((section = gst_message_parse_mpegts_section(msg))) {
				GstMpegtsSectionType section_type = GST_MPEGTS_SECTION_TYPE(section);
				if (GST_MPEGTS_SECTION_PAT == section_type) {
					dump_pat(section, handle);
					gst_mpegts_section_unref(section);
				}
				if (GST_MPEGTS_SECTION_PMT == section_type) {
					dump_pmt(section, handle);
					gst_mpegts_section_unref(section);
					g_idle_add (idle_exit_loop, loop);
				}
			}
		}
        default:
            /* unhandled message */
            break;
    }

    /* we want to be notified again the next time there is a message
    * on the bus, so returning TRUE (FALSE means we want to stop watching
    * for messages on the bus and our callback should not be called again)
    */
    return TRUE;
}

static void
cb_typefind_demux(GstElement *typefind, guint probability,
			GstCaps *caps, gpointer data)
{
    TypeFindSt *handle = (TypeFindSt *)data;

    gchar *type = gst_caps_to_string (caps);
    GstStructure *structure = gst_caps_get_structure(caps, 0);
    const gchar *mime_type = gst_structure_get_name(structure);

    handle->media_info->container_type = get_container_type(mime_type);
    handle->media_info->demux_type = get_demux_type(mime_type);

    NXGLOGI("container_type (%d) demux_type(%d) Media type %s found, probability %d%%",
            handle->media_info->container_type, handle->media_info->demux_type, type, probability);

	structure = gst_caps_get_structure (caps, 0);
	mime_type = gst_structure_get_name(structure);

    // TODO: Check if it needs to get audioonly & type here

    g_free (type);

    g_idle_add (idle_exit_loop, handle->loop);
}

// demux<-->video_queue / demux<-->audio_queue
static void
on_demux_pad_added_num(GstElement *element, GstPad *pad, gpointer data)
{
	GstCaps *caps;
	GstStructure *str;
	gchar *name;
	GstPadLinkReturn rc;
	GstElement *targetqueue;
	GstPad *targetsink;

	NXGLOGI("START");

    TypeFindSt *handle = (TypeFindSt *)data;

	caps = gst_pad_get_current_caps(pad);
	if (NULL == caps) {
        NXGLOGE("Failed to get caps");
    }
	name = gst_pad_get_name(pad);
	str = gst_caps_get_structure(caps, 0);
    if (NULL == str) {
        NXGLOGE("Failed to get structure from caps");
    }

    const gchar *mime_type = gst_structure_get_name(str);
	NXGLOGI("new demux pad(%s) for ", name, mime_type);

	targetqueue = NULL;

	// TODO: is this the right way to match video/audio pads
	if (g_strrstr(mime_type, "video"))
    {
        // Set target sink as video queue
		targetqueue = handle->video_queue;

        // Get video info
        gint video_mpegversion, num, den = 0;
        VIDEO_TYPE video_type = get_video_codec_type(mime_type);
        int32_t v_idx = handle->media_info->ProgramInfo[0].n_video;
		
        handle->media_info->ProgramInfo[0].VideoInfo[v_idx].type = video_type;
        if ((str != NULL) && (video_type == VIDEO_TYPE_MPEG_V4))
        {
            gst_structure_get_int (str, "mpegversion", &video_mpegversion);
            if (video_mpegversion == 1) {
                handle->media_info->ProgramInfo[0].VideoInfo[v_idx].type = VIDEO_TYPE_MPEG_V1;
            } else if (video_mpegversion == 2) {
                handle->media_info->ProgramInfo[0].VideoInfo[v_idx].type = VIDEO_TYPE_MPEG_V2;
            }
			NXGLOGI("## mpegversion(%d)", video_mpegversion);
        }
        handle->media_info->ProgramInfo[0].n_video++;

        NXGLOGI("## type(%d), n_video(%d)",
                handle->media_info->ProgramInfo[0].VideoInfo[v_idx].type,
                handle->media_info->ProgramInfo[0].n_video);
	}
    else if (g_strrstr(mime_type, "audio"))
    {
        // Set target sink as audio queue
		targetqueue = handle->audio_queue;

        // Get audio info
        gint audio_mpegversion, channels, samplerate;
        AUDIO_TYPE audio_type = get_audio_codec_type(mime_type);
		int32_t a_idx = handle->media_info->ProgramInfo[0].n_audio;

        handle->media_info->ProgramInfo[0].AudioInfo[a_idx].type = audio_type;
        if (gst_structure_get_int (str, "mpegversion", &audio_mpegversion))
        {
            if (audio_mpegversion == 1) {
                handle->media_info->ProgramInfo[0].AudioInfo[a_idx].type = AUDIO_TYPE_MPEG_V1;
            } else if (audio_mpegversion == 2) {
                handle->media_info->ProgramInfo[0].AudioInfo[a_idx].type = AUDIO_TYPE_MPEG_V2;
            }
        }
        if (gst_structure_get_int(str, "channels", &channels)) {
            handle->media_info->ProgramInfo[0].AudioInfo[a_idx].n_channels = channels;
        }
        if (gst_structure_get_int(str, "rate", &samplerate)) {
            handle->media_info->ProgramInfo[0].AudioInfo[a_idx].samplerate = samplerate;
        }
        handle->media_info->ProgramInfo[0].n_audio++;
        NXGLOGI("n_channels(%d), samplerate(%d), type(%d)",
                channels, samplerate,
                handle->media_info->ProgramInfo[0].AudioInfo[a_idx].type);
	}
    else if (g_strrstr(mime_type, "subtitle"))
    {
        SUBTITLE_TYPE sub_type = get_video_codec_type(mime_type);
        int32_t sub_idx = handle->media_info->ProgramInfo[0].n_subtitle;

        handle->media_info->ProgramInfo[0].SubtitleInfo[sub_idx].type = sub_type;
        handle->media_info->ProgramInfo[0].n_subtitle++;

        NXGLOGI("n_subtitle(%d), subtitle_type(%d)",
                handle->media_info->ProgramInfo[0].n_subtitle,
                handle->media_info->ProgramInfo[0].SubtitleInfo[sub_idx].type);
    }

	if (targetqueue)
    {
		targetsink = gst_element_get_static_pad(targetqueue, "sink");
		if (NULL == targetsink) {
            NXGLOGE("Failed to get pad from targetqueue");
        }
		rc = gst_pad_link(pad, targetsink);
		NXGLOGI("%s to link %s:%s from %s:%s\n",
				(rc == GST_PAD_LINK_OK) ? "Succeed":"Failed",
				GST_DEBUG_PAD_NAME(pad),
				GST_DEBUG_PAD_NAME(targetsink));
		gst_object_unref(targetsink);
	}

	g_free(name);
	gst_caps_unref(caps);
	g_idle_add (idle_exit_loop, handle->loop);

	NXGLOGI("END");
}

gint
typefind_demux(struct GST_MEDIA_INFO *media_handle, const char* filePath)
{
    // Initialize struct 'TypeFindSt'
    TypeFindSt handle;
    memset(&handle, 0, sizeof(TypeFindSt));
    handle.media_info = media_handle;

    NXGLOGI("START");

    // init GStreamer
    if(!gst_is_initialized()) {
        gst_init(NULL, NULL);
    }
    handle.loop = g_main_loop_new (NULL, FALSE);

    // Create a new pipeline to hold the elements
    handle.pipeline = gst_pipeline_new("pipe");

    handle.bus = gst_pipeline_get_bus (GST_PIPELINE (handle.pipeline));
    gst_bus_add_watch (handle.bus, bus_callback, &handle);
    gst_object_unref (handle.bus);

    // Create file source and typefind element
    handle.filesrc = gst_element_factory_make ("filesrc", "source");
    g_object_set (G_OBJECT (handle.filesrc), "location", filePath, NULL);
    handle.filesrc_typefind = gst_element_factory_make ("typefind", "filesrc_typefind");
    g_signal_connect (handle.filesrc_typefind, "have-type", G_CALLBACK (cb_typefind_demux), &handle);
    handle.video_fakesink = gst_element_factory_make ("fakesink", "sink");

    // Add elements
    gst_bin_add_many (GST_BIN (handle.pipeline), handle.filesrc,
                        handle.filesrc_typefind, handle.video_fakesink, NULL);
    
    // Link elements
    gst_element_link_many (handle.filesrc, handle.filesrc_typefind,
                            handle.video_fakesink, NULL);

    // Set the state to PLAYING
    gst_element_set_state (GST_ELEMENT (handle.pipeline), GST_STATE_PLAYING);
    g_main_loop_run (handle.loop);

    // "Release"
    gst_element_set_state (GST_ELEMENT (handle.pipeline), GST_STATE_NULL);
    gst_object_unref (GST_OBJECT (handle.pipeline));

    NXGLOGI("END");

    return 0;
}

static void demuxer_notify_pat_info (GObject *obj, GParamSpec *pspec, gpointer data)
{
    GValueArray *patinfo = NULL;
    GValue * value = NULL;
    GObject *entry = NULL;
    guint program, pid;
    gint i;
    TypeFindSt *handle = data;

    NXGLOGI("START");

    g_object_get (obj, "pat-info", &patinfo, NULL);

    handle->media_info->n_program = patinfo->n_values;

    NXGLOGI("PAT: entries: %d\n", patinfo->n_values);  
    for (i = 0; i < (gint)patinfo->n_values; i++) {
        value = g_value_array_get_nth (patinfo, (guint)i);
        entry = (GObject*) g_value_get_object (value);
        g_object_get (entry, "program-number", &program, NULL);
        g_object_get (entry, "pid", &pid, NULL);
        NXGLOGI("    program: %04x pid: %04x\n", program, pid);
        handle->media_info->program_number[i] = program;
    }

    // TODO:
    //handle->media_info->VideoTrackTotNum = handle->media_info->n_program;

    NXGLOGI("END");
}

static void
decodebin_pad_added(GstElement *element, GstPad *pad, gpointer data)
{
    gchar *name;
    GstCaps *caps;
    GstStructure *structure;
    GstElement *target_sink;
    GstPad *target_sink_pad;
    TypeFindSt * handle = (TypeFindSt *)data;

    name = gst_pad_get_name(pad);
    NXGLOGI("A new pad %s was created for %s\n", name, gst_element_get_name(element));
    g_free(name);

    caps = gst_pad_get_current_caps(pad);
    if (caps == NULL) {
        NXGLOGE("Failed to get current caps");
        return;
    }

    structure = gst_caps_get_structure(caps, 0);
    if (structure == NULL) {
        NXGLOGE("Failed to get current caps");
        gst_caps_unref (caps);
        return;
    }

    const char *mime_type = gst_structure_get_name(structure);
    NXGLOGI("MIME-type:%s", mime_type);
    if (g_str_has_prefix(mime_type, "video/"))
    {
        gint video_mpegversion, width, height, num, den;
        NXGLOGI("width(%d), height(%d), framerate(%d/%d)", width, height, num, den);

        target_sink = handle->video_fakesink;
        NXGLOGV("element %s will be linked to %s\n",
                gst_element_get_name(element),
                gst_element_get_name(target_sink));

        target_sink_pad = gst_element_get_static_pad(target_sink, "sink");
        GstPadLinkReturn ret = gst_pad_link(pad, target_sink_pad);
        NXGLOGI("%s to link %s:%s to %s:%s",
                (ret != GST_PAD_LINK_OK) ? "Failed":"Succeed",
                GST_DEBUG_PAD_NAME(pad),
                GST_DEBUG_PAD_NAME(target_sink_pad));
        gst_object_unref (target_sink_pad);
    }
    gst_caps_unref (caps);
}

int get_stream_num_type(struct GST_MEDIA_INFO *media_handle, const char *filePath)
{
    TypeFindSt handle;

	NXGLOGI("START");

	gint ret = 0;
	gint demux_type = 0;

	handle.loop = g_main_loop_new (NULL, FALSE);
	handle.media_info = media_handle;
	demux_type = handle.media_info->demux_type;

	// create a new pipeline to hold the elements
	handle.pipeline = gst_pipeline_new ("pipe");

	handle.bus = gst_pipeline_get_bus (GST_PIPELINE (handle.pipeline));
	gst_bus_add_watch (handle.bus, bus_callback, &handle);
	gst_object_unref (handle.bus);

	// create file source and typefind element
	handle.filesrc = gst_element_factory_make ("filesrc", "source");
	g_object_set (G_OBJECT (handle.filesrc), "location", filePath, NULL);

	if (demux_type == DEMUX_TYPE_QTDEMUX) {
		handle.demux = gst_element_factory_make ("qtdemux", "demux");
    } else if (demux_type == DEMUX_TYPE_MATROSKADEMUX) {
		handle.demux = gst_element_factory_make ("matroskademux", "demux");
	} else if (demux_type == DEMUX_TYPE_AVIDEMUX) {
		handle.demux = gst_element_factory_make ("avidemux", "demux");
	} else if (demux_type == DEMUX_TYPE_MPEGDEMUX) {
		handle.demux = gst_element_factory_make ("mpegpsdemux", "demux");
		handle.video_parse = gst_element_factory_make ("mpegvideoparse", "parse_video");
		handle.audio_parse = gst_element_factory_make ("mpegaudioparse", "parse_audio");
	} else if (demux_type == DEMUX_TYPE_MPEGTSDEMUX) {
		handle.demux = gst_element_factory_make ("tsdemux", "demux");
	} else {
		NXGLOGE("Not supported demux_type(%d)", demux_type);
		return -1;
	}	

	handle.video_queue = gst_element_factory_make ("queue2", "video_queue");
	handle.video_fakesink = gst_element_factory_make ("fakesink", "sink_video");

	handle.audio_queue = gst_element_factory_make ("queue2", "audio_queue");
	handle.audio_fakesink = gst_element_factory_make ("fakesink", "sink_audio");

	if(demux_type == DEMUX_TYPE_MPEGTSDEMUX)
	{
        // Add elements
		gst_bin_add_many((handle.pipeline),
                        handle.filesrc, handle.demux,
                        handle.video_queue, handle.video_fakesink,
                        NULL);

		// Link elements (filesrc <--> demux <==> video_queue <--> video_fakesink)
		ret = gst_element_link(handle.filesrc, handle.demux);
		NXGLOGI("(%d) %s to link filesrc<-->demux", __LINE__, (ret == 0) ? "Failed":"Succeed");
		ret = gst_element_link_many(handle.video_queue, handle.video_fakesink, NULL);
		NXGLOGI("(%d) %s to link video_queue<-->video_fakesink", __LINE__, (ret == 0) ? "Failed":"Succeed");

		// Link elements (demux <==> video_queue)
		g_signal_connect (handle.demux, "pad-added", G_CALLBACK (on_pad_added), &handle);
		//g_signal_connect(handle.demux, "pad-added", G_CALLBACK(on_demux_pad_added_num), &handle);
	}
	else
	{
		// Link elements (demux <==> video_queue, demux <==> audio_queue)
		g_signal_connect(handle.demux, "pad-added", G_CALLBACK(on_demux_pad_added_num), &handle);

		gst_bin_add_many((handle.pipeline),
                        handle.filesrc, handle.demux,
                        handle.video_queue, handle.video_fakesink,
                        handle.audio_queue, handle.audio_fakesink,
                        NULL);

		// Link elements filesrc<-->demux <==> video_queue<-->video_fakesink, audio_queue<-->audio_fakesink)
		ret = gst_element_link(handle.filesrc, handle.demux);
		NXGLOGI("(%d) %s to link filesrc<-->demux", __LINE__, (ret == 0) ? "Failed":"Succeed");
		ret = gst_element_link_many(handle.video_queue, handle.video_fakesink, NULL);
		NXGLOGI("(%d) %s to link video_queue<-->video_fakesink", __LINE__, (ret == 0) ? "Failed":"Succeed");
		ret = gst_element_link_many (handle.audio_queue, handle.audio_fakesink, NULL);
		NXGLOGI("(%d) %s to link audio_queue<-->audio_fakesink", __LINE__, (ret == 0) ? "Failed":"Succeed");
	}

	gst_element_set_state ((handle.pipeline), GST_STATE_PLAYING);

	g_main_loop_run (handle.loop);

	gst_element_set_state ((handle.pipeline), GST_STATE_NULL);
	gst_object_unref (GST_OBJECT(handle.pipeline));

    NXGLOGI("END");

	return ret;
}

// demux<-->video_queue / demux<-->audio_queue
static void on_demux_pad_added_num_ps(GstElement *element, GstPad *pad, gpointer data)
{
	GstCaps *caps;
	GstStructure *str;
	gchar *name;
	GstPadLinkReturn rc;
	GstElement *targetqueue;
	GstPad *targetsink;

	NXGLOGI("START");

    TypeFindSt *handle = (TypeFindSt *)data;
	gint pIdx = handle->program_idx;

	caps = gst_pad_get_current_caps(pad);
	if (NULL == caps) {
        NXGLOGE("Failed to get caps");
    }
	name = gst_pad_get_name(pad);
	str = gst_caps_get_structure(caps, 0);
    if (NULL == str) {
        NXGLOGE("Failed to get structure from caps");
    }

    const gchar *mime_type = gst_structure_get_name(str);
	NXGLOGI("new demux pad(%s) for ", name, mime_type);

	targetqueue = NULL;

	// TODO: is this the right way to match video/audio pads
	if (g_strrstr(mime_type, "video"))
    {
        // Set target sink as video queue
		targetqueue = handle->video_queue;

        // Get video info
        gint video_mpegversion, num, den = 0;
        VIDEO_TYPE video_type = get_video_codec_type(mime_type);
        int32_t v_idx = handle->media_info->ProgramInfo[0].n_video;
		
        handle->media_info->ProgramInfo[0].VideoInfo[v_idx].type = video_type;
        if ((str != NULL) && (video_type == VIDEO_TYPE_MPEG_V4))
        {
            gst_structure_get_int (str, "mpegversion", &video_mpegversion);
            if (video_mpegversion == 1) {
                handle->media_info->ProgramInfo[0].VideoInfo[v_idx].type = VIDEO_TYPE_MPEG_V1;
            } else if (video_mpegversion == 2) {
                handle->media_info->ProgramInfo[0].VideoInfo[v_idx].type = VIDEO_TYPE_MPEG_V2;
            }
			NXGLOGI("## mpegversion(%d)", video_mpegversion);
        }
        handle->media_info->ProgramInfo[0].n_video++;

        NXGLOGI("## type(%d), n_video(%d)",
                handle->media_info->ProgramInfo[0].VideoInfo[v_idx].type,
                handle->media_info->ProgramInfo[0].n_video);
	}
    else if (g_strrstr(mime_type, "audio"))
    {
        // Set target sink as audio queue
		targetqueue = handle->audio_queue;

        // Get audio info
        gint audio_mpegversion, channels, samplerate;
        AUDIO_TYPE audio_type = get_audio_codec_type(mime_type);
		int32_t a_idx = handle->media_info->ProgramInfo[0].n_audio;

        handle->media_info->ProgramInfo[0].AudioInfo[a_idx].type = audio_type;
        if (gst_structure_get_int (str, "mpegversion", &audio_mpegversion))
        {
            if (audio_mpegversion == 1) {
                handle->media_info->ProgramInfo[0].AudioInfo[a_idx].type = AUDIO_TYPE_MPEG_V1;
            } else if (audio_mpegversion == 2) {
                handle->media_info->ProgramInfo[0].AudioInfo[a_idx].type = AUDIO_TYPE_MPEG_V2;
            }
        }
        if (gst_structure_get_int(str, "channels", &channels)) {
            handle->media_info->ProgramInfo[0].AudioInfo[a_idx].n_channels = channels;
        }
        if (gst_structure_get_int(str, "rate", &samplerate)) {
            handle->media_info->ProgramInfo[0].AudioInfo[a_idx].samplerate = samplerate;
        }
        handle->media_info->ProgramInfo[0].n_audio++;
        NXGLOGI("n_channels(%d), samplerate(%d), type(%d)",
                channels, samplerate,
                handle->media_info->ProgramInfo[0].AudioInfo[a_idx].type);
	}
    else if (g_strrstr(mime_type, "subtitle"))
    {
        SUBTITLE_TYPE sub_type = get_video_codec_type(mime_type);
        int32_t s_idx = handle->media_info->ProgramInfo[0].n_subtitle;

        handle->media_info->ProgramInfo[0].SubtitleInfo[s_idx].type = sub_type;
        handle->media_info->ProgramInfo[0].n_subtitle++;

        NXGLOGI("n_subtitle(%d), subtitle_type(%d)",
                handle->media_info->ProgramInfo[0].n_subtitle,
                handle->media_info->ProgramInfo[0].SubtitleInfo[s_idx].type);
    }

	if (targetqueue)
    {
		targetsink = gst_element_get_static_pad(targetqueue, "sink");
		if (NULL == targetsink) {
            NXGLOGE("Failed to get pad from targetqueue");
        }
		rc = gst_pad_link(pad, targetsink);
		NXGLOGI("%s to link %s:%s from %s:%s\n",
				(rc == GST_PAD_LINK_OK) ? "Succeed":"Failed",
				GST_DEBUG_PAD_NAME(pad),
				GST_DEBUG_PAD_NAME(targetsink));
		gst_object_unref(targetsink);
	}

	g_free(name);
	gst_caps_unref(caps);

	NXGLOGI("END");
}

static void
cb_typefind_video_ps(GstElement *typefind, guint probability,
					GstCaps *caps, gpointer data)
{
	GMainLoop *loop = NULL;
	TypeFindSt *ty_handle = (TypeFindSt *)data;

	NXGLOGI("START");

	loop = ty_handle->loop;
	g_idle_add (idle_exit_loop, loop);

	NXGLOGI("END");
}

int find_avcodec_num_ps(struct GST_MEDIA_INFO *media_handle, const char *filePath)
{
	TypeFindSt handle;

	NXGLOGI("START");

	gint ret = 0;

	handle.loop = g_main_loop_new (NULL, FALSE);
	handle.media_info = media_handle;

	// Create a new pipeline to hold the elements
	handle.pipeline = gst_pipeline_new ("pipe");

	handle.bus = gst_pipeline_get_bus (GST_PIPELINE (handle.pipeline));
	gst_bus_add_watch (handle.bus, bus_callback, NULL);
	gst_object_unref (handle.bus);

	// Create elements
	handle.filesrc = gst_element_factory_make ("filesrc", "source");
	g_object_set (G_OBJECT (handle.filesrc), "location", filePath, NULL);

	handle.demux = gst_element_factory_make ("mpegpsdemux", "demux");
	handle.video_decode = gst_element_factory_make ("decodebin", "video_decode");
	handle.video_typefind = gst_element_factory_make ("typefind", "typefinder_video");
	handle.audio_decode = gst_element_factory_make ("decodebin", "audio_decode");
		
	handle.video_queue = gst_element_factory_make ("queue2", "video_queue");
	handle.temp_queue = gst_element_factory_make ("queue2", "temp_queue");
	handle.video_fakesink = gst_element_factory_make ("fakesink", "sink_video");

	handle.audio_queue = gst_element_factory_make ("queue2", "audio_queue");
	handle.audio_fakesink = gst_element_factory_make ("fakesink", "sink_audio");

	gst_bin_add_many ((handle.pipeline),
						handle.filesrc, handle.demux,
						handle.video_queue, handle.video_decode,
						handle.video_typefind, handle.video_fakesink,
						handle.audio_queue, handle.audio_fakesink,
						NULL);

	// mpegpsdemux <==> video_queue && mpegpsdemux <==> audio_queue
	g_signal_connect (handle.demux, "pad-added", G_CALLBACK(on_demux_pad_added_num_ps), &handle);
	// Exit loop
	g_signal_connect (handle.video_typefind, "have-type", G_CALLBACK(cb_typefind_video_ps), &handle);
	// video_decode <==> video_typefind
	g_signal_connect(handle.video_decode, "pad-added", G_CALLBACK(on_video_decodebin_pad_added), &handle);

	// Link elements
	ret  = gst_element_link(handle.filesrc, handle.demux);
	NXGLOGI("(%d) %s to link filesrc<-->demux", __LINE__, (ret == 0) ? "Failed":"Succeed");		
	ret  = gst_element_link (handle.video_queue, handle.video_decode);
	NXGLOGI("(%d) %s to link video_queue<-->video_decode", __LINE__, (ret == 0) ? "Failed":"Succeed");
	ret  = gst_element_link(handle.video_typefind, handle.video_fakesink);	
	NXGLOGI("(%d) %s to link video_typefind<-->video_fakesink", __LINE__, (ret == 0) ? "Failed":"Succeed");
	ret  = gst_element_link(handle.audio_queue, handle.audio_fakesink);	
	NXGLOGI("(%d) %s to link audio_queue<-->audio_fakesink", __LINE__, (ret == 0) ? "Failed":"Succeed");

    g_type_class_ref (GST_TYPE_MPEGTS_SECTION_TYPE);
    g_type_class_ref (GST_TYPE_MPEGTS_SECTION_TABLE_ID);
    g_type_class_ref (GST_TYPE_MPEGTS_RUNNING_STATUS);
    g_type_class_ref (GST_TYPE_MPEGTS_DESCRIPTOR_TYPE);
    g_type_class_ref (GST_TYPE_MPEGTS_DVB_DESCRIPTOR_TYPE);
    g_type_class_ref (GST_TYPE_MPEGTS_ATSC_DESCRIPTOR_TYPE);
    g_type_class_ref (GST_TYPE_MPEGTS_ISDB_DESCRIPTOR_TYPE);
    g_type_class_ref (GST_TYPE_MPEGTS_MISC_DESCRIPTOR_TYPE);
    g_type_class_ref (GST_TYPE_MPEGTS_ISO639_AUDIO_TYPE);
    g_type_class_ref (GST_TYPE_MPEGTS_DVB_SERVICE_TYPE);
    g_type_class_ref (GST_TYPE_MPEGTS_DVB_TELETEXT_TYPE);
    g_type_class_ref (GST_TYPE_MPEGTS_STREAM_TYPE);
    g_type_class_ref (GST_TYPE_MPEGTS_SECTION_DVB_TABLE_ID);
    g_type_class_ref (GST_TYPE_MPEGTS_SECTION_ATSC_TABLE_ID);
    g_type_class_ref (GST_TYPE_MPEGTS_SECTION_SCTE_TABLE_ID);
    g_type_class_ref (GST_TYPE_MPEGTS_MODULATION_TYPE);
    g_type_class_ref (GST_TYPE_MPEGTS_DVB_CODE_RATE);
    g_type_class_ref (GST_TYPE_MPEGTS_CABLE_OUTER_FEC_SCHEME);
    g_type_class_ref (GST_TYPE_MPEGTS_TERRESTRIAL_TRANSMISSION_MODE);
    g_type_class_ref (GST_TYPE_MPEGTS_TERRESTRIAL_GUARD_INTERVAL);
    g_type_class_ref (GST_TYPE_MPEGTS_TERRESTRIAL_HIERARCHY);
    g_type_class_ref (GST_TYPE_MPEGTS_DVB_LINKAGE_TYPE);
    g_type_class_ref (GST_TYPE_MPEGTS_DVB_LINKAGE_HAND_OVER_TYPE);
    g_type_class_ref (GST_TYPE_MPEGTS_COMPONENT_STREAM_CONTENT);
    g_type_class_ref (GST_TYPE_MPEGTS_CONTENT_NIBBLE_HI);
    g_type_class_ref (GST_TYPE_MPEGTS_SCTE_STREAM_TYPE);
    g_type_class_ref (GST_TYPE_MPEGTS_SECTION_SCTE_TABLE_ID);

	gst_element_set_state ((handle.pipeline), GST_STATE_PLAYING);

	g_main_loop_run (handle.loop);

	// Unset
	gst_element_set_state ((handle.pipeline), GST_STATE_NULL);
	gst_object_unref (GST_OBJECT(handle.pipeline));

	NXGLOGI("END");

	return ret;
}