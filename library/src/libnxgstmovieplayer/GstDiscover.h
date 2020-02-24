#ifndef __GST_DISCOVER_H
#define __GST_DISCOVER_H

#include "NX_GstTypes.h"

#ifdef __cplusplus
extern "C" {
#endif	//	__cplusplus

static struct {
    char *mimetype;
    CONTAINER_TYPE  type;
    DEMUX_TYPE demux_type;
    char *demux_name;
} CONTAINER_DESC[] = {
    {"video/quicktime",         CONTAINER_TYPE_QUICKTIME,   DEMUX_TYPE_QTDEMUX,         "qtdemux"},
    {"application/ogg",         CONTAINER_TYPE_OGG,         DEMUX_TYPE_OGGDEMUX,        "oggdemux"},
    {"application/vnd.rn-realmedia", CONTAINER_TYPE_REALMEDIA,   DEMUX_TYPE_RMDEMUX,    "rmdemux"},
    {"video/x-msvideo",         CONTAINER_TYPE_MSVIDEO,     DEMUX_TYPE_AVIDEMUX,        "avidemux"},
    {"video/x-ms-asf",          CONTAINER_TYPE_ASF,         DEMUX_TYPE_ASFDEMUX,        "asfdemux"},
    {"video/x-matroska",        CONTAINER_TYPE_MATROSKA,    DEMUX_TYPE_MATROSKADEMUX,   "matroskademux"},
    {"video/x-flv",             CONTAINER_TYPE_FLV,         DEMUX_TYPE_FLVDEMUX,        "flvdemux"},
    {"video/mpeg",              CONTAINER_TYPE_MPEG,        DEMUX_TYPE_MPEGDEMUX,       "mpegdemux"},
    {"video/mpegts",            CONTAINER_TYPE_MPEGTS,      DEMUX_TYPE_MPEGTSDEMUX,     "mpegtsdemux"},
    {"video/x-dv",              CONTAINER_TYPE_DV,          DEMUX_TYPE_DVDEMUX,         "dvdemux"},
    {"application/x-3gp",       CONTAINER_TYPE_3GP,         DEMUX_TYPE_QTDEMUX ,        "qtdemux"},
    {"application/x-annodex",   CONTAINER_TYPE_ANNODEX,     DEMUX_TYPE_OGGDEMUX,        "oggdemux"},
    {"audio/x-wav",             CONTAINER_TYPE_WAV,         DEMUX_TYPE_WAVPARSE,        "wavparse"},
    //{"application/x-id3",     CONTAINER_TYPE_,            DEMUX_TYPE_,                ""},
    //{"audio/x-flac",          CONTAINER_TYPE_,            DEMUX_TYPE_,                ""},
    //{"audio/x-m4a",           CONTAINER_TYPE_,            DEMUX_TYPE_,                ""},
    //{"audio/mpeg",            CONTAINER_TYPE_,            DEMUX_TYPE_,                ""},
    //{"audio/x-ac3",           CONTAINER_TYPE_,            DEMUX_TYPE_,                ""},
    //{"audio/x-dts",           CONTAINER_TYPE_,            DEMUX_TYPE_,                ""},
    {NULL,                      CONTAINER_TYPE_UNKNOWN,     DEMUX_TYPE_UNKNOWN,         ""},
};

static struct {
    char *mimetype;
    VIDEO_TYPE type;
    char *parser;
} VIDEO_DESC[] = {
    {"video/x-h264",            VIDEO_TYPE_H264,        "h264parse"},
	{"video/x-h263",            VIDEO_TYPE_H263,        NULL},
	{"video/mpeg",              VIDEO_TYPE_MPEG_V4,     NULL},
    {"video/x-h265",            VIDEO_TYPE_H265,        NULL},
	{"video/x-flash-video",     VIDEO_TYPE_FLV,         NULL},
	{"video/x-pn-realvideo",    VIDEO_TYPE_RV,          NULL},
	{"video/x-divx",            VIDEO_TYPE_DIVX,        NULL},
	{"video/x-ms-asf",          VIDEO_TYPE_ASF,         NULL},
	{"video/x-wmv",             VIDEO_TYPE_WMV,         NULL},
	{"video/x-theora",          VIDEO_TYPE_THEORA,      NULL},
	{"video/x-xvid",            VIDEO_TYPE_XVID,        NULL},
	{NULL,                      VIDEO_TYPE_UNKNOWN,     NULL},
};

static struct {
    char *mimetype;
    AUDIO_TYPE type;
    char *parser;
} AUDIO_DESC[] = {
    {"audio/x-raw",             AUDIO_TYPE_RAW,         NULL},
    {"audio/mpeg",              AUDIO_TYPE_MPEG,        NULL},
	{"audio/mp3",               AUDIO_TYPE_MP3,         NULL},
	{"audio/aac",               AUDIO_TYPE_AAC,         NULL},
	{"audio/x-wma",             AUDIO_TYPE_WMA,         NULL},
	{"audio/x-vorbis",          AUDIO_TYPE_OGG,         NULL},
	{"audio/x-ac3",             AUDIO_TYPE_AC3,         NULL},
	{"audio/x-private1-ac3",    AUDIO_TYPE_AC3_PRI,     NULL},
	{"audio/x-flac",            AUDIO_TYPE_FLAC,        NULL},
	{"audio/x-pn-realaudio",    AUDIO_TYPE_RA,          NULL},
	{"audio/x-dts",             AUDIO_TYPE_DTS,         NULL},
	{"audio/x-private1-dts",    AUDIO_TYPE_DTS_PRI,     NULL},
    {"audio/x-wav",             AUDIO_TYPE_WAV,         NULL},
	{NULL,                      AUDIO_TYPE_UNKNOWN,     NULL},
};

static struct {
    char *mimetype;
    SUBTITLE_TYPE type;
    char *parser;
} SUBTITLE_DESC[] = {
    {"text/x-raw",              SUBTITLE_TYPE_RAW,        NULL},
    {"application/x-ssa",       SUBTITLE_TYPE_SSA,        NULL},
    {"application/x-ass",       SUBTITLE_TYPE_ASS,        NULL},
    {"application/x-usf",       SUBTITLE_TYPE_USF,        NULL},
    {"application/x-dvd",       SUBTITLE_TYPE_DVD,        NULL},
	{"subpicture/x-dvb",        SUBTITLE_TYPE_DVB,        NULL},
	{NULL,                      SUBTITLE_TYPE_UNKNOWN,    NULL},
};

enum NX_GST_ERROR StartDiscover(const char* pUri, struct GST_MEDIA_INFO *pInfo);

#ifdef __cplusplus
}
#endif

#undef LOG_TAG

#endif // __GST_DISCOVER_H

