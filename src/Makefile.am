libnxgstvplayer_la_CFLAGS = \
	$(WARN_CFLAGS) \
	-O -DDEBUG -g \
	-I${includedir} \
	-I${libdir}/glib-2.0/include \
	-I${includedir}/glib-2.0 \
	-I${includedir}/gstreamer-1.0 \
	-I${includedir}/gdk-pixbuf-2.0

libnxgstvplayer_la_CPPFLAGS = \
	$(WARN_CFLAGS) \
	-O -DDEBUG -g \
	-I${includedir} \
	-I${libdir}/glib-2.0/include \
	-I${includedir}/glib-2.0 \
	-I${includedir}/gstreamer-1.0 \
	-I${includedir}/gdk-pixbuf-2.0

libnxgstvplayer_la_LTLIBRARIES = libnxgstvplayer.la
libnxgstvplayer_ladir = ${libdir}/libnxgstvplayer

# library version
libnxgstvplayer_la_LDFLAGS = -version-number 0:9:0

# add dependency libraries
libnxgstvplayer_la_LDFLAGS += \
	-lglib-2.0 \
	-lgstmpegts-1.0 \
	-lgstreamer-1.0 \
	-lgstpbutils-1.0 \
	-lgdk_pixbuf-2.0

libnxgstvplayer_la_SOURCES = \
	NX_GstDiscover.c \
	NX_GstLog.c \
	NX_GstThumbnail.c \
	NX_TypeFind.c \
	NX_TSProgram.c \
	NX_OMXSemaphore.c \
	NX_GstMediaInfo.cpp \
	NX_GstMoviePlay.cpp
