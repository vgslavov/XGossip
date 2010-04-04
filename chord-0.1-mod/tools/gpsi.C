/*	$Id: gpsi.C,v 1.34 2010/03/28 16:29:26 vsfgd Exp vsfgd $	*/

#include <cmath>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
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

static char rcsid[] = "$Id: gpsi.C,v 1.34 2010/03/28 16:29:26 vsfgd Exp vsfgd $";
extern char *__progname;

dhashclient *dhash;
int out;

chordID maxID;
static const char* dsock;
static const char* gsock;
static char *logfile;
static char *irrpolyfile;
int lshseed;
int plist = 0;
int discardmsg = 0;
int peers = 0;

// TODO: command line args?
int lfuncs = 10;
int mgroups = 20;
 
void accept_connection(int);
void add2vecomap(std::vector<std::vector <POLY> >, std::vector<double>, std::vector<double>);
int getdir(std::string, std::vector<std::string>&);
void calcfreq(std::vector<std::vector <POLY> >);
void calcfreqM(std::vector<std::vector <POLY> >, std::vector<double>, std::vector<double>);
void calcfreqO(std::vector<std::vector <POLY> >);
void doublefreq(int);
void inform_team(std::vector<chordID>);
void initgossiprecv(std::vector<POLY>, double, double);
DHTStatus insertDHT(chordID, char *, int, int = MAXRETRIES, chordID = 0);
std::vector<POLY> inverse(std::vector<POLY>);
void listen_gossip(void);
//void *listen_gossip(void *);
void merge_lists(void);
void printdouble(std::string, double);
void printlist(int, int);
chordID randomID(void);
void read_gossip(int);
void readsig(std::string, std::vector<std::vector <POLY> >&);
//void retrieveDHT(chordID ID, int, str&, chordID guess = 0);
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

typedef std::map<std::vector<POLY>, std::vector<double>, CompareSig> mapType;
typedef std::vector<mapType> vecomap;
// local list T[0] is stored in allT[0];
vecomap allT;

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

int
main(int argc, char *argv[])
{
	//pthread_t thread_ID;
	//void *exit_status;

	int Gflag, gflag, Lflag, lflag, rflag, Sflag, sflag, zflag, vflag, Hflag, dflag, jflag, mflag, uflag;

	int ch, intval, nids, valLen;
	int logfd;
	char *value;
	struct stat statbuf;
	time_t rawtime;
	chordID ID;
	DHTStatus status;
	std::string sigdir;
	std::vector<std::string> sigfiles;
	std::vector<std::vector<POLY> > sigList;

	Gflag = gflag = Lflag = lflag = rflag = Sflag = sflag = zflag = vflag = Hflag = dflag = jflag = mflag = uflag = 0;

	intval = nids = lshseed = 0;
	irrpolyfile = NULL;
	rxseq.clear();
	txseq.clear();

	while ((ch = getopt(argc, argv, "cd:G:gHhi:j:L:lmn:pq:rS:s:uvz")) != -1)
		switch(ch) {
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
		case 'i':
			intval = strtol(optarg, NULL, 10);
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
		for (int i = 0; i < nids; i++) {
			strbuf t;
			ID = make_randomID();
			//t << ID;
			//str tmpkey(t);
			//warnx << tmpkey << "\n";
			warnx << "ID: " << ID << "\n";
			warnx << "successorID: " << successorID(ID, 0) << "\n";
			//sleep(5);
		}
		return 0;
	}

	// TODO: make sure only one flag is set at a time
	// read, gossip, LSH or listen
	if (rflag == 0 && gflag == 0 && lflag == 0 && Hflag == 0) usage();

	// sockets are required when listening or gossiping
	if ((gflag == 1 || lflag == 1) && (Gflag == 0 || Sflag == 0)) usage();

	// TODO: check intval bound
	if (gflag == 1 && intval == 0 && sflag == 0) usage();

	if (rflag == 1 && sflag == 0) usage();
	if (Hflag == 1 && sflag == 0 && dflag == 0 && jflag == 0) usage();

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

	if (rflag == 1 || gflag == 1 || Hflag == 1) {
		getdir(sigdir, sigfiles);
		sigList.clear();

		// insert dummy
		// the polynomial "1" has a degree 0
		dummysig.clear();
		dummysig.push_back(1);
		sigList.push_back(dummysig);

		/*
		std::vector<POLY> testsig;
		testsig.clear();
		testsig.push_back(6228403);
		testsig.push_back(6228403);
		testsig.push_back(6228403);
		testsig.push_back(6228403);
		sigList.push_back(testsig);
		*/

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

	// LSH
	if (Hflag == 1 && dflag != 0 && sflag != 0 && jflag != 0) {
		// XXX: ugly, use templates
		// use findMod()
		double freq, weight;
		if (mflag == 1) {
			std::vector<POLY> minhash;
			std::vector<std::vector<POLY> > matrix;
			std::vector<POLY> sig;
			str sigbuf;
			int col;

			for (mapType::iterator itr = allT[0].begin(); itr != allT[0].end(); itr++) {
				sig = itr->first;
				freq = itr->second[0];
				weight = itr->second[1];

				lsh *myLSH = new lsh(sig.size(), lfuncs, mgroups, lshseed, irrpolyfile);

				// convert multiset to set
				if (uflag == 1) {
					/*
					for (unsigned int i = 0; i < sig.size(); i++) {
						warnx << "nomod..." << sig[i] << "\n";
					}
					for (unsigned int i = 0; i < sig.size(); i++) {
						warnx << "mod..." << (myLSH->getUniqueSet(sig))[i] << "\n";
					}
					*/

					sig2str(sig, sigbuf);
					warnx << "multiset: " << sigbuf << "\n";
					myLSH->getUniqueSet(sig);
					sig2str(sig, sigbuf);
					warnx << "set: " << sigbuf << "\n";
				}

				minhash = myLSH->getHashCodeFindMod(sig, myLSH->getIRRPoly());

				//warnx << "minhash.size(): " << minhash.size() << "\n";
				if (plist == 1) {
					warnx << "minhash IDs:\n";
					for (int i = 0; i < (int)minhash.size(); i++) {
						warnx << minhash[i] << "\n";
					}
				}

				matrix.push_back(minhash);
				delete myLSH;
			}
			// call srand/srandom right before calling rand/random
			//srand(time(NULL));
			srand(loseed);
			int range = (int)minhash.size();
			col = int((double)range * rand() / (RAND_MAX + 1.0));

			warnx << "POLYs in random column " << col << ":\n";
			for (int i = 0; i < (int)matrix.size(); i++) {
				warnx << matrix[i][col] << "\n";;
			}
		// use compute_hash()
		} else {
			std::vector<chordID> minhash;
			std::vector<std::vector<chordID> > matrix;
			std::vector<POLY> sig;
			int col;

			if (gflag == 1) {
				warnx << "listening for gossip...\n";
				listen_gossip();		
				sleep(5);
				warnx << "gossiping...\n";
				warnx << "interval: " << intval << "\n";
			}

			for (mapType::iterator itr = allT[0].begin(); itr != allT[0].end(); itr++) {
				sig = itr->first;
				freq = itr->second[0];
				weight = itr->second[1];

				lsh *myLSH = new lsh(sig.size(), lfuncs, mgroups, lshseed, irrpolyfile);
				minhash = myLSH->getHashCode(sig);

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
				warnx << "random col: " << col << "\n";
				ID = (matrix.back())[col];
				warnx << "ID in col: " << ID << "\n";

				if (gflag == 1) {
					strbuf t;
					t << ID;
					str key(t);
					makeKeyValue(&value, valLen, key, sig, freq, weight, INITGOSSIP);
					status = insertDHT(ID, value, valLen, MAXRETRIES);
					cleanup(value);

					// do not exit if insert FAILs!
					if (status != SUCC) {
						warnx << "insert FAILed\n";
					} else {
						warnx << "insert SUCCeeded\n";
					}

					warnx << "sleeping...\n";
					sleep(intval);
				}

				delete myLSH;
			}
		
			/*
			warnx << "chordIDs in random column " << col << ":\n";
			for (int i = 0; i < (int)matrix.size(); i++) {
				warnx << matrix[i][col] << "\n";;
			}
			*/
		}

		return 0;
	}

	time(&rawtime);
	warnx << "ctime: " << ctime(&rawtime);
	warnx << "sincepoch: " << time(&rawtime) << "\n";
	warnx << "rcsid: " << rcsid << "\n";
	warnx << "host: " << host << "\n";
	warnx << "pid: " << getpid() << "\n";
	warnx << "peers: " << peers << "\n";
	warnx << "loseed: " << loseed << "\n";

	if (gflag == 1) {
		warnx << "listening for gossip...\n";
		//pthread_mutex_init(&lock, NULL);
		//pthread_create(&thread_ID, NULL, listen_gossip, NULL);
		listen_gossip();		
		sleep(5);
		warnx << "gossiping...\n";
		warnx << "interval: " << intval << "\n";

		srandom(loseed);
		txseq.push_back(0);
		while (1) {
			ID = make_randomID();
			strbuf z;
			z << ID;
			str key(z);
			//std::vector<POLY> sig;
			//sig.clear();

			//pthread_mutex_lock(&lock);
			double beginTime = getgtod();    
			merge_lists();
			double endTime = getgtod();    
			printdouble("merge lists time: ", endTime - beginTime);
			warnx << "\n";
			warnx << "inserting:\ntxseq: " << txseq.back()
			      << " txID: " << ID << "\n";
			if (plist == 1) {
				printlist(0, txseq.back());
			}
			makeKeyValue(&value, valLen, key, allT[0], txseq.back(), GOSSIP);
			//pthread_mutex_unlock(&lock);
			status = insertDHT(ID, value, valLen, MAXRETRIES);
			cleanup(value);

			// do not exit if insert FAILs!
			if (status != SUCC) {
				warnx << "insert FAILed\n";
				// to preserve mass conservation:
				// "send" msg to yourself
				doublefreq(0);
			} else {
				warnx << "insert SUCCeeded\n";
			}
			txseq.push_back(txseq.back() + 1);
			//pthread_join(thread_ID, &exit_status);
			warnx << "sleeping...\n";
			sleep(intval);
		}

		// when do we destroy the thread?
		//pthread_mutex_destroy(&lock);
	} else if (rflag == 1) {
		return 0;
	} else if (lflag == 1) {
		warnx << "listening for gossip...\n";
		listen_gossip();
		//listen_gossip(NULL);
	} else
		usage();

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

//void *
//listen_gossip(void *)
void
listen_gossip(void)
{
	unlink(gsock);
	int fd = unixsocket(gsock);
	if (fd < 0) fatal << "Error creating GOSSIP socket" << strerror (errno) << "\n";

	// make socket non-blocking
	make_async(fd);

	if (listen(fd, MAXCONN) < 0) {
		// TODO: what's %m?
		fatal("Error from listen: %m\n");
		close(fd);
		return;
	}

	fdcb(fd, selread, wrap(accept_connection, fd));

	//return NULL;
}

void
accept_connection(int fd)
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
	
	fdcb(cs, selread, wrap(read_gossip, cs));
}

void
read_gossip(int fd)
{
	strbuf buf;
	strbuf totalbuf;
	str key;
	str sigbuf;
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
			warnx << "read_gossip: read failed\n";
			// disable readability callback?
			fdcb(fd, selread, 0);
			// do you have to close?
			//close(fd);
			return;
		}

		// EOF
		if (n == 0) {
			warnx << "read_gossip: no more to read\n";
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
		warnx << "\nread_gossip: n: " << n << "\n";
		warnx << "read_gossip: recvlen: " << recvlen << "\n";
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
	if (msgtype == GOSSIP)
		warnx << "GOSSIP";
	else if (msgtype == INITGOSSIP) {
		warnx << "INITGOSSIP";
		warnx << "\n";
		double freq, weight;
		std::vector<POLY> sig;
		sig.clear();
		ret = getKeyValue(gmsg.cstr(), key, sig, freq, weight, recvlen);
		if (ret == -1) {
			fdcb(fd, selread, 0);
			close(fd);
			return;
		}

		sig2str(sig, sigbuf);
		warnx << "rxID: " << key;
		warnx << " sig: " << sigbuf;
		printdouble(" freq: ", freq);
		printdouble(" weight: ", weight);
		warnx << "\n";

		initgossiprecv(sig, freq, weight);
		
		return;
	} else
		warnx << "invalid";

	ret = getKeyValue(gmsg.cstr(), key, sigList, freqList, weightList, seq, recvlen);
	if (ret == -1) {
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
	//calcfreqM(sigList, freqList, weightList);
	//if (plist == 1) printlist(0);
}

// based on:
// http://www.linuxquestions.org/questions/programming-9/c-list-files-in-directory-379323/
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
	warnx << "readsig: Size of sig list: " << sigList.size() << "\n";
	finishTime = getgtod();
	fclose(sigfp);
}

void
initgossiprecv(std::vector<POLY> sig, double freq, double weight)
{
	str sigbuf;
	std::vector<POLY> isig;

	isig.clear();
	isig = inverse(sig);
	sig2str(isig, sigbuf);
	warnx << "isig: " << sigbuf << "\n";

	mapType::iterator itr1 = allT[0].find(sig);
	mapType::iterator itr2 = allT[0].find(isig);
	if ((itr1 != allT[0].end()) && (sig[0] != 0)) {
		// update freq
		itr1->second[0] += freq;
	} else if ((itr1 != allT[0].end()) && (itr2 != allT[0].end())) {
		// delete special multiset tuple
		allT[0].erase(itr2);
	} else {
		if ((sig[0] == 0) && (itr2 != allT[0].end())) {
			// delete regular multiset tuple
			allT[0].erase(itr1);
		} else {
			allT[0][sig].push_back(freq);
			allT[0][sig].push_back(weight);

			lsh *myLSH = new lsh(sig.size(), lfuncs, mgroups, lshseed, irrpolyfile);
			std::vector<chordID> minhash = myLSH->getHashCode(sig);

			inform_team(minhash);

			delete myLSH;
		}
	}
}

void
inform_team(std::vector<chordID> minhash)
{

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

// deprecated: use merge_lists() instead
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
bool
sigcmp(const std::vector<POLY>& s1, const std::vector<POLY>& s2)
{
	// Return TRUE if s1 < s2
	//warnx << "sigcmp: S1 size: " << s1.size()
	//      << ", S2 size: " << s2.size() << "\n";

	if (s1.size() < s2.size()) return true;
	else if (s1.size() == s2.size()) {
		for (int i = 0; i < (int) s1.size(); i++) {
			if (s1[i] < s2[i]) return true;
			else if (s1[i] > s2[i]) return false;
		}
		return false;
	} else return false;
}

void
merge_lists()
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
			// DO NOT skip lists which are done
			// use their dummy
			if (citr[i] == allT[i].end()) {
				//warnx << "no minsig in T_" << i
				//      << " (list ended)\n";
				sumf += allT[i][dummysig][0];
				sumw += allT[i][dummysig][1];
				//continue;
			} else if (citr[i]->first == minsig) {
				//warnx << "minsig found in T_" << i << "\n";
				sumf += citr[i]->second[0];
				sumw += citr[i]->second[1];
				// XXX: itr of T_0 will be incremented later
				if (i != 0) ++citr[i];
			} else {
				//warnx << "no minsig in T_" << i << "\n";
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
printlist(int listnum, int seq)
{
	int n = 0;
	double freq, weight, avg;
	double sumavg = 0;
	double sumsum = 0;
	std::vector<POLY> sig;
	std::vector<POLY> isig;
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

		isig = inverse(sig);
		sig2str(isig, sigbuf);
		warnx << "isig" << n << ": " << sigbuf << "\n";
		isig = inverse(isig);
		sig2str(isig, sigbuf);
		warnx << "isig^2" << n << ": " << sigbuf << "\n";
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
	warn << "Usage: " << __progname << " [-h] [options...]\n\n";
	warn << "Options:\n"
	     << "	-c		discard out-of-round messages\n"
	     << "	-d		<random prime number for LSH seed>\n"
	     << "	-G		<gossip socket>\n"
	     << "	-H		generate chordIDs/POLYs using LSH when gossiping\n"
					"\t\t\t(requires  -g, -s, -d, -j)\n"
	     << "      	-i		<how often>\n"
	     << "      	-j		<irrpoly file>\n"
	     << "	-L		<log file>\n"
	     << "      	-m		call findMod instead of compute_hash\n"
	     << "      	-n		<how many>\n"
	     << "      	-p		print list of signatures\n"
	     << "      	-q		<estimate of # of peers in DHT>\n"
	     << "	-S		<dhash socket>\n"
	     << "	-s		<dir with sigs>\n"
	     << "	-u		convert multiset to set\n\n"
	     << "Actions:\n"
	     << "	-g		gossip (requires -S, -G, -s, -i)\n"
	     << "	-H		generate chordIDs/POLYs using LSH (requires  -s, -d, -j)\n"
	     << "	-l		listen for gossip (requires -S, -G, -L)\n"
	     << "	-r		read signatures (requires -s)\n"
	     << "	-v		print version\n"
	     << "	-z		generate random chordID (requires -n)\n";
	exit(0);
}
