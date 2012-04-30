AM_CPPFLAGS = $(GLIB_CFLAGS) $(ZMQ_CFLAGS)
LIBS = $(GLIB_LIBS) $(ZMQ_LIBS) -lmsgpack

bin_PROGRAMS = pcmad pcmac

pcmac_SOURCES = common.c client.c
pcmad_SOURCES = common.c mlockfile.c server.c

noinst_HEADERS = common.h mlockfile.h client.h server.h
