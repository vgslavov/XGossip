/*	$Id: gpsi.C,v 1.69 2011/03/24 20:52:00 vsfgd Exp vsfgd $	*/

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <iostream>
#include <map>
#include <fstream>
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

#include "../utils/id_utils.h"
#include "../utils/utils.h"
#include "nodeparams.h"
#include "gpsi.h"

//#define _DEBUG_
#define _ELIMINATE_DUP_

static char rcsid[] = "$Id: gpsi.C,v 1.69 2011/03/24 20:52:00 vsfgd Exp vsfgd $";
extern char *__progname;

dhashclient *dhash;
int out;
int groupsleep;
int waitsleep;
int initsleep;
int gossipsleep;
int openfd = 0;
int closefd = 0;

chordID maxID;
static const char *dsock;
static const char *gsock;
static char *hashfile;
static char *irrpolyfile;
static char *initfile;
static char *logfile;
static char *xpathfile;
std::vector<POLY> irrnums;
std::vector<int> hasha;
std::vector<int> hashb;
int lshseed = 0;
int plist = 0;
int gflag = 0;
int uflag = 0;
int discardmsg = 0;
int peers = 0;
int teamsize = 5;
bool initphase = 0;

// paper:k, slides:bands
int mgroups = 10;
// paper:l, slides:rows
int lfuncs = 16;

// map sigs to freq and weight
typedef std::map<std::vector<POLY>, std::vector<double>, CompareSig> mapType;
typedef std::vector<mapType> vecomap;
// local list T_m is stored in allT[0];
// use only for storing the sigs initially
vecomap allT;

// vector of vector of maps for each team
typedef std::vector<vecomap> totalvecomap;
totalvecomap totalT;

// map teamID to team's list index in totalT
typedef std::map<chordID, int> teamid2totalT;
teamid2totalT teamindex;

typedef std::map<chordID, int> teamidfreq;
teamidfreq teamidminhash;

// map chordIDs/POLYs to sig indices
typedef std::map<chordID, std::vector<int> > chordID2sig;
typedef std::map<POLY, std::vector<int> > poly2sig;

void acceptconn(int);
// XGossip(+)
void add2vecomapx(std::vector<std::vector <POLY> >, std::vector<double>, std::vector<double>, chordID);
// Vanilla Gossip
void add2vecomapv(std::vector<std::vector <POLY> >, std::vector<double>, std::vector<double>);
int getdir(std::string, std::vector<std::string>&);
void calcfreq(std::vector<std::vector <POLY> >);
void calcfreqM(std::vector<std::vector <POLY> >, std::vector<double>, std::vector<double>);
void calcteamids(std::vector <chordID>);
void doublefreq(vecomap&, int);
void doublefreqgroup(int, mapType);
void informteam(chordID, chordID, std::vector<POLY>);
void initgossiprecv(chordID, chordID, std::vector<POLY>, double, double);
DHTStatus insertDHT(chordID, char *, int, int = MAXRETRIES, chordID = 0);
std::vector<POLY> inverse(std::vector<POLY>);
void listengossip(void);
//void *listengossip(void *);
void loginitstate(FILE *);
void loadinitstate(FILE *);
int make_team(chordID, chordID, std::vector<chordID>&);
void mergelists(vecomap&);
char *netstring_decode(FILE *);
void netstring_encode(const char *, FILE *);
void printdouble(std::string, double);
void printlist(vecomap, int, int);
void printteamids();
chordID randomID(void);
void readgossip(int);
void readsig(std::string, std::vector<std::vector <POLY> >&);
void readValues(FILE *, std::multimap<POLY, std::pair<std::string, enum OPTYPE> >&);
//void retrieveDHT(chordID ID, int, str&, chordID guess = 0);
void delspecial(int);
bool sig2str(std::vector<POLY>, str&);
bool string2sig(std::string, std::vector<POLY>&);
bool sigcmp(std::vector<POLY>, std::vector<POLY>);
double sig_inter(std::vector<POLY>, std::vector<POLY>, bool&);
double sig_uni(std::vector<POLY>, std::vector<POLY>, bool&);
double signcmp(std::vector<POLY>, std::vector<POLY>, bool&);
void splitfreq(vecomap&, int);
void tokenize(const std::string&, std::vector<std::string>&, const std::string&);
void usage(void);

DHTStatus insertStatus;
std::string INDEXNAME;

// entries of a node
vec<str> nodeEntries;

bool insertError;
bool retrieveError;

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

void
groupsleepnow()
{
	warnx << "group sleep interval is up\n";
	++groupsleep;
}

void
waitsleepnow()
{
	warnx << "wait sleep interval is up\n";
	++waitsleep;
}

void
initsleepnow()
{
	warnx << "init sleep interval is up\n";
	++initsleep;
}

void
gossipsleepnow()
{
	warnx << "gossip sleep interval is up\n";
	++gossipsleep;
}

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
	std::vector<POLY> lshsig;
	str sigbuf;
	chordID ID;
	//DHTStatus status;
	//char *value;
	//int col, valLen;
	double freq, weight;

	sig.clear();
	lshsig.clear();
	for (mapType::iterator itr = allT[listnum].begin(); itr != allT[listnum].end(); itr++) {
		sig = itr->first;
		freq = itr->second[0];
		weight = itr->second[1];

		lsh *myLSH = new lsh(sig.size(), lfuncs, mgroups, lshseed, 0, irrnums, hasha, hashb);
		// convert multiset to set
		if (uflag == 1) {
			sig2str(sig, sigbuf);
			warnx << "multiset: " << sigbuf << "\n";
			lshsig = myLSH->getUniqueSet(sig);
			sig2str(lshsig, sigbuf);
			warnx << "set: " << sigbuf << "\n";
		}

		// XXX
		std::vector<POLY> polyhash;
		std::vector<chordID> idhash;
		warnx << "typeid: " << typeid(minhash).name() << "\n";
		// TODO: getHashCode on sig or lshsig
		if (typeid(minhash) == typeid(polyhash)) {
			polyhash = myLSH->getHashCodeFindMod(lshsig, myLSH->getIRRPoly());
		} else if (typeid(minhash) == typeid(idhash)) {
			idhash = myLSH->getHashCode(lshsig);
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
lshchordID(vecomap teamvecomap, std::vector<std::vector<chordID> > &matrix, int intval = -1, InsertType msgtype = INVALID, int listnum = 0, int col = 0)
{
	std::vector<chordID> minhash;
	std::vector<chordID> buckets;
	std::vector<POLY> sig;
	std::vector<POLY> sig1;
	std::vector<POLY> sig2;
	std::vector<POLY> lshsig;
	chordID ID;
	DHTStatus status;
	char *value;
	int valLen, n, range, randcol;
	double freq, weight;
	str sigbuf;

	minhash.clear();
	sig.clear();
	lshsig.clear();
	n = 0;
	for (mapType::iterator itr = teamvecomap[listnum].begin(); itr != teamvecomap[listnum].end(); itr++) {
		sig = itr->first;
		freq = itr->second[0];
		weight = itr->second[1];

		// will hold last two signatures;
		if (n % 2 == 0)
			sig1 = sig;
		else
			sig2 = sig;

		lsh *myLSH = new lsh(sig.size(), lfuncs, mgroups, lshseed, col, irrnums, hasha, hashb);
		// convert multiset to set
		if (uflag == 1) {
			//sig2str(sig, sigbuf);
			//warnx << "multiset: " << sigbuf << "\n";
			lshsig = myLSH->getUniqueSet(sig);
			//sig2str(lshsig, sigbuf);
			//warnx << "set: " << sigbuf << "\n";
			minhash = myLSH->getHashCode(lshsig);
		} else {
			minhash = myLSH->getHashCode(sig);
		}

		//warnx << "minhash.size(): " << minhash.size() << "\n";
		if (plist == 1) {
			warnx << "sig" << n << ": ";
			warnx << "minhash IDs: " << "\n";
			sort(minhash.begin(), minhash.end());
			for (int i = 0; i < (int)minhash.size(); i++) {
				warnx << minhash[i] << "\n";
			}
		}

		calcteamids(minhash);

		/*
		matrix.push_back(minhash);
		range = (int)minhash.size();
		// randomness verified
		randcol = randomNumGenZ(range-1);
		//randcol = range / 2;
		ID = (matrix.back())[randcol];
		warnx << "ID in randcol " << randcol << ": " << ID << "\n";
		buckets.push_back(ID);
		*/
	
		if (gflag == 1 && msgtype != INVALID) {
		std::vector<chordID> team;
		chordID teamID;
		team.clear();
		for (int i = 0; i < (int)minhash.size(); i++) {
			teamID = minhash[i];
			warnx << "teamID: " << teamID << "\n";
			// TODO: check return status
			make_team(NULL, teamID, team);
			range = team.size();
			// randomness verified
			randcol = randomNumGenZ(range-1);
			ID = team[randcol];
			//ID = (matrix.back())[randcol];
			//warnx << "ID in randcol " << randcol << ": " << ID << "\n";
			if (gflag == 1 && msgtype != INVALID) {
				strbuf t;
				strbuf p;
				t << ID;
				p << team[0];
				str key(t);
				str teamID(p);
				warnx << "inserting " << msgtype << ":\n";
				makeKeyValue(&value, valLen, key, teamID, sig, freq, weight, msgtype);
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
				//sleep(intval);
				warnx << "sleeping (lsh)...\n";

				initsleep = 0;
				delaycb(intval, 0, wrap(initsleepnow));
				while (initsleep == 0) acheck();
			}
			// don't forget to clear team list!
			team.clear();
		}
		}
		// needed?
		minhash.clear();
		delete myLSH;
		++n;
	}

	/*
	sig2str(sig1, sigbuf);
	warnx << "sig1: " << sigbuf << "\n";
	sig2str(sig2, sigbuf);
	warnx << "sig2: " << sigbuf << "\n";
	std::vector<POLY> gcdPoly;
	gcdPoly.clear();
	gcdSpecial(gcdPoly, sig1, sig2);
	int ismetric = 0;
	int deg = pSim(sig1, sig2, gcdPoly, ismetric);
	//sig2 = sig1;
	//gcdPoly.clear();
	//gcdSpecial(gcdPoly, sig1, sig2);
	int degOpt = pSimOpt(sig1, sig2, gcdPoly, ismetric);
	warnx << "pSim: " << deg << "\n";
	warnx << "pSimOpt: " << degOpt << "\n";
	*/

	/*
	sort(buckets.begin(), buckets.end());
	warnx << "sorted buckets:\n";
	for (int i = 0; i < (int)buckets.size(); i++) {
		warnx << "ID: " << buckets[i] << "\n";
	}
	*/

	if (plist == 1) printteamids();

	return 0;
}

// TODO: allT -> totalT
int
lshpoly(vecomap teamvecomap, std::vector<std::vector<POLY> > &matrix, int intval = -1, InsertType msgtype = INVALID, int listnum = 0, int col = 0)
{
	std::vector<POLY> minhash;
	std::vector<POLY> sig;
	std::vector<POLY> lshsig;
	chordID ID;
	POLY mypoly;
	DHTStatus status;
	char *value;
	int valLen;
	double freq, weight;
	//str sigbuf;

	minhash.clear();
	sig.clear();
	lshsig.clear();
	for (mapType::iterator itr = teamvecomap[listnum].begin(); itr != teamvecomap[listnum].end(); itr++) {
		sig = itr->first;
		freq = itr->second[0];
		weight = itr->second[1];

		lsh *myLSH = new lsh(sig.size(), lfuncs, mgroups, lshseed, col, irrnums, hasha, hashb);
		// convert multiset to set
		if (uflag == 1) {
			//sig2str(sig, sigbuf);
			//warnx << "multiset: " << sigbuf << "\n";
			lshsig = myLSH->getUniqueSet(sig);
			//sig2str(lshsig, sigbuf);
			//warnx << "set: " << sigbuf << "\n";
			minhash = myLSH->getHashCodeFindMod(lshsig, myLSH->getIRRPoly());
		} else {
			minhash = myLSH->getHashCodeFindMod(sig, myLSH->getIRRPoly());
		}

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
		int randcol = randomNumGenZ(range-1);
		mypoly = (matrix.back())[randcol];
		warnx << "POLY in randcol " << randcol << ": " << mypoly << "\n";

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
			// TODO: generate teamID
			makeKeyValue(&value, valLen, key, key, sig, freq, weight, msgtype);
			status = insertDHT(ID, value, valLen, MAXRETRIES);
			cleanup(value);

			// do not exit if insert FAILs!
			if (status != SUCC) {
				warnx << "error: insert FAILed\n";
			} else {
				warnx << "insert SUCCeeded\n";
			}

			warnx << "sleeping (lsh)...\n";
			initsleep = 0;
			delaycb(intval, 0, wrap(initsleepnow));
			while (initsleep == 0) acheck();
		}
		delete myLSH;
	}
	return 0;
}
//
// TODO: verify
int
querychordID(std::vector<std::vector<chordID> > &matrix, int intval = -1, InsertType msgtype = INVALID, int listnum = 0, int col = 0)
{
	std::vector<chordID> minhash;
	std::vector<POLY> sig;
	std::vector<POLY> lshsig;
	chordID ID;
	DHTStatus status;
	char *value;
	int valLen;
	double freq, weight;

	minhash.clear();
	sig.clear();
	lshsig.clear();
	for (mapType::iterator itr = allT[listnum].begin(); itr != allT[listnum].end(); itr++) {
		sig = itr->first;
		freq = itr->second[0];
		weight = itr->second[1];

		lsh *myLSH = new lsh(sig.size(), lfuncs, mgroups, lshseed, col, irrnums, hasha, hashb);
		// convert multiset to set
		if (uflag == 1) {
			lshsig = myLSH->getUniqueSet(sig);
			minhash = myLSH->getHashCode(lshsig);
		} else {
			minhash = myLSH->getHashCode(sig);
		}

		matrix.push_back(minhash);

		if (msgtype != INVALID) {
			int range = (int)minhash.size();
			// randomness verified
			int randcol = randomNumGenZ(range-1);
			ID = (matrix.back())[randcol];
			//warnx << "ID in randcol " << randcol << ": " << ID << "\n";
			strbuf t;
			t << ID;
			str key(t);
			warnx << "inserting " << msgtype << ":\n";
			// TODO: generate teamID
			makeKeyValue(&value, valLen, key, key, sig, freq, weight, msgtype);
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
			//sleep(intval);
			warnx << "sleeping (lsh)...\n";

			initsleep = 0;
			delaycb(intval, 0, wrap(initsleepnow));
			while (initsleep == 0) acheck();
		}
		delete myLSH;
	}
	return 0;
}

int
main(int argc, char *argv[])
{
	int Gflag, Lflag, lflag, rflag, Sflag, sflag, zflag, vflag, Hflag, dflag, jflag, mflag, Iflag, Eflag, Pflag, Dflag, Mflag, Fflag, xflag, Qflag;
	int ch, gintval, initintval, waitintval, nids, valLen, logfd, tix;
	double beginTime, endTime;
	char *value;
	struct stat statbuf;
	time_t rawtime;
	chordID ID;
	chordID teamID;
	std::vector<chordID> team;
	DHTStatus status;
	str sigbuf;
	std::string initdir;
	std::string sigdir;
	std::vector<std::string> initfiles;
	std::vector<std::string> sigfiles;
	std::vector<std::vector<POLY> > sigList;
	std::vector<POLY> sig;

	Gflag = Lflag = lflag = rflag = Sflag = sflag = zflag = vflag = Hflag = dflag = jflag = mflag = Eflag = Iflag = Pflag = Dflag = Mflag = Fflag = xflag = Qflag = 0;

	gintval = waitintval = nids = 0;
	initintval = -1;
	rxseq.clear();
	txseq.clear();
	// init or txseq.back() segfaults!
	txseq.push_back(0);
	// init dummysig!
	dummysig.clear();
	dummysig.push_back(1);

	while ((ch = getopt(argc, argv, "B:cD:d:EF:G:gHhIj:L:lMmn:P:pQq:R:rS:s:T:t:uvw:x:Z:z")) != -1)
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
		case 'D':
			Dflag = 1;
			initdir = optarg;
			break;
		case 'd':
			dflag = 1;
			lshseed = strtol(optarg, NULL, 10);
			break;
		case 'E':
			Eflag = 1;
			break;
		case 'F':
			Fflag = 1;
			hashfile = optarg;
			break;
		case 'I':
			Iflag = 1;
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
		case 'T':
			initintval = strtol(optarg, NULL, 10);
			break;
		case 't':
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
		case 'M':
			Mflag = 1;
			break;
		case 'm':
			mflag = 1;
			break;
		case 'n':
			nids = strtol(optarg, NULL, 10);
			break;
		case 'P':
			Pflag = 1;
			initfile = optarg;
			break;
		case 'p':
			plist = 1;
			break;
		case 'Q':
			Qflag = 1;
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
		case 'x':
			xflag = 1;
			xpathfile = optarg;
			break;
		case 'Z':
			teamsize = strtol(optarg, NULL, 10);
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
		std::vector<chordID> IDs;
		warnx << "unsorted IDs:\n";
		for (int i = 0; i < nids; i++) {
			strbuf t;
			ID = make_randomID();
			warnx << "ID: " << ID << "\n";
			IDs.push_back(ID);
		}
		sort(IDs.begin(), IDs.end());
		warnx << "sorted IDs:\n";
		for (int i = 0; i < (int)IDs.size(); i++) {
			warnx << "ID: " << IDs[i] << "\n";
		}
		warnx << "\n";
		for (int i = 0; i < (int)IDs.size(); i++) {
			warnx << "team_" << i << ":\n";
			make_team(NULL, IDs[i], team);
		}
		return 0;
	}

	// TODO: make sure only one flag is set at a time
	// at least one action: read, gossip, LSH, query or listen
	if (rflag == 0 && gflag == 0 && lflag == 0 && Hflag == 0 && Mflag == 0 && Qflag == 0) usage();

	// sockets are required when listening or gossiping
	if ((gflag == 1 || lflag == 1) && (Gflag == 0 || Sflag == 0)) usage();

	// dhash socket is required when querying
	if (Qflag == 1 && Sflag == 0) usage();

	// sig or xpath are required when querying
	if (Qflag == 1 && (sflag == 0 && xflag == 0)) usage();

	// TODO: handle sflag & Pflag
	if (gflag == 1 && gintval == 0) usage();

	if (Mflag == 1 && Dflag == 0) usage();

	// action H (for testing)
	if ((Hflag == 1 || Qflag == 1) && (dflag == 0 || jflag == 0 || Fflag == 0)) usage();

	// option H (for gossiping)
	// TODO: handle sflag & Pflag & Fflag
	if ((Hflag == 1 && gflag == 1) && (waitintval == 0 || initintval == -1) &&
	    (Eflag == 1 || Iflag == 1))
		usage();

	// sigs are required when listening or reading
	if ((lflag == 1 || rflag == 1) && sflag == 0) usage();

	if (Lflag == 1) {
		// overwrite existing file
		logfd = open(logfile, O_CREAT|O_WRONLY|O_TRUNC, 0644);
		if (logfd < 0) fatal << "can't open log file " << logfile << "\n";
		lseek(logfd, 0, SEEK_END);
		errfd = logfd;
	}

	FILE *initfp = NULL;
	std::string acc;
	if (Pflag == 1) {
		if (Iflag == 1 || rflag == 1)
			acc = "w+";
		else
			acc = "r";

		if ((initfp = fopen(initfile, acc.c_str())) == NULL) {
			fatal << "can't open init file " << initfile << "\n";
		}
	}

	// copied from psi.C
	// read XPath query in memory
	if (Qflag == 1 && xflag == 1) {
		if (stat(xpathfile, &statbuf) != 0)
			fatal << "'" << xpathfile << "' does not exist" << "\n";

		// Read the query signatures from a file
		FILE *qfp = fopen(xpathfile, "r");
		assert(qfp);

		// Open the tags files too...
		std::string tagsfile = xpathfile + std::string(TAGFILE);
			
		FILE *fpTags = fopen(tagsfile.c_str(), "r");
		assert(fpTags);

		// Open the tag depth file...
		std::string tagdepth = xpathfile + std::string(TAGDEPTH);
		FILE *fpTagDepth = fopen(tagdepth.c_str(), "r");
		assert(fpTagDepth);

		// Open the value file...
		std::string valfile = xpathfile + std::string(VALFILE);
		FILE *fpVal = fopen(valfile.c_str(), "r");
		assert(fpVal);

		// DON'T use readData to retrieve signatures from input files...
		// since the size filed uses POLY as a basic unit and not byte...
		// Format is <n = # of sigs><sig size><sig>... n times...
		int numSigs;
		if (fread(&numSigs, sizeof(numSigs), 1, qfp) != 1) {
			warnx << "break\n";
			//break;
		}
		assert(numSigs > 0);

		std::vector<std::vector<POLY> > listOfSigs;
		listOfSigs.clear();
			
		for (int t = 0; t < numSigs; t++) {
			POLY *buf;
			int size;
			if (fread(&size, sizeof(int), 1, qfp) != 1) {
				assert(0);
			}
				
			buf = new POLY[size];
			assert(buf);
			if (fread(buf, sizeof(POLY), size, qfp) != (size_t) size) {
				assert(0);
			}
				
			std::vector<POLY> sig;
			sig.clear();
			for (int i = 0; i < size; i++) {
				sig.push_back(buf[i]);
			}

			listOfSigs.push_back(sig);
			sig2str(sig, sigbuf);
			warnx << sigbuf << "\n";

			// free the allocated memory
			delete[] buf;
		}
			
		//warnx << "******* Processing query " << count << " ********\n";
		//numReads = numWrites = dataReadSize = cacheHits = 0;

		// Read the distinct tags
		std::vector<std::string> distinctTags;
		distinctTags.clear();

		// Tag depth
		std::vector<std::string> tagDepth;
		tagDepth.clear();
			
		readTags(fpTags, distinctTags);
		warnx << "distinctTags:\n";
		for (int i = 0; i < (int) distinctTags.size(); i++) {
			warnx << distinctTags[i].c_str() << "\n";
		}
		readTags(fpTagDepth, tagDepth);
		warnx << "tagDepth:\n";
		for (int i = 0; i < (int) tagDepth.size(); i++) {
			warnx << tagDepth[i].c_str() << "\n";
		}

		// Pick the highest depth entry
		int maxDepth = -1;
		int maxDepthId = -1;
		for (int p = 0; p < (int) tagDepth.size(); p++) {
			if (atoi(tagDepth[p].c_str()) > maxDepth) {
				maxDepth = atoi(tagDepth[p].c_str());
				maxDepthId = p;
			}
		}
		warnx << "maxDepth: " << maxDepth << ", maxDepthId: " << maxDepthId << "\n";

		// Read the values
		std::multimap<POLY, std::pair<std::string, enum OPTYPE> > valList;
		valList.clear();
		readValues(fpVal, valList);

		/*
		std::vector<POLY> valSig;
		valSig.clear();
		*/

		// TODO: verify
	}

	FILE *hashfp = NULL;
	if (Fflag == 1) {
		// init phase OR
		// generate chordIDs using LSH (no gossip)
		// but don't if querying
		// TODO: verify
		if ((Iflag == 1 || gflag == 0) && (Qflag != 1)) {
			acc = "w+";
			if ((hashfp = fopen(hashfile, acc.c_str())) == NULL) {
				fatal << "can't write hash file " << hashfile << "\n";
			}

			int random_integer_a;
			int random_integer_b;
			int lowest_a = 1, highest_a = -9;
			int lowest_b = 0, highest_b = -9;
			highest_a = highest_b = lshseed;
			int range_a = (highest_a - lowest_a) + 1;
			int range_b = (highest_b - lowest_b) + 1;
			hasha.clear();
			hashb.clear();
			srand(lshseed);
			int totalfuncs = lfuncs * mgroups;
			for (int i = 0; i < totalfuncs; i++) {
				// TODO: verify randomness
				random_integer_a  = lowest_a + int((double)range_a*rand()/(RAND_MAX + 1.0));
				random_integer_b  = lowest_b + int((double)range_b*rand()/(RAND_MAX + 1.0));
				hasha.push_back(random_integer_a);
				hashb.push_back(random_integer_b);
			}

			warnx << "writing " << hashfile << "...\n";
			for (int i = 0; i < (int)hasha.size(); i++) {
				fprintf(hashfp, "%d\n", hasha[i]);
			}

			for (int i = 0; i < (int)hashb.size(); i++) {
				fprintf(hashfp, "%d\n", hashb[i]);
			}
			fclose(hashfp);
		// exec phase
		} else {
			//acc = "r";
			//if ((hashfp = fopen(hashfile, acc.c_str())) == NULL) {
			//	fatal << "can't read hash file " << hashfile << "\n";
			//}

			std::ifstream hashstream(hashfile);
			int totalfuncs = lfuncs * mgroups;
			if (hashstream.is_open()) {
				int hashnum;
				hasha.clear();
				hashb.clear();
				int z = 0;
				while (hashstream >> hashnum) {
					++z;
					if (z <= totalfuncs)
						hasha.push_back(hashnum);
					else
						hashb.push_back(hashnum);
				}
				assert(hasha.size() == hashb.size());
				assert((int)hasha.size() == totalfuncs);
				hashstream.close();
			} else {
				fatal << "can't read hash file " << hashfile << "\n";
			}
		}
	}

	if (jflag == 1) {
		if (stat(irrpolyfile, &statbuf) != 0)
			fatal << "'" << irrpolyfile << "' does not exist" << "\n";

		irrnums.clear();
		std::ifstream polystream;
                polystream.open(irrpolyfile);
                POLY num;
                while (polystream >> num) {
                        irrnums.push_back(num);
                }
                polystream.close();
	}

	if (gflag == 1 || lflag == 1 || Qflag == 1) {
		if (stat(dsock, &statbuf) != 0)
			fatal << "'" << dsock << "' does not exist" << "\n";
		dhash = New dhashclient(dsock);
	}

	if ((rflag == 1 || gflag == 1 || Hflag == 1 || lflag == 1 || Qflag == 1) && sflag == 1) {
		getdir(sigdir, sigfiles);
		sigList.clear();

		// insert dummy
		// the polynomial "1" has a degree 0
		// don't add dummy when querying and when gossiping using LSH
		if (Hflag == 0 && Qflag == 0) {
			sigList.push_back(dummysig);
		}

		warnx << "reading signatures from files...\n";
		for (unsigned int i = 0; i < sigfiles.size(); i++) {
			readsig(sigfiles[i], sigList);
		}
		warnx << "calculating frequencies...\n";
		beginTime = getgtod();    
		calcfreq(sigList);
		endTime = getgtod();    
		printdouble("calcfreq time (+sorting): ", endTime - beginTime);
		warnx << "\n";
		if (plist == 1) {
			// both init phases use allT
			printlist(allT, 0, -1);
		}

		// create log.init of sigs (why?)
		/*
		if (Pflag == 1) {
			warnx << "writing " << initfile << "...\n";
			loginitstate(initfp);
			fclose(initfp);
		}
		*/
	}

	time(&rawtime);
	warnx << "init ctime: " << ctime(&rawtime);
	warnx << "init sincepoch: " << time(&rawtime) << "\n";
	warnx << "rcsid: " << rcsid << "\n";
	warnx << "host: " << host << "\n";
	warnx << "pid: " << getpid() << "\n";
	warnx << "peers: " << peers << "\n";
	warnx << "loseed: " << loseed << "\n";
	warnx << "mgroups: " << mgroups << "\n";
	warnx << "lfuncs: " << lfuncs << "\n";
	std::vector<std::vector<POLY> > pmatrix;
	std::vector<std::vector<chordID> > cmatrix;

	if (gflag == 1 || lflag == 1) {
		warnx << "listening for gossip...\n";
		listengossip();		
		//sleep(5);
	}

	// XGossip+ init phase
	if (Hflag == 1 && Iflag == 1) {
		warnx << "initgossipsend...\n";
		warnx << "init interval: " << initintval << "\n";

		// enter init phase
		initphase = 1;
		beginTime = getgtod();    
	
		// InitGossipSend: use findMod()
		if (mflag == 1) {
			// T_m is 1st list
			lshpoly(allT, pmatrix, initintval, INITGOSSIP);
			pmatrix.clear();
		// InitGossipSend: use compute_hash()
		} else {
			lshchordID(allT, cmatrix, initintval, INITGOSSIP);
			cmatrix.clear();
		}

		endTime = getgtod();    
		printdouble("xgossip+ init phase time: ", endTime - beginTime);
		warnx << "\n";

		if (gflag == 1) {
			warnx << "wait interval: " << waitintval << "\n";
			warnx << "waiting for all peers to finish init phase...\n";

			//sleep(waitintval);
			waitsleep = 0;
			delaycb(waitintval, 0, wrap(waitsleepnow));
			while (waitsleep == 0) acheck();

			warnx << "writing " << initfile << "...\n";
			loginitstate(initfp);
			fclose(initfp);
			if (plist == 1) {
				for (int i = 0; i < (int)totalT.size(); i++) {
					printlist(totalT[i], 0, -1);
				}
			}
			warnx << "openfd: " << openfd << "\n";
			warnx << "closefd: " << closefd << "\n";
		}

		// exit init phase
		initphase = 0;
	}

	// XGossip exec phase
	if (gflag == 1 && Hflag == 0) {
		warnx << "xgossip exec...\n";
		warnx << "gossip interval: " << gintval << "\n";

		totalT.push_back(allT);

		srandom(loseed);
		while (1) {
			ID = make_randomID();
			strbuf z;
			z << ID;
			str key(z);
			beginTime = getgtod();    
			// store everything in totalT[0]
			mergelists(totalT[0]);
			//mergelists(allT);
			endTime = getgtod();    
			printdouble("merge lists time: ", endTime - beginTime);
			warnx << "\n";
			warnx << "inserting XGOSSIP:\n"
			      << "txseq: " << txseq.back()
			      << " txID: " << ID << "\n";
			if (plist == 1) {
				printlist(totalT[0], 0, txseq.back());
				//printlist(allT, 0, txseq.back());
			}
			// after merging, everything is stored in totalT[0][0]
			makeKeyValue(&value, valLen, key, key, totalT[0][0], txseq.back(), XGOSSIP);
			//makeKeyValue(&value, valLen, key, key, allT[0], txseq.back(), XGOSSIP);
			status = insertDHT(ID, value, valLen, MAXRETRIES);
			cleanup(value);

			// do not exit if insert FAILs!
			if (status != SUCC) {
				warnx << "error: insert FAILed\n";
				// to preserve mass conservation:
				// "send" msg to yourself
				doublefreq(totalT[0], 0);
				//doublefreq(allT, 0);
			} else {
				warnx << "insert SUCCeeded\n";
			}
			txseq.push_back(txseq.back() + 1);
			warnx << "sleeping (gossip)...\n";
			//sleep(gintval);
			gossipsleep = 0;
			delaycb(gintval, 0, wrap(gossipsleepnow));
			while (gossipsleep == 0) acheck();
		}
	// XGossip+ exec phase
	} else if (Hflag == 1 && Eflag == 1) {
		if (Pflag == 1) {
			warnx << "state: loading from init file...\n";
			loadinitstate(initfp);
			if (plist == 1) {
				teamid2totalT::iterator teamitr = teamindex.begin();
				for (int i = 0; i < (int)totalT.size(); i++) {
					// TODO: need assert?
					//assert(teamitr == teamindex.end());
					if (teamitr == teamindex.end()) {
						warnx << "teamindex < totalT at i=" << i
						      << " : " << teamindex.size()
						      << " < " << totalT.size() << "\n";
						break;
					}
					teamID = teamitr->first;
					tix = teamitr->second;
					warnx << "teamID(i=" << i << ",tix=" << tix << "): " << teamID << "\n";
					printlist(totalT[i], 0, -1);
					++teamitr;
				}
			}
		} else if (sflag == 1) {
			warnx << "state: signature files\n";
		} else if (Iflag != 1 || Eflag != 1) {
			fatal << "state: none available\n";
		} else {
			warnx << "state: init phase\n";
		}

		warnx << "xgossip+ exec...\n";
		warnx << "gossip interval: " << gintval << "\n";
		warnx << "exec ctime: " << ctime(&rawtime);
		warnx << "exec sincepoch: " << time(&rawtime) << "\n";

		// xgossip+ sigs grouped by lsh chordid/poly
		//vecomap groupedT;

		// needed?
		//srandom(loseed);
		while (1) {
			beginTime = getgtod();    
			warnx << "totalT.size(): " << totalT.size() << "\n";
			for (int i = 0; i < (int)totalT.size(); i++) {
				warnx << "merging totalT[" << i << "]\n";
				mergelists(totalT[i]);
			}
			endTime = getgtod();    
			printdouble("merge lists time: ", endTime - beginTime);
			warnx << "\n";
			//delspecial(0);
			if (plist == 1) {
				teamid2totalT::iterator teamitr = teamindex.begin();
				for (int i = 0; i < (int)totalT.size(); i++) {
					// TODO: need assert?
					//assert(teamitr == teamindex.end());
					if (teamitr == teamindex.end()) {
						warnx << "teamindex < totalT at i=" << i
						      << " : " << teamindex.size()
						      << " < " << totalT.size() << "\n";
						break;
					}
					teamID = teamitr->first;
					warnx << "teamID(" << i << "): " << teamID << "\n";
					printlist(totalT[i], 0, txseq.back());
					++teamitr;
				}
			}

			//srand(loseed);
			// TODO: the same as minhash.size()?
			//int range = mgroups;
			//int randcol = int((double)range * rand() / (RAND_MAX + 1.0));
			// TODO: verify randomness
			//int randcol = randomNumGenZ(range-1);
			// TODO: use findMod()
			if (mflag == 1) {
				fatal << "not implemented\n";
				/*
				// don't send anything
				//lshpoly(0, pmatrix, 0, -1, randcol);
				lshpoly(pmatrix);
				poly2sig polyindex;
				warnx << "pmatrix.size(): " << pmatrix.size() << "\n";
				warnx << "POLYs in random column " << randcol << ":\n";
				for (int i = 0; i < (int)pmatrix.size(); i++) {
					warnx << pmatrix[i][randcol] << "\n";;
					// store index of signature associated with POLY
					polyindex[pmatrix[i][randcol]].push_back(i);
				}
				warnx << "POLY: [sig indices].size()\n";
				for (poly2sig::iterator itr = polyindex.begin();
				     itr != polyindex.end(); itr++) {
					warnx << itr->first << ": " << itr->second.size() << "\n";
				}
				pmatrix.clear();
				*/
			// use compute_hash()
			} else {
				teamid2totalT::iterator teamitr = teamindex.begin();
				for (int i = 0; i < (int)totalT.size(); i++) {
					/*
					beginTime = getgtod();    
					warnx << "running lshchordID...\n";
					//lshchordID(cmatrix, -1, INVALID, 0, randcol);
					lshchordID(totalT[i], cmatrix);
					endTime = getgtod();    
					printdouble("lshchordID time: ", endTime - beginTime);
					warnx << "\n";
					chordID2sig idindex;
					warnx << "cmatrix.size(): " << cmatrix.size() << "\n";
					warnx << "cmatrix[0].size(): " << cmatrix[0].size() << "\n";
					warnx << "IDs in random column " << randcol << ":\n";
					*/

					// TODO: needed?
					if (gflag == 1) {
						warnx << "inserting XGOSSIPP:\n"
						      << "txseq: " << txseq.back() << "\n";

						// TODO: need assert?
						//assert(teamitr == teamindex.end());
						if (teamitr == teamindex.end()) {
							warnx << "teamindex < totalT at i=" << i
							      << " : " << teamindex.size()
							      << " < " << totalT.size() << "\n";
							break;
						}
						teamID = teamitr->first;
						warnx << "teamID(" << i << "): " << teamID << "\n";

						/*
						// TODO: inefficient?
						// get teamID for this totalT[i]
						for (teamid2totalT::iterator itr = teamindex.begin(); itr != teamindex.end(); itr++) {
							if (itr->second == i) {
								teamID = itr->first;
								warnx << "teamID: " << teamID << "\n";
							}
						}
						*/

						// TODO: check return status
						make_team(NULL, teamID, team);
						int range = team.size();
						// randomness verified
						int randcol = randomNumGenZ(range-1);
						ID = team[randcol];
						// TODO: check if p is succ(ID)
						//ID = (matrix.back())[randcol];
						warnx << "ID in randcol " << randcol << ": " << ID << "\n";
						strbuf t;
						strbuf p;
						t << ID;
						p << team[0];
						str key(t);
						str teamID(p);
						warnx << "txID: " << ID << "\n";
						makeKeyValue(&value, valLen, key, teamID, totalT[i][0], txseq.back(), XGOSSIPP);
						status = insertDHT(ID, value, valLen, MAXRETRIES);
						cleanup(value);
						// don't forget to clear team list!
						team.clear();

						// do not exit if insert FAILs!
						if (status != SUCC) {
							warnx << "error: insert FAILed\n";
							// to preserve mass conservation:
							// "send" msg to yourself
							doublefreq(totalT[i], 0);
						} else {
							warnx << "insert SUCCeeded\n";
						}

						warnx << "sleeping (groups)...\n";
						//sleep(initintval);
						groupsleep = 0;
						delaycb(initintval, 0, wrap(groupsleepnow));
						while (groupsleep == 0) acheck();
					} else {
						return 0;
					}
					++teamitr;
				}
				txseq.push_back(txseq.back() + 1);
				//cmatrix.clear();
				// CLEAR ME!
				//groupedT.clear();
				warnx << "sleeping (xgossip+)...\n";
				//sleep(gintval);
				gossipsleep = 0;
				delaycb(gintval, 0, wrap(gossipsleepnow));
				while (gossipsleep == 0) acheck();

				/* old algorithm
				//col = 0;
				for (int i = 0; i < (int)cmatrix.size(); i++) {
					//for (int j = 0; j < (int)cmatrix[i].size(); j++) {
					//	warnx << cmatrix[i][j] << " ";
					//}
					//warnx << "\n";
					//warnx << cmatrix[i][randcol] << "\n";
					// store index of signature associated with chordID
					idindex[cmatrix[i][randcol]].push_back(i);
					//idindex[cmatrix[i][0]].push_back(i);
					//chordID2sig::iterator itr = idindex.find(matrix[i][randcol]);
					//if (itr != idindex.end()) {
					//	itr->second[0] += 1;
					//} else {
						// store index of signature associated with chordID
						//idindex[matrix[i][randcol]].push_back(i);
					//}

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

				}
				warnx << "uniqueids: " << uniqueids << "\n";
				warnx << "groupedT.size(): " << groupedT.size() << "\n";

				if (gflag == 1) {
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
						//warnx << "groupedT[" << i << "]:\n";
						//double freq, weight;
						//for (mapType::iterator jitr = groupedT[i].begin(); jitr != groupedT[i].end(); jitr++) {
							//sig = jitr->first;
							//freq = jitr->second[0];
							//weight = jitr->second[1];
							//sig2str(sig, sigbuf);
							//warnx << "sig: " << sigbuf;
							//printdouble(" ", freq);
							//printdouble(" ", weight);
							//warnx << "\n";
						//}
						ID = itr->first;
						strbuf z;
						z << ID;
						str key(z);
						warnx << "txID: " << ID << "\n";

						makeKeyValue(&value, valLen, key, groupedT[i], txseq.back(), XGOSSIPP);

						status = insertDHT(ID, value, valLen, MAXRETRIES);
						cleanup(value);

						// do not exit if insert FAILs!
						if (status != SUCC) {
							warnx << "error: insert FAILed\n";
							// to preserve mass conservation:
							// "send" msg to yourself
							doublefreqgroup(0, groupedT[i]);
						} else {
							warnx << "insert SUCCeeded\n";
						}

						++itr;

						warnx << "sleeping (groups)...\n";
						//sleep(initintval);
						groupsleep = 0;
						delaycb(initintval, 0, wrap(groupsleepnow));
						while (groupsleep == 0) acheck();
					}
					txseq.push_back(txseq.back() + 1);
					cmatrix.clear();
					// CLEAR ME!
					groupedT.clear();
					warnx << "sleeping (xgossip+)...\n";
					//sleep(gintval);
					gossipsleep = 0;
					delaycb(gintval, 0, wrap(gossipsleepnow));
					while (gossipsleep == 0) acheck();
				} else {
					return 0;
				}
				*/
			}
		}
	} else if (Qflag == 1) {
		std::vector<std::vector<chordID> > cmatrix;
		cmatrix.clear();
		initintval = 1;
		warnx << "querying ";
		beginTime = getgtod();
		if (xflag == 1) {
			warnx << "using xpath...\n";
			querychordID(cmatrix, initintval, QUERYX);
			cmatrix.clear();
		} else {
			warnx << "using sig...\n";
			querychordID(cmatrix, initintval, QUERYS);
			cmatrix.clear();
		}
		endTime = getgtod();    
		printdouble("query time: ", endTime - beginTime - initintval);
		warnx << "\n";

		return 0;
	} else if (Mflag == 1) {
		warnx << "testing merging...\n";
		getdir(initdir, initfiles);
		acc = "r";
		warnx << "loading lists from files...\n";
		for (unsigned int i = 0; i < initfiles.size(); i++) {
			if ((initfp = fopen(initfiles[i].c_str(), acc.c_str())) == NULL) {
				warnx << "can't open init file" << initfiles[i].c_str() << "\n";
				continue;
			}
			loadinitstate(initfp);
			fclose(initfp);
		}
		beginTime = getgtod();    
		warnx << "totalT.size(): " << totalT.size() << "\n";
		for (int i = 0; i < (int)totalT.size(); i++) {
			mergelists(totalT[i]);
		}
		endTime = getgtod();    
		printdouble("merge lists time: ", endTime - beginTime);
		warnx << "\n";
		if (plist == 1) {
			for (int i = 0; i < (int)totalT.size(); i++) {
				printlist(totalT[i], 0, -1);
			}
		}
		return 0;
	// don't exit if listening
	} else if (lflag == 1) {
		warnx << "\n";
	} else if (rflag == 1 || Hflag == 1) {
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

// copied from psi.C
void
readValues(FILE *fp, std::multimap<POLY, std::pair<std::string, enum OPTYPE> >&valList)
{
    if (!fp) return;

    int num;
    if (fread(&num, sizeof(num), 1, fp) != 1) {
        assert(0);
    }

    for (int i = 0; i < num; i++) {
        POLY p;
        if (fread(&p, sizeof(p), 1, fp) != 1) {
            assert(0);
        }

        int size;
        if (fread(&size, sizeof(size), 1, fp) != 1) {
            assert(0);
        }

        char predVal[128];
        if (fread(predVal, size, 1, fp) != 1) {
            assert(0);
        }
        predVal[size] = 0;

        enum OPTYPE OP;
        if (fread(&OP, sizeof(OP), 1, fp) != 1) {
            assert(0);
        }

        std::pair<std::string, enum OPTYPE> e(std::string(predVal), OP);
        valList.insert(std::pair<POLY, std::pair<std::string, enum OPTYPE> >(p, e));
    }
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
void
listengossip(void)
{
	unlink(gsock);
	int fd = unixsocket(gsock);
	if (fd < 0) fatal << "listen: error creating socket: " << strerror(errno) << "\n";

	// make socket non-blocking
	make_async(fd);

	if (listen(fd, MAXCONN) < 0) {
		// TODO: what's %m?
		fatal("listen: error listening: %m\n");
		close(fd);
		++closefd;
	} else {
		fdcb(fd, selread, wrap(acceptconn, fd));
	}
}

// verified
void
acceptconn(int lfd)
{
	sockaddr_un sun;
	bzero(&sun, sizeof(sun));
	socklen_t sunlen = sizeof(sun);

	//int cs = accept(lfd, (struct sockaddr *) &sin, &sinlen);
	int cs = accept(lfd, reinterpret_cast<sockaddr *> (&sun), &sunlen);
	if (cs >= 0) {
		warnx << "accept: connection on local socket: cs=" << cs << "\n";
		++openfd;
	//} else if (errno != EAGAIN) {
	} else {
		warnx << "accept: error accepting: " << strerror(errno) << "\n";
		// disable readability callback?
		fdcb(lfd, selread, NULL);
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
	str keyteamid;
	str sigbuf;
	chordID ID;
	chordID teamID;
	std::vector<std::vector<POLY> > sigList;
	std::vector<double> freqList;
	std::vector<double> weightList;
	InsertType msgtype;
	std::vector<POLY> sig;
	double freq, weight;
	int n, msglen, recvlen, nothing;
	int ret, seq;

	msglen = recvlen = nothing = 0;
	warnx << "reading from socket:\n";

	do {
		n = buf.tosuio()->input(fd);

		if (n < 0) {
			warnx << "readgossip: read failed\n";
			fdcb(fd, selread, NULL);
			close(fd);
			++closefd;
			return;
		}

		// EOF
		if (n == 0) {
			warnx << "readgossip: recvlen=" << recvlen << "\n";
			warnx << "readgossip: nothing received\n";
			fdcb(fd, selread, NULL);
			return;
			/*
			++nothing;
			if (nothing == 5) {
				warnx << "readgossip: giving up\n";
				fdcb(fd, selread, NULL);
				close(fd);
				++closefd;
				return;
			}
			continue;
			*/
		}

		// run 1st time only
		if (msglen == 0) {
			str gbuf = buf;
			msglen = getKeyValueLen(gbuf);
			warnx << "rxmsglen: " << msglen;
		}

		recvlen += n;

#ifdef _DEBUG_
		warnx << "\nreadgossip: n=" << n << "\n";
		warnx << "readgossip: recvlen=" << recvlen << "\n";
#endif

		totalbuf << buf;
		buf.tosuio()->clear();

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

		ret = getKeyValue(gmsg.cstr(), key, keyteamid, sigList, freqList, weightList, seq, recvlen);
		if (ret == -1) {
			warnx << "error: getKeyValue failed\n";
			fdcb(fd, selread, NULL);
			close(fd);
			++closefd;
			return;
		}

		warnx << " rxlistlen: " << sigList.size()
		      << " rxseq: " << seq
		      << " txseq-cur: " << txseq.back()
		      << " rxID: " << key;

		if (msgtype == XGOSSIPP) warnx << " teamID: " << keyteamid;

		warnx  << "\n";

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
		str2chordID(keyteamid, teamID);
		if (msgtype == XGOSSIP) {
			add2vecomapv(sigList, freqList, weightList);
		} else {
			add2vecomapx(sigList, freqList, weightList, teamID);
		}
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

		sig.clear();
		ret = getKeyValue(gmsg.cstr(), key, keyteamid, sig, freq, weight, recvlen);
		if (ret == -1) {
			warnx << "error: getKeyValue failed\n";
			fdcb(fd, selread, NULL);
			close(fd);
			++closefd;
			return;
		}

		warnx << " rxID: " << key << "\n";
		warnx << " teamID: " << keyteamid << "\n";
		//sig2str(sig, sigbuf);
		//warnx << "sig: " << sigbuf;
		//printdouble(" ", freq);
		//printdouble(" ", weight);
		//warnx << "\n";
		str2chordID(key, ID);
		str2chordID(keyteamid, teamID);
		initgossiprecv(ID, teamID, sig, freq, weight);
	} else if (msgtype == QUERYS || msgtype == QUERYX) {
		if (msgtype == QUERYS) {
			warnx << "QUERYS";
		} else {
			warnx << "QUERYX";
		}

		sig.clear();
		ret = getKeyValue(gmsg.cstr(), key, keyteamid, sig, freq, weight, recvlen);
		if (ret == -1) {
			warnx << "error: getKeyValue failed\n";
			fdcb(fd, selread, NULL);
			close(fd);
			++closefd;
			return;
		}

		warnx << " rxID: " << key << "\n";
		warnx << " teamID: " << keyteamid << "\n";
		sig2str(sig, sigbuf);
		warnx << "sig: " << sigbuf << "\n";
		warnx << "query result: ";

		// TODO: search other lists (not yet merged) and for other sigs?
		mapType::iterator itr = allT[0].find(sig);
		if (itr != allT[0].end()) {
			freq = itr->second[0];
			weight = itr->second[1];
			warnx << " found\n";
			printdouble("f: ", freq);
			printdouble(", w: ", weight);
			printdouble(", avg: ", freq/weight);
			warnx << "\n";
		} else {
			warnx << " NOT found\n";
		}
	} else {
		warnx << "error: invalid msgtype\n";
	}

	// always disable readability callback before closing a f*cking fd!
	warnx << "readgossip: done reading\n";
	fdcb(fd, selread, NULL);
	close(fd);
	++closefd;
}

void
loadinitstate(FILE *initfp)
{
	mapType uniqueSigList;
	vecomap tmpvecomap;
	chordID teamID;
	std::vector<POLY> sig;
	std::vector<std::string> tokens;
	std::string linestr;
	int tind;
	int n = 0;
	char *line = NULL;
	size_t len = 0;
	ssize_t read;
	double freq, weight;

	uniqueSigList.clear();
	tmpvecomap.clear();
	while ((read = getline(&line, &len, initfp)) != -1) {
		linestr.clear();
		linestr.assign(line);
		tokens.clear();
		tokenize(linestr, tokens, ":");
		sig.clear();
		int toksize = tokens.size();
		if (strcmp(tokens[0].c_str(), "index") == 0) {
			// list index
			tind = strtol(tokens[1].c_str(), NULL, 10);
			// strip "\n" from chordID
			std::string t = tokens[toksize-1].substr(0,tokens[toksize-1].size()-1);
			str z(t.c_str());
			str2chordID(z, teamID);
			warnx << "teamID(" << tind << "): " << teamID << "\n";
			teamindex[teamID] = tind;
			// done with previous team, add its vecomap
			// (but don't do it for the very first index line
			if (n != 0) {
				tmpvecomap.push_back(uniqueSigList);
				totalT.push_back(tmpvecomap);
				uniqueSigList.clear();
				tmpvecomap.clear();
			}
			++n;
			continue;
		}
		string2sig(tokens[0], sig);
		freq = strtod(tokens[1].c_str(), NULL);
		weight = strtod(tokens[2].c_str(), NULL);
		uniqueSigList[sig].push_back(freq);
		uniqueSigList[sig].push_back(weight);
	}
	tmpvecomap.push_back(uniqueSigList);
	totalT.push_back(tmpvecomap);

	if (line) free(line);
	warnx << "loadinitstate: teams added: " << n << "\n";
}

void
loginitstate(FILE *initfp)
{
	std::vector<POLY> sig;
	sig.clear();
	str sigbuf;
	// merged list is first
	int listnum = 0;
	double freq, weight;

	teamid2totalT::iterator teamitr = teamindex.begin();
	for (int i = 0; i < (int)totalT.size(); i++) {
		// TODO: need assert?
		//assert(teamitr == teamindex.end());
		if (teamitr == teamindex.end()) {
			warnx << "teamindex < totalT at i=" << i
			      << " : " << teamindex.size()
			      << " < " << totalT.size() << "\n";
			break;
		}
		strbuf z;
		z << teamitr->first;
		str teamID(z);
		fprintf(initfp, "index:%d:listsize:%d:teamID:%s\n", i, totalT[i][listnum].size(), teamID.cstr());
		for (mapType::iterator itr = totalT[i][listnum].begin();
		     itr != totalT[i][listnum].end(); itr++) {
			sig = itr->first;
			freq = itr->second[0];
			weight = itr->second[1];
			sig2str(sig, sigbuf);
			fprintf(initfp, "%s:", sigbuf.cstr());
			fprintf(initfp, "%f:", freq);
			fprintf(initfp, "%f\n", weight);
		}
		++teamitr;
	}
}

char *
netstring_decode(FILE *f)
{
	int n, len;
	char *buf;

	/* FIXME: invalid length edge case */
	if (fscanf(f, "%d", &len) < 1) {
		return NULL;
	}

	if (fgetc(f) != ':') {
		warnx << "bad format\n";
		warnx << "bad format\n";
		return NULL;
	}

	if ((buf = (char *)malloc(len + 1)) == NULL) {
		warnx << "out of memory\n";
		exit(1);
	}

	if (!buf) {
		warnx << "can't allocate memory\n";
		return NULL;
	}

	/* fread: is this the best way? */
	if ((n = fread(buf, 1, len, f)) < len) {
		warnx << "error reading value\n";
		return NULL;
	}

	if (fgetc(f) != ',') {
		warnx << "bad format\n";
		return NULL;
	}

	return buf;
}

void
netstring_encode(const char *buf, FILE *f)
{
	int len;

	len = strlen(buf);
	if (fprintf(f, "%d:", len) < 0) warnx << "invalid length\n";
	if (fwrite(buf, 1, len, f) < (unsigned int)len) warnx << "can't write to stream\n";
	if (fputc(',', f) < 0) warnx << "can't write to stream\n";
}

// based on:
// http://www.linuxquestions.org/questions/programming-9/c-list-files-in-directory-379323/
// verified
int
getdir(std::string dir, std::vector<std::string> &files)
{
	DIR *dp;
	struct dirent *dirp;

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
	warnx << "Document signature (sorted): ";
	for (int i = 0; i < size; i++) {
		if (fread(&e, sizeof(POLY), 1, sigfp) != 1) {
			assert(0);
		}
		sig.push_back(e);
	}
	sort(sig.begin(), sig.end());
	str buf;
	sig2str(sig, buf);
	warnx << buf << "\n";
	sigList.push_back(sig);

	warnx << "readsig: Size of sig list: " << sigList.size() << "\n";
	finishTime = getgtod();
	fclose(sigfp);
}

// TODO: verify
void
initgossiprecv(chordID ID, chordID teamID, std::vector<POLY> sig, double freq, double weight)
{
	int tind;
	// init phase uses 1st vecomap only
	int listnum = 0;
	std::vector<POLY> hldsig;
	mapType uniqueSigList;
	vecomap tmpveco;
	str buf;

	dummysig.clear();
	dummysig.push_back(1);
	hldsig.clear();
	hldsig.push_back(0);

	// find if list for team exists
	teamid2totalT::iterator teamitr = teamindex.find(teamID);
	if (teamitr != teamindex.end()) {
		warnx << "teamID found\n";
		tind = teamitr->second;
	} else {
		warnx << "teamID NOT found: adding new vecomap\n";
		warnx << "totalT.size() [before]: " << totalT.size() << "\n";
		tind = totalT.size();
		// don't add 1, since it's 0-based
		teamindex[teamID] = tind;
		// ?
		totalT.push_back(tmpveco);
	}
	warnx << "tind: " << tind << "\n";
	warnx << "totalT.size(): " << totalT.size() << "\n";
	warnx << "teamindex.size(): " << teamindex.size() << "\n";
	warnx << "totalT[tind].size(): " << totalT[tind].size() << "\n";

	if (totalT[tind].size() == 0) {
		warnx << "totalT.size() == 0\n";
		uniqueSigList[hldsig].push_back(0);
		uniqueSigList[hldsig].push_back(1);
		totalT[tind].push_back(uniqueSigList);
	}

	mapType::iterator sigitr = totalT[tind][listnum].find(sig);
	mapType::iterator dummysigitr = totalT[tind][listnum].find(dummysig);

	// s is regular and T_h[s] exists
	if ((sig != dummysig) && (sigitr != totalT[tind][listnum].end())) {
		warnx << "initgossiprecv: update freq\n";
		sigitr->second[0] += freq;
	// s is regular and T_h[s] does not exist
	} else if ((sig != dummysig) && (sigitr == totalT[tind][listnum].end())) {
		warnx << "initgossiprecv: inserting sig\n";
		totalT[tind][0][sig].push_back(freq);
		totalT[tind][0][sig].push_back(weight);

		if (dummysigitr == totalT[tind][listnum].end()) {
			totalT[tind][listnum][dummysig].push_back(0);
			totalT[tind][listnum][dummysig].push_back(1);
			warnx << "initgossiprecv: inform team\n";
			informteam(ID, teamID, dummysig);
		}
	// s is a special multiset
	} else {
		warnx << "s is a special multiset\n";
		if (sigitr == totalT[tind][listnum].end()) {
			warnx << "initgossiprecv: inserting sig\n";
			totalT[tind][listnum][sig].push_back(freq);
			totalT[tind][listnum][sig].push_back(weight);
			warnx << "initgossiprecv: inform team\n";
			informteam(ID, teamID, sig);
		}
	}
}

// TODO: verify
void
informteam(chordID myID, chordID teamID, std::vector<POLY> sig)
{
	DHTStatus status;
	std::vector<chordID> team;
	chordID nextID;
	char *value;
	int valLen;
	int idindex;

	warnx << "teamID: " << teamID << "\n";
	idindex = make_team(myID, teamID, team);
	
	if (idindex == -1) {
		warnx << "myID not in team\n";
		return;
	// if last member of team, don't inform first (it already knows)
	} else if (idindex == ((int)team.size() - 1)) {
		warnx << "i am the last team member, not informing\n";
		// don't exit if last
		//return;
	} else {
		// is this addition always safe?
		nextID = team[idindex+1];
	}

	strbuf z;
	z << nextID;
	str key(z);
	strbuf y;
	y << teamID;
	str teamidstr(y);
	double freq = 0;
	double weight = 1;
	makeKeyValue(&value, valLen, key, teamidstr, sig, freq, weight, INFORMTEAM);
	warnx << "inserting INFORMTEAM:\n";
	warnx << "myID: " << myID << " nextID: " << nextID << " teamID: " << teamID << "\n";
	status = insertDHT(nextID, value, valLen, MAXRETRIES);
	cleanup(value);

	if (status != SUCC) {
		// TODO: do I care?
		warnx << "error: insert FAILed\n";
	} else {
		warnx << "insert SUCCeeded\n";
	}
}

void
calcteamids(std::vector<chordID> minhash)
{
	teamidfreq::iterator teamitr;

	for (int i = 0; i < (int)minhash.size(); i++) {
		teamitr = teamidminhash.find(minhash[i]);
		if (teamitr != teamidminhash.end()) {
			//warnx << "teamID found in teamidminhash\n";
			teamitr->second += 1;
		} else {
			//warnx << "teamID NOT found in teamidminhash\n";
			teamidminhash[minhash[i]] = 1;
		}
	}
}

// return -1 if myID is not in team
// verify
int
make_team(chordID myID, chordID teamID, std::vector<chordID> &team)
{
	int idindex = -1;
	chordID curID;

	if (myID == teamID) {
		warnx << "myID == teamID, index = 0\n";
		idindex = 0;
	}

	// size of ring
	bigint rngmax = (bigint (1) << 160)  - 1;
	bigint arclen = rngmax / teamsize;
	warnx << "arclen: " << arclen << "\n";
	chordID b = 1;
	b <<= NBIT;
	team.clear();
	team.push_back(teamID);
	curID = teamID;
	warnx << "ID_0: " << teamID << "\n";
	// start at 1 because of team id
	for (int j = 1; j < teamsize; j++) {
		curID += arclen;
		// wraparound
		if (curID >= b) curID -= b;
		warnx << "ID_" << j << ": " << curID << "\n";
		if (myID == curID) {
			warnx << "myID == curID, index = " << j << "\n";
			idindex = j;
		}
		team.push_back(curID);
	}
	//warnx << "team.size(): " << team.size() << "\n";

	return idindex;
}

// obsolete
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

// copied from http://oopweb.com/CPP/Documents/CPPHOWTO/Volume/C++Programming-HOWTO-7.html
void
tokenize(const std::string& str, std::vector<std::string>& tokens, const std::string &delimiters = " ")
{
	// Skip delimiters at beginning.
	std::string::size_type lastPos = str.find_first_not_of(delimiters, 0);
	// Find first "non-delimiter".
	std::string::size_type pos = str.find_first_of(delimiters, lastPos);

	while (std::string::npos != pos || std::string::npos != lastPos) {
		// Found a token, add it to the vector.
		tokens.push_back(str.substr(lastPos, pos - lastPos));
		// Skip delimiters.  Note the "not_of"
		lastPos = str.find_first_not_of(delimiters, pos);
		// Find next "non-delimiter"
		pos = str.find_first_of(delimiters, lastPos);
	}
}
 
bool
string2sig(std::string buf, std::vector<POLY> &sig)
{
	std::vector<std::string> tokens;

	if (buf.length() == 0) return false;

	tokenize(buf, tokens, "-");
	for (int i = 0; i < (int)tokens.size(); i++)
		sig.push_back(strtol(tokens[i].c_str(), NULL, 10));

	return true;
}

// verified
bool
sig2str(std::vector<POLY> sig, str &buf)
{
	strbuf s;

	if (sig.size() <= 0) return false;

	for (int i = 0; i < (int)sig.size(); i++)
		s << sig[i] << "-";

	buf = s;
	return true;
}

// verified
// use allT only in init phases
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
void
calcfreqM(std::vector<std::vector<POLY> > sigList, std::vector<double> freqList, std::vector<double> weightList)
{
	//int deg;

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
}

// xgossip(+)
void
add2vecomapx(std::vector<std::vector<POLY> > sigList, std::vector<double> freqList, std::vector<double> weightList, chordID teamID)
{
	int tind;
	mapType uniqueSigList;
	vecomap tmpvecomap;

	warnx << "add2vecomap\n";

	for (int i = 0; i < (int) sigList.size(); i++) {
		uniqueSigList[sigList[i]].push_back(freqList[i]);
		uniqueSigList[sigList[i]].push_back(weightList[i]);
	}

	// find if list for team exists
	teamid2totalT::iterator teamitr = teamindex.find(teamID);
	if (teamitr != teamindex.end()) {
		tind = teamitr->second;
		warnx << "teamID found at teamindex[" << tind << "]\n";
		totalT[tind].push_back(uniqueSigList);
	} else {
		warnx << "teamID NOT found: adding new vecomap\n";
		warnx << "totalT.size() [before]: " << totalT.size() << "\n";
		tmpvecomap.push_back(uniqueSigList);
		totalT.push_back(tmpvecomap);
		tind = totalT.size() - 1;
		teamindex[teamID] = tind;
	}

	warnx << "tind: " << tind << "\n";
	warnx << "totalT.size(): " << totalT.size() << "\n";
	warnx << "teamindex.size(): " << teamindex.size() << "\n";
	warnx << "totalT[tind].size(): " << totalT[tind].size() << "\n";
}

// vanilla gossip
void
add2vecomapv(std::vector<std::vector<POLY> > sigList, std::vector<double> freqList, std::vector<double> weightList)
{
        mapType uniqueSigList;

	warnx << "add2vecomap\n";

        for (int i = 0; i < (int) sigList.size(); i++) {
                uniqueSigList[sigList[i]].push_back(freqList[i]);
                uniqueSigList[sigList[i]].push_back(weightList[i]);
        }

	totalT[0].push_back(uniqueSigList);
	warnx << "totalT[0].size(): " << totalT[0].size() << "\n";
}


// copied from psi.C
// return TRUE if s1 < s2
bool
sigcmp(std::vector<POLY> s1, std::vector<POLY> s2)
{
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

// return number of elements in the intersection of s1 and s2
// s1 and s2 are assumed to be sorted
// logic copied from set_intersection() in <algorithm>
// works with multisets (skips duplicates)
// complexity:
// at most, performs 2*(count1+count2)-1 comparisons or applications of comp
// (where countX is the distance between firstX and lastX)
int
set_inter(std::vector<POLY> s1, std::vector<POLY> s2, bool &multi)
{
	std::vector<POLY>::iterator s1itr = s1.begin();
	std::vector<POLY>::iterator s2itr = s2.begin();
	POLY s1prev, s2prev;
	int n, s1skip, s2skip;

	//warnx << "\n";
	s1prev = s2prev = n = s1skip = s2skip = 0;
	while (s1itr != s1.end() && s2itr != s2.end()) {
		// don't count duplicates
		if (s1prev == *s1itr) {
			//warnx << "skipping duplicate\n";
			++s1skip;
			s1prev = *s1itr;
			++s1itr;
		} else if (s2prev == *s2itr) {
			//warnx << "skipping duplicate\n";
			++s2skip;
			s2prev = *s2itr;
			++s2itr;
		} else if (*s1itr < *s2itr) {
			s1prev = *s1itr;
			++s1itr;
		} else if (*s2itr < *s1itr) {
			s2prev = *s2itr;
			++s2itr;
		} else {
			//warnx << "both: " << *s1itr << "\n";
			s1prev = *s1itr;
			s2prev = *s2itr;
			++s1itr;
			++s2itr;
			// count only same elements
			++n;
		}
	}
	//warnx << "s1skip(inter): " << s1skip << "\n";
	//warnx << "s2skip(inter): " << s2skip << "\n";
	// TODO: if s1 is multi (s1 is prevsig, s2 is cursig)
	if (s2skip != 0)
		multi = 1;
	else
		multi = 0;

	return n;
}

// return number of elements in the union of s1 and s2
// s1 and s2 are assumed to be sorted
// logic copied from set_union() in <algorithm>:
// works with multisets (skips duplicates)
// complexity:
// at most, performs 2*(count1+count2)-1 comparisons or applications of comp
// (where countX is the distance between firstX and lastX)
int
set_uni(std::vector<POLY> s1, std::vector<POLY> s2, bool &multi)
{
	std::vector<POLY>::iterator s1itr = s1.begin();
	std::vector<POLY>::iterator s2itr = s2.begin();
	POLY s1prev, s2prev;
	int n, s1skip, s2skip;

	s1prev = s2prev = n = s1skip = s2skip = 0;
	while (true) {
		// don't count duplicates
		if (s1prev == *s1itr) {
			//warnx << "skipping duplicate\n";
			++s1skip;
			++s1itr;
		} else if (s2prev == *s2itr) {
			//warnx << "skipping duplicate\n";
			++s2skip;
			++s2itr;
		} else if (*s1itr < *s2itr) {
			//warnx << "s1: " << *s1itr << "\n";
			s1prev = *s1itr;
			++s1itr;
			++n;
		} else if (*s2itr < *s1itr) {
			//warnx << "s2: " << *s2itr << "\n";
			s2prev = *s2itr;
			++s2itr;
			++n;
		} else {
			//warnx << "both: " << *s1itr << "\n";
			s1prev = *s1itr;
			s2prev = *s2itr;
			++s1itr;
			++s2itr;
			++n;
		}

		if (s1itr == s1.end()) {
			//warnx << "end of s1";
			//warnx << "n: " << n << "\n";
			while (s2itr != s2.end()) {
				//warnx << "s2: " << *s2itr << "\n";
				if (s2prev == *s2itr) {
					//warnx << "skipping duplicate\n";
					++s2skip;
					++s2itr;
					continue;
				}
				s2prev = *s2itr;
				++s2itr;
				++n;
			}
			break;
		}
		if (s2itr == s2.end()) {
			//warnx << "end of s2";
			//warnx << "n: " << n << "\n";
			while (s1itr != s1.end()) {
				//warnx << "s1: " << *s1itr << "\n";
				if (s1prev == *s1itr) {
					//warnx << "skipping duplicate\n";
					++s1skip;
					++s1itr;
					continue;
				}
				s1prev = *s1itr;
				++s1itr;
				++n;
			}
			break;
		}
	}

	//warnx << "s1skip(uni): " << s1skip << "\n";
	//warnx << "s2skip(uni): " << s2skip << "\n";
	// TODO: if s1 is multi (s1 is prevsig, s2 is cursig)
	if (s2skip != 0)
		multi = 1;
	else 
		multi = 0;

	return n;
}

// s1 and s2 are assumed to be sorted
double
signcmp(std::vector<POLY> s1, std::vector<POLY> s2, bool &multi)
{
	int ninter, nunion;

	ninter = set_inter(s1, s2, multi);
	//warnx << "intersection has " << ninter << " elements\n";

	nunion = set_uni(s1, s2, multi);
	//warnx << "union  has " << nunion << " elements\n";

	return (double)ninter/nunion;
}

// s1 and s2 are assumed to be sorted
double
signcmp_lib(std::vector<POLY> s1, std::vector<POLY> s2)
{
	std::vector<POLY> v;
	std::vector<POLY>::iterator itr;
	int ninter, nunion;

	set_intersection(s1.begin(), s1.end(), s2.end(), s2.end(), back_inserter(v));
	ninter = v.size();
	warnx << "intersection has " << ninter << " elements\n";

	v.clear();
	set_union(s1.begin(), s1.end(), s2.end(), s2.end(), back_inserter(v));
	nunion = v.size();
	warnx << "union has " << nunion << " elements\n";

	return double(ninter/nunion);
}

double
signcmp_bad(std::vector<POLY> s1, std::vector<POLY> s2)
{
	int same = 0;
	int total = 0;	

	for (int i = 0; i < (int) s1.size(); i++) {
		for (int j = 0; j < (int) s2.size(); j++) {
			if (s1[i] == s2[j])
				++same;
		}
	}

	// incorrect!
	if (s1.size() > s2.size())
		total = s1.size();
	else
		total = s2.size();

	//warnx << "\nsame: " << same << "\n";
	//warnx << "total: " << total << "\n";
	return (double)same/total;
}

// TODO: verify
void
mergelists(vecomap &teamvecomap)
{
	double sumf, sumw;
	str sigbuf;
	std::vector<POLY> minsig;
	std::vector<POLY> tmpsig;
	minsig.clear();

	warnx << "merging:\n";

	int n = teamvecomap.size();
	warnx << "initial teamvecomap.size(): " << n << "\n";
	if (n == 1) {
		splitfreq(teamvecomap, 0);
		return;
	} else {
#ifdef _DEBUG_
		for (int i = 0; i < n; i++)
			printlist(teamvecomap, i, -1);
#endif
	}

	// init pointers to beginning of lists
	std::vector<mapType::iterator> citr;
	for (int i = 0; i < n; i++) {
		citr.push_back(teamvecomap[i].begin());
		// skip dummy
		tmpsig = citr[i]->first;
		sig2str(tmpsig, sigbuf);
		warnx << "1st dummy: " << sigbuf << "\n";
		++citr[i];
		// skip 2nd dummy only for xgossip(+)
		// TODO: distinguisth b/w xgossip and vanilla gossip
		/*
		tmpsig = citr[i]->first;
		sig2str(tmpsig, sigbuf);
		warnx << "2nd dummy: " << sigbuf << "\n";
		++citr[i];
		*/
	}

	while (1) {
		int j = n;
		// check if at end of lists
		for (int i = 0; i < n; i++) {
			if (citr[i] == teamvecomap[i].end()) {
				//warnx << "end of T_" << i << "\n";
				--j;
			}
		}
		// done with all lists
		if (j == 0) {
			sumw = 0;
			for (int i = 0; i < n; i++) {
				sumw += teamvecomap[i][dummysig][1];
			}
			printdouble("new local dummy sumw/2: ", sumw/2);
			warnx << "\n";
			teamvecomap[0][dummysig][1] = sumw / 2;
			warnx << "mergelists: breaking from loop\n";
			break;
		}

		// find min sig
		int z = 0;
		for (int i = 0; i < n; i++) {
			// skip lists which are done
			if (citr[i] == teamvecomap[i].end()) {
				continue;
			} else if (z == 0){
				minsig = citr[i]->first;
//#ifdef _DEBUG_
				sig2str(minsig, sigbuf);
				warnx << "initial minsig: "
				      << sigbuf << "\n";
//#endif
				z = 1;

			}
			if (sigcmp(citr[i]->first, minsig) == 1)
				minsig = citr[i]->first;
		}

//#ifdef _DEBUG_
		sig2str(minsig, sigbuf);
		warnx << "actual minsig: " << sigbuf << "\n";
//#endif

		sumf = sumw = 0;
		// add all f's and w's for a particular sig
		for (int i = 0; i < n; i++) {
			// check for end of map first and then compare to minsig!
			if ((citr[i] != teamvecomap[i].end()) && (citr[i]->first == minsig)) {
				warnx << "minsig found in T_" << i << "\n";
				sumf += citr[i]->second[0];
				sumw += citr[i]->second[1];
				// XXX: itr of T_0 will be incremented later
				if (i != 0) ++citr[i];
			} else {
#ifdef _DEBUG_
				warnx << "no minsig in T_" << i;
				if (citr[i] == teamvecomap[i].end()) {
					warnx << " (list ended)";
				}
				warnx << "\n";
#endif
				sumf += teamvecomap[i][dummysig][0];
				sumw += teamvecomap[i][dummysig][1];
			}
		}

#ifdef _DEBUG_
		printdouble("sumf: ", sumf);
		printdouble(", sumw: ", sumw);
		warnx << "\n";
#endif

		// update teamvecomap[0]
		// check for end of map first and then compare to minsig!
		if ((citr[0] != teamvecomap[0].end()) && (citr[0]->first == minsig)) {
			// update sums of existing sig
			warnx << "T_0: updating sums of minsig\n";
			teamvecomap[0][minsig][0] = sumf/2;
			teamvecomap[0][minsig][1] = sumw/2;
			// XXX: see above XXX
			++citr[0];
		} else {
			warnx << "T_0: no minsig in T_0";
			if (citr[0] == teamvecomap[0].end()) {
				warnx << " (list ended)";
			}
			warnx << "\n";
			// insert new sig
			warnx << "T_0: inserting minsig...\n";
			teamvecomap[0][minsig].push_back(sumf/2);
			teamvecomap[0][minsig].push_back(sumw/2);

			// not needed:
			// if the minsig was missing in T_i,
			// itr already points to the next sig
			//citr[0] = teamvecomap[0].find(minsig);
			//++citr[0];

		}
	}

	// delete all except first (doesn't free memory but it's O(1))
	while (teamvecomap.size() > 1) {
		//warnx << "teamvecomap.size(): " << teamvecomap.size() << "\n";
		teamvecomap.pop_back();
	}
	warnx << "teamvecomap.size() after pop: " << teamvecomap.size() << "\n";
	warnx << "totalT.size() after pop: " << totalT.size() << "\n";
}

// obsolete
void
mergelistspold()
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
		splitfreq(allT, 0);
		return;
	} else {
#ifdef _DEBUG_
		//for (int i = 0; i < n; i++)
			//printlist(i, -1);
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
			warnx << "done with all lists\n";
			break;
		}

		// find min sig
		int z = 0;
		for (int i = 0; i < n; i++) {
			// skip lists which are done
			if (citr[i] == allT[i].end()) {
				continue;
			// DO NOT skip special multisets
			/*
			} else if (citr[i]->first[0] == 0) {
				++citr[i];
				continue;
			*/
			} else if (z == 0) {
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

		if (minsig[0] == 0) {
			warnx << "minsig is a special multiset\n";
		}
#endif

		sumf = sumw = 0;
		// add all f's and w's for a particular sig
		for (int i = 0; i < n; i++) {
			if ((citr[i] != allT[i].end()) && (citr[i]->first == minsig)) {
				//warnx << "minsig found in T_" << i << "\n";
				sumf += citr[i]->second[0];
				sumw += citr[i]->second[1];
				// XXX: itr of T_0 will be incremented later
				if (i != 0) ++citr[i];
			} else if (minsig[0] != 0) {
#ifdef _DEBUG_
				warnx << "no minsig in T_" << i
				      << " (list ended)\n";
#endif
				isig.clear();
				isig = inverse(minsig);
				mapType::iterator itr = allT[i].find(isig);
				if (itr != allT[i].end()) {
#ifdef _DEBUG_
					warnx << "special minsig found in T_" << i << "\n";
#endif
					sumf += allT[i][isig][0];
					sumw += allT[i][isig][1];
				}
			}
			// DO NOT skip lists which are done:
			// use special multiset
		
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
	warnx << "allT.size() after pop: " << allT.size() << "\n";
}

// TODO: obsolete?
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

void
doublefreq(vecomap &teamvecomap, int listnum)
{
	double freq, weight;

	for (mapType::iterator itr = teamvecomap[listnum].begin(); itr != teamvecomap[listnum].end(); itr++) {
		freq = itr->second[0];
		itr->second[0] = freq * 2;
		weight = itr->second[1];
		itr->second[1] = weight * 2;
	}
	warnx << "doublefreq: setsize: " << teamvecomap[listnum].size() << "\n";
}

void
splitfreq(vecomap &teamvecomap, int listnum)
{
	double freq, weight;

	for (mapType::iterator itr = teamvecomap[listnum].begin(); itr != teamvecomap[listnum].end(); itr++) {

		freq = itr->second[0];
		itr->second[0] = freq / 2;
		weight = itr->second[1];
		itr->second[1] = weight / 2;
	}
	warnx << "splitfreq: setsize: " << teamvecomap[listnum].size() << "\n";
}

// obsolete
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

void
printteamids()
{
	warnx << "printteamds:\n";

	for (teamidfreq::iterator itr = teamidminhash.begin(); itr != teamidminhash.end(); itr++) {
		warnx << "teamID: " << itr->first;
		warnx << " freq: " << itr->second;
		warnx << "\n";
	}
	warnx << "teamidminhash.size(): " << teamidminhash.size() << "\n";
}

void
printlist(vecomap teamvecomap, int listnum, int seq)
{
	bool multi;
	int n, nmulti, ixsim, setsize;
	double freq, weight, avg;
	double sumavg, sumsum;
	double avgsim, highsim, cursim;
	std::vector<POLY> sig;
	std::vector<POLY> prevsig;
	sig.clear();
	prevsig.clear();
	str sigbuf;

	sumavg = sumsum = 0;
	avgsim = highsim = cursim = 0;
	n = nmulti = ixsim = 0;
	warnx << "list T_" << listnum << ": txseq: " << seq << " len: " << teamvecomap[listnum].size() << "\n";
	warnx << "hdrB: sig freq weight avg avg*n:peers avg*q:mgroups cmp2prev multi\n";
	for (mapType::iterator itr = teamvecomap[listnum].begin(); itr != teamvecomap[listnum].end(); itr++) {
		sig = itr->first;
		freq = itr->second[0];
		weight = itr->second[1];
		avg = freq / weight;
		sumavg += avg;
		sumsum += (avg * peers);
		sig2str(sig, sigbuf);
		warnx << "sig" << n << ": " << sigbuf;
		printdouble(" ", freq);
		printdouble(" ", weight);
		printdouble(" ", avg);
		printdouble(" ", avg * peers);
		printdouble(" ", avg * mgroups);
		// only check if 1st sig is a multiset
		if (n == 0) {
			prevsig = sig;
			cursim = signcmp(prevsig, sig, multi);
			cursim = 0;
		} else {
			cursim = signcmp(prevsig, sig, multi);
		}
		avgsim += cursim;
		if (multi == 1) ++nmulti;
		if (highsim < cursim) {
			highsim = cursim;
			ixsim = n;
		}
		printdouble(" ", cursim);
		warnx << " " << multi << "\n";
		++n;
		prevsig = sig;
	}
	warnx << "hdrE: sig freq weight avg avg*n:peers avg*q:mgroups cmp2prev multi\n";
	printdouble("printlist: sumavg: ", sumavg);
	printdouble(" multisetsize: ", sumsum);
	// subtract dummy (what about 2 dummies?)
	setsize = teamvecomap[listnum].size() - 1;
	warnx << " setsize: " << setsize;
	warnx << " highsim@sig" << ixsim;
	printdouble(": ", highsim);
	// 1st sig doesn't have sim
	if ((setsize - 1) == 0)
		avgsim = 0;
	else
		avgsim /= (setsize - 1);
	printdouble(" avgsim: ", avgsim);
	warnx << " multisets: " << nmulti << "\n";
}

void
usage(void)
{
	warn << "Usage: " << __progname << " [-h] [actions...] [options...]\n\n";
	warn << "Examples:\n\n";
	warn << "Send signature query:\n";
	warn << "\t" << __progname << " -Q -S dhash-sock -s sigdir -F hashfile -d 1122941 -j irrpoly-deg9.dat -B 10 -R 16 -u\n\n";
	warn << "Send xpath query:\n";
	warn << "\t" << __progname << " -Q -S dhash-sock -x xpathsig -F hashfile -d 1122941 -j irrpoly-deg9.dat -B 10 -R 16 -u\n\n";
	warn << "LSH on sig dir:\n";
	warn << "\t" << __progname << " -H -I -s sigdir -F hashfile -d 1122941 -j irrpoly-deg9.dat -B 10 -R 16 -u -p\n\n";
	warn << "Generate init file for sigdir:\n";
	warn << "\t" << __progname << " -r -s sigdir -P initfile\n\n";
	warn << "Vanilla Gossip:\n";
	warn << "\t" << __progname << " -S dhash-sock -G g-sock -L log.gpsi -s sigdir -g -t 120 -q 165 -p\n\n";
	warn << "XGossip+:\n";
	warn << "\t" << __progname << " -S dhash-sock -G g-sock -L log.gpsi -s sigdir -g -t 120 -T 1 -w 900\n"
	     << "\t     -H -d 1122941 -j irrpoly-deg9.dat -u -B 50 -R 10 -I -E -P initfile -p\n\n";
	warn << "Options:\n"
	     << "	-B		bands for LSH (a.k.a. m groups)\n"
	     << "	-c		discard out-of-round messages\n"
	     << "	-D		<dir with init files>\n"
	     << "	-d		<random prime number for LSH seed>\n"
	     << "	-E		exec phase of XGossip+ (requires -H)\n"
	     << "      	-F		<hash funcs file>\n"
	     << "	-G		<gossip socket>\n"
	     << "	-H		generate chordIDs/POLYs using LSH when gossiping\n"
					"\t\t\t(requires -g, -s, -d, -j, -I, -w, -F)\n"
	     << "	-I		init phase of XGossip+ (requires -H)\n"
	     << "      	-j		<irrpoly file>\n"
	     << "	-L		<log file>\n"
	     << "      	-m		use findMod instead of compute_hash\n"
					"\t\t\t(vector of POLYs instead of chordIDs)\n"
	     << "      	-n		<how many>\n"
	     << "      	-P		<init phase file>\n"
					"\t\t\t(after XGossip+ init state is complete)\n"
	     << "      	-p		verbose (print list of signatures)\n"
	     << "      	-q		<initial estimate of # of peers in DHT>\n"
	     << "	-R		rows for LSH (a.k.a. l hash functions)\n"
	     << "	-S		<dhash socket>\n"
	     << "	-s		<dir with sigs>\n"
	     << "      	-T		<how often>\n"
					"\t\t\t(interval between inserts in XGossip+)\n"
	     << "      	-t		<how often>\n"
					"\t\t\t(gossip interval)\n"
	     << "	-u		make POLYs unique (convert multiset to set)\n"
	     << "      	-w		<how long>\n"
					"\t\t\t(wait time after XGossip+ init phase is done)\n"
	     << "	-Z		<team size>\n\n"
	     << "Actions:\n"
	     << "	-g		gossip (requires -S, -G, -s, -t)\n"
	     << "	-H		generate chordIDs/POLYs using LSH (requires  -s, -d, -j, -F)\n"
					"\t\t\t(XGossip+)\n"
	     << "	-C		calculate distance\n"
	     << "	-l		listen for gossip (requires -S, -G, -s)\n"
	     << "	-M		test mergelists(p) (requires -D)\n"
	     << "	-Q		send query (requires -S, -d, -j, -s or -x)\n"
	     << "	-r		read signatures (requires -s)\n"
	     << "	-v		print version\n"
	     << "	-z		generate random chordID (requires -n)\n";
	exit(0);
}
