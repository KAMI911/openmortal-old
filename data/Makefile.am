 ## Process this file with automake to produce Makefile.in

if MAIN
MSUBDIRS = fonts gfx script sound
else
MSUBDIRS =
endif


CSUBDIRS = characters
#CSUBDIRS = 


SUBDIRS= $(CSUBDIRS) $(MSUBDIRS)
DIST_SUBDIRS= $(CSUBDIRS) $(MSUBDIRS)

# The demos are no longer in the distribution
# pkgdata_DATA=demo0.om demo1.om demo2.om demo3.om demo4.om demo5.om
pkgdata_DATA=

EXTRA_DIST=$(pkgdata_DATA)
