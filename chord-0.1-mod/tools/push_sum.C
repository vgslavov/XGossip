/*	$Id: push_sum.C,v 1.2 2009/06/28 17:43:59 vsfgd Exp vsfgd $ */

#include <ctime>
#include <iostream>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <chord.h>
#include <dhash_common.h>
#include <dhashclient.h>
#include <dhblock.h>

#include "../utils/utils.h"
#include "nodeparams.h"
#include "psi.h"

#define DEBUG 1

//static char rcsid[] = "$Id: push_sum.C,v 1.2 2009/06/28 17:43:59 vsfgd Exp vsfgd $";
extern char *__progname;

dhashclient *dhash;
int out;

chordID maxID;
static const char* dhash_sock;

void getKeyValue(str&, str&, int&);
void makeKeyValue(char **, int&, str&, int&, InsertType);
DHTStatus insertDHT(chordID, char *, int, int = MAXRETRIES, chordID = 0);
void retrieveDHT(chordID ID, int, str&, chordID guess = 0);

DHTStatus insertStatus;

// entries of a node
vec<str> nodeEntries;

bool insertError;
bool retrieveError;

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

void usage(void);

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
		//warnx << "Failed to store key...\n";
		insertError = true;
	} else {
		numWrites++;
		//warnx << "Key stored successfully...\n";
	}
	out++;
}

int
main(int argc, char *argv[])
{
	int i, fflag, sflag, valLen;
	double startTime, endTime;
	struct stat statbuf;
	char *buf;
	char *cmd;
	char *name;
	char *value;
	chordID ID;
	DHTStatus status;
	time_t rawtime;

	fflag = sflag = 0;
	switch(argc) {
	case 4:
		sflag = 1;
		break;
	case 5:
		fflag = 1;
		break;
	default:
		usage();
	}

	dhash_sock = argv[1];
	if (stat(dhash_sock, &statbuf) != 0) fatal("socket does not exist\n");
	dhash = New dhashclient(dhash_sock);

	cmd = argv[2];
	system("date");

	if (strcmp(cmd, "-s") == 0 && sflag) {
		srand(time(NULL));
		i = rand() % 100 + 1;
		warnx << "Inserting value: " << i << "\n";

		// TODO: generate truly random chordID
		buf = ctime(&rawtime);
		ID = compute_hash(buf, strlen(buf));

		strbuf s;
		s << ID;
		str key(s);

		makeKeyValue(&value, valLen, key, i, GOSSIP);
		startTime = getgtod();
		status = insertDHT(ID, value, valLen, MAXRETRIES);
		endTime = getgtod();
		//warnx << "ID used: " << ID << "\n";
		//std::cout << "Insert time: " << endTime - startTime << " secs" << std::endl;
		cleanup(value);

		if (status != SUCC) fatal("Insert FAILed\n");
		else warnx << "Insert SUCCeeded\n";
	} else if (strcmp(cmd, "-f") == 0 && fflag) {
		str junk("");
		name = argv[3];
		str2chordID(name, ID);
		startTime = getgtod();
		retrieveDHT(ID, RTIMEOUT, junk);

		if (retrieveError) {
			delete dhash;
			fatal("Unable to read node ID\n");
		}
		endTime = getgtod();
		//warnx << "ID used: " << ID << "\n";
		//std::cout << "Retrieve time: " << endTime - startTime << " secs" << std::endl;
		str a;
		int b;
		getKeyValue(nodeEntries[0], a, b);
		warnx << "KEY: " << a << "\nVALUE: " << b << "\n";
	} else
		usage();

	//amain();
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

void
usage(void)
{
	warn << "usage: " << __progname << " socket -f ID [interval]\n";
	warn << "       " << __progname << " socket -s [interval]\n";
	exit(0);
}
