/*	$Id: gpsi.C,v 1.3 2009/08/10 06:30:57 vsfgd Exp vsfgd $	*/

#include <cmath>
#include <ctime>
#include <dirent.h>
#include <fstream>
#include <iostream>
#include <map>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <vector>

#include <chord.h>
#include <dhash_common.h>
#include <dhashclient.h>
#include <dhblock.h>

#include "../utils/utils.h"
#include "nodeparams.h"
#include "gpsi.h"

#define _DEBUG_
#define _ELIMINATE_DUP_

//static char rcsid[] = "$Id: gpsi.C,v 1.3 2009/08/10 06:30:57 vsfgd Exp vsfgd $";
extern char *__progname;

dhashclient *dhash;
int out;

chordID maxID;
static const char* dsock;
static const char* gsock;

//void getKeyValue(const char*, str&, std::map<std::vector<POLY>, double, CompareSig>&, int&);
void makeKeyValue(char **, int&, str&, std::map<std::vector<POLY>, double, CompareSig>&, int&, InsertType);
void getKeyValue(const char*, str&, std::vector<POLY>&, double&, int&);
void makeKeyValue(char **, int&, str&, std::vector<POLY>&, double&, int&, InsertType);
DHTStatus insertDHT(chordID, char *, int, int = MAXRETRIES, chordID = 0);
void retrieveDHT(chordID ID, int, str&, chordID guess = 0);

chordID randomID(void);
void listen_gossip(void);
void accept_connection(int);
void read_gossip(int);
int getdir(std::string, std::vector<std::string>&);
void readsig(std::string, std::vector<std::vector <POLY> >&);
bool sig2str(std::vector<POLY>, str&);
void calcfreq(std::vector<std::vector <POLY> >);
void calcfreqO(std::vector<std::vector <POLY> >);
void usage(void);

DHTStatus insertStatus;
std::string INDEXNAME;

// entries of a node
vec<str> nodeEntries;

bool insertError;
bool retrieveError;

std::map<std::vector<POLY>, double, CompareSig> uniqueSigList;
std::vector<int> rxseq;
std::vector<int> txseq;

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

int
main(int argc, char *argv[])
{
	int ch, intval, fflag, Gflag, gflag, lflag, Sflag, sflag, valLen;
	double freq;
	char *name;
	char *value;
	struct stat statbuf;
	chordID ID;
	DHTStatus status;
	std::string sigdir;
	std::vector<std::string> sigfiles;
	std::vector<std::vector<POLY> > sigList;

	fflag = Gflag = gflag = lflag = Sflag = sflag = intval = 0;
	while ((ch = getopt(argc, argv, "fG:ghi:lS:s:")) != -1)
		switch(ch) {
		case 'f':
			fflag = 1;
			break;
		case 'G':
			Gflag = 1;
			gsock = optarg;
			break;
		case 'g':
			gflag = 1;
			break;
		case 'i':
			intval = atoi(optarg);
			break;
		case 'l':
			lflag = 1;
			break;
		case 'S':
			Sflag = 1;
			dsock = optarg;
			break;
		case 's':
			sflag = 1;
			sigdir = optarg;
			break;
		case 'h':
		case '?':
		default:
			usage();
			break;
		}
	argc -= optind;
	argv += optind;

	// required flags
	if (Gflag == 0 || Sflag == 0 || sflag == 0) usage();

	// FIXME: make sure only one flag is set at a time
	if (fflag == 0 && gflag == 0 && lflag == 0) usage();

	// check intval bound
	if (gflag == 1 && intval == 0) usage();

	if (stat(dsock, &statbuf) != 0) fatal("socket does not exist\n");
	dhash = New dhashclient(dsock);

	getdir(sigdir, sigfiles);
	sigList.clear();
	warnx << "reading signatures from files...\n\n";
	for (unsigned int i = 0; i < sigfiles.size(); i++) {
		//std::cout << sigfiles[i] << "\n";
		readsig(sigfiles[i], sigList);
	}

	warnx << "calculating frequencies...\n\n";
	calcfreq(sigList);

	system("date");

	if (gflag == 1) {
		warnx << "starting to listen...\n";
		listen_gossip();		
		warnx << "starting to gossip...\n";
		sleep(5);

		rxseq.clear();
		txseq.clear();
		txseq.push_back(0);
		while (1) {
			ID = randomID();
			strbuf z;
			z << ID;
			str key(z);
			str buf;
			std::vector<POLY> sig;
			sig.clear();

			for (std::map<std::vector<POLY>, double >::iterator itr = uniqueSigList.begin();
			     itr != uniqueSigList.end(); itr++) {

				sig = itr->first;
				freq = itr->second / 2;
				sig2str(sig, buf);
				std::cout << "\ninserting:\n\ttxseq: " << txseq.back() << "\n\tsig: "
				      << buf << "\n\tfreq/2: " << freq << "\n";
				//makeKeyValue(&value, valLen, key, sig, freq, txseq.back(), GOSSIP);
				makeKeyValue(&value, valLen, key, uniqueSigList, txseq.back(), GOSSIP);
				status = insertDHT(ID, value, valLen, MAXRETRIES);
				cleanup(value);

				if (status != SUCC) fatal("insert FAILed\n");
				else warnx << "insert SUCCeeded\n";
				// should every sig message have a different seq?
				txseq.push_back(txseq.back() + 1);
			}
			warnx << "waiting " << intval << " seconds...\n\n";
			sleep(intval);
		}
	} else if (fflag == 1) {
		str junk("");
		name = argv[3];
		str2chordID(name, ID);
		retrieveDHT(ID, RTIMEOUT, junk);

		if (retrieveError) {
			delete dhash;
			fatal("Unable to read node ID\n");
		}
	} else if (lflag == 1) {
		rxseq.clear();
		listen_gossip();		
	} else
		usage();

	amain();
	return 0;
}

// copied from psi.C
DHTStatus
insertDHT(chordID ID, char *value, int valLen, int STOPCOUNT, chordID guess)
{
	warnx << "insert operation on key: " << ID << "\n";
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
		std::cerr << "key insert time: " << endTime - beginTime << " secs\n";
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

// TODO: verify randomness
chordID
randomID(void)
{
	char buf[16];
	int j;
	strbuf s;
	chordID ID;

	srand(time(NULL));
	for (int i = 0; i < 40; i++) {
		j = rand() % 16;
		sprintf(buf, "%x", j);
		s << buf;
	}
	str t(s);
	str2chordID(t, ID);
	return ID;
}

void
listen_gossip(void)
{
	unlink(gsock);
	int fd = unixsocket(gsock);
	if (fd < 0) fatal << "Error creating GOSSIP socket" << strerror (errno) << "\n";

	// make socket non-blocking
	make_async(fd);

	if (listen(fd, 5) < 0) {
		fatal("Error from listen: %m\n");
		close(fd);
	}
	fdcb(fd, selread, wrap(accept_connection, fd));
}

void
accept_connection(int lfd)
{
	sockaddr_un sun;
	bzero(&sun, sizeof(sun));
	socklen_t sunlen = sizeof(sun);
	// vs "struct sockaddr *"
	int cs = accept(lfd, reinterpret_cast<sockaddr *> (&sun), &sunlen);
	if (cs >= 0) warnx << "accepted connection on local socket\n";
	//if (cs >= 0) warnx << "accepted connection. file descriptor = " << cs << "\n";
	else if (errno != EAGAIN) fatal << "\taccept; errno = " << errno << "\n";
	
	fdcb(cs, selread, wrap(read_gossip, cs));
}

void
read_gossip(int fd)
{
	strbuf buf;
	str key, t;
	std::vector<POLY> sig;
	std::vector<std::vector <POLY> > sigList;
	int n, s;
	double freq;

	n = buf.tosuio()->input(fd);
	if (n < 0) fatal("read failed\n");

	if (n == 0) {
		fdcb(fd, selread, 0);
		close(fd);
		// what's the difference?
		// exit(0);
		return;
	}
	str gElem(buf);
	getKeyValue(gElem.cstr(), key, sig, freq, s);

	sig2str(sig, t);
	std::cout << "\nreading from socket:\n\trxseq: " << s << "\n\tID: " << key
	      << "\n\tsig: " << t << "\n\tfreq: " << freq << "\n";

	// what about freq?
	rxseq.push_back(s);
	sigList.clear();
	sigList.push_back(sig);
	calcfreq(sigList);
}

// based on:
// http://www.linuxquestions.org/questions/programming-9/c-list-files-in-directory-379323/
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

	// open tags
	//tagsfile = sigfile + std::string(TAGFILE);
	//warnx << "opening tagsfile: " << tagsfile.c_str() << "...\n";
	//tagsfp = fopen(tagsfile.c_str(), "r");
	// change to if?
	//assert(tagsfp);

	// TODO: add maxcount arg?

	// DONT use readData to retrieve signatures from input files...
	// since the size filed uses POLY as a basic unit and not byte...
	// Read numSigs <it should be 1> for data signatures...
	int numSigs;
	if (fread(&numSigs, sizeof(int), 1, sigfp) != 1) {
		warnx << "numSigs: " << numSigs << "\n";
	}
	warnx << "NUM sigs: " << numSigs << "\n";
	assert(numSigs == 1);

	int size;

	if (fread(&size, sizeof(int), 1, sigfp) != 1) {
		assert(0);
	}
	warnx << "Signature size: " << size << "\n";

	std::vector<POLY> sig;
	sig.clear();
	POLY e;
	std::cout << "Document signature: ";
	for (int i = 0; i < size; i++) {
		if (fread(&e, sizeof(POLY), 1, sigfp) != 1) {
			assert(0);
		}
		sig.push_back(e);
		std::cout << e << ".";
	}
	std::cout << "\n";
	sigList.push_back(sig);
	warnx << "Size of sig list: " << sigList.size() << "\n\n";
	finishTime = getgtod();
	fclose(sigfp);

	/*
	char hostname[64];
	gethostname(hostname, sizeof(hostname));
#ifdef _LIVE_
	strcpy(hostname, "");
#endif
	// TODO: add docidType arg?
	char docId[128];
	// Read the document id
	int len;
	// len of docid string includes \0
	if (fread(&len, sizeof(len), 1, sigfp) != 1) {
		assert(0);
	}

	if (fread(&docId, len, 1, sigfp) != 1) {
		assert(0);
	}
	// XXX: add the hostname
	strcat(docId, ".");
	strcat(docId, hostname);
	strcat(docId, "#");

	//warnx << "**********************************************************\n";
	//warnx << "*            Inserting document " << docId << " ********\n";
	//warnx << "**********************************************************\n";

	// Read the distinct tags
	std::vector<std::string> distinctTags;
	distinctTags.clear();

	readTags(tagsfp, distinctTags);

	// Each H-index based on tagname!
	for (int ts = 0; ts < (int) distinctTags.size(); ts++) {
		// Do not insert for attributes
		if (distinctTags[ts].find(ATTRSUFFIX) != std::string::npos) {
			continue;
		}
		INDEXNAME = distinctTags[ts];

		// is REPLICAID needed?
		std::string root = INDEXNAME + std::string(ROOTNODEID);
		//std::string root = INDEXNAME + std::string(ROOTNODEID) + std::string(REPLICAID);
		//warnx << "********* H-index chosen for inserting signature:  " << distinctTags[ts].c_str() << " ********** \n";
		rootNodeID = compute_hash(root.c_str(), root.length());
		//warnx << "Root node id for index " << INDEXNAME.c_str() << " : " << rootNodeID << "\n";

		// ROOT node id			
		strbuf s;
		s << rootNodeID;
		str rootID(s);

		//warnx << "ROOT ID: " << rootID << "\n";
#ifdef _DEBUG_
		for (int l = 0; l < (int) sig.size(); l++) {
			warnx << sig[l] << " ";
		}
		//warnx << "\n";
#endif
	}
	fclose(sigfp);
	fclose(tagsfp);

	//warnx << "Data read in bytes: " << dataFetched << "\n";
	//warnx << "Data written in bytes: " << dataStored << "\n";
	//warnx << "Number of documents inserted: " << 1 << "\n";
	// TODO: count?
	//warnx << "Number of documents inserted: " << count - 1 << "\n";
	//warnx << "Number of full nodes: " << cacheOfFullNodes.size() << "\n";
	finishTime = getgtod();
	//std::cerr << "Time taken to process XML tags: " << finishTime - startTime << " secs " << std::endl;
	*/
}

bool
sig2str(std::vector<POLY> sig, str &buf)
{
	strbuf s;

	if (sig.size() <= 0) return false;

	for (int i = 0; i < (int) sig.size(); i++)
		s << sig[i] << ".";

	buf = s;
	return true;
}

void
calcfreq(std::vector<std::vector <POLY> > sigList)
{
	str buf;

	for (int i = 0; i < (int) sigList.size(); i++) {
		std::map<std::vector<POLY>, double >::iterator itr = uniqueSigList.find(sigList[i]);
		sig2str(sigList[i], buf);
		if (itr != uniqueSigList.end()) {
			itr->second += 1;
			//warnx << "sig: " << buf << "\nfreq: " << itr->second << "\n";
		} else {
			uniqueSigList[sigList[i]] = 1;
			//warnx << "sig: " << buf << "\nfreq: " << uniqueSigList[sigList[i]] << "\n";
		}
		//warnx << "Size of unique sig list: " << uniqueSigList.size() << "\n\n";
	}
	warnx << "Size of unique sig list: " << uniqueSigList.size() << "\n\n";
}

// copied from psi.C
void
calcfreqO(std::vector<std::vector <POLY> > sigList)
{
	std::vector<int> group1;
	std::vector<int> group2;
	std::vector<POLY> lcm1;
	std::vector<POLY> lcm2;
	std::multimap<int, int> sortedSigList;

#ifdef _ELIMINATE_DUP_
	std::map<std::vector<POLY>, std::vector<int>, CompareSig> uniqueSigList;
	std::map<int, std::map<std::vector<POLY>, std::vector<int>, CompareSig>::const_iterator > duplicateList;

	// Sort them based on degree
	for (int i = 0; i < (int) sigList.size(); i++) {
		std::map<std::vector<POLY>, std::vector<int> >::iterator itr = uniqueSigList.find(sigList[i]);
		if (itr != uniqueSigList.end()) {
			itr->second.push_back(i+1);
		} else {
			int deg = getDegree(sigList[i]);
			//warnx << "Check degree: " << deg << "\n";
			sortedSigList.insert(std::pair<int, int>(deg, i));
			std::vector<int> e;
			e.clear();
			e.push_back(i+1);
			uniqueSigList[sigList[i]] = e;

			std::map<std::vector<POLY>, std::vector<int>, CompareSig>:: const_iterator uitr = 
			uniqueSigList.find(sigList[i]);
			//warnx << "Size of unique sig list: " << uniqueSigList.size() << "\n";
			assert(uitr != uniqueSigList.end());

			duplicateList[i+1] = uniqueSigList.find(sigList[i]);
		}
	}
	assert(uniqueSigList.size() > 0);

	// If all are identical
	if (uniqueSigList.size() == 1) {
		for (int i = 0; i < (int) floor(MAXENTRIES/2.0); i++) {
			group1.push_back(i+1);
		}
		lcm1 = uniqueSigList.begin()->first;

		for (int i = (int) floor(MAXENTRIES/2.0); i < MAXENTRIES; i++) {
			group2.push_back(i+1);
		}
		lcm2 = uniqueSigList.begin()->first;
		return;
	}
	warnx << "Size of unique sig list: " << uniqueSigList.size() << "\n";
	warnx << "Size of dup sig list: " << duplicateList.size() << "\n";
#else
	// Sort them based on degree
	for (int i = 0; i < (int) sigList.size(); i++) {
		int deg = getDegree(sigList[i]);
		//warnx << "Check degree: " << deg << "\n";
		sortedSigList.insert(std::pair<int, int>(deg, i));
	}
#endif

	warnx << "Size of sorted sig list: " << sortedSigList.size() << "\n";

	int seed1 = -1, seed2 = -1;
	std::vector<POLY> gcdPoly;
	int minDeg = INT_MAX;

	// Find the two most dissimilar
	std::multimap<int, int>::reverse_iterator endItrI = sortedSigList.rbegin();
	int i = 0;
	int MAXSIGS = 10;
	// Adjust the last sig to be checked
	while (i < MAXSIGS && endItrI != sortedSigList.rend()) {
		endItrI++;
		i++;
	}		

	for (std::multimap<int, int>::reverse_iterator itrI = sortedSigList.rbegin();
	     itrI != endItrI; itrI++) {

		std::multimap<int, int>::reverse_iterator itrJ = itrI;
		itrJ++;

		for (; itrJ != endItrI; itrJ++) {
			gcdPoly.clear();
			gcdSpecial(gcdPoly, sigList[itrI->second], sigList[itrJ->second]);
			std::vector<POLY> mylcm;
			mylcm = sigList[itrI->second];

			//int deg = pSim(sigList[itrI->second], sigList[itrJ->second], gcdPoly,
			//    metric);
			int deg = pSimOpt(mylcm, sigList[itrJ->second], gcdPoly, metric);
			//warnx << "Degree: " << deg << "\n";
			if (deg < minDeg) {
				seed1 = itrI->second;
				seed2 = itrJ->second;
				minDeg = deg;
			}

			if (deg < 0) break;
		}
	}

	//warnx << "MINDEGREE: " << minDeg << "\n";
	group1.push_back(seed1+1);
	group2.push_back(seed2+1);

	//warnx << "SEED1 : " << seed1
	//      << "  SEED2 : " << seed2
	//      << "\n";
	lcm1 = sigList[seed1];
	lcm2 = sigList[seed2];

	std::vector<POLY> gcd1, gcd2;
	std::vector<POLY> testLCM1, testLCM2;

	for (std::multimap<int, int>::iterator itr = sortedSigList.begin();
	     itr != sortedSigList.end(); itr++) {
		if (itr->second == seed1 || itr->second == seed2) continue;

		gcd1.clear();
		gcd2.clear();

		gcdSpecial(gcd1, lcm1, sigList[itr->second]);
		gcdSpecial(gcd2, lcm2, sigList[itr->second]);

		testLCM1 = lcm1;
		testLCM2 = lcm2;

		//int deg1 = getDegree(gcd1);
		//int deg2 = getDegree(gcd2);
		int deg1 = pSimOpt(testLCM1, sigList[itr->second], gcd1, metric);
		int deg2 = pSimOpt(testLCM2, sigList[itr->second], gcd2, metric);

		if (deg1 > deg2) {
			//lcm(lcm1, sigList[itr->second]);
			lcm1 = testLCM1;
			group1.push_back(itr->second+1);
		}
		else if (deg1 < deg2) {
			//lcm(lcm2, sigList[itr->second]);
			lcm2 = testLCM2;
			group2.push_back(itr->second+1);
		} else {
			if (group1.size() <= group2.size()) {
				//lcm(lcm1, sigList[itr->second]);
				lcm1 = testLCM1;
				group1.push_back(itr->second+1);
			} else {
				//lcm(lcm2, sigList[itr->second]);
				lcm2 = testLCM2;
				group2.push_back(itr->second+1);
			}
		}		
	}

#ifdef _ELIMINATE_DUP_
	int group1Size = group1.size();
	int group2Size = group2.size();
	warnx << "Group1 size: " << group1Size << "\n";

	for (int i = 0; i < group1Size; i++) {
		std::map<int, std::map<std::vector<POLY>, std::vector<int>, CompareSig>::const_iterator >::const_iterator itr = 
		duplicateList.find(group1[i]);

		assert(itr != duplicateList.end());

		std::map<std::vector<POLY>, std::vector<int>, CompareSig>::const_iterator ditr = itr->second;
		//warnx << "--> " << ditr->second.size() << "\n";
		// Skip the first entry -- already in group1
		for (int k = 1; k < (int) ditr->second.size(); k++) {
			group1.push_back(ditr->second[k]);	
		}
	}

	warnx << "Group2 size: " << group2Size << "\n";

	for (int i = 0; i < group2Size; i++) {
		std::map<int, std::map<std::vector<POLY>, std::vector<int>, CompareSig>::const_iterator >::const_iterator itr = 
		duplicateList.find(group2[i]);

		assert(itr != duplicateList.end());

		std::map<std::vector<POLY>, std::vector<int>, CompareSig>::const_iterator ditr = itr->second;

		//warnx << "--> " << ditr->second.size() << "\n";
		// Skip the first entry -- already in group2
		for (int k = 1; k < (int) ditr->second.size(); k++) {
			group2.push_back(ditr->second[k]);	
		}
	}
	warnx << "Sig list size: " << sigList.size() << "\nTotal size: " << group1.size() + group2.size() << "\n";
	// Header is absent - one less
	//assert(group1.size() + group2.size() == (size_t) MAXENTRIES);
#endif

	return;
}

void
usage(void)
{
	warn << "usage: " << __progname << " [-h] [-S dhash-sock] [-G g-sock] [-s sigdir] [-f | -l | -g [-i interval in sec]]\n";
	exit(0);
}
