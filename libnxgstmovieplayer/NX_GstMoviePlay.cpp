#include "NX_GstMoviePlay.h"
#include "GstDiscover.h"
#include "NX_DbgMsg.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define LOG_TAG "[NxGstVideoPlayer]"
#include <NX_Log.h>

#include <math.h>

struct MOVIE_TYPE {
	GstElement *pipeline;

	GstElement *source;

	GstElement *audio_queue;
	GstElement *video_queue;

	GstElement *demuxer;
	GstElement *video_parser;
	GstElement *nxdecoder;
	GstElement *nxvideosink;

	GstElement *decodebin;
	GstElement *audio_parser;
	GstElement *audio_decoder;
	GstElement *audioconvert;
	GstElement *audioresample;
	GstElement *autoaudiosink;

	GstElement *volume;

	GMainLoop *loop;
	GThread *thread;
	GstBus *bus;

	pthread_mutex_t apiLock;

	gboolean pipeline_is_linked;

	//	Medi Information & URI
	NX_URI_TYPE uri_type;
	gchar *uri;

	GST_MEDIA_INFO gst_media_info;

	NX_GST_ERROR error;

	//	Callback
	void (*callback)(void *, unsigned int EventType, unsigned int EventData, unsigned int param);
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

static gpointer loop_func(gpointer data);
static void start_loop_thread(MP_HANDLE handle);
static void stop_my_thread(MP_HANDLE handle);
static gboolean gst_bus_callback(GstBus *bus, GstMessage *msg, MP_HANDLE handle);

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

static void
on_pad_added_demux (GstElement *element,
                        GstPad *pad, gpointer data)
{
    GstPad *sinkpad;
    GstCaps *caps;
	GstStructure *structure;
	const gchar *mime_type;
	GstElement *target_sink_pad = NULL;
	gint width = 0, height = 0;
	MP_HANDLE handle = (MP_HANDLE)data;

	FUNC_IN();

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
	NXLOGI("%s() mime_type:%s", __FUNCTION__, mime_type);
	if (g_str_has_prefix(mime_type, "video/")) {
		target_sink_pad = handle->video_queue;
        sinkpad = gst_element_get_static_pad (target_sink_pad, "sink");

		// caps parsing
		gst_structure_get_int (structure, "width", &width);
		gst_structure_get_int (structure, "height", &height);
	} else if (g_str_has_prefix(mime_type, "audio/")) {
	    target_sink_pad = handle->audio_queue;
        sinkpad = gst_element_get_static_pad (target_sink_pad, "sink");
    }

	if (g_str_has_prefix(mime_type, "video/") || g_str_has_prefix(mime_type, "audio/"))
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
			handle->callback(NULL, (int)MP_EVENT_DEMUX_LINK_FAILED, 0, 0);
        }

        NXLOGI("%s() Succeed to create dynamic pad link %s:%s to %s:%s",
                __FUNCTION__,
                GST_DEBUG_PAD_NAME(pad),
                GST_DEBUG_PAD_NAME(sinkpad));

        gst_object_unref (sinkpad);
    }

	gst_caps_unref (caps);
	FUNC_OUT();
}

static gboolean gst_bus_callback (GstBus *bus, GstMessage *msg, MP_HANDLE handle)
{
    GMainLoop *loop = (GMainLoop*)handle->loop;

    switch (GST_MESSAGE_TYPE (msg))
    {
        case GST_MESSAGE_EOS:
            NXLOGI("%s() End-of-stream", __FUNCTION__);
            handle->callback(NULL, (int)MP_EVENT_EOS, 0, 0);
            break;
        case GST_MESSAGE_ERROR:
        {
            gchar *debug = NULL;
            GError *err = NULL;

            gst_message_parse_error (msg, &err, &debug);

            NXLOGE("%s() Gstreamer error: %s", __FUNCTION__, err->message);
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
            if(g_strcmp0("NxGstMoviePlay", GST_OBJECT_NAME (msg->src)) == 0) {
                NXLOGI("%s Element '%s' changed state from  '%s' to '%s'"
					   , __FUNCTION__
					   , GST_OBJECT_NAME (msg->src)
					   , gst_element_state_get_name (old_state)
					   , gst_element_state_get_name (new_state));
            }
            break;
        }
        default:
        {
            GstMessageType type = GST_MESSAGE_TYPE(msg);
			// TODO: parse tag, latency, stream-status, reset-time, async-done, new-clock, etc
            //NXLOGV("%s() Received GST_MESSAGE_TYPE [%s]",
            //       __FUNCTION__, gst_message_type_get_name(type));
            break;
        }
    }
    return TRUE;
}

void PrintMediaInfo(MP_HANDLE handle, const char *pUri)
{
	_CAutoLock lock(&handle->apiLock);

	FUNC_IN();

	NXLOGI("%s() [%s] container(%s), video codec(%s)"
		   ", audio codec(%s), seekable(%s), width(%d), height(%d)"
		   ", duration: (%" GST_TIME_FORMAT ")\r"
		   , __FUNCTION__
		   , pUri
		   , handle->gst_media_info.container_format
		   , handle->gst_media_info.video_codec
		   , handle->gst_media_info.audio_codec
		   , handle->gst_media_info.isSeekable ? "yes":"no"
		   , handle->gst_media_info.iWidth
		   , handle->gst_media_info.iHeight
		   , GST_TIME_ARGS (handle->gst_media_info.iDuration));

	FUNC_OUT();
}

void GetAspectRatio(int srcWidth, int srcHeight,
						  int dspWidth, int dspHeight,
                          DSP_RECT *pDspDstRect)
{
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

    NXLOGI("%s() srcWidth(%d), srcHeight(%d), dspWidth(%d), dspWidth(%d)"
		   " ==> iX(%d), iY(%d), iWidth(%d), iHeight(%d)"
           , __FUNCTION__, srcWidth, srcHeight, dspWidth, dspHeight,
           pDspDstRect->iX, pDspDstRect->iY, pDspDstRect->iWidth, pDspDstRect->iHeight);
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
    handle->autoaudiosink = gst_element_factory_make ("autoaudiosink", "autoaudiosink");

	if (!handle->audio_queue || !handle->decodebin || !handle->audioconvert ||
		!handle->audioresample || !handle->autoaudiosink)
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
	if (g_strcmp0(handle->gst_media_info.video_codec, "video/x-h264") == 0)
	{
		handle->video_parser = gst_element_factory_make ("h264parse", "parser");
		if (!handle->video_parser) {
			NXLOGE("%s() Failed to create h264parse element", __func__);
			return NX_GST_RET_ERROR;
		}
	}
	else if ((g_strcmp0(handle->gst_media_info.video_codec, "video/mpeg") == 0) &&
		     (handle->gst_media_info.video_mpegversion <= 2))
	{
		handle->video_parser = gst_element_factory_make ("mpegvideoparse", "parser");
		if (!handle->video_parser) {
			NXLOGE("%s() Failed to create mpegvideoparse element", __func__);
			return NX_GST_RET_ERROR;
		}
	}

	handle->nxdecoder = gst_element_factory_make ("nxvideodec", "nxvideodec");
    handle->nxvideosink = gst_element_factory_make ("nxvideosink", "nxvideosink");

	if(!handle->video_queue || !handle->nxdecoder || !handle->nxvideosink)
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

NX_GST_RET add_elements_to_bin(MP_HANDLE handle)
{
	FUNC_IN();

	if (!handle)
	{
		NXLOGE("%s() handle is NULL", __func__);
		return NX_GST_RET_ERROR;
	}

	if ((g_strcmp0(handle->gst_media_info.video_codec, "video/x-h264") == 0) ||
		 ((g_strcmp0(handle->gst_media_info.video_codec, "video/mpeg") == 0) && (handle->gst_media_info.video_mpegversion <= 2)) )
	{
		gst_bin_add_many(GST_BIN(handle->pipeline)
						 , handle->source, handle->demuxer
						 , handle->audio_queue, handle->decodebin, handle->audioconvert, handle->audioresample, handle->autoaudiosink
						 , handle->video_queue, handle->video_parser, handle->nxdecoder, handle->nxvideosink
						 , NULL);
	}
	else
	{
		gst_bin_add_many(GST_BIN(handle->pipeline)
						 , handle->source, handle->demuxer
						 , handle->audio_queue, handle->decodebin, handle->audioconvert, handle->audioresample, handle->autoaudiosink
						 , handle->video_queue, handle->nxdecoder, handle->nxvideosink
						 , NULL);
	}

	FUNC_OUT();

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
        NXLOGE("%s() Succeed to link %s<-->%s", __FUNCTION__,
				gst_element_get_name(handle->source), gst_element_get_name(handle->demuxer));
	}

	if((g_strcmp0(handle->gst_media_info.video_codec, "video/x-h264") == 0) ||
	    ((g_strcmp0(handle->gst_media_info.video_codec, "video/mpeg") == 0) && (handle->gst_media_info.video_mpegversion <= 2)) )
	{
		if (!gst_element_link_many(handle->video_queue, handle->video_parser, handle->nxdecoder, handle->nxvideosink, NULL)) {
			NXLOGE("%s() Failed to link video elements with video_parser", __func__);
			return NX_GST_RET_ERROR;
		}
	}
	else
	{
		if(!gst_element_link_many(handle->video_queue, handle->nxdecoder, handle->nxvideosink, NULL)) {// avi
			NXLOGE("%s() Failed to link video elements", __FUNCTION__);
			return NX_GST_RET_ERROR;
		}
		else
		{
			NXLOGE("%s() Succeed to link video_queue<-->nxdecoder<-->nxvideosink", __FUNCTION__);
		}
	}

	if (!gst_element_link(handle->audio_queue, handle->decodebin))
	{
		NXLOGE("%s() Failed to link audio_queue<-->decodebin", __FUNCTION__);
		return NX_GST_RET_ERROR;
	}

    if (!gst_element_link_many(handle->audioconvert, handle->audioresample, handle->autoaudiosink, NULL))
    {
        NXLOGE("%s() Failed to link audioconvert<-->audioresample<-->autoaudiosink", __FUNCTION__);
        return NX_GST_RET_ERROR;
    }

	FUNC_OUT();

	return NX_GST_RET_OK;
}

MP_HANDLE NX_GSTMP_CreateMPHandler()
{
	MP_HANDLE handle;
	handle = (MP_HANDLE)g_malloc0(sizeof(MOVIE_TYPE));
	if(NULL == handle) {
		NXLOGE("%s() Failed to alloc memory for handle", __func__);
		return NULL;
	}
	return handle;
}

NX_GST_RET NX_GSTMP_SetUri(MP_HANDLE handle, const char *pfilePath)
{
	FUNC_IN();

	GstStateChangeReturn ret;

	_CAutoLock lock(&handle->apiLock);

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
		   , pGstMInfo->video_codec
		   , pGstMInfo->audio_codec
		   , pGstMInfo->isSeekable ? "yes":"no"
		   , pGstMInfo->iWidth
		   , pGstMInfo->iHeight
		   , GST_TIME_ARGS (pGstMInfo->iDuration));

	if(handle->pipeline_is_linked) {
		NXLOGI("%s() pipeline is already linked", __func__);
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
	gst_bus_add_watch(handle->bus, (GstBusFunc)gst_bus_callback, handle);
	gst_object_unref(GST_OBJECT(handle->bus));

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

	// demuxer <--> audio_queue/video_queue
	g_signal_connect(handle->demuxer,	"pad-added", G_CALLBACK (on_pad_added_demux), handle);
	// decodbin <--> audio_converter
	g_signal_connect(handle->decodebin, "pad-added", G_CALLBACK (on_decodebin_pad_added_demux), handle);

	handle->pipeline_is_linked = TRUE;

	ret = gst_element_set_state(handle->pipeline, GST_STATE_READY);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        NXLOGE("%s() Failed to set the pipeline to the READY state", __FUNCTION__);
        gst_object_unref(handle->pipeline);
        return NX_GST_RET_ERROR;
    }
	
	start_loop_thread(handle);

	FUNC_OUT();

    return NX_GST_RET_OK;
}

NX_GST_RET NX_GSTMP_Open(MP_HANDLE *pHandle,
						  	   void (*cb)(void *owner, unsigned int msg,
                  				          unsigned int param1, unsigned int param2),
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
		stop_my_thread(handle);
		gst_element_set_state(handle->pipeline, GST_STATE_NULL);
		if (NULL != handle->pipeline)
		{
			gst_object_unref(GST_OBJECT(handle->pipeline));
			handle->pipeline = NULL;
		}
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
		return NX_GST_RET_ERROR;

	memcpy(pGstMInfo, &handle->gst_media_info, sizeof(GST_MEDIA_INFO));
 
	NXLOGI("%s() container(%s), video codec(%s)"
		   ", audio codec(%s), seekable(%s), width(%d), height(%d)"
		   ", duration: (%" GST_TIME_FORMAT ")\r"
		   , __FUNCTION__
		   , pGstMInfo->container_format
		   , pGstMInfo->video_codec
		   , pGstMInfo->audio_codec
		   , pGstMInfo->isSeekable ? "yes":"no"
		   , pGstMInfo->iWidth
		   , pGstMInfo->iHeight
		   , GST_TIME_ARGS (pGstMInfo->iDuration));

	FUNC_OUT();

    return NX_GST_RET_OK;
}

NX_GST_RET NX_GSTMP_SetAspectRatio(MP_HANDLE handle, int dspWidth, int dspHeight)
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

	DSP_RECT rect;
	memset(&rect, 0, sizeof(DSP_RECT));

    GetAspectRatio(handle->gst_media_info.iWidth,
		  		   handle->gst_media_info.iHeight,
                   dspWidth, dspHeight,
                   &rect);

	handle->gst_media_info.iX = rect.iX;
	handle->gst_media_info.iY = rect.iY;

	NXLOGD("%s() iX(%d), iY(%d), width(%d), height(%d), dspWidth(%d), dspHeight(%d)"
		   , __FUNCTION__
		   , rect.iX, rect.iY
		   , rect.iWidth, rect.iHeight
		   , dspWidth, dspHeight);

	g_object_set (G_OBJECT (handle->nxvideosink), "dst-x", rect.iX, NULL);
	g_object_set (G_OBJECT (handle->nxvideosink), "dst-y", rect.iY, NULL);
	g_object_set (G_OBJECT (handle->nxvideosink), "dst-w", rect.iWidth, NULL);
	g_object_set (G_OBJECT (handle->nxvideosink), "dst-h", rect.iHeight, NULL);

	FUNC_OUT();

    return NX_GST_RET_OK;
}

gint64 NX_GSTMP_GetPosition(MP_HANDLE handle)
{
	_CAutoLock lock(&handle->apiLock);

	GstState state, pending;
	GstStateChangeReturn ret;
	gint64 position;

	FUNC_IN();

	if (NULL == handle)
		return -1;

	ret = gst_element_get_state(handle->pipeline, &state, &pending, 500000000);		//	wait 500 msec
	if(GST_STATE_CHANGE_FAILURE != ret)
	{
		if(state == GST_STATE_PLAYING || state == GST_STATE_PAUSED)
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
            //NXLOGE("%s() Invalid state to query POSITION", __func__);
			return -1;
		}
	}
	else
	{
        //NXLOGE("%s() Failed to query POSITION", __func__);
		return -1;
	}

	FUNC_OUT();

	return position;
}

gint64 NX_GSTMP_GetDuration(MP_HANDLE handle)
{
	_CAutoLock lock(&handle->apiLock);

	GstState state, pending;
	GstStateChangeReturn ret;
	gint64 duration;

	FUNC_IN();

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
				//NXLOGI("%s() Duration: %" GST_TIME_FORMAT "\r",
                //        __func__, GST_TIME_ARGS (duration));
			}
		}
		else
		{
			//NXLOGE("%s() Invalid state to query DURATION", __func__);
			return -1;
		}
	}
	else
	{
		//NXLOGE("%s() Failed to query DURATION", __func__);
		return -1;
	}

	FUNC_OUT();

	return duration;
}

NX_GST_RET NX_GSTMP_SetVolume(MP_HANDLE handle, int volume)
{
	_CAutoLock lock(&handle->apiLock);

	FUNC_IN();

	if( !handle || !handle->autoaudiosink || !handle->volume)
		return NX_GST_RET_ERROR;

    gdouble vol = (double)volume/100.;
    NXLOGI("%s() set volume to %f", __func__, vol);
    g_object_set (G_OBJECT (handle->volume), "volume", vol, NULL);

	FUNC_OUT();

    return NX_GST_RET_OK;
}

static NX_GST_RET seek_to_time (GstElement *pipeline, gint64 time_nanoseconds)
{
    if(!gst_element_seek (pipeline, 1.0, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH,
                          GST_SEEK_TYPE_SET, time_nanoseconds,
                          GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE)) {
        NXLOGE("%s() Failed to seek %lld!", __func__, time_nanoseconds);
        return NX_GST_RET_ERROR;
    }
    return NX_GST_RET_OK;
}

NX_GST_RET NX_GSTMP_Seek(MP_HANDLE handle, gint64 seekTime)
{
	_CAutoLock lock(&handle->apiLock);

	GstState state, pending;
	GstStateChangeReturn ret;

	FUNC_IN();

	if(!handle || !handle->pipeline_is_linked)
	{
		NXLOGE("%s() : invalid state or invalid operation.(%p,%d)\n",
                __func__, handle, handle->pipeline_is_linked);
		return NX_GST_RET_ERROR;
	}

	ret = gst_element_get_state(handle->pipeline, &state, &pending, 500000000);		//	wait 500 msec
	if(GST_STATE_CHANGE_FAILURE != ret)
	{
		if(state == GST_STATE_PLAYING || state == GST_STATE_PAUSED || state == GST_STATE_READY)
		{
            return seek_to_time(handle->pipeline, seekTime*(1000*1000)); /*mSec to NanoSec*/
		}
		else
		{
			NXLOGE("%s() Invalid state to seek", __func__);
			return NX_GST_RET_ERROR;
		}
	}
	else
	{
		NXLOGE("%s() Failed to seek", __func__);
		return NX_GST_RET_ERROR;
	}

	FUNC_OUT();

	return NX_GST_RET_ERROR;
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

	ret = gst_element_set_state(handle->pipeline, GST_STATE_PLAYING);
	if(GST_STATE_CHANGE_FAILURE == ret)
    {
		NXLOGE("%s() Failed to set the pipeline to the PLAYING state(ret=%d)", __func__, ret);
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
		NXLOGE("%s() Failed to set the pipeline to the PLAYING state(ret=%d)", __func__, ret);
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

	ret = gst_element_set_state (handle->pipeline, GST_STATE_NULL);
	if(GST_STATE_CHANGE_FAILURE == ret)
    {
		NXLOGE("%s() Failed to set the pipeline to the PLAYING state(ret=%d)", __FUNCTION__, ret);
		return NX_GST_RET_ERROR;
	}

	FUNC_OUT();

	return	NX_GST_RET_OK;
}

NX_MEDIA_STATE NX_GSTMP_GetState(MP_HANDLE handle)
{
	_CAutoLock lock(&handle->apiLock);

	if(!handle || !handle->pipeline_is_linked)
	{
        NXLOGE("%s() invalid state or invalid operation.(%p,%d)\n",
                __FUNCTION__, handle, handle->pipeline_is_linked);
		return MP_STATE_UNKNOWN;
	}

	FUNC_IN();

	GstState state, pending;
	NX_MEDIA_STATE nx_state = MP_STATE_UNKNOWN;
	GstStateChangeReturn ret;
	ret = gst_element_get_state(handle->pipeline, &state, &pending, 500000000);		//	wait 500 msec
	if(GST_STATE_CHANGE_FAILURE != ret)
	{
		if (state == GST_STATE_NULL)
			nx_state = MP_STATE_STOPPED;
		else if (state == GST_STATE_READY)
			nx_state = MP_STATE_READY;
		else if (state == GST_STATE_PAUSED)
			nx_state = MP_STATE_PAUSED;
		else if (state == GST_STATE_PLAYING)
			nx_state = MP_STATE_PLAYING;
		else
			nx_state = MP_STATE_UNKNOWN;
	}
	else
	{
        NXLOGE("%s() Failed to query POSITION", __func__);
		nx_state = MP_STATE_UNKNOWN;
	}

	FUNC_OUT();
	return nx_state;
}

