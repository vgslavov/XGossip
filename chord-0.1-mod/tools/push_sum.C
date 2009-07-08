/*	$Id: push_sum.C,v 1.4 2009/07/06 20:46:46 vsfgd Exp vsfgd $ */

#include <cmath>
#include <ctime>
#include <iostream>
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
#include "push_sum.h"

#define DEBUG 1

//static char rcsid[] = "$Id: push_sum.C,v 1.4 2009/07/06 20:46:46 vsfgd Exp vsfgd $";
extern char *__progname;

dhashclient *dhash;
int out;

chordID maxID;
static const char* dhash_sock;

void getKeyValue(str&, str&, double&, double&);
void makeKeyValue(char **, int&, str&, double&, double&, InsertType);
DHTStatus insertDHT(chordID, char *, int, int = MAXRETRIES, chordID = 0);
void retrieveDHT(chordID ID, int, str&, chordID guess = 0);

chordID randomID(void);
void startg(void);
void accept_connection(int);
void read_gossip(int);
void usage(void);

DHTStatus insertStatus;

// entries of a node
vec<str> nodeEntries;

bool insertError;
bool retrieveError;

str gsock;

vec<double> sum;
//std::vector<double> sum;
vec<double> weight;
//std::vector<double> weight;

// hash table statistics
int numReads;
int numWrites;
int numDocs;
int cacheHits;

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
		warnx << "Failed to store key...\n";
		insertError = true;
	} else {
		numWrites++;
		warnx << "Key stored successfully...\n";
	}
	out++;
}

int
main(int argc, char *argv[])
{
	double s, w;
	int intval, fflag, lflag, gflag, valLen;
	char *cmd;
	char *name;
	char *value;
	struct stat statbuf;
	chordID ID;
	DHTStatus status;

	fflag = lflag = gflag = 0;
	switch(argc) {
	case 3:
		lflag = 1;
		break;
	case 4:
		fflag = 1;
		gflag = 1;
		break;
	default:
		usage();
	}

	dhash_sock = argv[1];
	if (stat(dhash_sock, &statbuf) != 0) fatal("socket does not exist\n");
	dhash = New dhashclient(dhash_sock);

	cmd = argv[2];
	system("date");

	if (strcmp(cmd, "-g") == 0 && gflag) {
		intval = atoi(argv[3]);
		//startg();		
		srand(time(NULL));
		sum.push_back(rand() % 100 + 1);
		weight.push_back(1);

		while (1) {
			ID = randomID();
			strbuf z;
			z << ID;
			str key(z);

			std::cout << "s_0=" << sum.back() << ", w_0=" << weight.back() << "\n";
			s = sum.back()/2;
			w = weight.back()/2;
			std::cout << "Gossiping (s/2, w/2): (" << s << ", " << w << ")\n";
			makeKeyValue(&value, valLen, key, s, w, GOSSIP);
			status = insertDHT(ID, value, valLen, MAXRETRIES);
			cleanup(value);

			if (status != SUCC) fatal("Insert FAILed\n");
			else warnx << "Insert SUCCeeded\n";
			warnx << "waiting " << intval << " seconds...\n";
			sleep(intval);
		}
		//amain();
	} else if (strcmp(cmd, "-f") == 0 && fflag) {
		str junk("");
		name = argv[3];
		str2chordID(name, ID);
		retrieveDHT(ID, RTIMEOUT, junk);

		if (retrieveError) {
			delete dhash;
			fatal("Unable to read node ID\n");
		}
		str k;
		getKeyValue(nodeEntries[0], k, s, w);
		std::cout << "KEY: " << k << "\nVALUE (s, w): (" << s << ", " << w << ")\n";
	} else if (strcmp(cmd, "-l") == 0 && lflag) {
		startg();		
		amain();
	} else
		usage();

	return 0;
}

// copied from psi.C
DHTStatus
insertDHT(chordID ID, char *value, int valLen, int STOPCOUNT, chordID guess)
{
	warnx << "Insert operation on key: " << ID << "\n";
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
		std::cerr << "Key insert time: " << endTime - beginTime << " secs\n";
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
startg(void)
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
	// here or in main()?
	//amain();
}

void
accept_connection(int lfd)
{
	sockaddr_un sun;
	bzero(&sun, sizeof(sun));
	socklen_t sunlen = sizeof(sun);
	// vs "struct sockaddr *"
	int cs = accept(lfd, reinterpret_cast<sockaddr *> (&sun), &sunlen);
	if (cs >= 0) warnx << "accepted connection. file descriptor = " << cs << "\n";
	else if (errno != EAGAIN) fatal << "accept; errno = " << errno << "\n";
	
	fdcb(cs, selread, wrap(read_gossip, cs));
}

void
read_gossip(int fd)
{
	strbuf buf;
	str gKey;
	int n;
	double s, w;

	n = buf.tosuio()->input(fd);
	if (n < 0) fatal("read failed\n");

	if (n == 0) {
		fdcb(fd, selread, 0);
		close(fd);
		// what's the difference?
		// exit(0);
		return;
	}
	//warnx << "data read: " << buf << "\n";
	str gElem(buf);
	getKeyValue(gElem.cstr(), gKey, s, w);

	//if (!sum.empty() && !weight.empty() && (s == sum.back()) && (w == weight.back())) return;

	std::cout << "read from socket: KEY: " << gKey << ", VALUE (s, w): ("
		  << s << ", " << w << ")\n";

	if (sum.empty() || weight.empty()) {
		sum.push_back(s);
		weight.push_back(w);
	} else {
		sum.push_back(sum.back() + s);
		weight.push_back(weight.back() + w);
	}

	// check if sum and weight vecs have the same size

	std::cout << "current sum: " << sum.back() << ", current weight: "
		  << weight.back() << "\n";

	std::cout << "step t=" << sum.size() << ", avg s/w="
		  << sum.back()/weight.back() << "\n";
}

void
usage(void)
{
	warn << "usage: " << __progname << " socket -f{etch} ID\n";
	warn << "       " << __progname << " socket -g{ossip} [sec]\n";
	warn << "       " << __progname << " socket -l{isten}\n";
	exit(0);
}
