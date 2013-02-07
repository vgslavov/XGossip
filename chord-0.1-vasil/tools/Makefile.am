## Process this file with automake to produce Makefile.in
# $Id: Makefile.am,v 1.1.1.1 2008/01/02 16:51:36 rpraveen Exp $

$(PROGRAMS): $(LDEPS) $(DBDEPS)

if GTK_AVAILABLE
VIS = vis
endif

bin_PROGRAMS = dbm walk findroute filestore push_sum gpsi nodeq dbdump lsdping dbm_noauth psi invertedIndex gis $(VIS) 
bin_SCRIPTS = dbm.py nodeq-filter
chorddata_DATA =  RPC.py RPCProto.py vischat.py

EXTRA_DIST = $(bin_SCRIPTS) $(chorddata_DATA)
CLEANFILES = core *.core *~ *.rpo
MAINTAINERCLEANFILES = Makefile.in

noinst_PROGRAMS = coords kdb_stats 

dbm_noauth_SOURCES = dbm_noauth.C
dbm_noauth_LDADD = ../dhash/libdhashclient.a ../utils/libutil.a ../svc/libsvc.la $(LDADD)

lsdping_SOURCES = lsdping.C
lsdping_LDADD =  ../utils/libutil.a ../svc/libsvc.la $(LDADD)

walk_SOURCES = walk.C
walk_LDADD = ../chord/libchord.a ../utils/libutil.a ../svc/libsvc.la  $(LDADD) 

coords_SOURCES = walk_coords.C
coords_LDADD = ../chord/libchord.a ../utils/libutil.a ../svc/libsvc.la  $(LDADD) 

filestore_SOURCES = filestore.C
filestore_LDADD = ../dhash/libdhashclient.a  ../utils/libutil.a ../svc/libsvc.la  $(LDADD) 

push_sum_SOURCES = push_sum.C
push_sum_LDADD = ../dhash/libdhashclient.a  ../utils/libutil.a ../svc/libsvc.la  $(LDADD) 

gpsi_SOURCES = gpsi.C
gpsi_LDADD = ../dhash/libdhashclient.a  ../utils/libutil.a ../svc/libsvc.la  $(LDADD) 

dbm_SOURCES = dbm.C
dbm_LDADD = ../dhash/libdhashclient.a ../utils/libutil.a ../svc/libsvc.la $(LDADD)

dbdump_SOURCES = dbdump.C
dbdump_LDADD = $(DBLIB) ../utils/dbfe_sleepycat.o $(LDADD)

findroute_SOURCES = findroute.C
findroute_LDADD = ../chord/libchord.a ../utils/libutil.a ../svc/libsvc.la $(LDADD)

nodeq_SOURCES = nodeq.C
nodeq_LDADD = ../utils/libutil.a ../svc/libsvc.la ../merkle/libmerkle.a $(LDADD)

kdb_stats_SOURCES = kdb_stats.C
kdb_stats_LDADD = ../utils/libutil.a ../svc/libsvc.la $(LIBARPC)  $(LIBSFSCRYPT) $(LIBGMP) $(DBLIB) $(LIBASYNC) 

psi_SOURCES = psi.C
psi_LDADD = ../dhash/libdhashclient.a  ../utils/libutil.a ../svc/libsvc.la  $(LDADD) 

gis_SOURCES = gis.C
gis_LDADD = -ldb -ldb_cxx ../dhash/libdhashclient.a ../utils/libutil.a ../svc/libsvc.la  $(LDADD) 

invertedIndex_SOURCES = invertedIndex.C
invertedIndex_LDADD = ../dhash/libdhashclient.a  ../utils/libutil.a ../svc/libsvc.la  $(LDADD) 

if GTK_AVAILABLE
vis_SOURCES = vis.C vis.h vis_update.C vis_cmd.C
INCLUDES = $(GSTUFF_CFLAGS) 
vis_LDADD =  ../chord/libchord.a ../utils/libutil.a ../svc/libsvc.la $(GSTUFF_LIBS) $(LDADD)
endif
