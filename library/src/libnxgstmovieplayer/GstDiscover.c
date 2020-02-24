//------------------------------------------------------------------------------
// Refer to basic tutorial 9
//------------------------------------------------------------------------------
#include <string.h>
#include <gst/pbutils/pbutils.h>

#include "GstDiscover.h"
#include "NX_GstLog.h"
#define LOG_TAG "[GstDiscover]"
#include "NX_GstTypes.h"

//#define ASYNC_DISCOVER

/* Structure to contain all our information, so we can pass it around */
typedef struct _DiscoverData {
    GstDiscoverer *discoverer;
    GMainLoop *loop;
} DiscoverData;

/* Print a tag in a human-readable format (name: value) */
static void print_tag_foreach (const GstTagList *tags, const gchar *tag, gpointer user_data)
{
    FUNC_IN();

    GValue val = { 0, };
    gchar *str;
    gint depth = GPOINTER_TO_INT(user_data);

    gst_tag_list_copy_value(&val, tags, tag);

    if (G_VALUE_HOLDS_STRING(&val))
    {
        str = g_value_dup_string(&val);
    }
    else
    {
        str = gst_value_serialize(&val);
    }

    NXGLOGI("%*s%s: %s", 2 * depth, " ", gst_tag_get_nick(tag), str);

    g_free(str);
    g_value_unset(&val);

    FUNC_OUT();
}

static void  get_gst_stream_info(GstDiscovererStreamInfo *sinfo, gint depth,
                                   struct GST_MEDIA_INFO* pMediaInfo)
{
    FUNC_IN();

    gchar *desc = NULL;
    GstCaps *caps;
    gint video_mpegversion = 0;
    gint audio_mpegversion = 0;

    const gchar *stream_type = gst_discoverer_stream_info_get_stream_type_nick(sinfo);
    caps = gst_discoverer_stream_info_get_caps(sinfo);
    if (caps) {
        if (gst_caps_is_fixed (caps))
            desc = gst_pb_utils_get_codec_description(caps);
        else
            desc = gst_caps_to_string (caps);
    }

    GstStructure *structure = gst_caps_get_structure(caps, 0);
    if (structure == NULL)
    {
        NXGLOGE("Failed to get current caps");
        gst_caps_unref (caps);
        return;
    }
    gst_caps_unref (caps);

    const gchar *mime_type = gst_structure_get_name(structure);
    NXGLOGI("%*s%s: %s ==> %s",
            (2 * 1), " ", (stream_type ? stream_type : ""),
            (desc ? desc : ""), (mime_type ? mime_type : ""));
    if (desc)
    {
        g_free (desc);
        desc = NULL;
    }

	const GstTagList *tags = gst_discoverer_stream_info_get_tags (sinfo);
    if (tags) {
        NXGLOGI("** %*sTags:", 2 * (depth + 1), " ");
        gst_tag_list_foreach (tags, print_tag_foreach, GINT_TO_POINTER (depth + 2));
        NXGLOGI ("** %*sTags:End", 2 * (depth + 1), " ");
    }

    if (GST_IS_DISCOVERER_CONTAINER_INFO (sinfo))
    {
        pMediaInfo->n_container++;
        pMediaInfo->container_type = get_container_type(mime_type);
        NXGLOGI("n_container(%d) container_type(%d)",
                pMediaInfo->n_container, pMediaInfo->container_type);
    }
    else if (GST_IS_DISCOVERER_VIDEO_INFO (sinfo))
    {
        GstDiscovererVideoInfo *video_info = (GstDiscovererVideoInfo *) sinfo;
        guint width = gst_discoverer_video_info_get_width (video_info);
        guint height = gst_discoverer_video_info_get_height (video_info);
        guint framerate = gst_discoverer_video_info_get_framerate_num(video_info);

        const char* stream_id = gst_discoverer_stream_info_get_stream_id(video_info);
        if (width == 0 || height == 0 || stream_id == NULL)
        {
            NXGLOGE("Invalid video");
            return;
        }

        int32_t index = pMediaInfo->StreamInfo->n_video;
        pMediaInfo->StreamInfo->VideoInfo[index].type = get_video_codec_type(mime_type);
        if ((structure != NULL) && (g_strcmp0(mime_type, "video/mpeg") == 0))
        {
            gst_structure_get_int (structure, "mpegversion", &video_mpegversion);
            if (video_mpegversion > 0 && video_mpegversion < 4) {
                pMediaInfo->StreamInfo->VideoInfo[index].type = VIDEO_TYPE_MPEG_V2;
            }
        }

        pMediaInfo->StreamInfo->VideoInfo[index].stream_id = stream_id;
        pMediaInfo->StreamInfo->VideoInfo[index].width = width;
        pMediaInfo->StreamInfo->VideoInfo[index].height = height;
        pMediaInfo->StreamInfo->VideoInfo[index].framerate = framerate;
        pMediaInfo->StreamInfo->n_video++;

        NXGLOGI("n_video(%u), stream_id(%s), video_width(%d), "
                "video_height(%d), framerate(%d), video_type(%d)",
                pMediaInfo->StreamInfo->n_video,
                stream_id, framerate, width, height,
                pMediaInfo->StreamInfo->VideoInfo[index].type);
    }
    else if (GST_IS_DISCOVERER_AUDIO_INFO (sinfo))
    {
        const char* stream_id = gst_discoverer_stream_info_get_stream_id(sinfo);
        guint n_channels = gst_discoverer_audio_info_get_channels(sinfo);
        guint samplerate = gst_discoverer_audio_info_get_sample_rate(sinfo);
        guint bitrate = gst_discoverer_audio_info_get_bitrate(sinfo);

        int32_t index = pMediaInfo->StreamInfo->n_audio;
        pMediaInfo->StreamInfo->AudioInfo[index].type = get_audio_codec_type(mime_type);
        if ((structure != NULL) && (g_strcmp0(mime_type, "audio/mpeg") == 0))
        {
            gst_structure_get_int (structure, "mpegversion", &audio_mpegversion);
            if (audio_mpegversion > 0 && audio_mpegversion < 4) {
                pMediaInfo->StreamInfo->AudioInfo[index].type = AUDIO_TYPE_MPEG_V2;
            }
        }

        pMediaInfo->StreamInfo->AudioInfo[index].stream_id = stream_id;
        pMediaInfo->StreamInfo->AudioInfo[index].n_channels = n_channels;
        pMediaInfo->StreamInfo->AudioInfo[index].samplerate = samplerate;
        pMediaInfo->StreamInfo->AudioInfo[index].bitrate = bitrate;
        pMediaInfo->StreamInfo->n_audio++;

        NXGLOGI("n_audio(%d) n_channels(%d), samplerate(%d),"
                "bitrate(%d), stream_id(%s), audio_mime_type(%d)",
                pMediaInfo->StreamInfo->n_audio,
                n_channels, samplerate, bitrate, stream_id,
                pMediaInfo->StreamInfo->AudioInfo[index].type);
    }
    else if (GST_IS_DISCOVERER_SUBTITLE_INFO (sinfo))
    {
        int32_t index = pMediaInfo->StreamInfo->n_subtitle;
        pMediaInfo->StreamInfo->SubtitleInfo[index].type = get_subtitle_codec_type(mime_type);
        pMediaInfo->StreamInfo->SubtitleInfo[index].stream_id = gst_discoverer_stream_info_get_stream_id(sinfo);
        pMediaInfo->StreamInfo->n_subtitle++;

        NXGLOGI("n_subtitle(%d), stream_id(%s), subtitle_type(%d), subtitle_lang(%s)",
                pMediaInfo->StreamInfo->n_subtitle,
                pMediaInfo->StreamInfo->SubtitleInfo[index].stream_id,
                pMediaInfo->StreamInfo->SubtitleInfo[index].type,
                gst_discoverer_subtitle_info_get_language(sinfo));
    }
    else
    {
        NXGLOGI("Unknown stream info");
    }

    FUNC_OUT();
}

/* Print information regarding a stream and its substreams, if any */
static void print_topology(GstDiscovererStreamInfo *sinfo, gint depth,
            struct GST_MEDIA_INFO* pMediaInfo)
{
    if (!sinfo)
    {
        NXGLOGE("GstDiscovererStreamInfo is NULL");
        return;
    }

    // Get Stream Info
    get_gst_stream_info(sinfo, depth, pMediaInfo);

    GstDiscovererStreamInfo *next = gst_discoverer_stream_info_get_next(sinfo);
    if (next)
    {
        print_topology(next, depth + 1, pMediaInfo);
        gst_discoverer_stream_info_unref(next);
    }
    else if (GST_IS_DISCOVERER_CONTAINER_INFO(sinfo))
    {
        GList *tmp, *streams;

        // Get Container Info from Stream Info
        streams = gst_discoverer_container_info_get_streams(GST_DISCOVERER_CONTAINER_INFO(sinfo));
        for (tmp = streams; tmp; tmp = tmp->next)
        {
            GstDiscovererStreamInfo *tmpinf = (GstDiscovererStreamInfo *) tmp->data;
            print_topology(tmpinf, depth + 1, pMediaInfo);
        }
        gst_discoverer_stream_info_list_free(streams);
    }
}

void parse_GstDiscovererInfo(GstDiscovererInfo *info, GError *err,
                        struct GST_MEDIA_INFO* pMediaInfo)
{
    const gchar *uri;
    const GstTagList *tags;
    GstDiscovererStreamInfo *sinfo;
    GstDiscovererResult result;
    gboolean isSeekable = FALSE;
    gint64 duration;

    uri = gst_discoverer_info_get_uri(info);
    result = gst_discoverer_info_get_result(info);
    switch (result) {
        case GST_DISCOVERER_URI_INVALID:
            NXGLOGE("Invalid URI '%s'", uri);
            break;
        case GST_DISCOVERER_ERROR:
            if (err != NULL)
                NXGLOGE("Discoverer error: %s",  err->message);
            else
                NXGLOGE("Discoverer error",  __FUNCTION__);
            break;
        case GST_DISCOVERER_TIMEOUT:
                NXGLOGE("Timeout");
                break;
        case GST_DISCOVERER_BUSY:
                NXGLOGE("Busy");
                break;
        case GST_DISCOVERER_MISSING_PLUGINS:{
            const GstStructure *s;
            gchar *str;

            s = gst_discoverer_info_get_misc (info);
            str = gst_structure_to_string (s);

            NXGLOGE("Missing plugins: %s", str);
            g_free (str);
            break;
        }
        case GST_DISCOVERER_OK:
            NXGLOGI("Discovered '%s'", uri);
            break;
    }

    if (result != GST_DISCOVERER_OK) {
        NXGLOGE("This URI cannot be played");
        return;
    }

    duration = gst_discoverer_info_get_duration(info);
    /* If we got no error, show the retrieved information */
    NXGLOGI("Duration: %" GST_TIME_FORMAT,
            GST_TIME_ARGS (duration));

    tags = gst_discoverer_info_get_tags(info);
    if (tags) {
        NXGLOGI("Tags:");
        gst_tag_list_foreach (tags, print_tag_foreach, GINT_TO_POINTER(1));
    }

    isSeekable = gst_discoverer_info_get_seekable(info);
    NXGLOGI("Seekable: %s", (isSeekable ? "yes" : "no"));

    sinfo = gst_discoverer_info_get_stream_info(info);
    if (!sinfo) {
        NXGLOGE("%s(): Failed to get stream info");
        return;
    }

    NXGLOGI("Stream information:");

    print_topology(sinfo, 1, pMediaInfo);

    gst_discoverer_stream_info_unref(sinfo);

    pMediaInfo->StreamInfo->seekable = isSeekable;
    pMediaInfo->StreamInfo->duration = duration;
}

#ifdef ASYNC_DISCOVER
/* This function is called every time the discoverer has information regarding
 * one of the URIs we provided.*/
static void on_discovered_cb(GstDiscoverer *discoverer, GstDiscovererInfo *info,
                GError *err, /*DiscoverData *data, */struct GST_MEDIA_INFO* pMediaInfo)
{
    parse_GstDiscovererInfo(info, err, pMediaInfo);
}

/* This function is called when the discoverer has finished examining
 * all the URIs we provided.*/
static void on_finished_cb (GstDiscoverer *discoverer, DiscoverData *data)
{
    NXGLOGI("Finished discovering");

    g_main_loop_quit(data->loop);
}
#endif

static int start_discover(const char* filePath, struct GST_MEDIA_INFO *pMediaInfo)
{
    FUNC_IN();

    GError *err = NULL;
    gchar *uri;

    if (gst_uri_is_valid (filePath))
        uri = g_strdup (filePath);
    else
        uri = gst_filename_to_uri (filePath, NULL);

    DiscoverData data;
    memset (&data, 0, sizeof (data));

    NXGLOGI("Start to discover '%s'", uri);

    /* Instantiate the Discoverer */
    data.discoverer = gst_discoverer_new(5 * GST_SECOND, &err);
    if (!data.discoverer) {
        NXGLOGI("%s(): Error creating discoverer instance: %s\n", err->message);
        g_clear_error(&err);
        return -1;
    }

#ifdef ASYNC_DISCOVER
    /* Connect to the interesting signals */
    g_signal_connect(data.discoverer, "discovered", G_CALLBACK (on_discovered_cb), &data);
    g_signal_connect(data.discoverer, "finished", G_CALLBACK (on_finished_cb), &data);

    /* Start the discoverer process (nothing to do yet) */
    gst_discoverer_start(data.discoverer);

    /* Add a request to process asynchronously the URI passed through the command line */
    if (!gst_discoverer_discover_uri_async(data.discoverer, uri))
    {
        NXGLOGI("%s(): Failed to start async discovering URI '%s'\n", uri);
        g_free (uri);
        g_object_unref (data.discoverer);
        return -1;
    }
#else
    GstDiscovererInfo *pDiscInfo = gst_discoverer_discover_uri(data.discoverer, uri, &err);
    if (!pDiscInfo)
    {
        NXGLOGI("%s(): Failed to start sync discovering URI '%s'\n", uri);
        g_free (uri);
        g_object_unref (data.discoverer);
        return -1;
    }
    parse_GstDiscovererInfo(pDiscInfo, NULL, pMediaInfo);
    gst_discoverer_info_unref(pDiscInfo);
#endif
    g_free (uri);

#ifdef ASYNC_DISCOVER
    /* Create a GLib Main Loop and set it to run, so we can wait for the signals */
    data.loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(data.loop);

    /* Stop the discoverer process */
    gst_discoverer_stop(data.discoverer);
#endif
    /* Free resources */
    g_object_unref(data.discoverer);
#ifdef ASYNC_DISCOVER
    g_main_loop_unref(data.loop);
#endif

    FUNC_OUT();

    return 0;
}

int get_demux_type(const gchar* mimeType)
{
    int idx = 0;
    int len = strlen(mimeType);
    const char *str;

    while (CONTAINER_DESC[idx].mimetype) {
        str = CONTAINER_DESC[idx].mimetype;
        NXGLOGV("str = %s", str);

        if (str && strncmp(mimeType, str, len) == 0)
        {
            NXGLOGI("%s ==> [%d] %s", mimeType, CONTAINER_DESC[idx].demux_type, CONTAINER_DESC[idx].demux_name);
            return CONTAINER_DESC[idx].demux_type;
        }
        idx++;
    }

    return -1;
}

int get_container_type(const gchar* mimeType)
{
    int idx = 0;
    int len = strlen(mimeType);
    const char *str;

    while (CONTAINER_DESC[idx].mimetype) {
        str = CONTAINER_DESC[idx].mimetype;
        NXGLOGV("str = %s mimetype = %s", str, mimeType);

        if (str && strncmp(mimeType, str, len) == 0)
        {
            NXGLOGI("%s ==> [%d] - %s", mimeType, CONTAINER_DESC[idx].type, CONTAINER_DESC[idx].mimetype);
            return CONTAINER_DESC[idx].type;
        }
        idx++;
    }

    return -1;
}

int get_video_codec_type(const gchar* mimeType)
{
    int idx = 0;
    int len = strlen(mimeType);
    const char *str;

    while (VIDEO_DESC[idx].mimetype) {
        str = VIDEO_DESC[idx].mimetype;
        NXGLOGV("str = %s mimetype = %s", str, mimeType);

        if (str && strncmp(mimeType, str, len) == 0)
        {
            NXGLOGI("%s ==> [%d]", mimeType, VIDEO_DESC[idx].type);
            return VIDEO_DESC[idx].type;
        }
        idx++;
    }

    return -1;
}

int get_audio_codec_type(const gchar* mimeType)
{
    int idx = 0;
    int len = strlen(mimeType);
    const char *str;

    while (AUDIO_DESC[idx].mimetype) {
        str = AUDIO_DESC[idx].mimetype;
        NXGLOGV("str = %s mimetype = %s", str, mimeType);

        if (str && strncmp(mimeType, str, len) == 0)
        {
            NXGLOGI("%s ==> [%d]", mimeType, AUDIO_DESC[idx].type);
            return AUDIO_DESC[idx].type;
        }
        idx++;
    }

    return -1;
}

int get_subtitle_codec_type(const gchar* mimeType)
{
    int idx = 0;
    int len = strlen(mimeType);
    const char *str;

    while (SUBTITLE_DESC[idx].mimetype) {
        str = SUBTITLE_DESC[idx].mimetype;
        NXGLOGV("str = %s mimetype = %s", str, mimeType);

        if (str && strncmp(mimeType, str, len) == 0)
        {
            NXGLOGI("%s ==> [%d]", mimeType, SUBTITLE_DESC[idx].type);
            return SUBTITLE_DESC[idx].type;
        }
        idx++;
    }

    return -1;
}

gboolean isSupportedContents(struct GST_MEDIA_INFO *pMediaInfo)
{
    NXGLOGI();
    CONTAINER_TYPE container_type = pMediaInfo->container_type;
    NXGLOGI("container_type(%d)", container_type);
    VIDEO_TYPE video_type = pMediaInfo->StreamInfo[0].VideoInfo[0].type;
    NXGLOGI("video_type(%d)", video_type);
    SUBTITLE_TYPE subtitle_type = pMediaInfo->StreamInfo[0].SubtitleInfo[0].type;

    /* Quicktime, 3GP, Matroska, AVI, MPEG (vob) */
    if ((container_type == CONTAINER_TYPE_MPEGTS) ||
        (container_type == CONTAINER_TYPE_QUICKTIME) ||
        (container_type == CONTAINER_TYPE_MSVIDEO) ||
        (container_type == CONTAINER_TYPE_MATROSKA) ||
        (container_type == CONTAINER_TYPE_3GP) ||
        (container_type == CONTAINER_TYPE_MPEG)
#ifdef SW_V_DECODER
        || (container_type == CONTAINER_TYPE_FLV)
#endif
        )
    {
        if (video_type >= VIDEO_TYPE_FLV)
        {
            NXGLOGE("Not supported video type(%d)", video_type);
            return FALSE;
        }

        return TRUE;
    }
    else
    {
        NXGLOGE("Not supported container type(%d)", container_type);
        return FALSE;
    }
}

enum NX_GST_ERROR StartDiscover(const char* pUri, struct GST_MEDIA_INFO *pInfo)
{
    enum NX_GST_ERROR ret = 0;

    NXGLOGI();

    ret = start_discover(pUri, pInfo);
    if (ret < 0)
    {
        NXGLOGE("Failed to discover");
        return NX_GST_ERROR_DISCOVER_FAILED;
    }
    if (FALSE == isSupportedContents(pInfo))
    {
        return NX_GST_ERROR_NOT_SUPPORTED_CONTENTS;
    }

    NXGLOGI("Done to discover");

    return NX_GST_ERROR_NONE;
}
