/*	$Id$ */

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

//static char rcsid[] = "$Id$";
extern char *__progname;

dhashclient *dhash;
int out;

chordID maxID;
static const char* dhash_sock;

void makeKeyValue(char **, int&, str&, int&, InsertType);
DHTStatus insertDHT(chordID, char *, int, int = -1, int = MAXRETRIES, chordID = 0);

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

void usage(void);

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
	int i, valLen;
	double startTime;
	struct stat statbuf;
	char *value;
	char *buf;
	chordID ID;
	DHTStatus stat;
	time_t rawtime;

	srand(time(NULL));
	i = rand() % 100 + 1;
	warnx << "random i (1-100): " << i << "\n";

	if (argc < 2) usage();

	dhash_sock = argv[1];

	if (stat(dhash_sock, &statbuf) != 0) fatal("socket does not exist\n");

	dhash = New dhashclient(dhash_sock);

	startTime = getgtod();

	system("date");

	// TODO: generate random chordID
	buf = ctime(&rawtime);
	ID = compute_hash(buf, strlen(buf));

	makeKeyValue(&value, valLen, GOSSIP);
	stat = insertDHT(ID, value, valLen, -1, MAXRETRIES);
	cleanup(value);

	if (stat != SUCC) fatal("insert FAILed\n");

	//amain();
	return 0;
}

DHTStatus
insertDHT(chordID ID, char *value, int valLen, int randomNum, int STOPCOUNT, chordID guess)
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

void
usage(void)
{
	warn << "usage: " << __progname << " socket [interval]\n";
	exit(0);
}
