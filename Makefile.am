ACLOCAL_AMFLAGS = -I config

include $(top_srcdir)/Makefile.common

EXTRA_DIST = \
	bootstrap \
	configure \
	config.h.in \
	Makefile.swig \
	Makefile.swig.gen.t \
	version.sh \
	README \
	README.hacking

SUBDIRS = config src swig python

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA =

DISTCLEANFILES = omnipod*.tar.gz
