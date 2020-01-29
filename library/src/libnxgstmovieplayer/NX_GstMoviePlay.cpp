#include "NX_GstMoviePlay.h"
#include "GstDiscover.h"
#include "NX_DbgMsg.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#include <gdk-pixbuf/gdk-pixbuf.h>
#include "NX_GstThumbnail.h"
#include <NX_GstMovie.h>

#define LOG_TAG "[NxGstVideoPlayer]"
#include <NX_Log.h>

#include <math.h>

static gboolean hasSubTitles = FALSE;

struct MOVIE_TYPE {
	GstElement *pipeline;

	GstElement *source;

	GstElement *audio_queue;
	GstElement *video_queue;
	GstElement *subtitle_queue;

	GstElement *demuxer;
	GstElement *video_parser;
	GstElement *nxvideodec;
	GstElement *nxvideosink;
	GstElement *nxvideosink_hdmi;
	GstElement *tee;
	GstElement *tee_queue_primary;
	GstElement *tee_queue_secondary;

	GstPad *tee_primary_pad;
	GstPad *tee_secondary_pad;
	GstPad *queue_primary_pad;
	GstPad *queue_secondary_pad;

	DISPLAY_MODE isSecondaryDsp;

	GstElement *fakesink;
	GstElement *capsfilter;

	GstElement *decodebin;
	GstElement *audio_parser;
	GstElement *audio_decoder;
	GstElement *audioconvert;
	GstElement *audioresample;
	GstElement *alsasink;

	GstElement *volume;
	gdouble rate;                 /* Current playback rate (can be negative) */

	GMainLoop *loop;
	GThread *thread;
	GstBus *bus;
	guint bus_watch_id;

	pthread_mutex_t apiLock;
	pthread_mutex_t stateLock;

	gboolean pipeline_is_linked;

	//	Medi Information & URI
	NX_URI_TYPE uri_type;
	gchar *uri;

	GST_MEDIA_INFO gst_media_info;

	NX_GST_ERROR error;

	//	Callback
	void (*callback)(void *, unsigned int EventType, unsigned int EventData, void* param);
	void *owner;
} ;

class _CAutoLock
{
	public:
		_CAutoLock(pthread_mutex_t * pLock)
			: m_pLock( pLock )
		{
			pthread_mutex_lock( m_pLock );
		}
		~_CAutoLock()
		{
			pthread_mutex_unlock( m_pLock );
		}
	private:
		pthread_mutex_t *m_pLock;
};

NX_MEDIA_STATE GstState2NxState(GstState state);
static gpointer loop_func(gpointer data);
static void start_loop_thread(MP_HANDLE handle);
static void stop_my_thread(MP_HANDLE handle);
static gboolean gst_bus_callback(GstBus *bus, GstMessage *msg, MP_HANDLE handle);
static NX_GST_RET seek_to_time (MP_HANDLE handle, gint64 time_nanoseconds);

static gpointer loop_func(gpointer data)
{
	FUNC_IN();

	MP_HANDLE handle = (MP_HANDLE)data;	
	g_main_loop_run (handle->loop); 
	
	FUNC_OUT();

	return NULL;
}

static void start_loop_thread(MP_HANDLE handle)
{
	FUNC_IN();

	handle->loop = g_main_loop_new (NULL, FALSE);
	handle->thread = g_thread_create (loop_func, handle, TRUE, NULL);

	FUNC_OUT();
}

static void stop_my_thread(MP_HANDLE handle)
{
	FUNC_IN();

	if (NULL != handle->loop)
	{
		g_main_loop_quit (handle->loop);
	}
	if (NULL != handle->thread)
	{
		g_thread_join (handle->thread);
	}
	if (NULL != handle->loop)
	{
		g_main_loop_unref (handle->loop);
	}

	FUNC_OUT();
}

static void on_decodebin_pad_added_demux (GstElement *element,
                        GstPad *pad, gpointer data)
{
	FUNC_IN();
	
    GstPad *sinkpad = NULL;
    GstCaps *caps = NULL;
    GstStructure *new_pad_structure = NULL;
	const gchar *mime_type = NULL;
	MP_HANDLE handle = (MP_HANDLE)data;

	GstElement *sink_pad_audio = handle->audioconvert;

    caps = gst_pad_get_current_caps (pad);
    if (caps == NULL) {
        NXLOGE("%s() Failed to get current caps", __FUNCTION__);
        return;
    }

    new_pad_structure = gst_caps_get_structure (caps, 0);
	if (new_pad_structure == NULL) {
        NXLOGE("%s() Failed to get current caps", __FUNCTION__);
        return;
    }

	mime_type = gst_structure_get_name(new_pad_structure);
	NXLOGI("%s() MIME-type:%s", __FUNCTION__, mime_type);
	if (g_str_has_prefix(mime_type, "audio/")) {
        sinkpad = gst_element_get_static_pad (sink_pad_audio, "sink");
		if (NULL == sinkpad) {
			NXLOGE("%s() Failed to get static pad", __FUNCTION__);
			gst_caps_unref (caps);
			return;
		}

		if (GST_PAD_LINK_FAILED(gst_pad_link (pad, sinkpad))) {
			NXLOGE("%s() Failed to link %s:%s to %s:%s",
					__FUNCTION__,
					GST_DEBUG_PAD_NAME(pad),
					GST_DEBUG_PAD_NAME(sinkpad));
        }

		NXLOGI("%s() Succeed to link %s:%s to %s:%s",
				__FUNCTION__,
				GST_DEBUG_PAD_NAME(pad),
				GST_DEBUG_PAD_NAME(sinkpad));

		gst_object_unref(sinkpad);
    }

	gst_caps_unref (caps);

	FUNC_OUT();
}

SUBTITLE_INFO* setSubtitleInfo(GstClockTime startTime, GstClockTime endTime, GstClockTime duration, const char* subtitle)
{
	SUBTITLE_INFO*	m_pSubtitleInfo = (SUBTITLE_INFO*) g_malloc0(sizeof(SUBTITLE_INFO));

	m_pSubtitleInfo->startTime = (gint64) startTime;
	m_pSubtitleInfo->endTime = (gint64) endTime;
	m_pSubtitleInfo->duration = (gint64) duration;
	m_pSubtitleInfo->subtitleText = g_strdup(subtitle);

	NXLOGI("%s() subtitle:%s startTime:%" GST_TIME_FORMAT ", duration: %" GST_TIME_FORMAT ", endTime: %" GST_TIME_FORMAT
			, __FUNCTION__, subtitle, GST_TIME_ARGS(startTime), GST_TIME_ARGS(duration), GST_TIME_ARGS(endTime));

	return m_pSubtitleInfo;
}

void on_handoff (GstElement* object,
				GstBuffer* buffer,
				GstPad* pad,
				gpointer user_data)
{
	char text_msg[128];
	MP_HANDLE handle = (MP_HANDLE) user_data;

	gsize buffer_size, extracted_size;
	memset(text_msg, 0, sizeof(text_msg));

	buffer_size = gst_buffer_get_size(buffer);

	extracted_size = gst_buffer_extract(buffer, 0, text_msg, buffer_size);
	//NXLOGI ("%s() Buffer Size is %d, read %d, data: %s", __FUNCTION__, buffer_size, extracted_size, text_msg);

	static GstClockTime startTime, duration, endTime, timestamp;
	startTime = GST_BUFFER_PTS (buffer);
	duration = GST_BUFFER_DURATION (buffer);
	timestamp = GST_BUFFER_TIMESTAMP (buffer);
	endTime = startTime + duration;
	//NXLOGI("%s() startTime:%" GST_TIME_FORMAT ", duration: %" GST_TIME_FORMAT ", endTime: %" GST_TIME_FORMAT ", timestamp: %" GST_TIME_FORMAT
	//				, __FUNCTION__, GST_TIME_ARGS(startTime), GST_TIME_ARGS(duration), GST_TIME_ARGS(endTime), GST_TIME_ARGS(timestamp));

	SUBTITLE_INFO* subtitleInfo = setSubtitleInfo(startTime, endTime, duration, text_msg);

	handle = (MP_HANDLE)user_data;
	if (handle)
		handle->callback(NULL, (int)MP_EVENT_SUBTITLE_UPDATED, 0, subtitleInfo);

	gst_buffer_unref (buffer);
}

NX_GST_RET set_subtitle_element(MP_HANDLE handle)
{
	FUNC_IN();

	if (!handle) {
		NXLOGE("%s() handle is NULL", __func__);
		return NX_GST_RET_ERROR;
	}

	handle->subtitle_queue = gst_element_factory_make ("queue2", "subtitle_queue");

	handle->capsfilter = gst_element_factory_make ("capsfilter", "capsfilter");
	GstCaps* cap = gst_caps_new_simple("text/x-raw", NULL, NULL);
	g_object_set(G_OBJECT(handle->capsfilter), "caps", cap, NULL);

	handle->fakesink = gst_element_factory_make ("fakesink", "fakesink");

	if (!handle->capsfilter ||
		!handle->subtitle_queue || !handle->fakesink)
	{
		NXLOGE("%s() Failed to create subtitle elements", __func__);
		return NX_GST_RET_ERROR;
	}
	gst_caps_unref(cap);

	g_object_set (handle->fakesink, "signal-handoffs", TRUE, NULL);
	g_object_set (handle->fakesink, "sync", FALSE, NULL);

	g_signal_connect(handle->fakesink, "handoff", G_CALLBACK (on_handoff), handle);

	return NX_GST_RET_OK;
}

static GstPadProbeReturn
cb_have_data (GstPad          *pad,
              GstPadProbeInfo *info,
              gpointer         user_data)
{
	GstBuffer *buffer;
	char text_msg[128];
	MP_HANDLE handle = NULL;

	gint buffer_size, read_size;
	memset(text_msg, 0, sizeof(text_msg));
	buffer = GST_PAD_PROBE_INFO_BUFFER (info);
	buffer_size = gst_buffer_get_size(buffer);

	read_size = gst_buffer_extract(buffer, 0, text_msg, buffer_size);
	NXLOGI ("%s() Buffer Size is %d, read %d, data: %s", __FUNCTION__, buffer_size, read_size, text_msg);

	handle = (MP_HANDLE)user_data;
	if (handle)
		handle->callback(NULL, (int)MP_EVENT_SUBTITLE_UPDATED, 0, 0);

	return GST_PAD_PROBE_OK;
}

static void
on_pad_added_demux (GstElement *element,
                        GstPad *pad, gpointer data)
{
    GstPad *sinkpad = NULL;
    GstCaps *caps = NULL;
	GstStructure *structure;
	const gchar *mime_type;
	GstElement *target_sink_pad = NULL;
	gint width = 0, height = 0;
	MP_HANDLE handle = NULL;
	gboolean isLinkFailed = FALSE;
	gchar* padName = NULL;

	handle = (MP_HANDLE)data;
	padName = gst_pad_get_name(pad);

    caps = gst_pad_get_current_caps(pad);
    if (caps == NULL) {
        NXLOGE("%s() Failed to get current caps", __FUNCTION__);
        return;
    }

	structure = gst_caps_get_structure(caps, 0);
	if (structure == NULL) {
        NXLOGE("%s() Failed to get current caps", __FUNCTION__);
		gst_caps_unref (caps);
        return;
    }

	mime_type = gst_structure_get_name(structure);
	NXLOGI("%s() padName(%s) mime_type(%s)", __FUNCTION__, padName, mime_type);
	if (g_str_has_prefix(mime_type, "video/")) {
		target_sink_pad = handle->video_queue;
        sinkpad = gst_element_get_static_pad (target_sink_pad, "sink");

		// caps parsing
		gst_structure_get_int (structure, "width", &width);
		gst_structure_get_int (structure, "height", &height);
	} else if (g_str_has_prefix(mime_type, "audio/")) {
	    target_sink_pad = handle->audio_queue;
        sinkpad = gst_element_get_static_pad (target_sink_pad, "sink");
    } else if (hasSubTitles && g_str_has_prefix(padName, "subtitle_0")) {
		target_sink_pad = handle->subtitle_queue;
		sinkpad = gst_element_get_static_pad (target_sink_pad, "sink");
	} else {
		NXLOGI("%s() There is no available link for %s", __FUNCTION__, padName);
	}

	NXLOGI("%s() target_sink_pad:%s", __FUNCTION__, (NULL != target_sink_pad) ? GST_OBJECT_NAME(target_sink_pad):"");

	if (g_str_has_prefix(mime_type, "video/") || g_str_has_prefix(mime_type, "audio/") || (hasSubTitles && g_str_has_prefix(padName, "subtitle_0")))
    {
        if (NULL == sinkpad) {
            NXLOGE("%s() Failed to get static pad", __FUNCTION__);
			gst_caps_unref (caps);
            return;
        }

        if (GST_PAD_LINK_FAILED(gst_pad_link(pad, sinkpad))) {
            NXLOGE("%s() Failed to link %s:%s to %s:%s",
                    __FUNCTION__,
                    GST_DEBUG_PAD_NAME(pad),
                    GST_DEBUG_PAD_NAME(sinkpad));
			isLinkFailed = TRUE;
        }
		else
		{
	        NXLOGI("%s() Succeed to create dynamic pad link %s:%s to %s:%s",
                    __FUNCTION__,
                    GST_DEBUG_PAD_NAME(pad),
                    GST_DEBUG_PAD_NAME(sinkpad));
		}
        gst_object_unref (sinkpad);
    }
	/*if (hasSubTitles && g_str_has_prefix(padName, "subtitle"))
	{
		NXLOGI("%s Add probe to pad", __FUNCTION__);
		//gulong gst_pad_add_probe(GstPad*, GstPadProbeType,
		//						   GstPadProbeCallback, gpointer, GDestroyNotify)
		gst_pad_add_probe (pad, GST_PAD_PROBE_TYPE_BUFFER, (GstPadProbeCallback) cb_have_data, handle, NULL);
	}*/

	if (padName)	g_free(padName);
	gst_caps_unref (caps);

	/* Need to check subtitle_1, 2, etc*/
	if (TRUE == isLinkFailed) {
		handle->callback(NULL, (int)MP_EVENT_DEMUX_LINK_FAILED, 0, NULL);
	}

	FUNC_OUT();
}

static void print_tag(const GstTagList *list, const gchar *tag, gpointer unused)
{
	gint i, count;

	count = gst_tag_list_get_tag_size (list, tag);

	for (i = 0; i < count; i++) {
		gchar *str;

		if (gst_tag_get_type (tag) == G_TYPE_STRING) {
			g_assert (gst_tag_list_get_string_index (list, tag, i, &str));
		} else {
			str = g_strdup_value_contents (gst_tag_list_get_value_index (list, tag, i));
		}

		if (i == 0) {
			NXLOGD("%15s: %s", gst_tag_get_nick (tag), str);
		} else {
			NXLOGD("               : %s", str);
		}

		g_free (str);
	}
}

static gboolean gst_bus_callback (GstBus *bus, GstMessage *msg, MP_HANDLE handle)
{
    GMainLoop *loop = (GMainLoop*)handle->loop;

    switch (GST_MESSAGE_TYPE (msg))
    {
        case GST_MESSAGE_EOS:
            NXLOGI("%s() End-of-stream", __FUNCTION__);
            handle->callback(NULL, (int)MP_EVENT_EOS, 0, NULL);
            break;
        case GST_MESSAGE_ERROR:
		case GST_MESSAGE_WARNING:
        {
            gchar *debug = NULL;
            GError *err = NULL;

			if (msg->type == GST_MESSAGE_WARNING) {
				gst_message_parse_warning (msg, &err, &debug);
			} else {
				gst_message_parse_error (msg, &err, &debug);
			}

            NXLOGE("%s() Gstreamer %s: %s", __FUNCTION__,
				   (msg->type == GST_MESSAGE_WARNING)?"warning":"error", err->message);
            g_error_free (err);

            NXLOGI("%s() Debug details: %s", __FUNCTION__, debug);
            g_free (debug);

			handle->callback(NULL, (int)MP_EVENT_GST_ERROR, 0, 0);
            break;
        }
        case GST_MESSAGE_STATE_CHANGED:
        {
            GstState old_state, new_state;

            gst_message_parse_state_changed (msg, &old_state, &new_state, NULL);
			// TODO: workaround
            if(g_strcmp0("NxGstMoviePlay", GST_OBJECT_NAME (msg->src)) == 0) {
                NXLOGI("%s Element '%s' changed state from  '%s' to '%s'"
					   , __FUNCTION__
					   , GST_OBJECT_NAME (msg->src)
					   , gst_element_state_get_name (old_state)
					   , gst_element_state_get_name (new_state));
				//	Send Message
				handle->callback(NULL, (int)MP_EVENT_STATE_CHANGED, (int)GstState2NxState(new_state), NULL);
            }
            break;
        }
		case GST_MESSAGE_DURATION_CHANGED:
		{
			NXLOGI("%s() TODO:%s", __FUNCTION__, gst_message_type_get_name(GST_MESSAGE_TYPE(msg)));
			break;
		}
		case GST_MESSAGE_STREAM_STATUS:
		{
			GstStreamStatusType t;
			GstElement *owner;
			gst_message_parse_stream_status(msg, &t, &owner);
			break;
		}
		case GST_MESSAGE_ASYNC_DONE:
		{
			GstClockTime running_time;
			gst_message_parse_async_done(msg, &running_time);
			NXLOGI("%s() msg->src(%s) running_time(%" GST_TIME_FORMAT ")",
					__FUNCTION__, GST_OBJECT_NAME (msg->src), GST_TIME_ARGS (running_time));
			break;
		}
		case GST_MESSAGE_TAG:
		{
			GstTagList *received_tags = NULL;
			gst_message_parse_tag (msg, &received_tags);
			NXLOGD("%s() Got tags from element %s", __FUNCTION__, GST_OBJECT_NAME (msg->src));
			//gst_tag_list_foreach(received_tags, print_tag, NULL);
			gst_tag_list_unref (received_tags);
			break;
		}
        default:
        {
            GstMessageType type = GST_MESSAGE_TYPE(msg);
			// TODO: latency, stream-status, reset-time, async-done, new-clock, etc
            NXLOGV("%s() Received GST_MESSAGE_TYPE [%s]",
                   __FUNCTION__, gst_message_type_get_name(type));
            break;
        }
    }
    return TRUE;
}

void PrintMediaInfo(MP_HANDLE handle, const char *pUri)
{
	_CAutoLock lock(&handle->apiLock);

	FUNC_IN();

	NXLOGI("%s() [%s] container(%s), video mime-type(%s)"
		   ", audio mime-type(%s), seekable(%s), width(%d), height(%d)"
		   ", duration: (%" GST_TIME_FORMAT ")\r"
		   , __FUNCTION__
		   , pUri
		   , handle->gst_media_info.container_format
		   , handle->gst_media_info.video_mime_type
		   , handle->gst_media_info.audio_mime_type
		   , handle->gst_media_info.isSeekable ? "yes":"no"
		   , handle->gst_media_info.iWidth
		   , handle->gst_media_info.iHeight
		   , GST_TIME_ARGS (handle->gst_media_info.iDuration));

	FUNC_OUT();
}

NX_GST_RET set_demux_element(MP_HANDLE handle)
{
	FUNC_IN();

	if (!handle) {
		NXLOGE("%s() handle is NULL", __func__);
		return NX_GST_RET_ERROR;
	}

	//	Demux
	if ((g_strcmp0(handle->gst_media_info.container_format, "video/quicktime") == 0) ||	// Quicktime
		(g_strcmp0(handle->gst_media_info.container_format, "application/x-3gp") == 0))	// 3GP
	{
		handle->demuxer = gst_element_factory_make ("qtdemux", "demux");
	}
	else if (g_strcmp0(handle->gst_media_info.container_format, "video/x-matroska") == 0)	// Matroska
	{
		handle->demuxer = gst_element_factory_make ("matroskademux", "matroskademux");
	}
	else if (g_strcmp0(handle->gst_media_info.container_format, "video/x-msvideo") == 0)	// AVI
	{
		handle->demuxer = gst_element_factory_make ("avidemux", "avidemux");
	}
	else if (g_strcmp0(handle->gst_media_info.container_format, "video/mpeg") == 0)	// MPEG (vob)
	{
		handle->demuxer = gst_element_factory_make ("mpegpsdemux", "mpegpsdemux");
	}

	if (NULL == handle->demuxer)
	{
		NXLOGE("%s() Failed to create demuxer. Exiting", __FUNCTION__);
		return NX_GST_RET_ERROR;
	}

	FUNC_OUT();

	return NX_GST_RET_OK;
}

NX_GST_RET set_audio_elements(MP_HANDLE handle)
{
	FUNC_IN();

	if (!handle)
	{
		NXLOGE("%s() handle is NULL", __func__);
		return NX_GST_RET_ERROR;
	}

    handle->audio_queue = gst_element_factory_make ("queue2", "audio_queue");
	handle->decodebin = gst_element_factory_make ("decodebin", "decodebin");
    handle->audioconvert = gst_element_factory_make ("audioconvert", "audioconvert");
    handle->audioresample = gst_element_factory_make ("audioresample", "audioresample");
    handle->alsasink = gst_element_factory_make ("alsasink", "alsasink");

	if (!handle->audio_queue || 		!handle->decodebin || !handle->audioconvert ||
		!handle->audioresample || !handle->alsasink)
	{
		NXLOGE("%s() Failed to create audio elements", __func__);
		return NX_GST_RET_ERROR;
	}

	FUNC_OUT();

	return NX_GST_RET_OK;
}

NX_GST_RET set_video_elements(MP_HANDLE handle)
{
	FUNC_IN();

	if (!handle)
	{
		NXLOGE("%s() handle is NULL", __func__);
		return NX_GST_RET_ERROR;
	}

    handle->video_queue = gst_element_factory_make ("queue2", "video_queue");
	if (g_strcmp0(handle->gst_media_info.video_mime_type, "video/x-h264") == 0)
	{
		handle->video_parser = gst_element_factory_make ("h264parse", "parser");
		if (!handle->video_parser) {
			NXLOGE("%s() Failed to create h264parse element", __func__);
			return NX_GST_RET_ERROR;
		}
	}
	else if ((g_strcmp0(handle->gst_media_info.video_mime_type, "video/mpeg") == 0) &&
		     (handle->gst_media_info.video_mpegversion <= 2))
	{
		handle->video_parser = gst_element_factory_make ("mpegvideoparse", "parser");
		if (!handle->video_parser) {
			NXLOGE("%s() Failed to create mpegvideoparse element", __func__);
			return NX_GST_RET_ERROR;
		}
	}

	handle->nxvideodec = gst_element_factory_make ("nxvideodec", "nxvideodec");
	if (handle->isSecondaryDsp == DISPLAY_MODE_LCD_HDMI)
	{
		handle->tee = gst_element_factory_make ("tee", "tee");
		handle->tee_queue_primary = gst_element_factory_make ("queue", "tee_queue_primary");
		handle->tee_queue_secondary = gst_element_factory_make ("queue", "tee_queue_secondary");

		handle->nxvideosink_hdmi = gst_element_factory_make ("nxvideosink", "nxvideosink_hdmi");
		g_object_set(handle->nxvideosink_hdmi, "crtc-index", 1, NULL);	// SECONDARY
		g_object_set(handle->nxvideosink_hdmi, "layer-priority", 0, NULL);

		if(!handle->tee || !handle->tee_queue_primary || !handle->tee_queue_secondary || !handle->nxvideosink_hdmi)
		{
			NXLOGE("%s() Failed to create dual display elements", __func__);
			return NX_GST_RET_ERROR;
		}
	}
    
	handle->nxvideosink = gst_element_factory_make ("nxvideosink", "nxvideosink");
	if(!handle->video_queue || !handle->nxvideodec || !handle->nxvideosink)
    {
        NXLOGE("%s() Failed to create video elements", __func__);
        return NX_GST_RET_ERROR;
    }

	FUNC_OUT();

	return NX_GST_RET_OK;
}

NX_GST_RET set_source_element(MP_HANDLE handle)
{
	FUNC_IN();

	if (!handle)
	{
		NXLOGE("%s() handle is NULL", __FUNCTION__);
		return NX_GST_RET_ERROR;
	}

	handle->source = gst_element_factory_make ("filesrc", "source");
	if (NULL == handle->source)
	{
		NXLOGE("%s() Failed to create filesrc element", __func__);
		return NX_GST_RET_ERROR;
	}
	g_object_set(handle->source, "location", handle->uri, NULL);

	FUNC_OUT();

	return NX_GST_RET_OK;
}

void add_video_elements_to_bin(MP_HANDLE handle)
{
	gst_bin_add_many(GST_BIN(handle->pipeline), handle->video_queue,
					handle->nxvideodec, handle->nxvideosink, NULL);

	// tee, tee_queue_primary, tee_queue_secondary, nxvideosink_hdmi
	if (handle->isSecondaryDsp)
	{
		gst_bin_add_many(GST_BIN(handle->pipeline),
								handle->tee,
								handle->tee_queue_primary,
								handle->tee_queue_secondary,
								handle->nxvideosink_hdmi,
								NULL);
	}

	// video_parser
	if ((g_strcmp0(handle->gst_media_info.video_mime_type, "video/x-h264") == 0) ||
		 ((g_strcmp0(handle->gst_media_info.video_mime_type, "video/mpeg") == 0) && (handle->gst_media_info.video_mpegversion <= 2)) )
	{
		gst_bin_add(GST_BIN(handle->pipeline), handle->video_parser);
	}
}

void add_audio_elements_to_bin(MP_HANDLE handle)
{
	gst_bin_add_many(GST_BIN(handle->pipeline),
					handle->audio_queue, handle->decodebin,
					handle->audioconvert, handle->audioresample,
					handle->alsasink,
					NULL);
}

void add_subtitle_elements_to_bin(MP_HANDLE handle)
{
	gst_bin_add_many(GST_BIN(handle->pipeline),
					handle->subtitle_queue, handle->capsfilter, handle->fakesink,
					NULL);
}

NX_GST_RET add_elements_to_bin(MP_HANDLE handle)
{
	FUNC_IN();

	if (!handle)
	{
		NXLOGE("%s() handle is NULL", __func__);
		return NX_GST_RET_ERROR;
	}

	gst_bin_add_many(GST_BIN(handle->pipeline), handle->source, handle->demuxer, NULL);

	add_video_elements_to_bin(handle);
	add_audio_elements_to_bin(handle);
	if (hasSubTitles) {
		add_subtitle_elements_to_bin(handle);
	}

	FUNC_OUT();

	return NX_GST_RET_OK;
}

NX_GST_RET link_video_elements(MP_HANDLE handle)
{
	if((g_strcmp0(handle->gst_media_info.video_mime_type, "video/x-h264") == 0) ||
	    ((g_strcmp0(handle->gst_media_info.video_mime_type, "video/mpeg") == 0) && (handle->gst_media_info.video_mpegversion <= 2)) )
	{
		if (!gst_element_link_many(handle->video_queue, handle->video_parser, handle->nxvideodec, NULL)) {
			NXLOGE("%s() Failed to link video elements with video_parser", __func__);
			return NX_GST_RET_ERROR;
		}
	}
	else
	{
		if(!gst_element_link_many(handle->video_queue, handle->nxvideodec, NULL)) {
			NXLOGE("%s() Failed to link video elements", __FUNCTION__);
			return NX_GST_RET_ERROR;
		}
		else
		{
			NXLOGI("%s() Succeed to link video_queue<-->nxvideodec<-->nxvideosink", __FUNCTION__);
		}
	}

	if (handle->isSecondaryDsp)
	{
		if (!gst_element_link_many(handle->nxvideodec, handle->tee, NULL))
		{
			NXLOGE("%s() Failed to link nxvideodec<-->tee", __FUNCTION__);
		}

		if (!gst_element_link_many(handle->tee_queue_primary, handle->nxvideosink, NULL))
		{
			NXLOGE("%s() Failed to link tee_queue_primary<-->nxvideosink", __FUNCTION__);
		}

		if (!gst_element_link_many(handle->tee_queue_secondary, handle->nxvideosink_hdmi, NULL))
		{
			NXLOGE("%s() Failed to link tee_queue_secondary<-->nxvideosink_hdmi", __FUNCTION__);
		}

		// Manually link the Tee, which has "Request" pads
		handle->tee_primary_pad = gst_element_get_request_pad(handle->tee, "src_%u");
		NXLOGI("%s() Obtained request pad %s for the primary display",
				__FUNCTION__, gst_pad_get_name(handle->tee_primary_pad));
		handle->queue_primary_pad = gst_element_get_static_pad(handle->tee_queue_primary, "sink");

		handle->tee_secondary_pad = gst_element_get_request_pad(handle->tee, "src_%u");
		NXLOGI("%s() Obtained request pad %s for the secondary display",
				__FUNCTION__, gst_pad_get_name(handle->tee_secondary_pad));
		handle->queue_secondary_pad = gst_element_get_static_pad(handle->tee_queue_secondary, "sink");

		if (gst_pad_link(handle->tee_primary_pad, handle->queue_primary_pad) != GST_PAD_LINK_OK ||
			gst_pad_link(handle->tee_secondary_pad, handle->queue_secondary_pad) != GST_PAD_LINK_OK) {
			NXLOGE("%s() Tee could not be linked.", __FUNCTION__);
			//gst_object_unref (pipeline);
			return NX_GST_RET_ERROR;
		}
		gst_object_unref (handle->queue_primary_pad);
		gst_object_unref (handle->queue_secondary_pad);
	}
	else
	{
		if(!gst_element_link_many(handle->nxvideodec, handle->nxvideosink, NULL)) {
			NXLOGE("%s() Failed to link video elements", __FUNCTION__);
			return NX_GST_RET_ERROR;
		}
	}
	return NX_GST_RET_OK;
}

NX_GST_RET link_audio_elements(MP_HANDLE handle)
{
	if (!gst_element_link(handle->audio_queue, handle->decodebin))
	{
		NXLOGE("%s() Failed to link audio_queue<-->decodebin", __FUNCTION__);
		return NX_GST_RET_ERROR;
	}

    if (!gst_element_link_many(handle->audioconvert, handle->audioresample, handle->alsasink, NULL))
    {
        NXLOGE("%s() Failed to link audioconvert<-->audioresample<-->alsasink", __FUNCTION__);
        return NX_GST_RET_ERROR;
    }
	return NX_GST_RET_OK;
}

NX_GST_RET link_subtitle_elements(MP_HANDLE handle)
{
	if (!gst_element_link_many(handle->subtitle_queue, handle->capsfilter, handle->fakesink, NULL))
	{
		NXLOGE("%s() Failed to link subtitle_queue<-->fakesink", __FUNCTION__);
		return NX_GST_RET_ERROR;
	}
	else
	{
		NXLOGI("%s() Succeed to link subtitle_queue<-->fakesink", __FUNCTION__);
	}
	return NX_GST_RET_OK;
}

NX_GST_RET link_elements(MP_HANDLE handle)
{
	FUNC_IN();

	if(!handle)
	{
		NXLOGE("%s() handle is NULL", __FUNCTION__);
		return NX_GST_RET_ERROR;
	}

	if(!gst_element_link_many(handle->source, handle->demuxer, NULL))
    {
        NXLOGE("%s() Failed to link %s<-->%s", __FUNCTION__,
				gst_element_get_name(handle->source), gst_element_get_name(handle->demuxer));
        return NX_GST_RET_ERROR;
    }
	else
	{
        NXLOGI("%s() Succeed to link %s<-->%s", __FUNCTION__,
				gst_element_get_name(handle->source), gst_element_get_name(handle->demuxer));
	}

	link_video_elements(handle);
	link_audio_elements(handle);
	if (hasSubTitles)
	{
		link_subtitle_elements(handle);
	}

	FUNC_OUT();

	return NX_GST_RET_OK;
}

NX_GST_RET NX_GSTMP_SetDisplayMode(MP_HANDLE handle, DISPLAY_MODE in_mode)
{
	_CAutoLock lock(&handle->apiLock);

	FUNC_IN();

	if (NULL == handle)
	{
		NXLOGE("%s() handle is NULL", __FUNCTION__);
		return NX_GST_RET_ERROR;
	}

	handle->isSecondaryDsp = in_mode;
	NXLOGI("%s mode(%s)", __FUNCTION__,
			(in_mode == DISPLAY_MODE_LCD_HDMI)?"DISPLAY_MODE_LCD_HDMI":"DISPLAY_MODE_LCD_ONLY");
	return NX_GST_RET_OK;
}

NX_GST_RET NX_GSTMP_SetUri(MP_HANDLE handle, const char *pfilePath)
{
	_CAutoLock lock(&handle->apiLock);
	
	FUNC_IN();

	GstStateChangeReturn ret;

	GST_MEDIA_INFO *pGstMInfo = (GST_MEDIA_INFO*)g_malloc0(sizeof(GST_MEDIA_INFO));
	if (NULL == pGstMInfo)
	{
		NXLOGE("%s() Failed to alloc the memory to start discovering", __FUNCTION__);
		return NX_GST_RET_ERROR;
	}

	if(NULL == handle) {
		NXLOGE("%s() Failed to alloc memory for handle", __func__);
		return NX_GST_RET_ERROR;
	}
	handle->uri = g_strdup(pfilePath);
	handle->uri_type = URI_TYPE_FILE;
	handle->error = NX_GST_ERROR_NONE;

	NX_GST_ERROR err = StartDiscover(pfilePath, &pGstMInfo);
	if (NX_GST_ERROR_NONE != err)
	{
		handle->error = err;
        return NX_GST_RET_ERROR;
    }
	memcpy(&handle->gst_media_info, pGstMInfo, sizeof(GST_MEDIA_INFO));

	NXLOGD("%s() container(%s), video codec(%s)"
		   ", audio codec(%s), seekable(%s), width(%d), height(%d)"
		   ", duration: (%" GST_TIME_FORMAT ")\r"
		   , __FUNCTION__
		   , pGstMInfo->container_format
		   , pGstMInfo->video_mime_type
		   , pGstMInfo->audio_mime_type
		   , pGstMInfo->isSeekable ? "yes":"no"
		   , pGstMInfo->iWidth
		   , pGstMInfo->iHeight
		   , GST_TIME_ARGS (pGstMInfo->iDuration));

	if(handle->pipeline_is_linked) {
		NXLOGE("%s() pipeline is already linked", __func__);
		// TODO:
		return NX_GST_RET_OK;
	}

	handle->pipeline = gst_pipeline_new("NxGstMoviePlay");
	if (NULL == handle->pipeline)
	{
		NXLOGE("%s() pipeline is NULL", __func__);
		return NX_GST_RET_ERROR;
	}
	handle->bus = gst_pipeline_get_bus(GST_PIPELINE(handle->pipeline));
	handle->bus_watch_id = gst_bus_add_watch(handle->bus, (GstBusFunc)gst_bus_callback, handle);
	gst_object_unref(handle->bus);

	hasSubTitles = FALSE;
	if (pGstMInfo->n_subtitle > 0)
	{
		hasSubTitles = TRUE;
		set_subtitle_element(handle);
	}

	if (NX_GST_RET_ERROR == set_source_element(handle) ||
		NX_GST_RET_ERROR == set_demux_element(handle) ||
		NX_GST_RET_ERROR == set_audio_elements(handle) ||
		NX_GST_RET_ERROR == set_video_elements(handle))
	{
		NXLOGE("%s() Failed to set all elements", __FUNCTION__);
		return NX_GST_RET_ERROR;
	}

	if (NX_GST_RET_ERROR == add_elements_to_bin(handle) ||
		NX_GST_RET_ERROR == link_elements(handle))
	{
		NXLOGE("%s() Failed to add/link elements", __FUNCTION__);
		return NX_GST_RET_ERROR;
	}

	handle->rate = 1.0;

	// demuxer <--> audio_queue/video_queue
	g_signal_connect(handle->demuxer,	"pad-added", G_CALLBACK (on_pad_added_demux), handle);
	// decodbin <--> audio_converter
	g_signal_connect(handle->decodebin, "pad-added", G_CALLBACK (on_decodebin_pad_added_demux), handle);

	handle->pipeline_is_linked = TRUE;

	ret = gst_element_set_state(handle->pipeline, GST_STATE_READY);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        NXLOGE("%s() Failed to set the pipeline to the READY state", __FUNCTION__);
        return NX_GST_RET_ERROR;
    }
	
	start_loop_thread(handle);

	FUNC_OUT();

    return NX_GST_RET_OK;
}

NX_GST_RET NX_GSTMP_Open(MP_HANDLE *pHandle,
						  	   void (*cb)(void *owner, unsigned int eventType,
										unsigned int eventData, void * param),
           					   void *cbOwner)
{
	FUNC_IN();

	if(*pHandle)
	{
		// TODO:
		NXLOGE("%s() handle is not freed", __func__);
		return NX_GST_RET_ERROR;
	}

	MP_HANDLE handle = (MP_HANDLE)g_malloc0(sizeof(MOVIE_TYPE));
	if (NULL == handle)
	{
		NXLOGE("%s() Failed to alloc handle", __func__);
		return NX_GST_RET_ERROR;
	}

	pthread_mutex_init(&handle->apiLock, NULL);
	pthread_mutex_init(&handle->stateLock, NULL);

	handle->owner = cbOwner;
	handle->callback = cb;

	_CAutoLock lock(&handle->apiLock);

	if(!gst_is_initialized())
	{
		gst_init(NULL, NULL);
	}

	*pHandle = handle;

	FUNC_OUT();

	return NX_GST_RET_OK;
}

void NX_GSTMP_Close(MP_HANDLE handle)
{
	FUNC_IN();

	_CAutoLock lock(&handle->apiLock);

	if (NULL == handle)
	{
		NXLOGE("%s() handle is already NULL\n", __func__);
		return;
	}

	if(handle->pipeline_is_linked)
	{
		gst_element_set_state(handle->pipeline, GST_STATE_NULL);
		if (NULL != handle->pipeline)
		{
			gst_object_unref(handle->pipeline);
			handle->pipeline = NULL;
		}
		g_source_remove(handle->bus_watch_id);
		stop_my_thread(handle);
	}

	g_free(handle->uri);
	g_free(handle);

	pthread_mutex_destroy(&handle->apiLock);

	FUNC_OUT();
}

NX_GST_RET NX_GSTMP_GetMediaInfo(MP_HANDLE handle, GST_MEDIA_INFO *pGstMInfo)
{
	_CAutoLock lock(&handle->apiLock);

	FUNC_IN();

	if (NULL == pGstMInfo)
	{
		NXLOGE("%s pGstMInfo is NULL", __FUNCTION__);
		return NX_GST_RET_ERROR;
	}

	memcpy(pGstMInfo, &handle->gst_media_info, sizeof(GST_MEDIA_INFO));
 
	NXLOGI("%s() container(%s), video mime-type(%s)"
		   ", audio mime-type(%s), seekable(%s), width(%d), height(%d)"
		   ", duration: (%" GST_TIME_FORMAT ")\r"
		   , __FUNCTION__
		   , pGstMInfo->container_format
		   , pGstMInfo->video_mime_type
		   , pGstMInfo->audio_mime_type
		   , pGstMInfo->isSeekable ? "yes":"no"
		   , pGstMInfo->iWidth
		   , pGstMInfo->iHeight
		   , GST_TIME_ARGS (pGstMInfo->iDuration));

	FUNC_OUT();

    return NX_GST_RET_OK;
}

NX_GST_RET NX_GSTMP_SetDisplayInfo(MP_HANDLE handle, DISPLAY_TYPE type, int dspWidth, int dspHeight, DSP_RECT rect)
{
	_CAutoLock lock(&handle->apiLock);

	FUNC_IN();

	if (NULL == handle)
	{
		NXLOGE("%s() handle is NULL", __FUNCTION__);
		return NX_GST_RET_ERROR;
	}

	if (handle->gst_media_info.iWidth > dspWidth)
	{
		NXLOGE("%s() Not supported content(width:%d)",
				__FUNCTION__, handle->gst_media_info.iWidth);
		return NX_GST_RET_ERROR;
	}

	handle->gst_media_info.iX = rect.iX;
	handle->gst_media_info.iY = rect.iY;
	handle->gst_media_info.iWidth= rect.iWidth;
	handle->gst_media_info.iHeight = rect.iHeight;

	NXLOGD("%s() iX(%d), iY(%d), width(%d), height(%d), dspWidth(%d), dspHeight(%d)"
		   , __FUNCTION__
		   , rect.iX, rect.iY
		   , rect.iWidth, rect.iHeight
		   , dspWidth, dspHeight);

	if (type == DISPLAY_TYPE_SECONDARY)
	{
		g_object_set (G_OBJECT (handle->nxvideosink_hdmi), "dst-x", rect.iX, NULL);
		g_object_set (G_OBJECT (handle->nxvideosink_hdmi), "dst-y", rect.iY, NULL);
		g_object_set (G_OBJECT (handle->nxvideosink_hdmi), "dst-w", rect.iWidth, NULL);
		g_object_set (G_OBJECT (handle->nxvideosink_hdmi), "dst-h", rect.iHeight, NULL);
	}
	else
	{
		g_object_set (G_OBJECT (handle->nxvideosink), "dst-x", rect.iX, NULL);
		g_object_set (G_OBJECT (handle->nxvideosink), "dst-y", rect.iY, NULL);
		g_object_set (G_OBJECT (handle->nxvideosink), "dst-w", rect.iWidth, NULL);
		g_object_set (G_OBJECT (handle->nxvideosink), "dst-h", rect.iHeight, NULL);
	}

	FUNC_OUT();

    return NX_GST_RET_OK;
}

gint64 NX_GSTMP_GetPosition(MP_HANDLE handle)
{
	_CAutoLock lock(&handle->apiLock);

	GstState state, pending;
	GstStateChangeReturn ret;
	gint64 position;

	//FUNC_IN();

	if (NULL == handle)
		return -1;

	ret = gst_element_get_state(handle->pipeline, &state, &pending, 500000000);		//	wait 500 msec
	if(GST_STATE_CHANGE_FAILURE != ret)
	{
		if((GST_STATE_PLAYING == state) || (GST_STATE_PAUSED == state))
		{
			GstFormat format = GST_FORMAT_TIME;
			if(gst_element_query_position(handle->pipeline, format, &position))
			{
                //NXLOGV("%s() Position: %" GST_TIME_FORMAT "\r",
                //        __func__, GST_TIME_ARGS (position));
			}
		}
		else
		{
            NXLOGE("%s() Invalid state to query POSITION", __func__);
			return -1;
		}
	}
	else
	{
        NXLOGE("%s() Failed to query POSITION", __func__);
		return -1;
	}

	//FUNC_OUT();

	return position;
}

gint64 NX_GSTMP_GetDuration(MP_HANDLE handle)
{
	_CAutoLock lock(&handle->apiLock);

	GstState state, pending;
	GstStateChangeReturn ret;
	gint64 duration;

	//FUNC_IN();

	if(!handle || !handle->pipeline_is_linked)
	{
		NXLOGE("%s() : invalid state or invalid operation.(%p,%d)\n",
                __FUNCTION__, handle, handle->pipeline_is_linked);
		return -1;
	}

	ret = gst_element_get_state( handle->pipeline, &state, &pending, 500000000);		//	wait 500 msec
	if(GST_STATE_CHANGE_FAILURE != ret)
	{
		if(state == GST_STATE_PLAYING || state == GST_STATE_PAUSED || state == GST_STATE_READY)
		{
			GstFormat format = GST_FORMAT_TIME;
			if(gst_element_query_duration(handle->pipeline, format, &duration))
			{
				//NXLOGV("%s() Duration: %" GST_TIME_FORMAT "\r",
                //        __func__, GST_TIME_ARGS (duration));
			}
		}
		else
		{
			NXLOGE("%s() Invalid state to query DURATION", __func__);
			return -1;
		}
	}
	else
	{
		NXLOGE("%s() Failed to query DURATION", __func__);
		return -1;
	}

	//FUNC_OUT();

	return duration;
}

NX_GST_RET NX_GSTMP_SetVolume(MP_HANDLE handle, int volume)
{
	_CAutoLock lock(&handle->apiLock);

	FUNC_IN();

	if( !handle || !handle->alsasink || !handle->volume)
		return NX_GST_RET_ERROR;

    gdouble vol = (double)volume/100.;
    NXLOGI("%s() set volume to %f", __func__, vol);
    g_object_set (G_OBJECT (handle->volume), "volume", vol, NULL);

	FUNC_OUT();

    return NX_GST_RET_OK;
}

static gboolean send_step_event (MP_HANDLE handle)
{
	gboolean ret;

	if (handle->nxvideosink == NULL) {
		/* If we have not done so, obtain the sink through which we will send the step events */
		g_object_get (handle->pipeline, "video-sink", &handle->nxvideosink, NULL);
	}

	ret = gst_element_send_event (handle->nxvideosink,
					gst_event_new_step (GST_FORMAT_BUFFERS, 1, ABS (handle->rate), TRUE, FALSE));

	NXLOGI("%s() Stepping one frame", __FUNCTION__);

	return ret;
}

static int send_seek_event (MP_HANDLE handle)
{
	gint64 position;
	GstEvent *seek_event;
	GstFormat format = GST_FORMAT_TIME;
	GstSeekFlags flags = (GstSeekFlags)(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_SNAP_AFTER | GST_SEEK_FLAG_KEY_UNIT);
	//GstSeekFlags flags = (GstSeekFlags)(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE);

	/* Obtain the current position, needed for the seek event */
	if (!gst_element_query_position (handle->pipeline, format, &position)) {
		NXLOGE("%s() Unable to retrieve current position", __FUNCTION__);
		return -1;
	}

	/* Create the seek event */
	if (handle->rate > 0) {
		seek_event =
			gst_event_new_seek (handle->rate, format, flags,
								GST_SEEK_TYPE_SET, position,
								GST_SEEK_TYPE_END, 0);
	} else {
		seek_event =
			gst_event_new_seek (handle->rate, format, flags,
								GST_SEEK_TYPE_SET, 0,
								GST_SEEK_TYPE_SET, position);
	}

	if (handle->nxvideosink == NULL) {
		/* If we have not done so, obtain the sink through which we will send the seek events */
		g_object_get (handle->pipeline, "video-sink", &handle->nxvideosink, NULL);
	}

	/* Send the event */
	gst_element_send_event (handle->nxvideosink, seek_event);

	NXLOGI("%s() Current rate: %g\n", __FUNCTION__, handle->rate);
	return 0;
}

static NX_GST_RET seek_to_time (MP_HANDLE handle, gint64 time_nanoseconds)
{
	GstFormat format = GST_FORMAT_TIME;

    if(!gst_element_seek (handle->pipeline, handle->rate, format,
						  GST_SEEK_FLAG_FLUSH,		/* gdouble rate */
                          GST_SEEK_TYPE_SET,		/* GstSeekType start_type */
						  time_nanoseconds,			/* gint64 start */
                          GST_SEEK_TYPE_NONE,		/* GstSeekType stop_type */
						  GST_CLOCK_TIME_NONE))		/* gint64 stop */
	{
        NXLOGE("%s() Failed to seek %lld!", __func__, time_nanoseconds);
        return NX_GST_RET_ERROR;
    }
	/* And wait for this seek to complete */
	gst_element_get_state (handle->pipeline, NULL, NULL, GST_CLOCK_TIME_NONE);

    return NX_GST_RET_OK;
}

NX_GST_RET NX_GSTMP_Seek(MP_HANDLE handle, gint64 seekTime)
{
	_CAutoLock lock(&handle->apiLock);

	FUNC_IN();

	NX_GST_RET ret = NX_GST_RET_ERROR;
	
	if(!handle || !handle->pipeline_is_linked)
	{
		NXLOGE("%s() : invalid state or invalid operation.(%p,%d)\n",
                __func__, handle, handle->pipeline_is_linked);
		return ret;
	}

	GstState state, pending;
	if(GST_STATE_CHANGE_FAILURE != gst_element_get_state(handle->pipeline, &state, &pending, 500000000))	
	{
		if(state == GST_STATE_PLAYING || state == GST_STATE_PAUSED || state == GST_STATE_READY)
		{
			NXLOGI("%s() state(%s) with the rate %f", __FUNCTION__, gst_element_state_get_name (state), handle->rate);
            ret = seek_to_time(handle, seekTime*(1000*1000)); /*mSec to NanoSec*/
		}
		else
		{
			NXLOGE("%s() Invalid state to seek", __func__);
			ret = NX_GST_RET_ERROR;
		}
	}
	else
	{
		NXLOGE("%s() Failed to seek", __func__);
		ret = NX_GST_RET_ERROR;
	}

	FUNC_OUT();

	return ret;
}

NX_GST_RET NX_GSTMP_Play(MP_HANDLE handle)
{
	_CAutoLock lock(&handle->apiLock);

	GstStateChangeReturn ret;

	FUNC_IN();

	if(!handle || !handle->pipeline_is_linked)
	{
		NXLOGE("%s() invalid state or invalid operation.(%p,%d)\n",
                __FUNCTION__, handle, handle->pipeline_is_linked);
		return NX_GST_RET_ERROR;
	}

	GstState state, pending;
	if(GST_STATE_CHANGE_FAILURE != gst_element_get_state(handle->pipeline, &state, &pending, 500000000))
	{
		NXLOGI("%s() The previous state '%s' with (x%d)", __FUNCTION__, gst_element_state_get_name (state), int(handle->rate));
		if(GST_STATE_PLAYING == state)
		{
			if (0 > send_seek_event(handle))
			{
				NXLOGE("%s() Failed to send seek event", __FUNCTION__);
				return NX_GST_RET_ERROR;
			}
			//send_step_event(handle);
		}
		else
		{
			ret = gst_element_set_state(handle->pipeline, GST_STATE_PLAYING);
			NXLOGI("%s() set_state(PLAYING) ==> ret(%s)", __FUNCTION__, get_gst_state_change_ret(ret));
			if(GST_STATE_CHANGE_FAILURE == ret)
			{
				NXLOGE("%s() Failed to set the pipeline to the PLAYING state(ret=%d)", __func__, ret);
				return NX_GST_RET_ERROR;
			}
		}
	}
	else {
		NXLOGE("%s() Failed to get state", __FUNCTION__);
		return NX_GST_RET_ERROR;
	}

	FUNC_OUT();

	return	NX_GST_RET_OK;
}

NX_GST_RET NX_GSTMP_Pause(MP_HANDLE handle)
{
	_CAutoLock lock(&handle->apiLock);

	FUNC_IN();

	GstStateChangeReturn ret;

	if(!handle || !handle->pipeline_is_linked)
	{
		NXLOGE("%s() invalid state or invalid operation.(%p,%d)\n",
                __FUNCTION__, handle, handle->pipeline_is_linked);
		return NX_GST_RET_ERROR;
	}

	ret = gst_element_set_state (handle->pipeline, GST_STATE_PAUSED);
	if(GST_STATE_CHANGE_FAILURE == ret)
    {
		NXLOGE("%s() Failed to set the pipeline to the PAUSED state(ret=%d)", __func__, ret);
		return NX_GST_RET_ERROR;
	}

	FUNC_OUT();

	return	NX_GST_RET_OK;
}

NX_GST_RET NX_GSTMP_Stop(MP_HANDLE handle)
{
	_CAutoLock lock(&handle->apiLock);

	FUNC_IN();

	GstStateChangeReturn ret;

	if(!handle || !handle->pipeline_is_linked)
	{
        NXLOGE("%s() invalid state or invalid operation.(%p,%d)\n",
                __FUNCTION__, handle, handle->pipeline_is_linked);
		return NX_GST_RET_ERROR;
	}

	handle->rate = 1.0;
	ret = gst_element_set_state (handle->pipeline, GST_STATE_NULL);
	NXLOGI("%s() set_state(NULL) ret(%s)", __FUNCTION__, get_gst_state_change_ret(ret));
	if(GST_STATE_CHANGE_FAILURE == ret)
    {
		NXLOGE("%s() Failed to set the pipeline to the NULL state(ret=%d)", __FUNCTION__, ret);
		return NX_GST_RET_ERROR;
	}

	FUNC_OUT();

	return	NX_GST_RET_OK;
}

NX_MEDIA_STATE NX_GSTMP_GetState(MP_HANDLE handle)
{
	_CAutoLock lock(&handle->apiLock);

	if (!handle || !handle->pipeline_is_linked)
	{
        NXLOGE("%s() invalid state or invalid operation.(%p,%d)\n",
                __FUNCTION__, handle, handle->pipeline_is_linked);
		return MP_STATE_STOPPED;
	}

	//FUNC_IN();

	GstState state, pending;
	NX_MEDIA_STATE nx_state = MP_STATE_STOPPED;
	GstStateChangeReturn ret;
	ret = gst_element_get_state(handle->pipeline, &state, &pending, 500000000);		//	wait 500 msec
	//NXLOGI("%s() ret(%s) state(%s), pending(%s)",
	//	   __FUNCTION__, get_gst_state_change_ret(ret), get_gst_state(state), get_gst_state(pending));
	if (GST_STATE_CHANGE_SUCCESS == ret || GST_STATE_CHANGE_NO_PREROLL == ret)
	{
		nx_state = GstState2NxState(state);
	}
	else if (GST_STATE_CHANGE_ASYNC == ret)
	{
		nx_state = GstState2NxState(pending);
	}
	else
	{
        NXLOGE("%s() Failed to get state", __func__);
		nx_state = MP_STATE_STOPPED;
	}

	//NXLOGI("%s() nx_state(%s)", __FUNCTION__, get_nx_media_state(nx_state));

	//FUNC_OUT();
	return nx_state;
}

NX_GST_RET NX_GSTMP_VideoMute(MP_HANDLE handle, int32_t bOnoff)
{
	FUNC_IN();

	if(!handle || !handle->pipeline_is_linked)
	{
        NXLOGE("%s() invalid state or invalid operation.(%p,%d)\n",
                __FUNCTION__, handle, handle->pipeline_is_linked);
		return NX_GST_RET_ERROR;
	}

	NXLOGI("%s bOnoff(%s) dst-y(%d)", __FUNCTION__
		   , bOnoff ? "Enable video mute":"Disable video mute"
		   , bOnoff ? (handle->gst_media_info.iHeight):(handle->gst_media_info.iY));

	_CAutoLock lock(&handle->apiLock);

	if (bOnoff)
	{
		g_object_set (G_OBJECT (handle->nxvideosink), "dst-y", handle->gst_media_info.iHeight, NULL);
	}
	else
	{
		g_object_set (G_OBJECT (handle->nxvideosink), "dst-y", handle->gst_media_info.iY, NULL);
	}
	return	NX_GST_RET_OK;

	FUNC_OUT();
}

gdouble NX_GSTMP_GetVideoSpeed(MP_HANDLE handle)
{
	gdouble speed = 1.0;

	if(!handle || !handle->pipeline_is_linked)
	{
        NXLOGE("%s() Return the default video speed since it's not ready", __FUNCTION__);
		return speed;
	}

	NXLOGI("%s() rate: %d", __FUNCTION__, (int)handle->rate);
	if ((int)handle->rate == 0)
		speed = 1.0;
	else
		speed = handle->rate;

	NXLOGI("%s() current playback speed (%f)", __FUNCTION__, speed);
	return speed;
}

/* It's available in PAUSED or PLAYING state */
NX_GST_RET NX_GSTMP_SetVideoSpeed(MP_HANDLE handle, gdouble speed)
{
	FUNC_IN();

	if(!handle || !handle->pipeline_is_linked)
	{
        NXLOGE("%s() invalid state or invalid operation.(%p,%d)\n",
                __FUNCTION__, handle, handle->pipeline_is_linked);
		return NX_GST_RET_ERROR;
	}

	if(false == handle->gst_media_info.isSeekable)
	{
		NXLOGE("%s This video doesn't support 'seekable'", __FUNCTION__);
		return NX_GST_RET_ERROR;
	}

	handle->rate = speed;
	return NX_GST_RET_OK;
}

gboolean NX_GSTMP_GetVideoSpeedSupport(MP_HANDLE handle)
{
	if(!handle || !handle->pipeline_is_linked)
	{
        NXLOGE("%s() invalid state or invalid operation.(%p,%d)\n",
                __FUNCTION__, handle, handle->pipeline_is_linked);
		return NX_GST_RET_ERROR;
	}

	return handle->gst_media_info.isSeekable;
}

const char* NX_GSTMP_GetThumbnail(const gchar *uri, gint64 pos_msec, gint width)
{
	return makeThumbnail(uri, pos_msec, width);
}

NX_MEDIA_STATE GstState2NxState(GstState state)
{
	switch(state) {
		case GST_STATE_VOID_PENDING:
			return MP_STATE_VOID_PENDING;
		case GST_STATE_NULL:
			return MP_STATE_STOPPED;
		case GST_STATE_READY:
			return MP_STATE_READY;
		case GST_STATE_PAUSED:
			return MP_STATE_PAUSED;
		case GST_STATE_PLAYING:
			return MP_STATE_PLAYING;
		default:
			break;
	}
	NXLOGE("%s() No matched state", __FUNCTION__);
	return MP_STATE_STOPPED;
}

const char* get_gst_state(GstState gstState)
{
	switch(gstState) {
		case GST_STATE_VOID_PENDING:
			return "GST_STATE_VOID_PENDING";
		case GST_STATE_NULL:
			return "GST_STATE_NULL";
		case GST_STATE_READY:
			return "GST_STATE_READY";
		case GST_STATE_PAUSED:
			return "GST_STATE_PAUSED";
		case GST_STATE_PLAYING:
			return "GST_STATE_PLAYING";
		default:
			break;
	}
	NXLOGE("%s() No matched state", __FUNCTION__);
	return NULL;
}

const char* get_gst_state_change_ret(GstStateChangeReturn gstStateChangeRet)
{
	switch(gstStateChangeRet) {
		case GST_STATE_CHANGE_FAILURE:
			return "GST_STATE_CHANGE_FAILURE";
		case GST_STATE_CHANGE_SUCCESS:
			return "GST_STATE_CHANGE_SUCCESS";
		case GST_STATE_CHANGE_ASYNC:
			return "GST_STATE_CHANGE_ASYNC";
		case GST_STATE_CHANGE_NO_PREROLL:
			return "GST_STATE_CHANGE_NO_PREROLL";
		default:
			break;
	}
	return NULL;
}
