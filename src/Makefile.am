include $(top_srcdir)/Makefile.common

# ----------------------------------------------------------------
# omnidemod C++ library: libgnuradio-omnidemod.so
# ----------------------------------------------------------------

# C/C++ headers get installed in ${prefix}/include/$(modname)
#modinclude_HEADERS = \
#	omnipod_demod.h

lib_LTLIBRARIES = libgnuradio-omnipod.la

libgnuradio_omnipod_la_SOURCES = \
	omnipod_demod.cc \
	circular_buffer.cc

libgnuradio_omnipod_la_LIBADD = \
	$(GNURADIO_CORE_LA)

libgnuradio_omnipod_la_LDFLAGS = $(NO_UNDEFINED) $(LTVERSIONFLAGS)

EXTRA_DIST = \
	     omnipod_demod.h \
	     circular_buffer.h
