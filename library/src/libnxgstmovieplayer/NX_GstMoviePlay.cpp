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
//	Module		: libnxgstvplayer.so
//	File		:
//	Description	:
//	Author		:
//	Export		:
//	History		:
//
//------------------------------------------------------------------------------

#include <gst/gst.h>
#include <string.h>

#include "NX_GstIface.h"
#include "GstDiscover.h"
#include "NX_GstThumbnail.h"

#include "NX_GstLog.h"
#define LOG_TAG "[NxGstVPLAYER]"

#define NX_GST_VTHREAD   "NxGstVThread"

//------------------------------------------------------------------------------
// Function Prototype
const char* get_gst_state_change_ret(GstStateChangeReturn gstStateChangeRet);
enum NX_MEDIA_STATE GstState2NxState(GstState state);
const char* get_display_mode_str(enum DISPLAY_MODE mode);

void add_video_elements_to_bin(MP_HANDLE handle);
void add_audio_elements_to_bin(MP_HANDLE handle);
void add_subtitle_elements_to_bin(MP_HANDLE handle);
NX_GST_RET add_elements_to_bin(MP_HANDLE handle);

NX_GST_RET link_video_elements(MP_HANDLE handle);
NX_GST_RET link_audio_elements(MP_HANDLE handle);
NX_GST_RET link_subtitle_elements(MP_HANDLE handle);
NX_GST_RET link_elements(MP_HANDLE handle);

NX_GST_RET link_display(MP_HANDLE handle, enum DISPLAY_TYPE type);
void unlink_display(enum DISPLAY_TYPE type);

enum NX_MEDIA_STATE GstState2NxState(GstState state);
static gpointer loop_func(gpointer data);
static void start_loop_thread(MP_HANDLE handle);
static void stop_my_thread(MP_HANDLE handle);
static gboolean gst_bus_callback(GstBus *bus, GstMessage *msg, MP_HANDLE handle);
static NX_GST_RET seek_to_time (MP_HANDLE handle, gint64 time_nanoseconds);

static const char* get_nx_gst_error(NX_GST_ERROR error);
//------------------------------------------------------------------------------

// For video sinks
static GList *primary_sinks;
static GList *secondary_sinks;

// tee_pad <--> queue <--> nxvideosink_hdmi
struct Sink
{
    GstPad              *tee_pad;
    GstElement          *queue;
    GstElement          *nxvideosink;
    gboolean            removing;
    GstElement          *pipeline;
    GstElement          *tee;
};

struct MOVIE_TYPE {
    GstElement  *pipeline;

    GstElement  *source;

    // For Video
    GstElement  *video_queue;
    GstElement  *demuxer;
    GstElement  *video_parser;
    GstElement  *video_decoder;
    GstElement  *nxvideosink;

    GstElement  *tee;
    GstElement  *tee_queue_primary;
    GstPad      *tee_primary_pad;

    // For Audio
    GstElement  *audio_queue;
    GstElement  *audio_parser;
    GstElement  *audio_decoder;
    GstElement  *audioconvert;
    GstElement  *audioresample;
    GstElement  *alsasink;

    // For Subtitle
    GstElement  *subtitle_queue;
    GstElement  *fakesink;
    GstElement  *capsfilter;

    GstState    state;

    GstElement  *volume;
    // Current playback rate
    gdouble     rate;

    GMainLoop *loop;
    GThread *thread;
    GstBus *bus;
    guint bus_watch_id;

    pthread_mutex_t apiLock;
    pthread_mutex_t stateLock;

    gboolean pipeline_is_linked;

    //	Media file path
    gchar *filePath;

    struct GST_MEDIA_INFO gst_media_info;

    // For Video Mode (LCD/HDMI)
    enum DISPLAY_MODE display_mode;

    // For aspect ratio
    struct DSP_RECT	primary_dsp_rect;
    struct DSP_RECT	secondary_dsp_rect;

    enum NX_GST_ERROR error;

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

    handle->loop = g_main_loop_new(NULL, FALSE);
    handle->thread = g_thread_new(NX_GST_VTHREAD, loop_func, handle);

    FUNC_OUT();
}

static void stop_my_thread(MP_HANDLE handle)
{
    FUNC_IN();

    if (NULL != handle->loop)
        g_main_loop_quit(handle->loop);

    if (NULL != handle->thread)
    {
        g_thread_join(handle->thread);
    }

    if (NULL != handle->loop)
    {
        g_main_loop_unref(handle->loop);
    }

    FUNC_OUT();
}

static void on_decodebin_pad_added(GstElement *element,
                            GstPad *pad, gpointer data)
{
    FUNC_IN();
    
    GstPad *sinkpad = NULL;
    GstCaps *caps = NULL;
    GstStructure *new_pad_structure = NULL;
    const gchar *mime_type = NULL;
    MP_HANDLE handle = (MP_HANDLE)data;

    GstElement *sink_pad_audio = handle->audioconvert;

    caps = gst_pad_get_current_caps(pad);
    if (caps == NULL)
    {
        NXGLOGE("Failed to get current caps");
        return;
    }

    new_pad_structure = gst_caps_get_structure(caps, 0);
    if (NULL == new_pad_structure)
    {
        NXGLOGE("Failed to get current caps");
        return;
    }

    mime_type = gst_structure_get_name(new_pad_structure);
    NXGLOGI("MIME-type:%s", mime_type);
    if (g_str_has_prefix(mime_type, "audio/"))
    {
        sinkpad = gst_element_get_static_pad (sink_pad_audio, "sink");
        if (NULL == sinkpad)
        {
            NXGLOGE("Failed to get static pad");
            gst_caps_unref(caps);
            return;
        }

        if (GST_PAD_LINK_FAILED(gst_pad_link(pad, sinkpad)))
        {
            NXGLOGE("Failed to link %s:%s to %s:%s",
                    GST_DEBUG_PAD_NAME(pad),
                    GST_DEBUG_PAD_NAME(sinkpad));
        }

        NXGLOGI("Succeed to link %s:%s to %s:%s",
                GST_DEBUG_PAD_NAME(pad),
                GST_DEBUG_PAD_NAME(sinkpad));

        gst_object_unref(sinkpad);
    }

    gst_caps_unref (caps);

    FUNC_OUT();
}

struct SUBTITLE_INFO* setSubtitleInfo(GstClockTime startTime, GstClockTime endTime,
                                GstClockTime duration, const char* subtitle)
{
    struct SUBTITLE_INFO* m_pSubtitleInfo = (struct SUBTITLE_INFO*) g_malloc0(sizeof(struct SUBTITLE_INFO));

    m_pSubtitleInfo->startTime = (gint64) startTime;
    m_pSubtitleInfo->endTime = (gint64) endTime;
    m_pSubtitleInfo->duration = (gint64) duration;
    m_pSubtitleInfo->subtitleText = g_strdup(subtitle);

    NXGLOGI("subtitle:%s startTime:%" GST_TIME_FORMAT
            ", duration: %" GST_TIME_FORMAT ", endTime: %" GST_TIME_FORMAT,
            subtitle, GST_TIME_ARGS(startTime),
            GST_TIME_ARGS(duration), GST_TIME_ARGS(endTime));

    return m_pSubtitleInfo;
}

void on_handoff(GstElement* object, GstBuffer* buffer,
                GstPad* pad, gpointer user_data)
{
    char text_msg[128];
    MP_HANDLE handle = (MP_HANDLE) user_data;

    gsize buffer_size, extracted_size;
    memset(text_msg, 0, sizeof(text_msg));

    buffer_size = gst_buffer_get_size(buffer);

    extracted_size = gst_buffer_extract(buffer, 0, text_msg, buffer_size);
    NXGLOGV("Buffer Size is %zu, read %zu, data: %s",
            buffer_size, extracted_size, text_msg);

    static GstClockTime startTime, duration, endTime;
    startTime = GST_BUFFER_PTS(buffer);
    duration = GST_BUFFER_DURATION(buffer);
    endTime = startTime + duration;
    NXGLOGV("startTime:%" GST_TIME_FORMAT ", duration: %" GST_TIME_FORMAT
             ", endTime: %" GST_TIME_FORMAT, GST_TIME_ARGS(startTime),
             GST_TIME_ARGS(duration), GST_TIME_ARGS(endTime));

    struct SUBTITLE_INFO* subtitleInfo = setSubtitleInfo(startTime, endTime, duration, text_msg);

    handle = (MP_HANDLE)user_data;
    if (handle) {
        handle->callback(NULL, (int)MP_EVENT_SUBTITLE_UPDATED, 0, subtitleInfo);
    }

    gst_buffer_unref (buffer);
}

NX_GST_RET set_subtitle_element(MP_HANDLE handle)
{
    FUNC_IN();

    if (!handle)
    {
        NXGLOGE("handle is NULL");
        return NX_GST_RET_ERROR;
    }

    handle->subtitle_queue = gst_element_factory_make("queue2", "subtitle_queue");

    handle->capsfilter = gst_element_factory_make("capsfilter", "capsfilter");
    GstCaps* cap = gst_caps_new_simple("text/x-raw", NULL, NULL);
    g_object_set(G_OBJECT(handle->capsfilter), "caps", cap, NULL);

    handle->fakesink = gst_element_factory_make ("fakesink", "fakesink");

    if (!handle->capsfilter || !handle->subtitle_queue || !handle->fakesink)
    {
        NXGLOGE("Failed to create subtitle elements");
        return NX_GST_RET_ERROR;
    }
    gst_caps_unref(cap);

    g_object_set(handle->fakesink, "signal-handoffs", TRUE, NULL);
    g_object_set(handle->fakesink, "sync", TRUE, NULL);

    g_signal_connect(handle->fakesink, "handoff", G_CALLBACK(on_handoff), handle);

    return NX_GST_RET_OK;
}

#ifdef TEST
static GstPadProbeReturn cb_have_data (GstPad          *pad,
                                        GstPadProbeInfo *info,
                                        gpointer         user_data)
{
    GstBuffer *buffer;
    char text_msg[128];
    MP_HANDLE handle = NULL;

    gint buffer_size, read_size;
    memset(text_msg, 0, sizeof(text_msg));
    buffer = GST_PAD_PROBE_INFO_BUFFER(info);
    buffer_size = gst_buffer_get_size(buffer);

    read_size = gst_buffer_extract(buffer, 0, text_msg, buffer_size);
    NXGLOGI("Buffer Size is %d, read %d, data: %s",
            buffer_size, read_size, text_msg);

    handle = (MP_HANDLE)user_data;
    if (handle)
    {
        handle->callback(NULL, (int)MP_EVENT_SUBTITLE_UPDATED, 0, 0);
    }

    return GST_PAD_PROBE_OK;
}
#endif

static void on_pad_added_demux(GstElement *element, 
                        GstPad *pad, gpointer data)
{
    GstPad *sinkpad = NULL;
    GstCaps *caps = NULL;
    GstStructure *structure;
    const gchar *mime_type;
    GstElement *target_sink_pad = NULL;
    MP_HANDLE handle = NULL;
    gboolean isLinkFailed = FALSE;
    gchar* padName = NULL;

    handle = (MP_HANDLE)data;
    padName = gst_pad_get_name(pad);

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

    mime_type = gst_structure_get_name(structure);
    NXGLOGI("padName(%s) mime_type(%s)", padName, mime_type);

    // Get sinkpad of queue for video/audio/subtitle
    if (g_str_has_prefix(mime_type, "video/"))
    {
        target_sink_pad = handle->video_queue;
        sinkpad = gst_element_get_static_pad(target_sink_pad, "sink");
    }
    else if (g_str_has_prefix(mime_type, "audio/"))
    {
        target_sink_pad = handle->audio_queue;
        sinkpad = gst_element_get_static_pad(target_sink_pad, "sink");
    }
    else if ((handle->gst_media_info.StreamInfo[0].n_subtitle >= 1) &&
            g_str_has_prefix(padName, "subtitle_0"))
    {
        if (handle->gst_media_info.StreamInfo[0].SubtitleInfo[0].type == SUBTITLE_TYPE_RAW)
        {
            target_sink_pad = handle->subtitle_queue;
            sinkpad = gst_element_get_static_pad(target_sink_pad, "sink");
        }
        else
        {
            NXGLOGE("Unsupported subtitle codec type %d",
                    handle->gst_media_info.StreamInfo[0].SubtitleInfo[0].type);
            if (padName)	g_free(padName);
            gst_caps_unref (caps);
            return;
        }
    }
    else
    {
        NXGLOGE("There is no available link for %s", padName);
        if (padName)	g_free(padName);
        gst_caps_unref (caps);
        return;
    }

    NXGLOGI("target_sink_pad:%s",
            (NULL != target_sink_pad) ? GST_OBJECT_NAME(target_sink_pad):"");

    // Link pads [demuxer <--> video_queue/audio_queue/subtitle_queue]
    if (g_str_has_prefix(mime_type, "video/") ||
        g_str_has_prefix(mime_type, "audio/") ||
        ((handle->gst_media_info.StreamInfo[0].n_subtitle >= 1) &&
            g_str_has_prefix(padName, "subtitle_0")))
    {
        if (NULL == sinkpad)
        {
            NXGLOGE("Failed to get static pad");
            gst_caps_unref (caps);
            return;
        }

        if (GST_PAD_LINK_FAILED(gst_pad_link(pad, sinkpad)))
        {
            NXGLOGE("Failed to link %s:%s to %s:%s",
                    GST_DEBUG_PAD_NAME(pad),
                    GST_DEBUG_PAD_NAME(sinkpad));
            isLinkFailed = TRUE;
        }
        else
        {
            NXGLOGI("Succeed to create dynamic pad link %s:%s to %s:%s",
                    GST_DEBUG_PAD_NAME(pad),
                    GST_DEBUG_PAD_NAME(sinkpad));
        }
        gst_object_unref (sinkpad);
    }

#ifdef TEST
    if ((handle->gst_media_info.StreamInfo[0].n_subtitle >= 1) && g_str_has_prefix(padName, "subtitle"))
    {
        NXGLOGI("Add probe to pad");
        gst_pad_add_probe(pad, GST_PAD_PROBE_TYPE_BUFFER,
                            (GstPadProbeCallback) cb_have_data,
                            handle, NULL);
    }
#endif

    if (padName)	g_free(padName);
    gst_caps_unref (caps);

    /* Need to check subtitle_1, 2, etc */
    if (TRUE == isLinkFailed)
    {
        handle->callback(NULL, (int)MP_EVENT_DEMUX_LINK_FAILED, 0, NULL);
        return;
    }

    if (g_str_has_prefix(mime_type, "video/"))
    {
        NXGLOGI("display_mode(%d)", handle->display_mode);
        if (handle->display_mode == DISPLAY_MODE_LCD_ONLY)
        {
            link_display(handle, DISPLAY_TYPE_PRIMARY);
        }
        else if (handle->display_mode == DISPLAY_MODE_HDMI_ONLY)
        {
            link_display(handle, DISPLAY_TYPE_SECONDARY);
        }
        else if (handle->display_mode == DISPLAY_MODE_LCD_HDMI)
        {
            link_display(handle, DISPLAY_TYPE_PRIMARY);
            link_display(handle, DISPLAY_TYPE_SECONDARY);
        }
        else
        {
            NXGLOGE("Failed to link display");
        }
    }

    FUNC_OUT();
}

static void print_tag(const GstTagList *list, const gchar *tag, gpointer unused)
{
    gint i, count;

    count = gst_tag_list_get_tag_size (list, tag);

    for (i = 0; i < count; i++)
    {
        gchar *str;

        if (gst_tag_get_type (tag) == G_TYPE_STRING)
        {
            g_assert (gst_tag_list_get_string_index (list, tag, i, &str));
        }
        else
        {
            str = g_strdup_value_contents (gst_tag_list_get_value_index (list, tag, i));
        }

        if (i == 0)
        {
            NXGLOGV("%15s: %s", gst_tag_get_nick (tag), str);
        } else {
            NXGLOGV("               : %s", str);
        }

        g_free (str);
    }
}

static gboolean gst_bus_callback(GstBus *bus, GstMessage *msg, MP_HANDLE handle)
{
    switch (GST_MESSAGE_TYPE (msg))
    {
        case GST_MESSAGE_EOS:
            NXGLOGI("End-of-stream");
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

            NXGLOGE("Gstreamer %s: %s",
                   (msg->type == GST_MESSAGE_WARNING)?"warning":"error", err->message);
            g_error_free (err);

            NXGLOGI("Debug details: %s", debug);
            g_free (debug);

            handle->callback(NULL, (int)MP_EVENT_GST_ERROR, 0, 0);
            break;
        }
        case GST_MESSAGE_STATE_CHANGED:
        {
            GstState old_state, new_state;

            gst_message_parse_state_changed (msg, &old_state, &new_state, NULL);
            // TODO: workaround
            //if(g_strcmp0("NxGstMoviePlay", GST_OBJECT_NAME (msg->src)) == 0) {
                NXGLOGI("Element '%s' changed state from  '%s' to '%s'"
                       , GST_OBJECT_NAME (msg->src)
                       , gst_element_state_get_name (old_state)
                       , gst_element_state_get_name (new_state));
                //	Send Message
                if(g_strcmp0("NxGstMoviePlay", GST_OBJECT_NAME (msg->src)) == 0)
                {
                    handle->state = new_state;
                handle->callback(NULL, (int)MP_EVENT_STATE_CHANGED, (int)GstState2NxState(new_state), NULL);
                }
            //}
            break;
        }
        case GST_MESSAGE_DURATION_CHANGED:
        {
            NXGLOGI("TODO:%s", gst_message_type_get_name(GST_MESSAGE_TYPE(msg)));
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
            NXGLOGI("msg->src(%s) running_time(%" GST_TIME_FORMAT ")",
                    GST_OBJECT_NAME (msg->src), GST_TIME_ARGS (running_time));
            break;
        }
        case GST_MESSAGE_TAG:
        {
            GstTagList *received_tags = NULL;
            gst_message_parse_tag (msg, &received_tags);
            NXGLOGV("Got tags from element %s", GST_OBJECT_NAME (msg->src));
            gst_tag_list_foreach(received_tags, print_tag, NULL);
            gst_tag_list_unref (received_tags);
            break;
        }
        default:
        {
            GstMessageType type = GST_MESSAGE_TYPE(msg);
            // TODO: latency, stream-status, reset-time, async-done, new-clock, etc
            NXGLOGV("Received GST_MESSAGE_TYPE [%s]",
                   gst_message_type_get_name(type));
            break;
        }
    }
    return TRUE;
}

void PrintMediaInfo(MP_HANDLE handle, const char *pUri)
{
    _CAutoLock lock(&handle->apiLock);

    FUNC_IN();

    NXGLOGI("[%s] container(%d), video type(%d),"
            "audio type(%d), seekable(%s), video_width(%d), video_height(%d),"
            "duration: (%" GST_TIME_FORMAT ")\r",
            pUri,
            handle->gst_media_info.container_type,
            handle->gst_media_info.StreamInfo[0].VideoInfo[0].type,
            handle->gst_media_info.StreamInfo[0].AudioInfo[0].type,
            handle->gst_media_info.StreamInfo[0].seekable ? "yes":"no",
            handle->gst_media_info.StreamInfo[0].VideoInfo[0].width,
            handle->gst_media_info.StreamInfo[0].VideoInfo[0].height,
            GST_TIME_ARGS (handle->gst_media_info.StreamInfo[0].duration));

    FUNC_OUT();
}

NX_GST_RET set_demux_element(MP_HANDLE handle)
{
    FUNC_IN();

    if (!handle)
    {
        NXGLOGE("handle is NULL");
        return NX_GST_RET_ERROR;
    }

    CONTAINER_TYPE  container_type = handle->gst_media_info.container_type;

    //	Set Demuxer
    if ((container_type == CONTAINER_TYPE_QUICKTIME) ||	// Quicktime
        (container_type == CONTAINER_TYPE_3GP))
    {
        handle->demuxer = gst_element_factory_make("qtdemux", "qtdemux");
    }
    else if (container_type == CONTAINER_TYPE_MATROSKA)     // MKV
    {
        handle->demuxer = gst_element_factory_make("matroskademux", "matroskademux");
    }
    else if (container_type == CONTAINER_TYPE_MSVIDEO)      // AVI
    {
        handle->demuxer = gst_element_factory_make("avidemux", "avidemux");
    }
    else if (container_type == CONTAINER_TYPE_MPEG)         // MPEG (vob)
    {
        handle->demuxer = gst_element_factory_make("mpegpsdemux", "mpegpsdemux");
    }
#ifdef SW_V_DECODER
    else if (container_type == CONTAINER_TYPE_FLV)          // FLV
    {
        handle->demuxer = gst_element_factory_make("flvdemux", "flvdemux");
    }
#endif
    else if (container_type == CONTAINER_TYPE_MPEGTS)       // MPEGTS
    {
        handle->demuxer = gst_element_factory_make("tsdemux", "tsdemux");
    }

    if (NULL == handle->demuxer)
    {
        NXGLOGE("Failed to create demuxer. Exiting");
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
        NXGLOGE("handle is NULL");
        return NX_GST_RET_ERROR;
    }

    handle->audio_queue = gst_element_factory_make("queue2", "audio_queue");

    // Audio parser & Audio decoder
    if ((handle->gst_media_info.StreamInfo[0].AudioInfo[0].type == AUDIO_TYPE_MPEG) &&
        (handle->gst_media_info.StreamInfo[0].AudioInfo[0].mpegaudioversion <= 2))
    {
        handle->audio_parser = gst_element_factory_make("mpegaudioparse", "mpegaudioparse");
        if (!handle->audio_parser)
        {
            NXGLOGE("Failed to create mpegaudioparse element");
            return NX_GST_RET_ERROR;
        }
        handle->audio_decoder = gst_element_factory_make("mpg123audiodec", "mpg123audiodec");
        if (!handle->audio_decoder)
        {
            NXGLOGE("Failed to create mpg123audiodec element");
            return NX_GST_RET_ERROR;
        }
    }
    else
    {
        handle->audio_decoder = gst_element_factory_make("decodebin", "decodebin");
        if (!handle->audio_decoder)
        {
            NXGLOGE("Failed to create decodebin element");
            return NX_GST_RET_ERROR;
        }
    }

    handle->audioconvert = gst_element_factory_make("audioconvert", "audioconvert");
    handle->audioresample = gst_element_factory_make("audioresample", "audioresample");
    handle->alsasink = gst_element_factory_make("alsasink", "alsasink");

    if (!handle->audio_queue || !handle->audioconvert ||
        !handle->audioresample || !handle->alsasink)
    {
        NXGLOGE("Failed to create audio elements");
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
        NXGLOGE("handle is NULL");
        return NX_GST_RET_ERROR;
    }

    // Queue for video
    handle->video_queue = gst_element_factory_make("queue2", "video_queue");

    // Video Parser
    if (handle->gst_media_info.StreamInfo[0].VideoInfo[0].type == VIDEO_TYPE_H264)
    {
        handle->video_parser = gst_element_factory_make("h264parse", "parser");
        if (!handle->video_parser)
        {
            NXGLOGE("Failed to create h264parse element");
            return NX_GST_RET_ERROR;
        }
    }
    else if (handle->gst_media_info.StreamInfo[0].VideoInfo[0].type == VIDEO_TYPE_MPEG_V2)
    {
        handle->video_parser = gst_element_factory_make("mpegvideoparse", "parser");
        if (!handle->video_parser)
        {
            NXGLOGE("Failed to create mpegvideoparse element");
            return NX_GST_RET_ERROR;
        }
    }

    // Video Decoder
#ifdef SW_V_DECODER
    if (g_strcmp0(handle->gst_media_info.video_mime_type, "video/x-flash-video") == 0)
    {
        handle->video_decoder = gst_element_factory_make("avdec_flv", "avdec_flv");
        //handle->video_convert = gst_element_factory_make("videoconvert", "videoconvert");
        //handle->video_scale = gst_element_factory_make("video_scale", "video_scale");
    }
    else
    {
        handle->video_decoder = gst_element_factory_make("nxvideodec", "nxvideodec");
    }
#else
    handle->video_decoder = gst_element_factory_make("nxvideodec", "nxvideodec");
#endif

    // Tee for video
    handle->tee = gst_element_factory_make("tee", "tee");
    if(!handle->video_queue || !handle->video_decoder)
    {
        NXGLOGE("Failed to create video elements");
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
        NXGLOGE("handle is NULL");
        return NX_GST_RET_ERROR;
    }

    handle->source = gst_element_factory_make("filesrc", "source");
    if (NULL == handle->source)
    {
        NXGLOGE("Failed to create filesrc element");
        return NX_GST_RET_ERROR;
    }
    g_object_set(handle->source, "location", handle->filePath, NULL);

    FUNC_OUT();

    return NX_GST_RET_OK;
}

void add_video_elements_to_bin(MP_HANDLE handle)
{
    gst_bin_add_many(GST_BIN(handle->pipeline), handle->video_queue,
                    handle->video_decoder, handle->tee, NULL);

    // video_parser
    if ((handle->gst_media_info.StreamInfo[0].VideoInfo[0].type == VIDEO_TYPE_H264) ||
        (handle->gst_media_info.StreamInfo[0].VideoInfo[0].type == VIDEO_TYPE_MPEG_V2))
    {
        gst_bin_add(GST_BIN(handle->pipeline), handle->video_parser);
    }
}

void add_audio_elements_to_bin(MP_HANDLE handle)
{
    if (handle->gst_media_info.StreamInfo[0].AudioInfo[0].type == AUDIO_TYPE_MPEG_V2)
    {
        gst_bin_add_many(GST_BIN(handle->pipeline),
                    handle->audio_queue, handle->audio_parser, handle->audio_decoder,
                    handle->audioconvert, handle->audioresample,
                    handle->alsasink,
                    NULL);
    }
    else
    {
        gst_bin_add_many(GST_BIN(handle->pipeline),
                    handle->audio_queue, handle->audio_decoder,
                    handle->audioconvert, handle->audioresample,
                    handle->alsasink,
                    NULL);
    }
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
        NXGLOGE("handle is NULL");
        return NX_GST_RET_ERROR;
    }

    gst_bin_add_many(GST_BIN(handle->pipeline), handle->source, handle->demuxer, NULL);

    add_video_elements_to_bin(handle);
    add_audio_elements_to_bin(handle);
    if (handle->gst_media_info.StreamInfo->n_subtitle >= 1 &&
        (handle->gst_media_info.StreamInfo->SubtitleInfo->type == SUBTITLE_TYPE_RAW))
    {
        add_subtitle_elements_to_bin(handle);
    }

    FUNC_OUT();

    return NX_GST_RET_OK;
}

NX_GST_RET link_video_elements(MP_HANDLE handle)
{
    if ((handle->gst_media_info.StreamInfo[0].VideoInfo[0].type == VIDEO_TYPE_H264) ||
        ((handle->gst_media_info.StreamInfo[0].VideoInfo[0].type == VIDEO_TYPE_MPEG_V2) &&
            (handle->gst_media_info.StreamInfo[0].VideoInfo[0].mpegversion <=2)))
    {
        if (!gst_element_link(handle->video_queue, handle->video_parser))
        {
            NXGLOGE("Failed to link video elements with video_queue<-->video_parser");
            return NX_GST_RET_ERROR;
        }
        if (!gst_element_link(handle->video_parser, handle->video_decoder))
        {
            NXGLOGE("Failed to link video elements with video_parser<-->video_decoder");
            return NX_GST_RET_ERROR;
        }
        NXGLOGI("Succeed to link video elements with video_queue<-->video_parser<-->video_decoder");
    }
    else
    {
        if (!gst_element_link(handle->video_queue, handle->video_decoder))
        {
            NXGLOGE("Failed to link video elements");
            return NX_GST_RET_ERROR;
        }
        else
        {
            NXGLOGI("Succeed to link video_queue<-->video_decoder");
        }
    }

    // video_decoder <--> tee
    if (!gst_element_link_many(handle->video_decoder, handle->tee, NULL))
    {
        NXGLOGE("Failed to link video_decoder<-->tee");
    }
    NXGLOGI("Succeed to link video elements with video_decoder<-->tee");

    return NX_GST_RET_OK;
}

NX_GST_RET link_audio_elements(MP_HANDLE handle)
{
    if ((handle->gst_media_info.StreamInfo[0].AudioInfo[0].type == AUDIO_TYPE_MPEG) &&
             (handle->gst_media_info.StreamInfo[0].AudioInfo[0].mpegaudioversion <= 2))
    {
        if (!gst_element_link(handle->audio_queue, handle->audio_parser))
        {
            NXGLOGE("Failed to link audio_queue<-->audio_parser");
            return NX_GST_RET_ERROR;
        }
        if (!gst_element_link(handle->audio_parser, handle->audio_decoder))
        {
            NXGLOGE("Failed to link audio_parser<-->audio_decoder");
            return NX_GST_RET_ERROR;
        }
        if (!gst_element_link(handle->audio_decoder, handle->audioconvert))
        {
            NXGLOGE("Failed to link audio_decoder<-->audioconvert");
            return NX_GST_RET_ERROR;
        }
        NXGLOGI("Succeed to link audio_queue<-->audio_parser<-->audio_decoder<-->audioconvert");
    }
    else
    {
        if (!gst_element_link(handle->audio_queue, handle->audio_decoder))
        {
            NXGLOGE("Failed to link audio_queue<-->audio_decoder");
            return NX_GST_RET_ERROR;
        }
        NXGLOGI("Succeed to link audio_queue<-->audio_decoder");
    }

    if (!gst_element_link_many(handle->audioconvert, handle->audioresample, handle->alsasink, NULL))
    {
        NXGLOGE("Failed to link audioconvert<-->audioresample<-->alsasink");
        return NX_GST_RET_ERROR;
    }
    NXGLOGI("Succeed to link audioconvert<-->audioresample<-->alsasink");

    return NX_GST_RET_OK;
}

NX_GST_RET link_subtitle_elements(MP_HANDLE handle)
{
    if (!gst_element_link(handle->subtitle_queue, handle->capsfilter))
    {
        NXGLOGE("Failed to link subtitle_queue<-->capsfilter");
        return NX_GST_RET_ERROR;
    }
    if (!gst_element_link(handle->capsfilter, handle->fakesink))
    {
        NXGLOGE("Failed to link capsfilter<-->fakesink");
        return NX_GST_RET_ERROR; 
    }

    NXGLOGI("Succeed to link subtitle_queue<-->fakesink");

    return NX_GST_RET_OK;
}

NX_GST_RET link_elements(MP_HANDLE handle)
{
    FUNC_IN();

    if (!handle)
    {
        NXGLOGE("handle is NULL");
        return NX_GST_RET_ERROR;
    }

    if (!gst_element_link_many(handle->source, handle->demuxer, NULL))
    {
        NXGLOGE("Failed to link %s<-->%s",
                gst_element_get_name(handle->source), gst_element_get_name(handle->demuxer));
        return NX_GST_RET_ERROR;
    }
    else
    {
        NXGLOGI("Succeed to link %s<-->%s",
                gst_element_get_name(handle->source), gst_element_get_name(handle->demuxer));
    }

    link_video_elements(handle);
    link_audio_elements(handle);
    if (handle->gst_media_info.StreamInfo[0].n_subtitle >= 1 &&
        handle->gst_media_info.StreamInfo[0].SubtitleInfo[0].type == SUBTITLE_TYPE_RAW)
    {
        link_subtitle_elements(handle);
    }

    FUNC_OUT();

    return NX_GST_RET_OK;
}

NX_GST_RET link_display(MP_HANDLE handle, enum DISPLAY_TYPE type)
{
    NXGLOGI("Display type [%s]", (type == DISPLAY_TYPE_PRIMARY)?"PRIMARY":"SECONDARY");

    if ((DISPLAY_TYPE_PRIMARY == type) && primary_sinks)
    {
        NXGLOGE("Failed to link the primary display");
        return NX_GST_RET_ERROR;
    }
    if ((DISPLAY_TYPE_SECONDARY == type) && secondary_sinks)
    {
        NXGLOGE("Failed to link the secondary display");
        return NX_GST_RET_ERROR;
    }

    struct Sink *sink = g_try_new0(struct Sink, 1);
    GstPad *sinkpad;
    GstPadTemplate *templ;
    DSP_RECT rect;
    gchar *video_sink_name, *queue_name;

    sink->tee = handle->tee;
    sink->pipeline = handle->pipeline;

    // Request a pad from tee
    templ = gst_element_class_get_pad_template (GST_ELEMENT_GET_CLASS (sink->tee), "src_%u");
    sink->tee_pad = gst_element_request_pad (sink->tee, templ, NULL, NULL);
    NXGLOGI("Obtained request pad %s:%s for the %s display",
            GST_DEBUG_PAD_NAME(sink->tee_pad),
            (type == DISPLAY_TYPE_PRIMARY)?"primary":"secondary");

    // Create elements 'queue' and 'nxvideosink'
    if (DISPLAY_TYPE_PRIMARY == type)
    {
        sink->queue = gst_element_factory_make ("queue2", "queue_primary");
        sink->nxvideosink = gst_element_factory_make ("nxvideosink", "nxvideosink");
    }
    else
    {
        sink->queue = gst_element_factory_make ("queue2", "queue_secondary");
        sink->nxvideosink = gst_element_factory_make ("nxvideosink", "nxvideosink_hdmi");
    }

    g_object_set (G_OBJECT (sink->nxvideosink), "async", false, NULL);
    g_object_get (G_OBJECT (sink->queue), "name", &queue_name, NULL);
    g_object_get (G_OBJECT (sink->nxvideosink), "name", &video_sink_name, NULL);

    NXGLOGI("queue_name(%s) videosink(%s)", queue_name, video_sink_name);

    if (DISPLAY_TYPE_PRIMARY == type) {
        rect = handle->primary_dsp_rect;
    } else {
        rect = handle->secondary_dsp_rect;
    }
    g_object_set(G_OBJECT(sink->nxvideosink), "dst-x", rect.left, NULL);
    g_object_set(G_OBJECT(sink->nxvideosink), "dst-y", rect.top, NULL);
    g_object_set(G_OBJECT(sink->nxvideosink), "dst-w", (rect.right - rect.left), NULL);
    g_object_set(G_OBJECT(sink->nxvideosink), "dst-h", (rect.bottom - rect.top), NULL);
    g_object_set(sink->nxvideosink, "crtc-index", type, NULL);

    if (!sink->queue || !sink->nxvideosink)
    {
        NXGLOGE("Failed to create dual display elements");
        return NX_GST_RET_ERROR;
    }
    sink->removing = FALSE;

    // Add elements 'queue, nxvideosink' to bin
    gst_bin_add_many(GST_BIN(sink->pipeline),
                            sink->queue,
                            sink->nxvideosink,
                            NULL);

    // Link queue<-->nxvideosink
    if (!gst_element_link_many(sink->queue, sink->nxvideosink, NULL))
    {
        NXGLOGE("Failed to link %s<-->%s", queue_name, video_sink_name);
        return NX_GST_RET_ERROR;
    }

    // Set sync state
    gst_element_sync_state_with_parent (sink->queue);
    gst_element_sync_state_with_parent (sink->nxvideosink);

    // Link tee_pad<-->queue sink pad
    sinkpad = gst_element_get_static_pad(sink->queue, "sink");
    gboolean ret = gst_pad_link(sink->tee_pad, sinkpad);
    if (GST_PAD_LINK_OK != ret)
    {
        NXGLOGI("%s to unlink %s:%s from %s:%s",
                (ret == GST_PAD_LINK_OK) ? "Succeed":"Failed",
                GST_DEBUG_PAD_NAME(sink->tee_pad),
                GST_DEBUG_PAD_NAME(sinkpad));
        gst_object_unref (sinkpad);
        return NX_GST_RET_ERROR;
    }
    else
    {
        NXGLOGI("Succeed to link %s:%s and %s:%s",
                GST_DEBUG_PAD_NAME(sink->tee_pad),
                GST_DEBUG_PAD_NAME(sinkpad));
    }
    gst_object_unref (sinkpad);

    if (DISPLAY_TYPE_PRIMARY == type) {
        primary_sinks = g_list_append (primary_sinks, sink);
    } else {
        secondary_sinks = g_list_append (secondary_sinks, sink);
    }

    g_free (queue_name);
    g_free (video_sink_name);

    return NX_GST_RET_OK;
}

static GstPadProbeReturn
unlink_display_cb (GstPad* pad, GstPadProbeInfo* info, gpointer user_data)
{
    struct Sink *sink = (struct Sink *)user_data;
    GstStateChangeReturn state_ret;

    NXGLOGI();

    if (!g_atomic_int_compare_and_exchange (&sink->removing, FALSE, TRUE))
    {
        NXGLOGI("- GST_PAD_PROBE_OK");
        return GST_PAD_PROBE_OK;
    }

    // Unlink tee:src_<-->queue:sink
    GstPad *sinkpad;
    sinkpad = gst_element_get_static_pad (sink->queue, "sink");
    gboolean ret = gst_pad_unlink (sink->tee_pad, sinkpad);
    NXGLOGI("%s to unlink %s:%s from %s:%s",
            (ret == true) ? "Succeed":"Failed",
            GST_DEBUG_PAD_NAME(sink->tee_pad),
            GST_DEBUG_PAD_NAME(sinkpad));
    gst_object_unref (sinkpad);

    // set state of 'nxvideosink' and 'queue' to NULL
    state_ret = gst_element_set_state (sink->nxvideosink, GST_STATE_NULL);
    if (state_ret == GST_STATE_CHANGE_FAILURE) {
        NXGLOGE("Failed to set nxvideosink to the NULL state");
    }
    state_ret = gst_element_set_state (sink->queue, GST_STATE_NULL);
    if (state_ret == GST_STATE_CHANGE_FAILURE) {
        NXGLOGE("Failed to set queue to the NULL state");
    }

    // remove queue and nxvideosink
    if (!gst_bin_remove (GST_BIN (sink->pipeline), sink->queue)) {
        NXGLOGE("Failed to remove queue from bin");
    }
    if (!gst_bin_remove (GST_BIN (sink->pipeline), sink->nxvideosink)) {
        NXGLOGE("Failed to remove nxvideosink from bin");
    }


    // release_request_pad and unref 'tee_pad'
    gst_element_release_request_pad (sink->tee, sink->tee_pad);
    gst_object_unref (sink->tee_pad);

    // GStreamer-CRITICAL **: gst_object_unref: assertion '((GObject *) object)->ref_count > 0' failed
    // unref queue and nxvideosink
    //gst_object_unref (sink->queue);
    //gst_object_unref (sink->nxvideosink);

    NXGLOGI("- GST_PAD_PROBE_REMOVE");
    return GST_PAD_PROBE_REMOVE;
}

void unlink_display(enum DISPLAY_TYPE type)
{
    NXGLOGI("Display type(%d)", type);

    struct Sink *sink;
    GList *sinks = (DISPLAY_TYPE_PRIMARY == type) ? primary_sinks:secondary_sinks;

    if (g_list_length(sinks) > 0)
    {
        if (DISPLAY_TYPE_PRIMARY == type)
        {
            sink = (Sink*)primary_sinks->data;
            primary_sinks = g_list_delete_link (primary_sinks, primary_sinks);
        }
        else
        {
            sink = (Sink*)secondary_sinks->data;
            secondary_sinks = g_list_delete_link (secondary_sinks, secondary_sinks);
        }
        gst_pad_add_probe (sink->tee_pad, GST_PAD_PROBE_TYPE_IDLE,
                            (GstPadProbeCallback)unlink_display_cb, sink,
                            (GDestroyNotify) g_free);
    }
}

NX_GST_RET NX_GSTMP_SetDisplayMode(MP_HANDLE handle, enum DISPLAY_MODE in_mode)
{
    _CAutoLock lock(&handle->apiLock);

    FUNC_IN();

    if (NULL == handle)
    {
        NXGLOGE("handle is NULL");
        return NX_GST_RET_ERROR;
    }

    if ((in_mode < DISPLAY_MODE_LCD_ONLY) ||
        (in_mode > DISPLAY_MODE_NONE))
    {
        NXGLOGE("Failed to set display mode - Invalid Display Mode");
        return NX_GST_RET_ERROR;
    }

    enum DISPLAY_MODE   old_mode = handle->display_mode;

    NXGLOGI("The display_mode is changed from (%s) to (%s) in state(%s)",
            get_display_mode_str(old_mode),
            get_display_mode_str(in_mode),
            gst_element_state_get_name (handle->state));

    if (old_mode != in_mode)
    {
        if (handle->state == GST_STATE_PAUSED || handle->state == GST_STATE_PLAYING)
        {
            if (DISPLAY_MODE_LCD_ONLY == old_mode)
            {
                if (DISPLAY_MODE_NONE == in_mode) {
                    unlink_display(DISPLAY_TYPE_PRIMARY);
                } else {
                    link_display(handle, DISPLAY_TYPE_SECONDARY);
                    if (DISPLAY_MODE_HDMI_ONLY == in_mode)
                    {
                        unlink_display(DISPLAY_TYPE_PRIMARY);
                    }
                }
            }
            else if (DISPLAY_MODE_HDMI_ONLY == old_mode)
            {
                if (DISPLAY_MODE_NONE == in_mode) {
                    unlink_display(DISPLAY_TYPE_SECONDARY);
                } else {
                    link_display(handle, DISPLAY_TYPE_PRIMARY);
                    if (DISPLAY_MODE_LCD_ONLY == in_mode)
                    {
                        unlink_display(DISPLAY_TYPE_SECONDARY);
                    }
                }
            }
            else if (DISPLAY_MODE_LCD_HDMI == old_mode)
            {
                if (DISPLAY_MODE_NONE == in_mode) {
                    unlink_display(DISPLAY_TYPE_PRIMARY);
                    unlink_display(DISPLAY_TYPE_SECONDARY);
                } else {
                    if (DISPLAY_MODE_LCD_ONLY == in_mode)
                    {
                        unlink_display(DISPLAY_TYPE_SECONDARY);
                    }
                    else if (DISPLAY_MODE_HDMI_ONLY == in_mode)
                    {
                        unlink_display(DISPLAY_TYPE_PRIMARY);
                    }
                }
            }
            else
            {
                if (DISPLAY_MODE_LCD_ONLY == in_mode) {
                    link_display(handle, DISPLAY_TYPE_PRIMARY);
                } else if (DISPLAY_MODE_HDMI_ONLY == in_mode) {
                    link_display(handle, DISPLAY_TYPE_SECONDARY);
                } else if (DISPLAY_MODE_LCD_HDMI == in_mode) {
                    link_display(handle, DISPLAY_TYPE_PRIMARY);
                    link_display(handle, DISPLAY_TYPE_SECONDARY);
                }
            }
        }
    }
    handle->display_mode = in_mode;

    return NX_GST_RET_OK;
}

NX_GST_RET NX_GSTMP_SetUri(MP_HANDLE handle, const char *pfilePath)
{
    _CAutoLock lock(&handle->apiLock);

    FUNC_IN();

    GstStateChangeReturn ret;

    struct GST_MEDIA_INFO *pGstMInfo = (struct GST_MEDIA_INFO*)g_malloc0(sizeof(struct GST_MEDIA_INFO));
    if (NULL == pGstMInfo)
    {
        NXGLOGE("Failed to alloc the memory to start discovering");
        return NX_GST_RET_ERROR;
    }

    if(NULL == handle)
    {
        NXGLOGE("Failed to alloc memory for handle");
        return NX_GST_RET_ERROR;
    }
    handle->filePath = g_strdup(pfilePath);
    handle->error = NX_GST_ERROR_NONE;

    enum NX_GST_ERROR err = StartDiscover(pfilePath, &pGstMInfo);
    if (NX_GST_ERROR_NONE != err)
    {
        handle->error = err;
        NXGLOGE("%s", get_nx_gst_error(err));
        return NX_GST_RET_ERROR;
    }
    memcpy(&handle->gst_media_info, pGstMInfo, sizeof(struct GST_MEDIA_INFO));

    NXGLOGD("container(%d), video type(%d),"
            "video_width(%d), video_height(%d),"
            "audio codec(%d), seekable(%s),"
            "duration: (%" GST_TIME_FORMAT ")\r",
            pGstMInfo->container_type,
            pGstMInfo->StreamInfo->VideoInfo->type,
            pGstMInfo->StreamInfo->VideoInfo->width,
            pGstMInfo->StreamInfo->VideoInfo->height,
            pGstMInfo->StreamInfo->AudioInfo->type,
            pGstMInfo->StreamInfo->seekable ? "yes":"no",
            GST_TIME_ARGS (pGstMInfo->StreamInfo->duration));

    if(handle->pipeline_is_linked)
    {
        NXGLOGE("pipeline is already linked");
        // TODO:
        return NX_GST_RET_OK;
    }

    handle->pipeline = gst_pipeline_new("NxGstMoviePlay");
    if (NULL == handle->pipeline)
    {
        NXGLOGE("pipeline is NULL");
        return NX_GST_RET_ERROR;
    }
    handle->bus = gst_pipeline_get_bus(GST_PIPELINE(handle->pipeline));
    handle->bus_watch_id = gst_bus_add_watch(handle->bus, (GstBusFunc)gst_bus_callback, handle);
    gst_object_unref(handle->bus);

    if ((pGstMInfo->StreamInfo[0].n_subtitle > 0) &&
        (pGstMInfo->StreamInfo[0].SubtitleInfo[0].type == SUBTITLE_TYPE_RAW))
    {
        set_subtitle_element(handle);
    }

    if (NX_GST_RET_ERROR == set_source_element(handle) ||
        NX_GST_RET_ERROR == set_demux_element(handle) ||
        NX_GST_RET_ERROR == set_audio_elements(handle) ||
        NX_GST_RET_ERROR == set_video_elements(handle))
    {
        NXGLOGE("Failed to set all elements");
        return NX_GST_RET_ERROR;
    }

    if (NX_GST_RET_ERROR == add_elements_to_bin(handle) ||
        NX_GST_RET_ERROR == link_elements(handle))
    {
        NXGLOGE("Failed to add/link elements");
        return NX_GST_RET_ERROR;
    }

    handle->rate = 1.0;

    // demuxer <--> audio_queue/video_queue
    g_signal_connect(handle->demuxer,	"pad-added", G_CALLBACK (on_pad_added_demux), handle);
    // decodbin <--> audio_converter
    g_signal_connect(handle->audio_decoder, "pad-added", G_CALLBACK (on_decodebin_pad_added), handle);

    handle->pipeline_is_linked = TRUE;

    ret = gst_element_set_state(handle->pipeline, GST_STATE_READY);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        NXGLOGE("Failed to set the pipeline to the READY state");
        return NX_GST_RET_ERROR;
    }
    
    start_loop_thread(handle);

    FUNC_OUT();

    return NX_GST_RET_OK;
}

NX_GST_RET NX_GSTMP_Open(MP_HANDLE *pHandle,
        void (*cb)(void *owner, unsigned int eventType,
        unsigned int eventData, void * param), void *cbOwner)
{
    NXGLOGI();
    FUNC_IN();

    if(*pHandle)
    {
        // TODO:
        NXGLOGE("handle is not freed");
        return NX_GST_RET_ERROR;
    }

    MP_HANDLE handle = (MP_HANDLE)g_malloc0(sizeof(MOVIE_TYPE));
    if (NULL == handle)
    {
        NXGLOGE("Failed to alloc handle");
        return NX_GST_RET_ERROR;
    }

    pthread_mutex_init(&handle->apiLock, NULL);
    pthread_mutex_init(&handle->stateLock, NULL);

    handle->owner = cbOwner;
    handle->callback = cb;
    handle->display_mode = DISPLAY_MODE_NONE;

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
        NXGLOGE("handle is already NULL");
        return;
    }

    if(handle->pipeline_is_linked)
    {
        gst_element_set_state(handle->pipeline, GST_STATE_NULL);

        unlink_display(DISPLAY_TYPE_PRIMARY);
        unlink_display(DISPLAY_TYPE_SECONDARY);

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
        NXGLOGE("pGstMInfo is NULL");
        return NX_GST_RET_ERROR;
    }

    memcpy(pGstMInfo, &handle->gst_media_info, sizeof(GST_MEDIA_INFO));
 
    NXGLOGI("container(%d), video mime-type(%d),"
           "audio mime-type(%d), seekable(%s), video_width(%d), video_height(%d),"
           "duration: (%" GST_TIME_FORMAT ")\r",
           pGstMInfo->container_type,
           pGstMInfo->StreamInfo->VideoInfo->type,
           pGstMInfo->StreamInfo->AudioInfo->type,
           pGstMInfo->StreamInfo->seekable ? "yes":"no",
           pGstMInfo->StreamInfo->VideoInfo->width,
           pGstMInfo->StreamInfo->VideoInfo->height,
           GST_TIME_ARGS (pGstMInfo->StreamInfo->duration));

    FUNC_OUT();

    return NX_GST_RET_OK;
}

NX_GST_RET
NX_GSTMP_SetDisplayInfo(MP_HANDLE handle, enum DISPLAY_TYPE type,
                        int dspWidth, int dspHeight, struct DSP_RECT rect)
{
    _CAutoLock lock(&handle->apiLock);

    FUNC_IN();

    if (NULL == handle)
    {
        NXGLOGE("handle is NULL");
        return NX_GST_RET_ERROR;
    }

    if (handle->gst_media_info.StreamInfo[0].VideoInfo[0].width > dspWidth)
    {
        NXGLOGE("Not supported content(width:%d)", handle->gst_media_info.StreamInfo[0].VideoInfo[0].width);
        return NX_GST_RET_ERROR;
    }

    memcpy(&handle->gst_media_info, &rect, sizeof(struct DSP_RECT));

    NXGLOGD("left(%d), right(%d), top(%d), bottom(%d), dspWidth(%d), dspHeight(%d)",
            rect.left, rect.right, rect.top, rect.bottom, dspWidth, dspHeight);

    if (type == DISPLAY_TYPE_SECONDARY)
    {
        memcpy(&handle->secondary_dsp_rect, &rect, sizeof(struct DSP_RECT));
    }
    else
    {
        memcpy(&handle->primary_dsp_rect, &rect, sizeof(struct DSP_RECT));
    }

    FUNC_OUT();

    return NX_GST_RET_OK;
}

int64_t NX_GSTMP_GetPosition(MP_HANDLE handle)
{
    _CAutoLock lock(&handle->apiLock);

    GstState state, pending;
    GstStateChangeReturn ret;
    gint64 position;

    //FUNC_IN();

    if (NULL == handle)
        return -1;

    ret = gst_element_get_state(handle->pipeline, &state, &pending, 500000000);		//	wait 500 msec
    if (GST_STATE_CHANGE_FAILURE != ret)
    {
        if ((GST_STATE_PLAYING == state) || (GST_STATE_PAUSED == state))
        {
            GstFormat format = GST_FORMAT_TIME;
            if (gst_element_query_position(handle->pipeline, format, &position))
            {
                //NXGLOGV("Position: %" GST_TIME_FORMAT "\r",
                //        __func__, GST_TIME_ARGS (position));
            }
        }
        else
        {
            NXGLOGE("Invalid state to query POSITION");
            return -1;
        }
    }
    else
    {
        NXGLOGE("Failed to query POSITION");
        return -1;
    }

    //FUNC_OUT();

    return position;
}

int64_t NX_GSTMP_GetDuration(MP_HANDLE handle)
{
    _CAutoLock lock(&handle->apiLock);

    GstState state, pending;
    GstStateChangeReturn ret;
    gint64 duration;

    //FUNC_IN();

    if (!handle || !handle->pipeline_is_linked)
    {
        NXGLOGE(": invalid state or invalid operation.(%p,%d)\n",
                handle, handle->pipeline_is_linked);
        return -1;
    }

    ret = gst_element_get_state(handle->pipeline, &state, &pending, 500000000);		//	wait 500 msec
    if (GST_STATE_CHANGE_FAILURE != ret)
    {
        if (state == GST_STATE_PLAYING || state == GST_STATE_PAUSED || state == GST_STATE_READY)
        {
            GstFormat format = GST_FORMAT_TIME;
            if (gst_element_query_duration(handle->pipeline, format, &duration))
            {
                NXGLOGV("Duration: %" GST_TIME_FORMAT "\r",
                        GST_TIME_ARGS (duration));
            }
        }
        else
        {
            NXGLOGE("Invalid state to query DURATION");
            return -1;
        }
    }
    else
    {
        NXGLOGE("Failed to query DURATION");
        return -1;
    }

    //FUNC_OUT();

    return duration;
}

NX_GST_RET NX_GSTMP_SetVolume(MP_HANDLE handle, int volume)
{
    _CAutoLock lock(&handle->apiLock);

    FUNC_IN();

    if (!handle || !handle->alsasink || !handle->volume)
    {
        return NX_GST_RET_ERROR;
    }

    gdouble vol = (double)volume/100.;
    NXGLOGI("set volume to %f", vol);
    g_object_set(G_OBJECT (handle->volume), "volume", vol, NULL);

    FUNC_OUT();

    return NX_GST_RET_OK;
}

#ifdef STEP_SEEK
static gboolean send_step_event(MP_HANDLE handle)
{
    gboolean ret;

    if (handle->nxvideosink == NULL)
    {
        /* If we have not done so, obtain the sink through which we will send the step events */
        g_object_get(handle->pipeline, "video-sink", &handle->nxvideosink, NULL);
    }

    ret = gst_element_send_event (handle->nxvideosink,
                    gst_event_new_step(GST_FORMAT_BUFFERS, 1,
                                        ABS (handle->rate), TRUE, FALSE));

    NXGLOGI("Stepping one frame");

    return ret;
}
#endif

static int send_seek_event(MP_HANDLE handle)
{
    gint64 position;
    GstEvent *seek_event;
    GstFormat format = GST_FORMAT_TIME;
    GstSeekFlags flags = (GstSeekFlags)(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_SNAP_AFTER | GST_SEEK_FLAG_KEY_UNIT);
    //GstSeekFlags flags = (GstSeekFlags)(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE);

    /* Obtain the current position, needed for the seek event */
    if (!gst_element_query_position (handle->pipeline, format, &position))
    {
        NXGLOGE("Unable to retrieve current position");
        return -1;
    }

    /* Create the seek event */
    if (handle->rate > 0)
    {
        seek_event =
            gst_event_new_seek (handle->rate, format, flags,
                                GST_SEEK_TYPE_SET, position,
                                GST_SEEK_TYPE_END, 0);
    }
    else
    {
        seek_event =
            gst_event_new_seek (handle->rate, format, flags,
                                GST_SEEK_TYPE_SET, 0,
                                GST_SEEK_TYPE_SET, position);
    }

    if (handle->nxvideosink == NULL)
    {
        /* If we have not done so, obtain the sink through which we will send the seek events */
        g_object_get (handle->pipeline, "video-sink", &handle->nxvideosink, NULL);
    }

    /* Send the event */
    gst_element_send_event (handle->nxvideosink, seek_event);

    NXGLOGI("Current rate: %g\n", handle->rate);
    return 0;
}

static NX_GST_RET seek_to_time (MP_HANDLE handle, gint64 time_nanoseconds)
{
    GstFormat format = GST_FORMAT_TIME;

    if (!gst_element_seek (handle->pipeline, handle->rate, format,
                          GST_SEEK_FLAG_FLUSH,		/* gdouble rate */
                          GST_SEEK_TYPE_SET,		/* GstSeekType start_type */
                          time_nanoseconds,			/* gint64 start */
                          GST_SEEK_TYPE_NONE,		/* GstSeekType stop_type */
                          GST_CLOCK_TIME_NONE))		/* gint64 stop */
    {
        NXGLOGE("Failed to seek %lld!", time_nanoseconds);
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
    
    if (!handle || !handle->pipeline_is_linked)
    {
        NXGLOGE("Invalid state or invalid operation.(%p,%d)\n",
                handle, handle->pipeline_is_linked);
        return ret;
    }

    GstState state, pending;
    if(GST_STATE_CHANGE_FAILURE != gst_element_get_state(handle->pipeline, &state, &pending, 500000000))	
    {
        if(state == GST_STATE_PLAYING || state == GST_STATE_PAUSED || state == GST_STATE_READY)
        {
            NXGLOGI("state(%s) with the rate %f", gst_element_state_get_name (state), handle->rate);
            ret = seek_to_time(handle, seekTime*(1000*1000)); /*mSec to NanoSec*/
        }
        else
        {
            NXGLOGE("Invalid state to seek");
            ret = NX_GST_RET_ERROR;
        }
    }
    else
    {
        NXGLOGE("Failed to seek");
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

    if (!handle || !handle->pipeline_is_linked)
    {
        NXGLOGE("invalid state or invalid operation.(%p,%d)\n",
                handle, handle->pipeline_is_linked);
        return NX_GST_RET_ERROR;
    }

    GstState state, pending;
    if (GST_STATE_CHANGE_FAILURE != gst_element_get_state(handle->pipeline, &state, &pending, 500000000))
    {
        NXGLOGI("The previous state '%s' with (x%d)", gst_element_state_get_name (state), int(handle->rate));
        ret = gst_element_set_state(handle->pipeline, GST_STATE_PLAYING);
        NXGLOGI("set_state(PLAYING) ==> ret(%s)", get_gst_state_change_ret(ret));
        if (GST_STATE_CHANGE_FAILURE == ret)
        {
            NXGLOGE("Failed to set the pipeline to the PLAYING state(ret=%d)", ret);
            return NX_GST_RET_ERROR;
        }
    }
    else
    {
        NXGLOGE("Failed to get state");
        return NX_GST_RET_ERROR;
    }

    FUNC_OUT();

    return	NX_GST_RET_OK;
}

NX_GST_RET NX_GSTMP_Pause(MP_HANDLE handle)
{
    _CAutoLock lock(&handle->apiLock);

    FUNC_IN();

    if (!handle || !handle->pipeline_is_linked)
    {
        NXGLOGE("invalid state or invalid operation.(%p,%d)\n",
                handle, handle->pipeline_is_linked);
        return NX_GST_RET_ERROR;
    }

    GstStateChangeReturn ret;
    ret = gst_element_set_state (handle->pipeline, GST_STATE_PAUSED);
    if (GST_STATE_CHANGE_FAILURE == ret)
    {
        NXGLOGE("Failed to set the pipeline to the PAUSED state(ret=%d)", ret);
        return NX_GST_RET_ERROR;
    }

    FUNC_OUT();

    return	NX_GST_RET_OK;
}

NX_GST_RET NX_GSTMP_Stop(MP_HANDLE handle)
{
    _CAutoLock lock(&handle->apiLock);

    FUNC_IN();

    if (!handle || !handle->pipeline_is_linked)
    {
        NXGLOGE("invalid state or invalid operation.(%p,%d)\n",
                handle, handle->pipeline_is_linked);
        return NX_GST_RET_ERROR;
    }

    GstStateChangeReturn ret;
    handle->rate = 1.0;
    ret = gst_element_set_state(handle->pipeline, GST_STATE_NULL);
    NXGLOGI("set_state(NULL) ret(%s)", get_gst_state_change_ret(ret));
    if(GST_STATE_CHANGE_FAILURE == ret)
    {
        NXGLOGE("Failed to set the pipeline to the NULL state(ret=%d)", ret);
        return NX_GST_RET_ERROR;
    }

    FUNC_OUT();

    return	NX_GST_RET_OK;
}

enum NX_MEDIA_STATE NX_GSTMP_GetState(MP_HANDLE handle)
{
    _CAutoLock lock(&handle->apiLock);

    if (!handle)
    {
        NXGLOGE("handle is null");
        return MP_STATE_STOPPED;
    }
    if (!handle->pipeline_is_linked)
    {
        NXGLOGE("pipeline is unlinked");
        return MP_STATE_STOPPED;
    }

    //FUNC_IN();

    GstState state, pending;
    enum NX_MEDIA_STATE nx_state = MP_STATE_STOPPED;
    GstStateChangeReturn ret;
    ret = gst_element_get_state(handle->pipeline, &state, &pending, 500000000);		//	wait 500 msec
    NXGLOGV("ret(%s) state(%s), pending(%s)",
    	   get_gst_state_change_ret(ret),
           gst_element_state_get_name(state),
           gst_element_state_get_name(pending));
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
        NXGLOGE("Failed to get state");
        nx_state = MP_STATE_STOPPED;
    }

    NXGLOGV("nx_state(%s)", get_nx_media_state(nx_state));

    //FUNC_OUT();
    return nx_state;
}

NX_GST_RET NX_GSTMP_VideoMute(MP_HANDLE handle, int32_t bOnoff)
{
    FUNC_IN();

    if (!handle || !handle->pipeline_is_linked)
    {
        NXGLOGE("invalid state or invalid operation.(%p,%d)\n",
                handle, handle->pipeline_is_linked);
        return NX_GST_RET_ERROR;
    }

    int dsp_top = handle->gst_media_info.dsp_rect.top;
    int dsp_height = handle->gst_media_info.dsp_rect.bottom - handle->gst_media_info.dsp_rect.top;

    NXGLOGI("bOnoff(%s) dst-y(%d)",
            bOnoff ? "Enable video mute":"Disable video mute",
            bOnoff ? dsp_height:dsp_top);

    _CAutoLock lock(&handle->apiLock);

    if (bOnoff)
    {
        g_object_set(G_OBJECT(handle->nxvideosink), "dst-y", dsp_height, NULL);
    }
    else
    {
        g_object_set(G_OBJECT(handle->nxvideosink), "dst-y", dsp_top, NULL);
    }
    return	NX_GST_RET_OK;

    FUNC_OUT();
}

double NX_GSTMP_GetVideoSpeed(MP_HANDLE handle)
{
    gdouble speed = 1.0;

    if (!handle || !handle->pipeline_is_linked)
    {
        NXGLOGE("Return the default video speed since it's not ready");
        return speed;
    }

    NXGLOGI("rate: %d", (int)handle->rate);
    if ((int)handle->rate == 0)
    {
        speed = 1.0;
    }
    else
    {
        speed = handle->rate;
    }

    NXGLOGI("current playback speed (%f)", speed);
    return speed;
}

/* It's available in PAUSED or PLAYING state */
NX_GST_RET NX_GSTMP_SetVideoSpeed(MP_HANDLE handle, double rate)
{
    FUNC_IN();

    if (!handle || !handle->pipeline_is_linked)
    {
        NXGLOGE("invalid state or invalid operation.(%p,%d)\n",
                handle, handle->pipeline_is_linked);
        return NX_GST_RET_ERROR;
    }

    if (false == handle->gst_media_info.StreamInfo[0].seekable)
    {
        NXGLOGE("This video doesn't support 'seekable'");
        return NX_GST_RET_ERROR;
    }

	if (rate == 0.0) {
		NXGLOGE("The playback rate 0.0 is not allowed");
		return NX_GST_RET_ERROR;
	}

    handle->rate = rate;
    send_seek_event(handle);
    return NX_GST_RET_OK;
}

NX_GST_RET NX_GSTMP_GetVideoSpeedSupport(MP_HANDLE handle)
{
    if(!handle || !handle->pipeline_is_linked)
    {
        NXGLOGE("invalid state or invalid operation.(%p,%d)\n",
                handle, handle->pipeline_is_linked);
        return NX_GST_RET_ERROR;
    }

    return (handle->gst_media_info.StreamInfo[0].seekable) ? NX_GST_RET_OK : NX_GST_RET_ERROR;
}

NX_GST_RET NX_GSTMP_MakeThumbnail(const gchar *uri, int64_t pos_msec, int32_t width, const char *outPath)
{
    return makeThumbnail(uri, pos_msec, width, outPath);
}

enum NX_MEDIA_STATE GstState2NxState(GstState state)
{
    switch(state)
    {
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
    NXGLOGE("No matched state");
    return MP_STATE_STOPPED;
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

const char* get_nx_media_state(enum NX_MEDIA_STATE state)
{
    switch(state) {
        case MP_STATE_VOID_PENDING:
            return "MP_STATE_VOID_PENDING";
        case MP_STATE_STOPPED:
            return "MP_STATE_STOPPED";
        case MP_STATE_READY:
            return "MP_STATE_READY";
        case MP_STATE_PAUSED:
            return "MP_STATE_PAUSED";
        case MP_STATE_PLAYING:
            return "MP_STATE_PLAYING";
        default:
            break;
    }
    return NULL;
}

static const char* get_nx_gst_error(NX_GST_ERROR error)
{
    switch(error) {
        case NX_GST_ERROR_NONE:
            return "NX_GST_ERROR_NONE";
        case NX_GST_ERROR_DISCOVER_FAILED:
            return "NX_GST_ERROR_DISCOVER_FAILED";
        case NX_GST_ERROR_NOT_SUPPORTED_CONTENTS:
            return "NX_GST_ERROR_NOT_SUPPORTED_CONTENTS";
        case NX_GST_ERROR_DEMUX_LINK_FAILED:
            return "NX_GST_ERROR_DEMUX_LINK_FAILED";
        case NX_GST_ERROR_NUM_ERRORS:
            return "NX_GST_ERROR_NUM_ERRORS";
        default:
            break;
    }
    return NULL;
}

const char* get_display_mode_str(enum DISPLAY_MODE mode)
{
    switch (mode) {
        case DISPLAY_MODE_LCD_ONLY:
            return "DISPLAY_MODE_LCD_ONLY";
        case DISPLAY_MODE_HDMI_ONLY:
            return "DISPLAY_MODE_HDMI_ONLY";
        case DISPLAY_MODE_LCD_HDMI:
            return "DISPLAY_MODE_LCD_HDMI";
        default:
            break;
    }
    return "DISPLAY_MODE_NONE";
}