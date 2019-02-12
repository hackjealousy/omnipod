include $(top_srcdir)/Makefile.common

AM_CPPFLAGS += -I$(top_srcdir)/src

if PYTHON
###################################
# SWIG Python interface and library

TOP_SWIG_IFILES = \
	omnipod.i

# Install so that they end up available as:
#   import omnipod
# This ends up at:
#   ${prefix}/lib/python${python_version}/site-packages/$(modname)

omnipod_la_swig_libadd =		\
	$(top_builddir)/src/libgnuradio-omnipod.la

omnipod_python = __init__.py

# additional SWIG files to be installed
#omnipod_swiginclude_headers = \
#	omnipod_demod.i

include $(top_srcdir)/Makefile.swig

# add some of the variables generated inside the Makefile.swig.gen
BUILT_SOURCES = $(swig_built_sources)

# Do not distribute the output of SWIG
no_dist_files = $(swig_built_sources)

EXTRA_DIST = omnipod_demod.i
endif
