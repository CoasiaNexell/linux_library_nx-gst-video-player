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
//	Module		: Thumbnail Class
//	File		:
//	Description	:
//	Author		:
//	Export		:
//	History		:
//
//------------------------------------------------------------------------------
#include <stdlib.h>
#include <stdio.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm/drm_fourcc.h>

#include <NX_FindDrmInfo.h>
#define LOG_TAG "[NxGstVideoPlayer]"
#include <NX_Log.h>

static int32_t find_video_plane( int fd, int crtcIdx, uint32_t *connId, uint32_t *crtcId, uint32_t *planeId )
{
    uint32_t possible_crtcs = 0;
    drmModeRes *res;
    drmModePlaneRes *pr;
    drmModePlane *plane;
    uint32_t i, j;
    int32_t found = 0;

    res = drmModeGetResources(fd);

    if( crtcIdx >= res->count_crtcs )
        goto ErrorExit;

    *crtcId = res->crtcs[ crtcIdx ];
    *connId = res->connectors[ crtcIdx ];

    possible_crtcs = 1<<crtcIdx;

    drmSetClientCap(fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);
    pr = drmModeGetPlaneResources( fd );

    for( i=0 ; i<pr->count_planes ; ++i )
    {
        plane = drmModeGetPlane( fd, pr->planes[i] );
        if( plane->possible_crtcs & possible_crtcs )
        {
            for( j=0 ; j<plane->count_formats ; j++ )
            {
                if( plane->formats[j]==DRM_FORMAT_YUV420 ||
                    plane->formats[j]==DRM_FORMAT_YVU420 ||
                    plane->formats[j]==DRM_FORMAT_UYVY ||
                    plane->formats[j]==DRM_FORMAT_VYUY ||
                    plane->formats[j]==DRM_FORMAT_YVYU ||
                    plane->formats[j]==DRM_FORMAT_YUYV )
                {
                    found = 1;
                    *planeId = plane->plane_id;
                }
            }
        }
    }
    drmModeFreeResources(res);
    return found?0:-1;
ErrorExit:
    drmModeFreeResources(res);
    return -1;
}

static int32_t find_rgb_plane( int fd, int32_t crtcIdx, int32_t layerIdx, uint32_t *connId, uint32_t *crtcId, uint32_t *planeId )
{
    uint32_t possible_crtcs = 0;
    drmModeRes *res;
    drmModePlaneRes *pr;
    drmModePlane *plane;
    uint32_t i, j;
    int32_t found = 0;
    int32_t findIdx = 0;
    int32_t isRgb = 0;

    res = drmModeGetResources(fd);
    if( crtcIdx >= res->count_crtcs )
        goto ErrorExit;

    *crtcId = res->crtcs[ crtcIdx ];
    *connId = res->connectors[ crtcIdx ];

    possible_crtcs = 1<<crtcIdx;

    drmSetClientCap(fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);
    pr = drmModeGetPlaneResources( fd );

    for( i=0 ; i<pr->count_planes ; i++ )
    {
        plane = drmModeGetPlane( fd, pr->planes[i] );
        if( plane->possible_crtcs & possible_crtcs )
        {
            isRgb = 0;
            for( j=0 ; j<plane->count_formats ; j++ )
            {
                if( plane->formats[j]==DRM_FORMAT_ABGR8888 ||
                    plane->formats[j]==DRM_FORMAT_RGBA8888 ||
                    plane->formats[j]==DRM_FORMAT_XBGR8888 ||
                    plane->formats[j]==DRM_FORMAT_RGBX8888 ||
                    plane->formats[j]==DRM_FORMAT_RGB888 ||
                    plane->formats[j]==DRM_FORMAT_BGR888 )
                {
                    isRgb = 1;
                }
            }

            if( isRgb )
            {
                if( findIdx == layerIdx )
                {
                    found = 1;
                    *planeId = plane->plane_id;
                    break;
                }
                findIdx++;
            }
        }
    }
    drmModeFreeResources(res);
    return found?0:-1;
ErrorExit:
    drmModeFreeResources(res);
    return -1;
}


int32_t FindPlaneForDisplay(int32_t crtcIdx,
        	                int32_t findRgb,
            	            int32_t layerIdx,
                	        MP_DRM_PLANE_INFO *pDrmPlaneInfo)
{
    int32_t hDrmFd = -1;
    uint32_t connId = 0;
    uint32_t crtcId = 0;
    uint32_t planeId = 0;

    hDrmFd = drmOpen( "nexell", NULL );

    if( 0 > hDrmFd )
	{
        NXLOGE("Fail, drmOpen().\n");
        return -1;
    }

    if( findRgb )
    {
        if( 0 == find_rgb_plane(hDrmFd, crtcIdx, layerIdx, &connId, &crtcId, &planeId) )
        {
			NXLOGI("RGB : connId = %d, crtcId = %d, planeId = %d\n", connId, crtcId, planeId);
        }
        else
        {
            NXLOGE("cannot found video format for %dth crtc\n", crtcIdx );
            drmClose( hDrmFd );
            return -1;
        }
    }
    else
    {
        if( 0 == find_video_plane(hDrmFd, crtcIdx, &connId, &crtcId, &planeId) )
        {
            NXLOGI("VIDEO : connId = %d, crtcId = %d, planeId = %d\n", connId, crtcId, planeId);
        }
        else
        {
            NXLOGE( "cannot found video format for %dth crtc\n", crtcIdx );
            drmClose( hDrmFd );
            return -1;
        }
    }
    drmClose( hDrmFd );

    pDrmPlaneInfo->iConnectorID     = connId;
    pDrmPlaneInfo->iCrtcId        	= crtcId;
    pDrmPlaneInfo->iPlaneId         = planeId;

    return 0;
}