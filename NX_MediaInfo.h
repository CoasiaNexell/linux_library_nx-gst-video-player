#ifndef NX_MEDIAINFO_H
#define NX_MEDIAINFO_H

#include <glib.h>

#define PROGRAM_MAX			16
#define MAX_TRACK_NUM		10

struct GST_TRACK_INFO {
    gint32			iTrackIndex;	// Track Index
    gint32			iTrackType;		// MP_TRACK_VIDEO, ...
    gint32			iCodecId;
    gint64			iDuration;		// Track Duration

    gboolean        bIsSeekable;

    gint32			iWidth;			// Only VideoTrack
    gint32			iHeight;		// Only VideoTrack
    gint32			iFrameRate;		// Only VideoTrack

    gint32			iChannels;		// Only AudioTrack
    gint32			iSampleRate;	// Only AudioTrack
    gint32			iBitrate;		// Only AudioTrack
};

struct GST_PROGRAM_INFO {
    gint32			iAudioNum;      // total audio number
    gint32			iVideoNum;
    gint32			iSubTitleNum;
    gint32			iDataNum;
    gint64			iDuration;
    GST_TRACK_INFO 	TrackInfo[MAX_TRACK_NUM];
};
/*
struct GST_MEDIA_INFO {
    gint32			iProgramNum;
    gint32			iAudioTrackNum;
    gint32			iVideoTrackNum;
    gint32			iSubTitleTrackNum;
    gint32			iDataTrackNum;
    GST_PROGRAM_INFO	ProgramInfo[PROGRAM_MAX];
};
typedef GST_MEDIA_INFO  GST_MEDIA_INFO;
*/
struct GST_MEDIA_INFO {
    gchar*          container_type;
    gint32          iWidth;
    gint32          iHeight;
    gboolean        isSeekable;
    gint64          iDuration;
};
typedef GST_MEDIA_INFO  GST_MEDIA_INFO;

#endif // NX_MEDIAINFO_H
