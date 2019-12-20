#include "CNX_MoviePlayer.h"
#include "CNX_Discover.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define LOG_TAG "[NxGstVideoPlayer]"
#include <NX_Log.h>

#include <math.h>

static GstState m_GstState = GST_STATE_NULL;
static gboolean bus_callback (GstBus *bus, GstMessage *msg, gpointer  data);
static void (*event_cb)(void *, unsigned int EventType, unsigned int EventData, unsigned int param);

//#define AUDIO_DEFAULT_DEVICE "plughw:0,0"
//#define AUDIO_HDMI_DEVICE    "plughw:0,3"

//------------------------------------------------------------------------------
CNX_MoviePlayer::CNX_MoviePlayer()
    : debug(false)
	, m_iMediaType( 0 )
	, m_bVideoMute( 0 )
	, m_pAudioDeviceName(NULL)
    , m_Loop(NULL)
    , m_Bus(NULL)
    , m_Pipeline(NULL)
    , m_WatchId(0)
{
	pthread_mutex_init( &m_hLock, NULL );

    gst_init(0, NULL);

    memset(&m_MediaInfo, 0, sizeof(GST_MEDIA_INFO));
    m_pDiscover = new CNX_Discover();
}

CNX_MoviePlayer::~CNX_MoviePlayer()
{
	pthread_mutex_destroy( &m_hLock );
}

//================================================================================================================
//public methods	commomn Initialize , close
int CNX_MoviePlayer::InitMediaPlayer(void (*pCbEventCallback)(void *privateDesc,
                                                                 unsigned int EventType,
                                                                 unsigned int EventData,
                                                                 unsigned int param),
                                     void *pCbPrivate,
                                     const char *pUri,
                                     int mediaType,
                                     int DspWidth,
                                     int DspHeight,
                                     char *pAudioDeviceName,
                                     void (*pCbQtUpdateImg)(void *pImg))
{
	CNX_AutoLock lock( &m_hLock );

	m_pAudioDeviceName = pAudioDeviceName;

    GetMediaInfo(pUri);
#if 0
    /* TODO: nxvideosink property 'dst-x, dst-y, dst-w, dst-h' using DSP_RECT */
    GetAspectRatio(imagWidth,imagHeight,
                   DspWidth,DspHeight,
                   &m_dstDspRect);
#endif
	return 0;
}

static void on_decodebin_pad_added_demux (GstElement *element,
                        GstPad *pad,
                        MovieData *data)
{
	NXLOGE("%s() ++++++++++++++++++++++++++++++=", __FUNCTION__);
    GstPad *sinkpad;
    GstCaps *caps;
    GstStructure *gstr;
	const gchar *mime_type;
    GstElement *sink_pad_audio = data->audioconvert;

    caps = gst_pad_get_current_caps (pad);
    if (caps == NULL) {
        NXLOGE("%s() Failed to get current caps", __FUNCTION__);
        return;
    }

    gstr = gst_caps_get_structure (caps, 0);
	if (gstr == NULL) {
        NXLOGE("%s() Failed to get current caps", __FUNCTION__);
        return;
    }

	mime_type = gst_structure_get_name(gstr);
	NXLOGI("%s(): name:%s", __FUNCTION__, mime_type);
	if (g_str_has_prefix(mime_type, "audio/")) {
        NXLOGI("%s(): Dynamic audio pad created, linking decodebin <-> audioconvert", __FUNCTION__);
        sinkpad = gst_element_get_static_pad (sink_pad_audio, "sink");
		if (NULL == sinkpad) {
			NXLOGE("%s() Failed to get static pad", __FUNCTION__);
			return;
		}

		if (GST_PAD_LINK_FAILED(gst_pad_link (pad, sinkpad))) {
			NXLOGE("%s() Failed to link %s:%s to %s:%s",
					__FUNCTION__,
					GST_DEBUG_PAD_NAME(pad),
					GST_DEBUG_PAD_NAME(sinkpad));
        }
    }
	NXLOGE("%s() -----------------------------------------", __FUNCTION__);
}

static void
on_pad_added_demux (GstElement *element,
                        GstPad *pad,
                        MovieData *data)
{
    GstPad *sinkpad;
    GstCaps *caps;
	GstStructure *structure;
	const gchar *mime_type;
	gint width = 0, height = 0;

    GstElement *sink_pad_audio = data->audio_queue;
    GstElement *sink_pad_video = data->video_queue;

    caps = gst_pad_get_current_caps (pad);
    if (caps == NULL) {
        NXLOGE("%s() Failed to get current caps", __FUNCTION__);
        return;
    }

	structure = gst_caps_get_structure (caps, 0);
	if (structure == NULL) {
        NXLOGE("%s() Failed to get current caps", __FUNCTION__);
        return;
    }

	mime_type = gst_structure_get_name(structure);
	NXLOGI("%s(): mime_type:%s", __FUNCTION__, mime_type);
	if (g_str_has_prefix(mime_type, "video/")) {
        sinkpad = gst_element_get_static_pad (sink_pad_video, "sink");

		// caps parsing
		gst_structure_get_int (structure, "width", &width);
		gst_structure_get_int (structure, "height", &height);
	} else if (g_str_has_prefix(mime_type, "audio/")) {
        sinkpad = gst_element_get_static_pad (sink_pad_audio, "sink");
    }

	if (g_str_has_prefix(mime_type, "video/") || g_str_has_prefix(mime_type, "audio/"))
    {
        if (NULL == sinkpad) {
            NXLOGE("%s() Failed to get static pad", __FUNCTION__);
            return;
        }

        if (GST_PAD_LINK_FAILED(gst_pad_link (pad, sinkpad))) {
            NXLOGE("%s() Failed to link %s:%s to %s:%s",
                    __FUNCTION__,
                    GST_DEBUG_PAD_NAME(pad),
                    GST_DEBUG_PAD_NAME(sinkpad));
        }

        NXLOGI("%s() Succeed to create dynamic pad link %s:%s to %s:%s",
                __FUNCTION__,
                GST_DEBUG_PAD_NAME(pad),
                GST_DEBUG_PAD_NAME(sinkpad));

        gst_object_unref (sinkpad);
    }
}

static void
cb_typefind_demux (GstElement *typefind,
					guint       probability,
					GstCaps    *caps,
					gpointer    data)
{
	gchar *type = NULL;

	type = gst_caps_to_string (caps);
	NXLOGI("%s() type:%s", __FUNCTION__, type);
}

void CNX_MoviePlayer::registerCb(void (*pCbEventCallback)(void *privateDesc, unsigned int EventType, unsigned int EventData, unsigned int param))
{
    event_cb = pCbEventCallback;
}

int CNX_MoviePlayer::SetupGStreamer(void (*pCbEventCallback)(void *privateDesc, unsigned int EventType, unsigned int EventData, unsigned int param),
                                    const char *uri, int DspWidth, int DspHeight)
{
    NXLOGI("%s(): start to setup GStreamer", __FUNCTION__);

    gdouble vol = 1.0;
    GstStateChangeReturn ret;

	if (0 > GetMediaInfo(uri))
	{
		NXLOGE("%s() Failed to get media info", __FUNCTION__);
		return -1;
	}

    memset(&m_data, 0, sizeof(MovieData));

    registerCb(pCbEventCallback);

    if (initialize() < 0) {
        NXLOGE("%s() Failed to init GStreamer", __FUNCTION__);
		return -1;
    }

    memset(&m_dstDspRect, 0, sizeof(DSP_RECT));
    GetAspectRatio(m_MediaInfo.iWidth, m_MediaInfo.iHeight,
                   DspWidth, DspHeight,
                   &m_dstDspRect);

    /* MP4
     * gst-launch-1.0 gst-launch-1.0 filesrc location=/tmp/media/sda1/05_A\ Pink\ -\ NoNoNo\ \(1080p_H.264-AAC\).mp4
     * ! qtdemux name=demux
     * demux.audio_0 ! queue2 ! decodebin ! audioconvert ! audioresample ! alsasink
     * demux.video_0 ! queue2 ! h264parse ! nxvideodec ! videoconvert ! videoscale ! nxvideosink
     */

    //	Source
	m_data.source = gst_element_factory_make ("filesrc", "source");
	g_object_set(m_data.source, "location", uri, NULL);

	m_data.filesrc_typefind = gst_element_factory_make ("typefind", "typefind");

	//	Demux
	if ((g_strcmp0(m_MediaInfo.container_format, "video/quicktime") == 0) ||	// Quicktime
		(g_strcmp0(m_MediaInfo.container_format, "application/x-3gp") == 0))	// 3GP
	{
		m_data.demuxer = gst_element_factory_make ("qtdemux", "demux");
	}
	else if (g_strcmp0(m_MediaInfo.container_format, "video/x-matroska") == 0)	// Matroska
	{
		m_data.demuxer = gst_element_factory_make ("matroskademux", "matroskademux");
	}
	else if (g_strcmp0(m_MediaInfo.container_format, "video/x-msvideo") == 0)	// AVI
	{
		m_data.demuxer = gst_element_factory_make ("avidemux", "avidemux");
	}
	else if (g_strcmp0(m_MediaInfo.container_format, "video/mpeg") == 0)	// MPEG (vob)
	{
		m_data.demuxer = gst_element_factory_make ("mpegpsdemux", "mpegpsdemux");
	}
	if (!m_data.demuxer)
	{
		NXLOGE("%s(): Failed to create demuxer. Exiting", __FUNCTION__);
		return -1;
	}

#if 1
	/* Audio */
    m_data.audio_queue = gst_element_factory_make ("queue2", "audio_queue");
	/*if ((g_strcmp0(m_MediaInfo.audio_codec, "audio/mpeg") == 0) && (m_MediaInfo.audio_mpegversion <= 2))
	{
		m_data.audio_parser = gst_element_factory_make ("mpegaudioparse", "mpegaudioparse");
		m_data.audio_decoder = gst_element_factory_make ("mpg123audiodec", "mpg123audiodec");
		if (!m_data.audio_parser || !m_data.audio_decoder)
		{
			NXLOGE("%s(): Failed to create audio_parser or audio_parser for the MIME-type 'audio/mpeg'. Exiting", __FUNCTION__);
			return -1;
		}
	}
	else
	{*/
		m_data.decodebin = gst_element_factory_make ("decodebin", "decodebin");
		if (!m_data.decodebin)
		{
			NXLOGE("%s(): Failed to create all elements. Exiting", __FUNCTION__);
			return -1;
		}
	//}
    m_data.audioconvert = gst_element_factory_make ("audioconvert", "audioconvert");
    m_data.audioresample = gst_element_factory_make ("audioresample", "audioresample");
    m_data.autoaudiosink = gst_element_factory_make ("autoaudiosink", "autoaudiosink");

	/* Video */
    m_data.video_queue = gst_element_factory_make ("queue2", "video_queue");
	if (g_strcmp0(m_MediaInfo.video_codec, "video/x-h264") == 0)
	{
		m_data.video_parser = gst_element_factory_make ("h264parse", "parser");
		if (!m_data.video_parser)
		{
			NXLOGE("%s(): Failed to create all elements. Exiting", __FUNCTION__);
			return -1;
		}
	}
	else if ((g_strcmp0(m_MediaInfo.video_codec, "video/mpeg") == 0) && (m_MediaInfo.video_mpegversion <= 2))
	{
		m_data.video_parser = gst_element_factory_make ("mpegvideoparse", "parser");
		if (!m_data.video_parser)
		{
			NXLOGE("%s(): Failed to create all elements. Exiting", __FUNCTION__);
			return -1;
		}
	}

	m_data.nxdecoder = gst_element_factory_make ("nxvideodec", "nxvideodec");
    m_data.nxvideosink = gst_element_factory_make ("nxvideosink", "nxvideosink");

	if(!m_data.source ||
			!m_data.audio_queue || !m_data.audioconvert ||
            !m_data.audioresample || !m_data.autoaudiosink ||
			!m_data.video_queue || !m_data.nxdecoder || !m_data.nxvideosink)
    {
        NXLOGE("%s(): Failed to create all elements. Exiting", __FUNCTION__);
        return -1;
    }

	NXLOGI("%s(): Add elements to bin", __FUNCTION__);
	if ( (g_strcmp0(m_MediaInfo.video_codec, "video/x-h264") == 0) ||
		  ((g_strcmp0(m_MediaInfo.video_codec, "video/mpeg") == 0) && (m_MediaInfo.video_mpegversion <= 2)) )
	{
		gst_bin_add_many(GST_BIN(m_Pipeline)
						 , m_data.source, m_data.filesrc_typefind, m_data.demuxer
						 , m_data.audio_queue, m_data.decodebin, m_data.audioconvert, m_data.audioresample, m_data.autoaudiosink
						 , m_data.video_queue, m_data.video_parser, m_data.nxdecoder, m_data.nxvideosink
						 , NULL);
	}
	else
	{
		/*if ((g_strcmp0(m_MediaInfo.audio_codec, "audio/mpeg") == 0) && (m_MediaInfo.audio_mpegversion <= 2))
		{
			gst_bin_add_many(GST_BIN(m_Pipeline)
							 , m_data.source, m_data.filesrc_typefind, m_data.demuxer
							 , m_data.audio_queue, m_data.audio_parser, m_data.audio_decoder, m_data.audioconvert, m_data.audioresample, m_data.autoaudiosink
							 , m_data.video_queue, m_data.video_parser, m_data.nxdecoder, m_data.nxvideosink
							 , NULL);
		}
		else {*/
			gst_bin_add_many(GST_BIN(m_Pipeline)
							 , m_data.source, m_data.filesrc_typefind, m_data.demuxer
							 , m_data.audio_queue, m_data.decodebin, m_data.audioconvert, m_data.audioresample, m_data.autoaudiosink
							 , m_data.video_queue, /*m_data.video_parser, */m_data.nxdecoder, m_data.nxvideosink
							 , NULL);
		//}
	}

    NXLOGI("%s(): Link elements", __FUNCTION__);
	if (!gst_element_link_many (m_data.source, m_data.filesrc_typefind, m_data.demuxer, NULL))
    {
        NXLOGE("%s(): Failed to link src<-->demux", __FUNCTION__);
        gst_object_unref(m_Pipeline);
        return -1;
    }
	/* Link video elements */
	if ((g_strcmp0(m_MediaInfo.video_codec, "video/x-h264") == 0) ||
			((g_strcmp0(m_MediaInfo.video_codec, "video/mpeg") == 0) && (m_MediaInfo.video_mpegversion <= 2)) )
	{
		if (!gst_element_link_many (m_data.video_queue, m_data.video_parser, m_data.nxdecoder, m_data.nxvideosink, NULL)) {
			NXLOGE("%s(): Failed to link video elements with video_parser", __FUNCTION__);
			gst_object_unref(m_Pipeline);
			return -1;
		}
	}
	else
	{
		// avi
		if (!gst_element_link_many (m_data.video_queue, m_data.nxdecoder, m_data.nxvideosink, NULL)) {
			NXLOGE("%s(): Failed to link video elements", __FUNCTION__);
			gst_object_unref(m_Pipeline);
			return -1;
		}
	}

	/*if ((g_strcmp0(m_MediaInfo.audio_codec, "audio/mpeg") == 0) && (m_MediaInfo.audio_mpegversion <= 2))
	{
		if (!gst_element_link_many (m_data.audio_queue, m_data.audio_parser, m_data.audio_decoder, NULL))
		{
			NXLOGE("%s(): Failed to link audio_queue<-->decodebin", __FUNCTION__);
			gst_object_unref(m_Pipeline);
			return -1;
		}
	}
	else
	{*/
		if (!gst_element_link (m_data.audio_queue, m_data.decodebin))
		{
			NXLOGE("%s(): Failed to link audio_queue<-->decodebin", __FUNCTION__);
			gst_object_unref(m_Pipeline);
			return -1;
		}
	//}

    if (!gst_element_link_many (m_data.audioconvert, m_data.audioresample, m_data.autoaudiosink, NULL))
    {
        NXLOGE("%s(): Failed to link audioconvert<-->audioresample<-->autoaudiosink", __FUNCTION__);
        gst_object_unref(m_Pipeline);
        return -1;
    }

    NXLOGI("%s(): set pad-added signal", __FUNCTION__);
	//g_signal_connect (m_data.filesrc_typefind,	"have-type", G_CALLBACK (cb_typefind_demux), &m_data);

	/* demuxer <--> audio_queue/video_queue */
	g_signal_connect (m_data.demuxer,	"pad-added", G_CALLBACK (on_pad_added_demux), &m_data);


	/*if ((g_strcmp0(m_MediaInfo.audio_codec, "audio/mpeg") == 0) && (m_MediaInfo.audio_mpegversion <= 2))
	{
		// audio_decoder <--> audio_converter
		g_signal_connect (m_data.audio_decoder, "pad-added", G_CALLBACK (on_decodebin_pad_added_demux), &m_data);
	}
	else
	{*/
		// decodbin <--> audio_converter
		g_signal_connect (m_data.decodebin, "pad-added", G_CALLBACK (on_decodebin_pad_added_demux), &m_data);
	//}
#else
	m_data.video_parser = gst_element_factory_make ("h264parse", "parser");
	if (!m_data.video_parser)
    {
        printf( "h264parse (Linux) could not be created. Exiting.\n");
        return -1;
    }

    //	Decoder
	m_data.nxdecoder = gst_element_factory_make ("nxvideodec", "decoder");
	if (!m_data.nxdecoder)
    {
        NXLOGE("%s(): Failed to create nxvideodec. Exiting", __FUNCTION__);
        return -1;
    }

    //	Video Sink
    m_data.nxvideosink = gst_element_factory_make("nxvideosink", "sink");
    if (!m_data.nxvideosink)
    {
        NXLOGE("%s(): Failed to create nxvideosink. Exiting", __FUNCTION__);
        return -1;
    }

	if(!m_data.source || !m_data.demuxer || !m_data.video_parser || !m_data.nxdecoder || !m_data.nxvideosink)
    {
        NXLOGE("%s(): Failed to create all elements. Exiting", __FUNCTION__);
        return -1;
    }

    //	Add Elements
    gst_bin_add_many(GST_BIN(m_Pipeline),
					  m_data.source, m_data.demuxer,
					  m_data.video_parser, m_data.nxdecoder, m_data.nxvideosink,
                      NULL);

    //	Link Elements
	if (!gst_element_link (m_data.source, m_data.demuxer)
			|| !gst_element_link_many (m_data.video_parser, m_data.nxdecoder, m_data.nxvideosink, NULL)) {
        NXLOGE("%s(): Failed to link elements", __FUNCTION__);
        gst_object_unref(m_Pipeline);
        return -1;
    }

    // Connect to the pad-added signal
	g_signal_connect(m_data.demuxer, "pad-added", G_CALLBACK (on_pad_added_demux), &m_data);
#endif

    //	Set Default Position
    g_object_set (G_OBJECT (m_data.nxvideosink), "dst-x", m_dstDspRect.iX, NULL);
    g_object_set (G_OBJECT (m_data.nxvideosink), "dst-y", m_dstDspRect.iY, NULL);
    g_object_set (G_OBJECT (m_data.nxvideosink), "dst-w", m_dstDspRect.iWidth, NULL);
    g_object_set (G_OBJECT (m_data.nxvideosink), "dst-h", m_dstDspRect.iHeight, NULL);

    //	Pipeline
    ret = gst_element_set_state (m_Pipeline, GST_STATE_READY);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        NXLOGE("%s(): Failed to set the pipeline to the READY state", __FUNCTION__);
        gst_object_unref(m_Pipeline);
        return -1;
    }

    return 0;
}

int CNX_MoviePlayer::CloseHandle()
{
    NXLOGE("%s()", __FUNCTION__);
	CNX_AutoLock lock( &m_hLock );

    if(NULL == m_Pipeline) {
        NXLOGE("%s(): Error! pipeline is NULL", __FUNCTION__);
        return -1;
    }
    deinitialize();
	return 0;
}

static gboolean cb_print_position (GstElement *pipeline)
{
    gint64 pos, dur;

    if(pipeline == NULL) {
        NXLOGE("%s(): Error! pipeline is NULL", __FUNCTION__);
        return FALSE;
    }

    if (!gst_element_query_position (pipeline, GST_FORMAT_TIME, &pos)
        || !gst_element_query_duration (pipeline, GST_FORMAT_TIME, &dur)) {
        NXLOGE("%s() Failed to query position or duration", __FUNCTION__);
    }

    //NXLOGD("%s(): Time: %" GST_TIME_FORMAT " / %" GST_TIME_FORMAT "\r",
    //        __FUNCTION__, GST_TIME_ARGS (pos), GST_TIME_ARGS (dur));

    return TRUE;
}

int CNX_MoviePlayer::initialize()
{
    NXLOGE("%s()", __FUNCTION__);

    m_Pipeline = gst_pipeline_new("my_pipeline");
    m_Loop = g_main_loop_new(NULL, false);
    m_Bus = gst_pipeline_get_bus(GST_PIPELINE(m_Pipeline));
    m_WatchId = gst_bus_add_watch(m_Bus, bus_callback, m_Loop);

    if (!m_Pipeline || !m_Loop || !m_Bus || !m_WatchId)
    {
        NXLOGE("%s(): Failed to create elements", __FUNCTION__);
        return -1;
    }

    //g_timeout_add(QUERY_TIMEOUT_300MS, (GSourceFunc) cb_print_position, m_Pipeline);
    return 0;
}

int CNX_MoviePlayer::deinitialize()
{
    NXLOGE("%s()", __FUNCTION__);

    if(NULL != m_Pipeline)
    {
        gst_object_unref (GST_OBJECT (m_Pipeline));
		m_Pipeline = NULL;
    }

    g_source_remove (m_WatchId);

    if(NULL != m_Loop)
    {
        g_main_loop_unref (m_Loop);
		m_Loop = NULL;
    }

    /* TODO */
    m_GstState = GST_STATE_NULL;
    return 0;
}

static gboolean bus_callback (GstBus *bus, GstMessage *msg, gpointer  data)
{
    GMainLoop *loop = (GMainLoop*)data;

    switch (GST_MESSAGE_TYPE (msg))
    {
        case GST_MESSAGE_EOS:
            NXLOGI("%s(): End-of-stream", __FUNCTION__);
            event_cb(NULL, GST_MESSAGE_EOS, 0, 0);
            break;
        case GST_MESSAGE_ERROR:
        {
            gchar *debug = NULL;
            GError *err = NULL;

            gst_message_parse_error (msg, &err, &debug);

            NXLOGE("%s(): Gstreamer error: %s", __FUNCTION__, err->message);
            g_error_free (err);

            NXLOGI("%s(): Debug details: %s", __FUNCTION__, debug);
            g_free (debug);

            g_main_loop_quit (loop);
            break;
        }
        case GST_MESSAGE_STATE_CHANGED:
        {
            GstState old_state, new_state;

            gst_message_parse_state_changed (msg, &old_state, &new_state, NULL);
            NXLOGI("%s() Element '%s' changed state from '%s' to '%s'",
                __FUNCTION__,
                GST_OBJECT_NAME (msg->src),
                gst_element_state_get_name (old_state),
                gst_element_state_get_name (new_state));

            // TODO: Send the status to player video frame
            if(strcmp("my_pipeline", GST_OBJECT_NAME (msg->src))) {
                m_GstState = new_state;
            }
            break;
        }
        default:
        {
            GstMessageType type = GST_MESSAGE_TYPE(msg);
            NXLOGI("%s(): Received GST_MESSAGE_TYPE [%s]",
                   __FUNCTION__, gst_message_type_get_name(type));
            break;
        }
    }
    return TRUE;
}


//================================================================================================================
//public methods	common Control
int CNX_MoviePlayer::SetVolume(int volume)
{
    CNX_AutoLock lock( &m_hLock );
    gdouble vol = 0.0;
    if(NULL == m_Pipeline)
    {
        NXLOGE("%s(): Error! pipeline is NULL", __FUNCTION__);
        return -1;
    }

    vol = (double)volume/100.;
    NXLOGI("%s() set volume to %f", __FUNCTION__, vol);
    g_object_set (G_OBJECT (m_data.volume), "volume", vol, NULL);
    // gst_stream_volume_convert_volume
    // gst_stream_volume_get_volume (GST_STREAM_VOLUME (volume), GST_STREAM_VOLUME_FORMAT_LINEAR/CUBIE);
    // gst_stream_volume_set_volume (GST_STREAM_VOLUME (volume), GST_STREAM_VOLUME_FORMAT_LINEAR/CUBIE, 1.0);

    return 0;
}

int CNX_MoviePlayer::Play()
{
	NXLOGI("%s() +++++++++++++++++++++++ GstState '%s'", __FUNCTION__, gst_element_state_get_name (m_GstState));
	CNX_AutoLock lock( &m_hLock );

    if(NULL == m_Pipeline)
    {
        NXLOGE("%s(): Error! pipeline is NULL", __FUNCTION__);
        return -1;
    }

    GstStateChangeReturn ret;
    ret = gst_element_set_state (m_Pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        NXLOGE("Failed to set the pipeline to the PLAYING state");
        return -1;
    }

	NXLOGE( "%s() ----------------------------------------", __FUNCTION__);
	return 0;
}

static int
seek_to_time (GstElement *pipeline,
          gint64      time_nanoseconds)
{
    if(!gst_element_seek (pipeline, 1.0, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH,
                          GST_SEEK_TYPE_SET, time_nanoseconds,
                          GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE)) {
        NXLOGE("%s(): Failed to seek %lld!", __FUNCTION__, time_nanoseconds);
        return -1;
    }
    return 0;
}

int CNX_MoviePlayer::Seek(qint64 position)
{
	CNX_AutoLock lock( &m_hLock );

    if(NULL == m_Pipeline)
    {
        NXLOGE("%s(): Error! pipeline is NULL", __FUNCTION__);
        return -1;
    }

    //NXLOGI("%s(): Try to seek %lld mSec!", __FUNCTION__, position);
    return seek_to_time(m_Pipeline, position*(1000*1000)/*mSec to NanoSec*/);
}

int CNX_MoviePlayer::Pause()
{
	NXLOGI( "%s()", __FUNCTION__);
	CNX_AutoLock lock( &m_hLock );

    if(NULL == m_Pipeline)
    {
        NXLOGE("%s(): Error! pipeline is NULL", __FUNCTION__);
        return -1;
    }

    GstStateChangeReturn ret;
    ret = gst_element_set_state(m_Pipeline, GST_STATE_PAUSED);
	if (ret == GST_STATE_CHANGE_FAILURE)
	{
        NXLOGE("Failed to set the pipeline to the PAUSED state");
    }
    return 0;
}

int CNX_MoviePlayer::Stop()
{
    NXLOGE("%s()", __FUNCTION__);
	CNX_AutoLock lock( &m_hLock );

    if(NULL == m_Pipeline)
    {
        NXLOGE("%s(): Error! pipeline is NULL", __FUNCTION__);
        return -1;
    }

    GstStateChangeReturn ret;
    ret = gst_element_set_state(m_Pipeline, GST_STATE_NULL);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        NXLOGE("Failed to set the pipeline to the NULL state");
        return -1;
    }

	return 0;
}

//================================================================================================================
//public methods	common information
qint64 CNX_MoviePlayer::GetMediaPosition()
{
    CNX_AutoLock lock( &m_hLock );

	//NXLOGI("%s() GstState '%s'", __FUNCTION__, gst_element_state_get_name (m_GstState));

    if(NULL == m_Pipeline)
    {
        NXLOGE("%s(): Error! pipeline is NULL", __FUNCTION__);
        return -1;
    }

    gint64 iPosition = 0;
    if(gst_element_query_position (m_Pipeline, GST_FORMAT_TIME, &iPosition))
    {
        if(debug)
        {
            NXLOGI("%s(): Time: %" GST_TIME_FORMAT "\r",
                    __FUNCTION__, GST_TIME_ARGS (iPosition));
        }
    }
    else {
        NXLOGE("%s(): Failed to query POSITION", __FUNCTION__);
        return -1;
    }

    return iPosition;
}

qint64 CNX_MoviePlayer::GetMediaDuration()
{
	CNX_AutoLock lock( &m_hLock );

	//NXLOGI("%s() GstState '%s'", __FUNCTION__, gst_element_state_get_name (m_GstState));

	if(NULL == m_Pipeline)
    {
		NXLOGE("%s(): Error! pipeline is NULL", __FUNCTION__);
		return -1;
    }

	if (GST_STATE_PLAYING != m_GstState)
	{
		//NXLOGI("%s() duration '%" GST_TIME_FORMAT "'\r"
		//	   , __FUNCTION__, GST_TIME_ARGS (m_MediaInfo.iDuration));
		return m_MediaInfo.iDuration;
	}

    qint64 duration = 0;
    if(gst_element_query_duration(m_Pipeline, GST_FORMAT_TIME, &duration))
    {
		if(debug)
        {
			NXLOGD("%s() Time: %" GST_TIME_FORMAT "\r",
                    __FUNCTION__, GST_TIME_ARGS (duration));
        }
    }
    else
    {
        NXLOGE("%s(): Failed to query DURATION", __FUNCTION__);
        return -1;
    }

    return duration;
}

GstState CNX_MoviePlayer::GetState()
{
	CNX_AutoLock lock( &m_hLock );

    NXLOGI("%s(): %s", __FUNCTION__, gst_element_state_get_name (m_GstState));
    return m_GstState;
}

void CNX_MoviePlayer::PrintMediaInfo( const char *pUri )
{

	NXLOGD("####################################################################################################\n");
	NXLOGD( "FileName : %s\n", pUri );

	NXLOGI("%s() container(%s), video codec(%s)"
		   ", audio codec(%s), seekable(%s), width(%d), height(%d)"
		   ", duration: (%" GST_TIME_FORMAT ")\r"
		   , __FUNCTION__
		   , m_MediaInfo.container_format
		   , m_MediaInfo.video_codec
		   , m_MediaInfo.audio_codec
		   , m_MediaInfo.isSeekable ? "yes":"no"
		   , m_MediaInfo.iWidth
		   , m_MediaInfo.iHeight
		   , GST_TIME_ARGS (m_MediaInfo.iDuration));

	NXLOGD( "####################################################################################################\n");
}

//================================================================================================================
//public methods	video information

//================================================================================================================
//private methods	for InitMediaPlayer
int CNX_MoviePlayer::OpenHandle( void (*pCbEventCallback)( void *privateDesc, unsigned int EventType, unsigned int /*EventData*/, unsigned int /*param*/ ),
								 void *cbPrivate )
{
#if 0
	MP_RESULT iResult = NX_MPOpen( &m_hPlayer, pCbEventCallback, cbPrivate );

	if( MP_ERR_NONE != iResult )
	{
		NXLOGE( "%s: Error! Handle is not initialized!\n", __FUNCTION__ );
		return -1;
	}
#endif
	return 0;
}

int CNX_MoviePlayer::GetMediaInfo(const char* uri)
{
    NXLOGE( "%s()", __FUNCTION__);

    if (m_pDiscover != NULL) {
        if (0 > m_pDiscover->StartDiscover(uri, &m_MediaInfo))
        {
            NXLOGE("%s() Failed to get media info", __FUNCTION__);
            return -1;
        }

		NXLOGI("%s() container(%s), video codec(%s)"
			   ", audio codec(%s), seekable(%s), width(%d), height(%d)"
			   ", duration: (%" GST_TIME_FORMAT ")\r"
			   , __FUNCTION__
			   , m_MediaInfo.container_format
			   , m_MediaInfo.video_codec
			   , m_MediaInfo.audio_codec
			   , m_MediaInfo.isSeekable ? "yes":"no"
			   , m_MediaInfo.iWidth
			   , m_MediaInfo.iHeight
			   , GST_TIME_ARGS (m_MediaInfo.iDuration));
    } else {
        NXLOGE("%s() Error! m_pDiscover is NULL", __FUNCTION__);
        return -1;
    }

	return 0;
}

//================================================================================================================
void CNX_MoviePlayer::DrmVideoMute(int bOnOff)
{
    if(NULL == m_Pipeline) {
        NXLOGE("%s(): Error! pipeline is NULL", __FUNCTION__);
        return;
    }

    if(NULL == m_data.nxvideosink) {
        NXLOGE("%s(): Error! sink is NULL", __FUNCTION__);
        return;
    }

    m_bVideoMute = bOnOff;
    /* TODO */
	if (bOnOff)
	{
		g_object_set (G_OBJECT (m_data.nxvideosink), "dst-y", /*m_Y*/720, NULL);
	}
	else
	{
		g_object_set (G_OBJECT (m_data.nxvideosink), "dst-y", 720, NULL);
	}
}

//================================================================================================================
void CNX_MoviePlayer::GetAspectRatio(int srcWidth, int srcHeight,
									 int dspWidth, int dspHeight,
                                     DSP_RECT *pDspDstRect)
{
    NXLOGI("%s() srcWidth(%d),  srcHeight(%d), dspWidth(%d), dspWidth(%d)"
           , __FUNCTION__, srcWidth, srcHeight, dspWidth, dspHeight);

	// Calculate Video Aspect Ratio
	double xRatio = (double)dspWidth / (double)srcWidth;
	double yRatio = (double)dspHeight / (double)srcHeight;

	if( xRatio > yRatio )
	{
		pDspDstRect->iWidth    = (int)((double)srcWidth * yRatio);
		pDspDstRect->iHeight   = dspHeight;
	}
	else
	{
		pDspDstRect->iWidth    = dspWidth;
		pDspDstRect->iHeight   = (int)((double)srcHeight * xRatio);
	}

	if(dspWidth != pDspDstRect->iWidth)
	{
		if(dspWidth > pDspDstRect->iWidth)
		{
			pDspDstRect->iX = (dspWidth - pDspDstRect->iWidth)/2;
		}
	}

	if(dspHeight != pDspDstRect->iHeight)
	{
		if(dspHeight > pDspDstRect->iHeight)
		{
			pDspDstRect->iY = (dspHeight - pDspDstRect->iHeight)/2;
		}
	}
}
