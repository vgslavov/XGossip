/*	$Id: gpsi.C,v 1.37 2010/06/07 01:35:09 vsfgd Exp vsfgd $	*/

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <typeinfo>
#include <vector>

#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <chord.h>
#include <dhash_common.h>
#include <dhashclient.h>
#include <dhblock.h>

//#include <pthread.h>

#include "../utils/id_utils.h"
#include "../utils/utils.h"
#include "nodeparams.h"
#include "gpsi.h"

//#define _DEBUG_
#define _ELIMINATE_DUP_

static char rcsid[] = "$Id: gpsi.C,v 1.37 2010/06/07 01:35:09 vsfgd Exp vsfgd $";
extern char *__progname;

dhashclient *dhash;
int out;

chordID maxID;
static const char* dsock;
static const char* gsock;
static char *logfile;
static char *irrpolyfile;
int lshseed = 0;
int plist = 0;
int gflag = 0;
int uflag = 0;
int discardmsg = 0;
int peers = 0;
bool initphase = 0;

// TODO: command line args?
int lfuncs = 5;
int mgroups = 100;

// map sigs to freq and weight
typedef std::map<std::vector<POLY>, std::vector<double>, CompareSig> mapType;
typedef std::vector<mapType> vecomap;
// local list T_m is stored in allT[0];
vecomap allT;

// map chordIDs/POLYs to sig indices
typedef std::map<chordID, std::vector<int> > chordID2sig;
typedef std::map<POLY, std::vector<int> > poly2sig;

void acceptconnection(int);
void add2vecomap(std::vector<std::vector <POLY> >, std::vector<double>, std::vector<double>);
int getdir(std::string, std::vector<std::string>&);
void calcfreq(std::vector<std::vector <POLY> >);
void calcfreqM(std::vector<std::vector <POLY> >, std::vector<double>, std::vector<double>);
void doublefreq(int);
void doublefreqgroup(int, mapType);
void informteam(chordID, std::vector<POLY>);
void initgossiprecv(chordID, std::vector<POLY>, double, double);
DHTStatus insertDHT(chordID, char *, int, int = MAXRETRIES, chordID = 0);
std::vector<POLY> inverse(std::vector<POLY>);
void listengossip(void);
//void *listengossip(void *);
void mergelists(void);
void mergelistsp(void);
void printdouble(std::string, double);
void printlist(int, int);
chordID randomID(void);
void readgossip(int);
void readsig(std::string, std::vector<std::vector <POLY> >&);
//void retrieveDHT(chordID ID, int, str&, chordID guess = 0);
void delspecial(int);
bool sig2str(std::vector<POLY>, str&);
bool sigcmp(const std::vector<POLY>&, const std::vector<POLY>&);
void splitfreq(int);
void usage(void);

DHTStatus insertStatus;
std::string INDEXNAME;

// entries of a node
vec<str> nodeEntries;

bool insertError;
bool retrieveError;

//pthread_mutex_t lock;

//std::vector<std::map<std::vector<POLY>, std::vector<double>, CompareSig> > allT;
//std::map<std::vector<POLY>, std::vector<double>, CompareSig> uniqueSigList;
//std::map<std::vector<POLY>, double, CompareSig> uniqueSigList;
//std::map<std::vector<POLY>, double, CompareSig> uniqueWeightList;

//std::vector<int> rxmsglen;
std::vector<int> rxseq;
std::vector<int> txseq;

std::vector<POLY> dummysig;

// hash table statistics
int numReads;
int numWrites;
int numDocs;
int cacheHits;

// pSim
bool metric = 1;

// data transfered
int dataFetched = 0, dataStored = 0;

dhash_stat retrieveStatus;

chordID myGuess;

// retrieve TIMEOUT
const int RTIMEOUT = 20;

// copied from psi.C
// Fetch callback for NOAUTH...
void
fetch_cb(dhash_stat stat, ptr<dhash_block> blk, vec<chordID> path)
{
	retrieveStatus = DHASH_OK;
   	myGuess = 0;
 
	if (stat != DHASH_OK) {
		//warnx << "Failed to fetch key...\n";
		if (stat == DHASH_DBERR) {
			warnx << "PANIC time...\n";
        	}
		retrieveStatus = stat;
		retrieveError = true;
    		out++;
        	return;
	}

	if (blk) {
		strbuf buf;
		
#ifdef _DEBUG_
		buf << " /times=";
		for (u_int i = 0; i < blk->times.size (); i++)
			buf << " " << blk->times[i];
		buf << " /";
		
		buf << "hops= " << blk->hops << " errors= " <<  blk->errors
		    << "vretries= " << blk->retries << " ";
		///////////////////
		buf << " path: ";
		for (u_int i=0; i < path.size (); i++) {
			buf << path[i] << " ";
		}
		
		buf << " \n";
		warnx << buf;
#endif

		myGuess = path.size() > 0 ? path[path.size() - 1] : 0;

		if (blk->vData.size () > 0) {
			//warn << blk->vData.size () << "\n";

			for (unsigned int i = 0; i < blk->vData.size (); i++) {
				nodeEntries.push_back(blk->vData[i]);
				dataFetched += nodeEntries[i].len();
			}
		} else {
		    nodeEntries.push_back(blk->data);
		}
		
		// Output hops
		warnx << "HOPS: " << blk->hops << "\n";
		numReads++;
		
#ifdef _DEBUG_ 
		warnx << "Retrieve information: " << buf << "\n";
#endif
	}
	out++;
}

// copied from psi.C
void
store_cb(dhash_stat status, ptr<insert_info> i)
{
	if (status == DHASH_NOTFIRST) {
		insertStatus = NOTFIRST;
	} else if (status == DHASH_FULL) {
		insertStatus = FULL;
	} else if (status == DHASH_RETRYINSERT) {
		insertStatus = RETRY;
	} else if (status == DHASH_CORRUPTHDR) {
		insertStatus = CORRUPTHDR;
		// Trying again
		//insertError = true;
	} else if (status != DHASH_OK) {
		warnx << "failed to store key...\n";
		insertError = true;
	} else {
		numWrites++;
		warnx << "key stored successfully...\n";
	}
	out++;
}

// TODO: why?
// error: no match for ‘operator=’ in ‘minhash = lsh::getHashCodeFindMod
// error: no match for ‘operator=’ in ‘minhash = lsh::getHashCode
template <class T>
int lshall(int listnum, std::vector<std::vector<T> > &matrix, unsigned int loseed, InsertType msgtype = 0)
{
	std::vector<T> minhash;
	std::vector<POLY> sig;
	str sigbuf;
	chordID ID;
	//DHTStatus status;
	//char *value;
	//int col, valLen;
	double freq, weight;

	for (mapType::iterator itr = allT[listnum].begin(); itr != allT[listnum].end(); itr++) {
		sig = itr->first;
		freq = itr->second[0];
		weight = itr->second[1];

		lsh *myLSH = new lsh(sig.size(), lfuncs, mgroups, lshseed, irrpolyfile);
		// convert multiset to set
		if (uflag == 1) {
			sig2str(sig, sigbuf);
			warnx << "multiset: " << sigbuf << "\n";
			myLSH->getUniqueSet(sig);
			sig2str(sig, sigbuf);
			warnx << "set: " << sigbuf << "\n";
		}

		// XXX
		std::vector<POLY> polyhash;
		std::vector<chordID> idhash;
		warnx << "typeid: " << typeid(minhash).name() << "\n";
		if (typeid(minhash) == typeid(polyhash)) {
			polyhash = myLSH->getHashCodeFindMod(sig, myLSH->getIRRPoly());
		} else if (typeid(minhash) == typeid(idhash)) {
			idhash = myLSH->getHashCode(sig);
		} else {
			warnx << "invalid type\n";
			return -1;
		}

		/*
		//warnx << "minhash.size(): " << minhash.size() << "\n";
		if (plist == 1) {
			warnx << "minhash IDs:\n";
			for (int i = 0; i < (int)minhash.size(); i++) {
				warnx << minhash[i] << "\n";
			}
		}

		matrix.push_back(minhash);
		//srand(time(NULL));
		srand(loseed);
		int range = (int)minhash.size();
		col = int((double)range * rand() / (RAND_MAX + 1.0));
		ID = (matrix.back())[col];
		warnx << "ID in col " << col << ": " << ID << "\n";

		if (gflag == 1 && msgtype != 0) {
			strbuf t;
			t << ID;
			str key(t);
			warnx << "inserting INITGOSSIP:\n";
			makeKeyValue(&value, valLen, key, sig, freq, weight, msgtype);
			status = insertDHT(ID, value, valLen, MAXRETRIES);
			cleanup(value);

			// do not exit if insert FAILs!
			if (status != SUCC) {
				warnx << "error: insert FAILed\n";
			} else {
				warnx << "insert SUCCeeded\n";
			}

			// TODO: how long (if at all)?
			warnx << "sleeping...\n";
			sleep(intval);
		}
		*/
		delete myLSH;
	}
	return 0;
}

// TODO: verify
int
lshchordID(int listnum, std::vector<std::vector<chordID> > &matrix, unsigned int loseed = 0, int intval = -1, InsertType msgtype = INVALID)
{
	std::vector<chordID> minhash;
	std::vector<POLY> sig;
	chordID ID;
	DHTStatus status;
	char *value;
	int valLen;
	double freq, weight;
	//str sigbuf;

	sig.clear();
	for (mapType::iterator itr = allT[listnum].begin(); itr != allT[listnum].end(); itr++) {
		sig = itr->first;
		freq = itr->second[0];
		weight = itr->second[1];

		lsh *myLSH = new lsh(sig.size(), lfuncs, mgroups, lshseed, irrpolyfile);
		// convert multiset to set
		if (uflag == 1) {
			//sig2str(sig, sigbuf);
			//warnx << "multiset: " << sigbuf << "\n";
			myLSH->getUniqueSet(sig);
			//sig2str(sig, sigbuf);
			//warnx << "set: " << sigbuf << "\n";
		}

		minhash = myLSH->getHashCode(sig);

		/*
		warnx << "minhash.size(): " << minhash.size() << "\n";
		if (plist == 1) {
			warnx << "minhash IDs:\n";
			for (int i = 0; i < (int)minhash.size(); i++) {
				warnx << minhash[i] << "\n";
			}
		}
		*/

		matrix.push_back(minhash);
		int range = (int)minhash.size();
		// randomness verified
		int col = randomNumGenZ(range-1);
		ID = (matrix.back())[col];
		warnx << "ID in col " << col << ": " << ID << "\n";

		if (gflag == 1 && msgtype != INVALID) {
			strbuf t;
			t << ID;
			str key(t);
			warnx << "inserting " << msgtype << ":\n";
			makeKeyValue(&value, valLen, key, sig, freq, weight, msgtype);
			status = insertDHT(ID, value, valLen, MAXRETRIES);
			cleanup(value);

			// do not exit if insert FAILs!
			if (status != SUCC) {
				// TODO: do I care?
				warnx << "error: insert FAILed\n";
			} else {
				warnx << "insert SUCCeeded\n";
			}

			// TODO: how long (if at all)?
			warnx << "sleeping (lsh)...\n";
			sleep(intval);
		}
		delete myLSH;
	}
	return 0;
}

// TODO: verify
int
lshpoly(int listnum, std::vector<std::vector<POLY> > &matrix, unsigned int loseed = 0, int intval = 0, InsertType msgtype = INVALID)
{
	std::vector<POLY> minhash;
	std::vector<POLY> sig;
	chordID ID;
	POLY mypoly;
	DHTStatus status;
	char *value;
	int valLen;
	double freq, weight;
	//str sigbuf;

	for (mapType::iterator itr = allT[listnum].begin(); itr != allT[listnum].end(); itr++) {
		sig = itr->first;
		freq = itr->second[0];
		weight = itr->second[1];

		lsh *myLSH = new lsh(sig.size(), lfuncs, mgroups, lshseed, irrpolyfile);
		// convert multiset to set
		if (uflag == 1) {
			//sig2str(sig, sigbuf);
			//warnx << "multiset: " << sigbuf << "\n";
			myLSH->getUniqueSet(sig);
			//sig2str(sig, sigbuf);
			//warnx << "set: " << sigbuf << "\n";
		}

		minhash = myLSH->getHashCodeFindMod(sig, myLSH->getIRRPoly());

		//warnx << "minhash.size(): " << minhash.size() << "\n";
		/*
		if (plist == 1) {
			warnx << "minhash IDs:\n";
			for (int i = 0; i < (int)minhash.size(); i++) {
				warnx << minhash[i] << "\n";
			}
		}
		*/

		matrix.push_back(minhash);
		int range = (int)minhash.size();
		// randomness verified
		int col = randomNumGenZ(range-1);
		mypoly = (matrix.back())[col];
		warnx << "POLY in col " << col << ": " << mypoly << "\n";

		if (gflag == 1 && msgtype != INVALID) {
			strbuf mybuf;
			mybuf << mypoly;
			str mystr(mybuf);
			// TODO: verify
			ID = compute_hash(mystr, mystr.len());
			warnx << "compute_hash(POLY): " << ID << "\n";
			strbuf t;
			t << ID;
			str key(t);
			warnx << "inserting " << msgtype << ":\n";
			makeKeyValue(&value, valLen, key, sig, freq, weight, msgtype);
			status = insertDHT(ID, value, valLen, MAXRETRIES);
			cleanup(value);

			// do not exit if insert FAILs!
			if (status != SUCC) {
				warnx << "error: insert FAILed\n";
			} else {
				warnx << "insert SUCCeeded\n";
			}

			// TODO: how long (if at all)?
			warnx << "sleeping (lsh)...\n";
			sleep(intval);
		}
		delete myLSH;
	}
	return 0;
}

int
main(int argc, char *argv[])
{
	//pthread_t thread_ID;
	//void *exit_status;

	int Gflag, Lflag, lflag, rflag, Sflag, sflag, zflag, vflag, Hflag, dflag, jflag, mflag;
	int ch, gintval, initintval, waitintval, nids, valLen, logfd;
	double beginTime, endTime;
	char *value;
	struct stat statbuf;
	time_t rawtime;
	chordID ID;
	DHTStatus status;
	str sigbuf;
	std::string sigdir;
	std::vector<std::string> sigfiles;
	std::vector<std::vector<POLY> > sigList;
	std::vector<POLY> sig;

	Gflag = Lflag = lflag = rflag = Sflag = sflag = zflag = vflag = Hflag = dflag = jflag = mflag = 0;

	gintval = initintval = waitintval = nids = 0;
	irrpolyfile = NULL;
	rxseq.clear();
	txseq.clear();
	// init or txseq.back() segfaults!
	txseq.push_back(0);

	while ((ch = getopt(argc, argv, "B:cd:G:gHhI:i:j:L:lmn:pq:R:rS:s:uvw:z")) != -1)
		switch(ch) {
		case 'B':
			mgroups = strtol(optarg, NULL, 10);
			break;
		case 'R':
			lfuncs = strtol(optarg, NULL, 10);
			break;
		case 'c':
			discardmsg = 1;
			break;
		case 'd':
			dflag = 1;
			lshseed = strtol(optarg, NULL, 10);
			break;
		case 'G':
			Gflag = 1;
			gsock = optarg;
			break;
		case 'g':
			gflag = 1;
			break;
		case 'H':
			Hflag = 1;
			break;
		case 'I':
			initintval = strtol(optarg, NULL, 10);
			break;
		case 'i':
			gintval = strtol(optarg, NULL, 10);
			break;
		case 'j':
			jflag = 1;
			irrpolyfile = optarg;
			break;
		case 'L':
			Lflag = 1;
			logfile = optarg;
			break;
		case 'l':
			lflag = 1;
			break;
		case 'm':
			mflag = 1;
			break;
		case 'n':
			nids = strtol(optarg, NULL, 10);
			break;
		case 'p':
			plist = 1;
			break;
		case 'q':
			peers = strtol(optarg, NULL, 10);
			break;
		case 'r':
			rflag = 1;
			break;
		case 'S':
			Sflag = 1;
			dsock = optarg;
			break;
		case 's':
			sflag = 1;
			sigdir = optarg;
			break;
		case 'u':
			uflag = 1;
			break;
		case 'w':
			waitintval = strtol(optarg, NULL, 10);
			break;
		case 'z':
			zflag = 1;
			break;
		case 'v':
			vflag = 1;
			break;
		case 'h':
		case '?':
		default:
			usage();
			break;
		}
	argc -= optind;
	argv += optind;

	if (vflag == 1) {
		warnx << "rcsid: " << rcsid << "\n";
		exit(0);
	}

	// set random local seed
	char host[256];
	struct timeval tv;
	unsigned int loseed;
	gethostname(host, 256);
	gettimeofday(&tv, NULL);
	loseed = ((long)tv.tv_sec + (long)tv.tv_usec) / (long)getpid();
	//warnx << "loseed: " << loseed << "\n";

	srandom(loseed);
	if (zflag == 1 && nids != 0) {
		std::vector<chordID> ids;
		warnx << "unsorted IDs:\n";
		for (int i = 0; i < nids; i++) {
			strbuf t;
			ID = make_randomID();
			//t << ID;
			//str tmpkey(t);
			//warnx << tmpkey << "\n";
			warnx << "ID: " << ID << "\n";
			ids.push_back(ID);
			//warnx << "successorID: " << successorID(ID, 0) << "\n";
			//sleep(5);
		}
		sort(ids.begin(), ids.end());
		warnx << "sorted IDs:\n";
		for (int i = 0; i < (int)ids.size(); i++) {
			warnx << "ID: " << ids[i] << "\n";
		}
		return 0;
	}

	// TODO: make sure only one flag is set at a time
	// read, gossip, LSH or listen
	if (rflag == 0 && gflag == 0 && lflag == 0 && Hflag == 0) usage();

	// sockets are required when listening or gossiping
	if ((gflag == 1 || lflag == 1) && (Gflag == 0 || Sflag == 0)) usage();

	if (gflag == 1 && (gintval == 0 || sflag == 0)) usage();

	// action H
	if (Hflag == 1 && (sflag == 0 || dflag == 0 || jflag == 0))
		usage();

	// option H (for gossiping)
	if ((Hflag == 1 && gflag == 1) && (waitintval == 0 || initintval == 0))
		usage();

	// sigs are required when listening or reading
	if ((lflag == 1 || rflag == 1) && sflag == 0) usage();

	if (Lflag == 1) {
		logfd = open(logfile, O_RDWR | O_CREAT, 0666);
		if (logfd < 0) fatal << "can't open log file " << logfile << "\n";
		lseek(logfd, 0, SEEK_END);
		errfd = logfd;
	}

	if (jflag == 1) {
		if (stat(irrpolyfile, &statbuf) != 0)
			fatal << "'" << irrpolyfile << "' does not exist" << "\n";
	}

	if (gflag == 1 || lflag == 1) {
		if (stat(dsock, &statbuf) != 0)
			fatal << "'" << dsock << "' does not exist" << "\n";
		dhash = New dhashclient(dsock);
	}

	if (rflag == 1 || gflag == 1 || Hflag == 1 || lflag == 1) {
		getdir(sigdir, sigfiles);
		sigList.clear();

		// insert dummy
		// the polynomial "1" has a degree 0
		if (Hflag == 0) {
			dummysig.clear();
			dummysig.push_back(1);
			sigList.push_back(dummysig);
		}

		warnx << "reading signatures from files...\n";
		for (unsigned int i = 0; i < sigfiles.size(); i++) {
			readsig(sigfiles[i], sigList);
		}
		warnx << "calculating frequencies...\n";
		calcfreq(sigList);
		if (plist == 1) {
			printlist(0, -1);
		}
	}

	time(&rawtime);
	warnx << "ctime: " << ctime(&rawtime);
	warnx << "sincepoch: " << time(&rawtime) << "\n";
	warnx << "rcsid: " << rcsid << "\n";
	warnx << "host: " << host << "\n";
	warnx << "pid: " << getpid() << "\n";
	warnx << "peers: " << peers << "\n";
	warnx << "loseed: " << loseed << "\n";
	warnx << "mgroups: " << mgroups << "\n";
	warnx << "lfuncs: " << lfuncs << "\n";
	std::vector<std::vector<POLY> > pmatrix;
	std::vector<std::vector<chordID> > cmatrix;

	// XGossip+ init phase
	if (Hflag == 1) {
		// enter init phase
		initphase = 1;
		if (gflag == 1) {
			warnx << "listening for gossip...\n";
			listengossip();		
			sleep(5);
			warnx << "initgossipsend...\n";
			warnx << "init interval: " << initintval << "\n";
		}

		// InitGossipSend: use findMod()
		if (mflag == 1) {
			// T_m is 1st list
			lshpoly(0, pmatrix, loseed, initintval, INITGOSSIP);
			pmatrix.clear();
		// InitGossipSend: use compute_hash()
		} else {
			lshchordID(0, cmatrix, loseed, initintval, INITGOSSIP);
			cmatrix.clear();
		}

		if (gflag == 1) {
			warnx << "xgossip+ init phase done\n";
			warnx << "wait interval: " << waitintval << "\n";
			warnx << "waiting for all peers to finish init phase...\n";
			sleep(waitintval);
		}

		// exit init phase
		initphase = 0;
	}

	// XGossip exec phase
	if (gflag == 1 && Hflag == 0) {
		warnx << "listening for gossip...\n";
		//pthread_mutex_init(&lock, NULL);
		//pthread_create(&thread_ID, NULL, listengossip, NULL);
		listengossip();		
		sleep(5);
		warnx << "xgossip exec...\n";
		warnx << "gossip interval: " << gintval << "\n";

		srandom(loseed);
		while (1) {
			ID = make_randomID();
			strbuf z;
			z << ID;
			str key(z);
			//std::vector<POLY> sig;
			//sig.clear();

			//pthread_mutex_lock(&lock);
			beginTime = getgtod();    
			mergelists();
			endTime = getgtod();    
			printdouble("merge lists time: ", endTime - beginTime);
			warnx << "\n";
			warnx << "inserting XGOSSIP:\n"
			      << "txseq: " << txseq.back()
			      << " txID: " << ID << "\n";
			if (plist == 1) {
				printlist(0, txseq.back());
			}
			makeKeyValue(&value, valLen, key, allT[0], txseq.back(), XGOSSIP);
			//pthread_mutex_unlock(&lock);
			status = insertDHT(ID, value, valLen, MAXRETRIES);
			cleanup(value);

			// do not exit if insert FAILs!
			if (status != SUCC) {
				warnx << "error: insert FAILed\n";
				// to preserve mass conservation:
				// "send" msg to yourself
				doublefreq(0);
			} else {
				warnx << "insert SUCCeeded\n";
			}
			txseq.push_back(txseq.back() + 1);
			//pthread_join(thread_ID, &exit_status);
			warnx << "sleeping (gossip)...\n";
			sleep(gintval);
		}
		// when do we destroy the thread?
		//pthread_mutex_destroy(&lock);
	// XGossip+ exec phase
	} else if (gflag == 1 && Hflag == 1) {
		//pthread_mutex_init(&lock, NULL);
		//pthread_create(&thread_ID, NULL, listengossip, NULL);
		warnx << "xgossip+ exec...\n";
		warnx << "gossip interval: " << gintval << "\n";

		// xgossip+ sigs grouped by lsh chordid/poly
		vecomap groupedT;

		// needed?
		//srandom(loseed);
		while (1) {
			//pthread_mutex_lock(&lock);
			beginTime = getgtod();    
			mergelistsp();
			endTime = getgtod();    
			printdouble("merge lists time: ", endTime - beginTime);
			warnx << "\n";
			delspecial(0);
			if (plist == 1) {
				printlist(0, txseq.back());
			}

			//srand(loseed);
			// TODO: the same as minhash.size()?
			int range = mgroups;
			//int col = int((double)range * rand() / (RAND_MAX + 1.0));
			// TODO: verify randomness
			int col = randomNumGenZ(range-1);
			// TODO: use findMod()
			if (mflag == 1) {
				// don't send anything
				lshpoly(0, pmatrix);
				poly2sig polyindex;
				warnx << "pmatrix.size(): " << pmatrix.size() << "\n";
				warnx << "POLYs in random column " << col << ":\n";
				for (int i = 0; i < (int)pmatrix.size(); i++) {
					warnx << pmatrix[i][col] << "\n";;
					// store index of signature associated with POLY
					polyindex[pmatrix[i][col]].push_back(i);
				}
				warnx << "POLY: [sig indices].size()\n";
				for (poly2sig::iterator itr = polyindex.begin();
				     itr != polyindex.end(); itr++) {
					warnx << itr->first << ": " << itr->second.size() << "\n";
				}
				pmatrix.clear();
			// use compute_hash()
			} else {
				// don't send anything
				lshchordID(0, cmatrix);
				chordID2sig idindex;
				warnx << "cmatrix.size(): " << cmatrix.size() << "\n";
				warnx << "IDs in random column " << col << ":\n";
				for (int i = 0; i < (int)cmatrix.size(); i++) {
					//warnx << cmatrix[i][col] << "\n";;
					// store index of signature associated with chordID
					idindex[cmatrix[i][col]].push_back(i);
					/*
					chordID2sig::iterator itr = idindex.find(matrix[i][col]);
					if (itr != idindex.end()) {
						itr->second[0] += 1;
					} else {
						// store index of signature associated with chordID
						idindex[matrix[i][col]].push_back(i);
					}
					*/

				}
				warnx << "idindex.size(): " << idindex.size() << "\n";
				warnx << "ID: sigs-per-id sigs-indices\n";
				int uniqueids = 0;
				for (chordID2sig::iterator itr = idindex.begin();
				     itr != idindex.end(); itr++) {
					uniqueids += itr->second.size();
					warnx << itr->first << ": " << itr->second.size();
					if (itr->second.size() > 1) {
						for (int i = 0; i < (int)itr->second.size(); i++) {
							warnx << " " << itr->second[i];
						}
						warnx << "\n";
					} else {
					      warnx << " " << itr->second[0] << "\n";
					}

					// create a map for each chordID
					mapType groupbyid;
					//pthread_mutex_lock(&lock);
					//warnx << "groupbyid:\n";
					for (int i = 0; i < (int)itr->second.size(); i++) {
						mapType::iterator j = allT[0].begin();
						// point to the nth map element (random access)
						std::advance(j, itr->second[i]);
						assert(j != allT[0].end());
						//sig2str(j->first, sigbuf);
						//warnx << "sig: " << sigbuf;
						// copy freq and weight of sig
						groupbyid[j->first].push_back(j->second[0]);
						groupbyid[j->first].push_back(j->second[1]);
						//printdouble(" ", j->second[0]);
						//printdouble(" ", j->second[1]);
						//warnx << "\n";
					}
					groupedT.push_back(groupbyid);
					//pthread_mutex_unlock(&lock);

				}
				warnx << "uniqueids: " << uniqueids << "\n";
				warnx << "groupedT.size(): " << groupedT.size() << "\n";

				warnx << "inserting XGOSSIPP:\n"
				      << "txseq: " << txseq.back() << "\n";

				chordID2sig::iterator itr = idindex.begin();
				for (int i = 0; i < (int)groupedT.size(); i++) {
					// end addresses the location succeeding the last element 
					// in a map (not the last element itself)
					if (itr == idindex.end()) {
						warnx << "idindex ended\n";
						break;
					}
					/*
					warnx << "groupedT[" << i << "]:\n";
					double freq, weight;
					for (mapType::iterator jitr = groupedT[i].begin(); jitr != groupedT[i].end(); jitr++) {
						sig = jitr->first;
						freq = jitr->second[0];
						weight = jitr->second[1];
						sig2str(sig, sigbuf);
						warnx << "sig: " << sigbuf;
						printdouble(" ", freq);
						printdouble(" ", weight);
						warnx << "\n";
					}
					*/
					ID = itr->first;
					strbuf z;
					z << ID;
					str key(z);
					warnx << "txID: " << ID << "\n";

					// TODO: same round for each group?
					makeKeyValue(&value, valLen, key, groupedT[i], txseq.back(), XGOSSIPP);

					status = insertDHT(ID, value, valLen, MAXRETRIES);
					cleanup(value);

					// do not exit if insert FAILs!
					if (status != SUCC) {
						warnx << "error: insert FAILed\n";
						// to preserve mass conservation:
						// "send" msg to yourself
						// TODO: needed?
						doublefreqgroup(0, groupedT[i]);
					} else {
						warnx << "insert SUCCeeded\n";
					}

					++itr;

					warnx << "sleeping (groups)...\n";
					// TODO: needed?
					sleep(initintval);
				}
				txseq.push_back(txseq.back() + 1);
				warnx << "sleeping (xgossip+)...\n";
				cmatrix.clear();
				sleep(gintval);
			}
		}
		// when do we destroy the thread?
		//pthread_mutex_destroy(&lock);
	} else if (rflag == 1) {
		return 0;
	} else if (lflag == 1) {
		warnx << "listening for gossip...\n";
		listengossip();
		//listengossip(NULL);
	} else if (Hflag == 1) {
		return 0;
	} else {
		usage();
	}

	amain();
	return 0;
}

// copied from psi.C
DHTStatus
insertDHT(chordID ID, char *value, int valLen, int STOPCOUNT, chordID guess)
{
	//warnx << "txID: " << ID << "\n";
	dataStored += valLen;
	int numTries = 0;
		
	// Make sure the guess is a successor of ID
	if (guess < ID) {
		if (maxID - ID + guess < ID - guess) {
		// nothing	
		} else {
			guess = 0;
		}
	} else {
		if (maxID - guess + ID < guess - ID) {
			guess = 0;	
		}
	}

	do {    
		out = 0;
		insertError = false;
		insertStatus = SUCC;
		double beginTime = getgtod();    

		if (guess > 0) {
			ptr<option_block> options = New refcounted<option_block>();
			options->flags = DHASHCLIENT_GUESS_SUPPLIED;
			options->guess = guess;
			dhash->insert(ID, (char *) value, valLen, wrap(store_cb), options, DHASH_NOAUTH);
		} else {
			dhash->insert(ID, (char *) value, valLen, wrap(store_cb), NULL, DHASH_NOAUTH);
		}

		while (out == 0) acheck();
		double endTime = getgtod();
		// XXX
		//char timebuf[1024];
		//snprintf(timebuf, sizeof(timebuf), "%f", endTime - beginTime);
		//warnx << "key insert time: " << timebuf << " secs\n";
		printdouble("key insert time: ", endTime - beginTime);
		warnx << "\n";
		// Insert was successful
		if (!insertError) {
			return insertStatus;
		}
		numTries++;
		warnx << "numTries: " << numTries << "\n";
		 
		if (insertError && numTries == STOPCOUNT) {
			warnx << "Error during INSERT operation...\n";
			return FAIL;
		}
					
		// If error then don't use guess
		//guess = 0;

		//warnx << "Sleeping 10 seconds...\n";
		//sleep(10);
	} while (insertError);

	return insertStatus;
}

// copied from psi.C
void
retrieveDHT(chordID keyID, int MAXWAIT, str& sigdata, chordID guess)
{
	if (((keyID >> 32) << 32) == 0) {
		warnx << "Bad node id... Skipping retrieve...\n";
		retrieveError = true;
		return;
	}

	// Make sure the guess is a successor of keyID
	if (guess < keyID) {
		guess = 0;
	}
	nodeEntries.clear();
	// now fetch the node
	out = 0;
	retrieveError = false;
	//warnx << "Retrieve: " << keyID << " " << sigdata << "\n";	
	//warnx << "Retrieve: " << keyID << "\n";
	double beginTime = getgtod();
	if (guess > 0) {
		ptr<option_block> options = New refcounted<option_block>();
		options->flags = DHASHCLIENT_GUESS_SUPPLIED;
		options->guess = guess;
		dhash->retrieve(keyID, DHASH_NOAUTH, wrap(fetch_cb), sigdata, options);
	} else {
		dhash->retrieve(keyID, DHASH_NOAUTH, wrap(fetch_cb), sigdata);
	}
	//double retrieveBegin = getgtod();
	int numTries = 0;
	while (out == 0) {
		acheck();
		/*double retrieveEnd = getgtod();
		if (retrieveEnd - retrieveBegin > MAXWAIT) {
		    retrieveError = true;
            break;
        }*/
#ifdef _LIVE_
        if (numTries == MAXRETRIEVERETRIES) {
#else
		if (numTries == 20) {
#endif
			retrieveError = true;
			break;
		}
		numTries++;
	}
	double endTime = getgtod();
	std::cerr << "Retrieve time: " << endTime - beginTime << std::endl;

	// Extra check
	if (!retrieveError && nodeEntries.size() == 0) {
		warnx << "EMPTY contents...\n";
		retrieveError = true;
	}
	//warnx << "# of node entries: " << nodeEntries.size() << "\n";
}

// deprecated: use make_randomID() instead
chordID
randomID(void)
{
	char buf[16];
	int j;
	strbuf s;
	chordID ID;

	srand(time(NULL));
	//warnx << "j:  ";
	for (int i = 0; i < 40; i++) {
		//j = randomNumGen();
		j = rand() % 16;
		sprintf(buf, "%x", j);
		//warnx << buf;
		s << buf;
	}
	//warnx << "\n";
	//warnx << "s:  " << s << "\n";
	str t(s);
	//warnx << "t:  " << t << "\n";
	//warnx << "len: " << t.len() << "\n";
	str2chordID(t, ID);
	return ID;
}

// verified
//void *
//listengossip(void *)
void
listengossip(void)
{
	unlink(gsock);
	int fd = unixsocket(gsock);
	if (fd < 0) fatal << "Error creating gsock" << strerror (errno) << "\n";

	// make socket non-blocking
	make_async(fd);

	if (listen(fd, MAXCONN) < 0) {
		// TODO: what's %m?
		fatal("Error from listen: %m\n");
		close(fd);
		return;
	}

	fdcb(fd, selread, wrap(acceptconnection, fd));

	//return NULL;
}

// verified
void
acceptconnection(int fd)
{
	sockaddr_in sin;
	unsigned sinlen = sizeof(sin);

	//bzero(&sin, sizeof(sin));
	int cs = accept(fd, (struct sockaddr *) &sin, &sinlen);
	if (cs >= 0) {
		warnx << "accept: connection on local socket: cs=" << cs << "\n";
	} else if (errno != EAGAIN) {
		warnx << "accept: error: " << strerror(errno) << "\n";
		// disable readability callback?
		//fdcb(fd, selread, 0);
		return;
	}
	
	fdcb(cs, selread, wrap(readgossip, cs));
}

// TODO: verify xgossip+ part
void
readgossip(int fd)
{
	strbuf buf;
	strbuf totalbuf;
	str key;
	str sigbuf;
	chordID ID;
	std::vector<std::vector<POLY> > sigList;
	std::vector<double> freqList;
	std::vector<double> weightList;
	InsertType msgtype;
	int n, msglen, recvlen; 
	int ret, seq;

	msglen = recvlen = 0;
	warnx << "reading from socket:\n";

	do {
		n = buf.tosuio()->input(fd);

		if (n < 0) {
			warnx << "readgossip: read failed\n";
			// disable readability callback?
			fdcb(fd, selread, 0);
			// do you have to close?
			//close(fd);
			return;
		}

		// EOF
		if (n == 0) {
			warnx << "readgossip: no more to read\n";
			// 0 or NULL?
			fdcb(fd, selread, 0);
			// do you have to close?
			close(fd);
			// what's the difference and should we continue?
			//exit(0);
			//return;
		}

		// run 1st time only
		if (msglen == 0) {
			str gbuf = buf;
			msglen = getKeyValueLen(gbuf);
			warnx << "rxmsglen: " << msglen;
		}

		recvlen += n;

#ifdef _DEBUG_
		warnx << "\nreadgossip: n: " << n << "\n";
		warnx << "readgossip: recvlen: " << recvlen << "\n";
#endif

		totalbuf << buf;
		buf.tosuio()->clear();

	// XXX: InsertType
	//} while ((recvlen + (int)sizeof(int)) < msglen);
	} while (recvlen < msglen);

	str gmsg(totalbuf);
	sigList.clear();
	freqList.clear();
	weightList.clear();

	msgtype = getKeyValueType(gmsg.cstr());
	warnx << " type: ";
	if (msgtype == XGOSSIP || msgtype == XGOSSIPP) {
		// discard msg if in init phase
		if (initphase == 1) {
			warnx << "warning: phase=init, msgtype=" << msgtype << "\n";
			warnx << "warning: msg discarded\n";
			return;
		} else if (msgtype == XGOSSIP) {
			warnx << "XGOSSIP";
		} else {
			warnx << "XGOSSIPP";
		}

		ret = getKeyValue(gmsg.cstr(), key, sigList, freqList, weightList, seq, recvlen);
		if (ret == -1) {
			warnx << "error: getKeyValue failed\n";
			fdcb(fd, selread, 0);
			close(fd);
			return;
		}

		warnx << " rxlistlen: " << sigList.size()
		      << " rxseq: " << seq
		      << " txseq-cur: " << txseq.back()
		      << " rxID: " << key << "\n";

		// count rounds off by one
		n = seq - txseq.back();
		if (n < 0) {
			warnx << "warning: rxseq<txseq n: " << abs(n) << "\n";
			if (discardmsg == 1) {
				if (abs(n) != 1) {
					warnx << "warning: msg discarded\n";
					return;
				}
			}
		} else if (n > 0) {
			warnx << "warning: rxseq>txseq n: " << n << "\n";
			if (discardmsg == 1) {
				if (n != 1) {
					warnx << "warning: msg discarded\n";
					return;
				}
			}
		}

#ifdef _DEBUG_
		for (int i = 0; i < (int) sigList.size(); i++) {
			sig2str(sigList[i], sigbuf);
			warnx << "sig[" << i << "]: " << sigbuf << "\n";
		}
		for (int i = 0; i < (int) freqList.size(); i++) {
			printdouble("freq[i]: ", freqList[i]);
			warnx << "\n";
		}
		for (int i = 0; i < (int) weightList.size(); i++) {
			printdouble("weight[i]: ", weightList[i]);
			warnx << "\n";
		}
#endif

		rxseq.push_back(seq);
		add2vecomap(sigList, freqList, weightList);
	} else if (msgtype == INITGOSSIP || msgtype == INFORMTEAM) {
		// discard msg if not in init phase
		if (initphase == 0) {
			warnx << "warning: phase=exec, msgtype=" << msgtype << "\n";
			warnx << "warning: msg discarded\n";
			return;
		} else if (msgtype == INITGOSSIP) {
			warnx << "INITGOSSIP";
		} else {
			warnx << "INFORMTEAM";
		}

		double freq, weight;
		std::vector<POLY> sig;
		sig.clear();
		ret = getKeyValue(gmsg.cstr(), key, sig, freq, weight, recvlen);
		if (ret == -1) {
			warnx << "error: getKeyValue failed\n";
			fdcb(fd, selread, 0);
			close(fd);
			return;
		}

		warnx << " rxID: " << key << "\n";
		//sig2str(sig, sigbuf);
		//warnx << "sig: " << sigbuf;
		//printdouble(" ", freq);
		//printdouble(" ", weight);
		//warnx << "\n";
		str2chordID(key, ID);
		initgossiprecv(ID, sig, freq, weight);
	} else {
		warnx << "error: invalid msgtype\n";
	}
}

// based on:
// http://www.linuxquestions.org/questions/programming-9/c-list-files-in-directory-379323/
// verified
int
getdir(std::string dir, std::vector<std::string> &files)
{
	DIR *dp;
	struct dirent *dirp;

	// TODO: move to main
	if ((dp = opendir(dir.c_str())) == NULL) {
		fatal("can't read dir\n");
		return errno;
	}
	while ((dirp = readdir(dp)) != NULL) {
		if (strcmp(dirp->d_name, ".") == 0 || strcmp(dirp->d_name, "..") == 0) continue;
		files.push_back(dir + std::string(dirp->d_name));
	}
	closedir(dp);
	return 0;
}

// TODO: work w/ 0-sized sigs and path w/o trailing slash
void
readsig(std::string sigfile, std::vector<std::vector <POLY> > &sigList)
{
	double startTime, finishTime;
	FILE *sigfp;
	//FILE *tagsfp;
	//std::string tagsfile;
	chordID rootNodeID;

	startTime = getgtod();
	// open signatures
	warnx << "sigfile: " << sigfile.c_str() << "\n";
	sigfp = fopen(sigfile.c_str(), "r");
	// change to if?
	assert(sigfp);

	// TODO: add maxcount arg?

	// DONT use readData to retrieve signatures from input files...
	// since the size filed uses POLY as a basic unit and not byte...
	// Read numSigs <it should be 1> for data signatures...
	int numSigs;
	if (fread(&numSigs, sizeof(int), 1, sigfp) != 1) {
		warnx << "numSigs: " << numSigs << "\n";
	}
	warnx << "NUM sigs: " << numSigs;
	assert(numSigs == 1);

	int size;

	if (fread(&size, sizeof(int), 1, sigfp) != 1) {
		assert(0);
		warnx << "\ninvalid signature\n";
	}
	warnx << ", Signature size: " << size * sizeof(POLY) << " bytes\n";

	std::vector<POLY> sig;
	sig.clear();
	POLY e;
	warnx << "Document signature: ";
	for (int i = 0; i < size; i++) {
		if (fread(&e, sizeof(POLY), 1, sigfp) != 1) {
			assert(0);
		}
		sig.push_back(e);
	}
	str buf;
	sig2str(sig, buf);
	warnx << buf << "\n";
	sigList.push_back(sig);

	/*
	// isig test
	str sigbuf;
	std::vector<POLY> isig;
	isig.clear();
	isig = inverse(sig);
	sig2str(isig, sigbuf);
	sigList.push_back(isig);
	warnx << "isig: " << sigbuf << "\n";
	*/

	warnx << "readsig: Size of sig list: " << sigList.size() << "\n";
	finishTime = getgtod();
	fclose(sigfp);
}

// TODO: verify
void
initgossiprecv(chordID ID, std::vector<POLY> sig, double freq, double weight)
{
	std::vector<POLY> isig;
	//str sigbuf;

	isig.clear();
	isig = inverse(sig);
	//sig2str(isig, sigbuf);
	//warnx << "isig: " << sigbuf << "\n";

	mapType::iterator itr = allT[0].find(sig);
	mapType::iterator iitr = allT[0].find(isig);
	// s is regular and exists
	if ((sig[0] != 0) && (itr != allT[0].end())) {
		warnx << "initgossiprecv: update freq\n";
		itr->second[0] += freq;
	// s is regular and s' exists
	} else if ((sig[0] != 0) && (iitr != allT[0].end())) {
		warnx << "initgossiprecv: delete special multiset tuple\n";
		// TODO: inefficient?
		allT[0].erase(iitr);
		allT[0][sig].push_back(freq);
		allT[0][sig].push_back(weight);
	// s and s' exist (s is special)
	} else if ((itr != allT[0].end()) && (iitr != allT[0].end())) {
		warnx << "initgossiprecv: do nothing\n";
	} else {
		warnx << "initgossiprecv: inserting sig\n";
		allT[0][sig].push_back(freq);
		allT[0][sig].push_back(weight);
		warnx << "initgossiprecv: inform team\n";
		informteam(ID, sig);
	}
}

// TODO: verify
void
informteam(chordID myID, std::vector<POLY> sig)
{
	chordID nextID;
	std::vector<POLY> isig;
	DHTStatus status;
	char *value;
	int valLen;
	//str sigbuf;

	lsh *myLSH = new lsh(sig.size(), lfuncs, mgroups, lshseed, irrpolyfile);
	// TODO: verify getUniqueSet works right
	//warnx << "informteam: getUniqueSet\n";
	myLSH->getUniqueSet(sig);
	//warnx << "informteam: getHashCode\n";
	// TODO: findMod vs compute_hash!
	std::vector<chordID> minhash = myLSH->getHashCode(sig);

	isig.clear();
	// is sig always a regular multiset?
	isig = inverse(sig);
	//sig2str(isig, sigbuf);
	//warnx << "informteam: isig: " << sigbuf << "\n";

	// O(n logn)
	sort(minhash.begin(), minhash.end());
	//for (int i = 0; i < (int)minhash.size(); i++) {
	//	warnx << "minhash[" << i << "]: "<< minhash[i] << "\n";
	//}

	// TODO: verify
	nextID = myID;
	if (myID == minhash.back()) {
		warnx << "warning: myID is last, using first\n";
		nextID = minhash.front();
	} else {
		for (int i = 0; i < (int)minhash.size(); i++) {
			if (myID < minhash[i]) {
				nextID = minhash[i];
				break;
			}
		}
	}

	strbuf z;
	z << nextID;
	str key(z);
	double freq = 0;
	double weight = 1;
	makeKeyValue(&value, valLen, key, isig, freq, weight, INFORMTEAM);
	// or strlen?
	//ID = compute_hash(value, sizeof(value));
	warnx << "inserting INFORMTEAM:\n";
	warnx << "myID: " << myID << " nextID: " << nextID << "\n";
	status = insertDHT(nextID, value, valLen, MAXRETRIES);
	cleanup(value);

	if (status != SUCC) {
		// TODO: do I care?
		warnx << "error: insert FAILed\n";
	} else {
		warnx << "insert SUCCeeded\n";
	}

	delete myLSH;
}

// TODO: make more efficient? in-place instead?
std::vector<POLY>
inverse(std::vector<POLY> sig)
{
	std::vector<POLY> isig;
	isig.clear();

	if (sig[0] == 0) {
		// convert to regular multiset
		for (int i = 1; i < (int) sig.size(); i++)
			isig.push_back(sig[i]);
		return isig;
	} else {
		// convert to special multiset
		// TODO: what's faster?
		sig.insert(sig.begin(), 1, 0);
		//isig.push_back(0);
		//for (int i = 0; i < (int) sig.size(); i++)
		//	isig.push_back(sig[i]);
		return sig;
	}
}

// verified
bool
sig2str(std::vector<POLY> sig, str &buf)
{
	strbuf s;

	if (sig.size() <= 0) return false;

	for (int i = 0; i < (int) sig.size(); i++)
		s << sig[i] << "-";

	buf = s;
	return true;
}

// verified
void
calcfreq(std::vector<std::vector<POLY> > sigList)
{
	mapType uniqueSigList;

	// dummy's freq is 0 and weight is 1
	uniqueSigList[sigList[0]].push_back(0);
	uniqueSigList[sigList[0]].push_back(1);

	// skip dummy
	for (int i = 1; i < (int) sigList.size(); i++) {
		mapType::iterator itr = uniqueSigList.find(sigList[i]);
		if (itr != uniqueSigList.end()) {
			// do not increment weight when sig has duplicates!
			// increment only freq
			itr->second[0] += 1;
		} else {
			// set freq
			uniqueSigList[sigList[i]].push_back(1);
			// set weight
			uniqueSigList[sigList[i]].push_back(1);
		}
	}
	allT.push_back(uniqueSigList);
	warnx << "calcfreq: setsize: " << allT[0].size() << "\n";
}

// deprecated: use mergelists() instead
// verified
void
calcfreqM(std::vector<std::vector<POLY> > sigList, std::vector<double> freqList, std::vector<double> weightList)
{
	//int deg;

	//pthread_mutex_lock(&lock);
	// skip dummy
	for (int i = 1; i < (int) sigList.size(); i++) {
		mapType::iterator itr = allT[0].find(sigList[i]);
		if (itr != allT[0].end()) {
			itr->second[0] += freqList[i];
			itr->second[1] += weightList[i];
		} else {
			allT[0][sigList[i]].push_back(freqList[i]);
			allT[0][sigList[i]].push_back(weightList[i]);
		}
	}
	warnx << "calcfreqM: Size of unique sig list: " << allT[0].size() << "\n";
	//pthread_mutex_unlock(&lock);
}

// verified
void
add2vecomap(std::vector<std::vector<POLY> > sigList, std::vector<double> freqList, std::vector<double> weightList)
{
	mapType uniqueSigList;

	//pthread_mutex_lock(&lock);
	for (int i = 0; i < (int) sigList.size(); i++) {
		uniqueSigList[sigList[i]].push_back(freqList[i]);
		uniqueSigList[sigList[i]].push_back(weightList[i]);
	}
	allT.push_back(uniqueSigList);
	//pthread_mutex_unlock(&lock);
	warnx << "add2vecomap: allT.size(): " << allT.size() << "\n";
}

// copied from psi.C
// return TRUE if s1 < s2
bool
sigcmp(const std::vector<POLY>& s1, const std::vector<POLY>& s2)
{
	//warnx << "sigcmp: S1 size: " << s1.size()
	//      << ", S2 size: " << s2.size() << "\n";

	if (s1.size() < s2.size()) {
		return true;
	} else if (s1.size() == s2.size()) {
		for (int i = 0; i < (int) s1.size(); i++) {
			if (s1[i] < s2[i])
				return true;
			else if (s1[i] > s2[i])
				return false;
		}
		return false;
	} else {
		return false;
	}
}

// verified
void
mergelists()
{
	double sumf, sumw;
	str sigbuf;
	std::vector<POLY> minsig;
	minsig.clear();

	warnx << "merging:\n";
	int n = allT.size();
	warnx << "initial allT.size(): " << n << "\n";
	if (n == 1) {
		splitfreq(0);
		return;
	} else {
#ifdef _DEBUG_
		for (int i = 0; i < n; i++)
			printlist(i, -1);
#endif
	}

	// init pointers to beginning of lists
	std::vector<mapType::iterator> citr;
	for (int i = 0; i < n; i++) {
		citr.push_back(allT[i].begin());
		// skip dummy
		++citr[i];
	}

	while (1) {
		int j = n;
		// check if at end of lists
		for (int i = 0; i < n; i++) {
			if (citr[i] == allT[i].end()) {
				//warnx << "end of T_" << i << "\n";
				--j;
			}
		}
		// done with all lists
		if (j == 0) {
			sumw = 0;
			for (int i = 0; i < n; i++) {
				sumw += allT[i][dummysig][1];
			}
			printdouble("new local dummy sumw/2: ", sumw/2);
			warnx << "\n";
			allT[0][dummysig][1] = sumw / 2;
			break;
		}

		// find min sig
		int z = 0;
		for (int i = 0; i < n; i++) {
			// skip lists which are done
			if (citr[i] == allT[i].end()) {
				continue;
			} else if (z == 0){
				minsig = citr[i]->first;
#ifdef _DEBUG_
				sig2str(minsig, sigbuf);
				warnx << "initial minsig: "
				      << sigbuf << "\n";
#endif
				z = 1;

			}
			if (sigcmp(citr[i]->first, minsig) == 1)
				minsig = citr[i]->first;
		}

#ifdef _DEBUG_
		sig2str(minsig, sigbuf);
		warnx << "actual minsig: " << sigbuf << "\n";
#endif

		sumf = sumw = 0;
		// add all f's and w's for a particular sig
		for (int i = 0; i < n; i++) {
			// DO NOT skip lists which are done:
			// use their dummy
			if (citr[i] == allT[i].end()) {
				warnx << "no minsig in T_" << i
				      << " (list ended)\n";
				sumf += allT[i][dummysig][0];
				sumw += allT[i][dummysig][1];
				//continue;
			} else if (citr[i]->first == minsig) {
				warnx << "minsig found in T_" << i << "\n";
				sumf += citr[i]->second[0];
				sumw += citr[i]->second[1];
				// XXX: itr of T_0 will be incremented later
				if (i != 0) ++citr[i];
			} else {
				warnx << "no minsig in T_" << i << "\n";
				sumf += allT[i][dummysig][0];
				sumw += allT[i][dummysig][1];
			}
		}

#ifdef _DEBUG_
		printdouble("sumf: ", sumf);
		printdouble(", sumw: ", sumw);
		warnx << "\n";
#endif

		// update allT[0]
		if (citr[0]->first == minsig) {
			// update sums of existing sig
			//warnx << "updating sums of minsig\n";
			allT[0][minsig][0] = sumf/2;
			allT[0][minsig][1] = sumw/2;
			// XXX: see above XXX
			++citr[0];
		} else {
			// insert new sig
			//warnx << "inserting minsig...\n";
			allT[0][minsig].push_back(sumf/2);
			allT[0][minsig].push_back(sumw/2);

			// not needed:
			// if the minsig was missing in T_i,
			// itr already points to the next sig
			//citr[0] = allT[0].find(minsig);
			//++citr[0];

		}
	}

	// delete all except first (doesn't free memory but it's O(1))
	while (allT.size() > 1) allT.pop_back();
	//warnx << "allT.size() after pop: " << allT.size() << "\n";
}

// TODO: verify
void
mergelistsp()
{
	double sumf, sumw;
	str sigbuf;
	std::vector<POLY> minsig;
	std::vector<POLY> isig;
	minsig.clear();
	isig.clear();

	warnx << "merging:\n";
	int n = allT.size();
	warnx << "initial allT.size(): " << n << "\n";
	if (n == 1) {
		splitfreq(0);
		return;
	} else {
#ifdef _DEBUG_
		for (int i = 0; i < n; i++)
			printlist(i, -1);
#endif
	}

	// init pointers to beginning of lists
	std::vector<mapType::iterator> citr;
	for (int i = 0; i < n; i++) {
		citr.push_back(allT[i].begin());
		// skip dummy
		//++citr[i];
	}

	while (1) {
		int j = n;
		// check if at end of lists
		for (int i = 0; i < n; i++) {
			if (citr[i] == allT[i].end()) {
				//warnx << "end of T_" << i << "\n";
				--j;
			}
		}
		// done with all lists
		if (j == 0) {
			/*
			sumw = 0;
			for (int i = 0; i < n; i++) {
				sumw += allT[i][dummysig][1];
			}
			printdouble("new local dummy sumw/2: ", sumw/2);
			warnx << "\n";
			allT[0][dummysig][1] = sumw / 2;
			*/
			// TODO: split weight of all special multisets or is that done already?
			break;
		}

		// find min sig
		int z = 0;
		for (int i = 0; i < n; i++) {
			// skip lists which are done
			if (citr[i] == allT[i].end()) {
				continue;
			// DO NOT skip special multisets?
			/*
			} else if (citr[i]->first[0] == 0) {
				++citr[i];
				continue;
			*/
			} else if (z == 0){
				minsig = citr[i]->first;
#ifdef _DEBUG_
				sig2str(minsig, sigbuf);
				warnx << "initial minsig: "
				      << sigbuf << "\n";
#endif
				z = 1;

			}
			if (sigcmp(citr[i]->first, minsig) == 1)
				minsig = citr[i]->first;
		}

		sig2str(minsig, sigbuf);
		warnx << "actual minsig: " << sigbuf << "\n";

		if (minsig[0] == 0) {
			warnx << "minsig is a special multiset\n";
		}

		sumf = sumw = 0;
		// add all f's and w's for a particular sig
		for (int i = 0; i < n; i++) {
			// TODO: if end of list, will it segfault?
			if (citr[i]->first == minsig) {
				//warnx << "minsig found in T_" << i << "\n";
				sumf += citr[i]->second[0];
				sumw += citr[i]->second[1];
				// XXX: itr of T_0 will be incremented later
				if (i != 0) ++citr[i];
			// DO NOT skip lists which are done:
			// use special multiset
			// TODO: necessary to check if at end of list?
			} else if ((citr[i] == allT[i].end()) && (minsig[0] != 0)) {
				warnx << "no minsig in T_" << i
				      << " (list ended)\n";
				isig.clear();
				isig = inverse(minsig);
				mapType::iterator itr = allT[i].find(isig);
				if (itr != allT[i].end()) {
					warnx << "special minsig found in T_" << i << "\n";
					sumf += allT[i][isig][0];
					sumw += allT[i][isig][1];
				}
			}
			// DO NOT skip special multisets?
			/*
			} else if (citr[i]->first[0] == 0) {
				++citr[i];
				continue;
			} else if (minsig[0] != 0) {
				// use special multiset based on LSH (instead of isig)
				//tmpsig = minsig;
				//lsh *myLSH = new lsh(tmpsig.size(), lfuncs, mgroups, lshseed, irrpolyfile);
				// TODO: verify getUniqueSet works right
				//myLSH->getUniqueSet(tmpsig);
				//std::vector<chordID> minhash = myLSH->getHashCode(tmpsig);

				isig.clear();
				isig = inverse(minsig);
				mapType::iterator itr = allT[i].find(isig);
				if (itr != allT[i].end()) {
					warnx << "no minsig in T_" << i << "\n";
					sumf += allT[i][isig][0];
					sumw += allT[i][isig][1];
				}
			}
			*/
		}

#ifdef _DEBUG_
		printdouble("sumf: ", sumf);
		printdouble(", sumw: ", sumw);
		warnx << "\n";
#endif

		// update allT[0]
		if (citr[0]->first == minsig) {
			// update sums of existing sig
			//warnx << "updating sums of minsig\n";
			allT[0][minsig][0] = sumf/2;
			allT[0][minsig][1] = sumw/2;
			// XXX: see above XXX
			++citr[0];
		} else {
			// insert new sig
			//warnx << "inserting minsig...\n";
			allT[0][minsig].push_back(sumf/2);
			allT[0][minsig].push_back(sumw/2);

			// not needed:
			// if the minsig was missing in T_i,
			// itr already points to the next sig
			//citr[0] = allT[0].find(minsig);
			//++citr[0];

		}
	}

	// delete all except first (doesn't free memory but it's O(1))
	while (allT.size() > 1) allT.pop_back();
	//warnx << "allT.size() after pop: " << allT.size() << "\n";
}

// TODO: verify
void
doublefreqgroup(int listnum, mapType groupbyid)
{
	double freq, weight;

	warnx << "doublefreqgroup: setsize: " << groupbyid.size() << "\n";
	for (mapType::iterator itr = groupbyid.begin(); itr != groupbyid.end(); itr++) {
		mapType::iterator jitr = allT[listnum].find(itr->first);
		if (jitr != allT[listnum].end()) {
			freq = itr->second[0];
			jitr->second[0] = freq * 2;
			weight = itr->second[1];
			jitr->second[1] = weight * 2;
		}
	}
}

// verified
void
doublefreq(int listnum)
{
	double freq, weight;

	for (mapType::iterator itr = allT[listnum].begin(); itr != allT[listnum].end(); itr++) {
		freq = itr->second[0];
		itr->second[0] = freq * 2;
		weight = itr->second[1];
		itr->second[1] = weight * 2;
	}
	warnx << "doublefreq: setsize: " << allT[listnum].size() << "\n";
}

// verified
void
splitfreq(int listnum)
{
	double freq, weight;

	for (mapType::iterator itr = allT[listnum].begin(); itr != allT[listnum].end(); itr++) {

		freq = itr->second[0];
		itr->second[0] = freq / 2;
		weight = itr->second[1];
		itr->second[1] = weight / 2;
	}
	warnx << "splitfreq: setsize: " << allT[listnum].size() << "\n";
}

// verified
void
delspecial(int listnum)
{
	std::vector<POLY> isig;
	isig.clear();

	warnx << "delspecial: setsize before: " << allT[listnum].size() << "\n";
	for (mapType::iterator itr = allT[listnum].begin(); itr != allT[listnum].end(); itr++) {
		if (itr->first[0] == 0) {
			isig = inverse(itr->first);
			mapType::iterator regitr = allT[listnum].find(isig);
			if (regitr != allT[listnum].end()) {
				// itr is special multiset
				allT[listnum].erase(itr);
			}
		}
	}
	warnx << "delspecial: setsize after: " << allT[listnum].size() << "\n";
}

// verified
void
printdouble(std::string fmt, double num)
{
	std::ostringstream oss;
	std::string ss;

	oss << num;
	ss = oss.str();
	warnx << fmt.c_str() << ss.c_str();
}

// verified
void
printlist(int listnum, int seq)
{
	int n = 0;
	double freq, weight, avg;
	double sumavg = 0;
	double sumsum = 0;
	std::vector<POLY> sig;
	//sig.clear();
	//std::vector<POLY> isig;
	str sigbuf;

	warnx << "list T_" << listnum << ": " << seq << " " << allT[listnum].size() << "\n";
	warnx << "hdrB: freq weight avg avg*p\n";
	for (mapType::iterator itr = allT[listnum].begin(); itr != allT[listnum].end(); itr++) {
		sig = itr->first;
		freq = itr->second[0];
		weight = itr->second[1];
		avg = freq / weight;
		sumavg += avg;
		sumsum += (avg * peers);

		/*
		isig = inverse(sig);
		sig2str(isig, sigbuf);
		warnx << "isig" << n << ": " << sigbuf << "\n";
		isig = inverse(isig);
		sig2str(isig, sigbuf);
		warnx << "isig^2" << n << ": " << sigbuf << "\n";
		*/
		sig2str(sig, sigbuf);
		warnx << "sig" << n << ": " << sigbuf;
		printdouble(" ", freq);
		printdouble(" ", weight);
		printdouble(" ", avg);
		printdouble(" ", avg * peers);
		warnx << "\n";
		++n;
	}
	warnx << "hdrE: freq weight avg avg*p\n";
	printdouble("printlist: sumavg: ", sumavg);
	printdouble(" multisetsize: ", sumsum);
	warnx << " setsize: " << allT[listnum].size() << "\n";
}

void
usage(void)
{
	warn << "Usage: " << __progname << " [-h] [options...]\n\n";
	warn << "Options:\n"
	     << "	-B		bands for LSH (a.k.a. m groups)\n"
	     << "	-R		rows for LSH (a.k.a. l hash functions)\n"
	     << "	-c		discard out-of-round messages\n"
	     << "	-d		<random prime number for LSH seed>\n"
	     << "	-G		<gossip socket>\n"
	     << "	-H		generate chordIDs/POLYs using LSH when gossiping\n"
					"\t\t\t(requires -g, -s, -d, -j, -I -w)\n"
	     << "      	-I		<how often>\n"
					"\t\t\t(init interval)\n"
	     << "      	-i		<how often>\n"
					"\t\t\t(gossip interval)\n"
	     << "      	-j		<irrpoly file>\n"
	     << "	-L		<log file>\n"
	     << "      	-m		use findMod instead of compute_hash\n"
					"\t\t\t(vector of POLYs instead of chordIDs)\n"
	     << "      	-n		<how many>\n"
	     << "      	-p		verbose (print list of signatures)\n"
	     << "      	-q		<estimate of # of peers in DHT>\n"
	     << "	-S		<dhash socket>\n"
	     << "	-s		<dir with sigs>\n"
	     << "	-u		make POLYs unique (convert multiset to set)\n"
	     << "      	-w		<how long>\n"
					"\t\t\t(wait interval b/w init and exec phase)\n\n"
	     << "Actions:\n"
	     << "	-g		gossip (requires -S, -G, -s, -i)\n"
	     << "	-H		generate chordIDs/POLYs using LSH (requires  -s, -d, -j)\n"
	     << "	-l		listen for gossip (requires -S, -G, -s)\n"
	     << "	-r		read signatures (requires -s)\n"
	     << "	-v		print version\n"
	     << "	-z		generate random chordID (requires -n)\n";
	exit(0);
}
