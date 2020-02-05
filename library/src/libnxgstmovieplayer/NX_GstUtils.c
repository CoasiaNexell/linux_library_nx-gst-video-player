#include "NX_GstMoviePlay.h"

const char* get_nx_media_state(NX_MEDIA_STATE state)
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

const char* get_nx_gst_event(NX_GST_EVENT event)
{
    switch(event) {
        case MP_EVENT_EOS:
            return "MP_EVENT_EOS";
        case MP_EVENT_DEMUX_LINK_FAILED:
            return "MP_EVENT_DEMUX_LINK_FAILED";
        case MP_EVENT_NOT_SUPPORTED:
            return "MP_EVENT_NOT_SUPPORTED";
        case MP_EVENT_GST_ERROR:
            return "MP_EVENT_GST_ERROR";
        case MP_EVENT_NUMS:
            return "MP_EVENT_NUMS";
        default:
            break;
    }
    return NULL;
}

const char* get_nx_gst_error(NX_GST_ERROR error)
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
