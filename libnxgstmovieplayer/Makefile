########################################################################
# Get Linux Build Enviornment:
include ../../../env.mk

######################################################################
# Includes
#INCLUDE += -I$(GLIB_INC) -I$(GLIB_LIB)/include
#INCLUDE += -I$(GST_INC)
#INCLUDE += -I./ -I$(LIBSDIR)/include
INCLUDE		+= -I. -I../../prebuilt/include

######################################################################
# Libraries
LIBRARY	+= -lstdc++ -lpthread -lm
#LIBRARY += -L$(GLIB_LIB) -lglib-2.0 
#LIBRARY += -L$(GST_LIB) -lgstreamer-1.0 -lgstreamer-pbutils-1.0
#LIBRARY += -L$(LIBSDIR)/lib -lnxtypefind

######################################################################
# Target
COBJS  	:= GstDiscover.o
#COBJS  	:= NX_GstMoviePlay.o

CPPOBJS	:=
CPPOBJS += NX_GstMoviePlay.o

CFLAGS		+= -g -Wall -Wextra `pkg-config --cflags glib-2.0 gstreamer-1.0 gstreamer-pbutils-1.0`
CFLAGS		+= -fPIC

LDFLAGS		=`pkg-config --libs glib-2.0 gstreamer-1.0 gstreamer-pbutils-1.0`

CPPFLAGS	+= -fPIC

LIBNAME	:= libnxgstmovplayer
TARGET	:= $(LIBNAME).so

######################################################################
# Build
OBJS	:= $(COBJS) $(CPPOBJS)

all: $(TARGET)

$(TARGET): depend $(OBJS)
	$(CC) $(LDFLAGS) -shared -Wl,-soname,$(TARGET) -o $@ $(OBJS) $(LIBRARY)

install:
	cp -av $(TARGET) ../../prebuilt/lib/
	cp -av NX_GstMovie.h ../../prebuilt/include

clean:
	rm -rf $(TARGET) *.o .depend

distclean: clean
	rm -rf ../../prebuilt/lib/$(TARGET)
	rm -rf ../../prebuilt/include/NX_GstMoviePlay.h

#########################################################################
# Dependency
ifeq (.depend,$(wildcard .depend))
include .depend
endif

SRCS := $(COBJS:.o=.c) $(CPPOBJS:.o=.cpp)
INCS := $(INCLUDE)
depend dep:
	$(CC)  -M  $(CFLAGS)   $(INCS) $(SRCS) > .depend
