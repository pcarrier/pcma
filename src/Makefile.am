bin_PROGRAMS = pcmad pcmac

pcmac_SOURCES = common.c pcmac.c
pcmac_CFLAGS  = $(ZMQ_CFLAGS)
pcmac_LDADD   = $(ZMQ_LIBS)

pcmad_SOURCES = common.c mlockfile.c pcmad.c
pcmad_CFLAGS = $(ZMQ_CFLAGS)
pcmad_LDADD  = $(ZMQ_LIBS)

noinst_HEADERS = common.h macros.h mlockfile.h pcmac.h pcmad.h
