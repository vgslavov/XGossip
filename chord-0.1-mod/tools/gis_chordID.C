// GiS - Graph Indexing via Signatures
// Author: Praveen Rao
// University of Missouri-Kansas City
#include "gis.h"
#include <chord.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <dhash_common.h>
#include <dhashclient.h>
#include <dhblock.h>
#include <dhblock_noauth.h>
#include <dhblock_keyhash.h>
#include <sys/time.h>
#include <string>
#include <math.h>
#include <iostream>
#include "poly.h"
#include "utils.h"
#include <vector>
#include <map>
#include <utility>
#include "utils.C"
#include "nodeparams.h"
#include "cache.h"
#include <db_cxx.h>

int out;
#define DIRECTIO false
static const int PAGE_SIZE = 8192;
static const int NUM_BUFFERS = 2048;

Db *myDB;
static const char *const SIGDB = "sigindex";

DHTStatus insertDocument(str&, std::vector<POLY>&, int, int, int, int, int);
DHTStatus insertDocument(str&, std::vector<POLY>&, int);

str adjust_data (chordID, char*, int, char*, int, dhash_stat&);
str pickChild(vec<str>&, std::vector<POLY>&);
void splitNode(vec<str>&, std::vector<int>&, std::vector<int>&,
    std::vector<POLY>&);
void getKey(str&, str&);
void getKeyValue(str&, str&, str&);
void makeKeyValue(char **, int&, str&, std::vector<POLY>&,
    InsertType);
void makeKeyValue(char **, int&, str&, Interval&,
    InsertType);
void makeKeyValue(char **, int&, vec<str>&, std::vector<int>&,
    InsertType);
DHTStatus insertBDB(chordID, char *, int);
DHTStatus retrieveBDB(chordID, vec<str>&);
DHTStatus performSplit(chordID, Interval&, chordID);

DHTStatus performSplitRoot(chordID, Interval&);
int pSim(std::vector<POLY>&, std::vector<POLY>&,
    std::vector<POLY>&, bool);


void queryProcess(str&, std::vector<std::vector<POLY> >&);
void verifyProcess(char *, std::vector<std::vector<POLY> >&, int);
DHTStatus mergeProcess(str&, std::vector<POLY>&);
int getNumReplicas(int, int);

DHTStatus insertStatus;
vec<chordID> fullNodeIDs;
std::vector<Interval> fullNodeInts;

// entries of a node
vec<str> nodeEntries;

bool insertError;
bool retrieveError;

// hash table statistics
int numReads;
int numWrites;
int numDocs;
int cacheHits;

// pSim
bool metric;

// to avoid repeated node visits during query processing
// can happen due to concurrent operations
// can be avoided by periodic cleanup
std::map<std::string, bool> listOfVisitedNodes;

// Node split related stuff
std::vector<int> group1, group2;
std::vector<POLY> lcm1, lcm2;

unsigned int seedForRand;

// Hash table to store nodes that are full, just serves as a cache!!!
std::map<std::string, bool> cacheOfFullNodes;

// For verification
std::map<int, std::vector<int> > docMatchList;

// Data transfered
int dataFetched = 0, dataStored = 0;

// Fetch callback for NOAUTH...
void
fetch_cb (dhash_stat stat, ptr<dhash_block> blk, vec<chordID> path)
{
    out++;

	if (stat != DHASH_OK) {
		warnx << "Failed to fetch key...\n";
		retrieveError = true;
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
		buf << " path: ";
        for (u_int i=0; i < path.size (); i++) {
            buf << path[i] << " ";
        }
        
        buf << " : ";
        
#endif
        if (blk->vData.size () > 0) {
            //warn << blk->vData.size () << "\n";
            
            for (unsigned int i = 0; i < blk->vData.size (); i++) {
                nodeEntries.push_back(blk->vData[i]);
                dataFetched += nodeEntries[i].len();
            }
        }
        else {
            nodeEntries.push_back(blk->data);
        }
        
        // Output hops
        warnx << "HOPS: " << blk->hops << "\n";
        numReads++;
        
#ifdef _DEBUG_ 
        warnx << "Retrieve information: " << buf << "\n";
#endif
    }
}

void
store_cb (dhash_stat status, ptr<insert_info> i)
{
    out++;

    if (status == DHASH_NOTFIRST) {
        insertStatus = NOTFIRST;
    }
    else if (status != DHASH_OK) {
        warnx << "Failed to store key...\n";
        insertError = true;
    }
    else {
        numWrites++;
        warnx << "Key stored successfully...\n";
    }
}

int
compare_ID(Db *dbp, const Dbt *a, const Dbt *b)
{
    
    // Returns: 
    // < 0 if a < b 
    // = 0 if a = b 
    // > 0 if a > b 
    //assert(a->get_ulen() == b->get_ulen());
    //std::cout << "key 1: " << (char *) a->get_data()
    //          << " key 1 size: " << a->get_size()
    //          << " key 2: " << (char *) b->get_data()
    //          << " key 2 size: " << b->get_size()
    //         << std::endl;
    if (a->get_size() != b->get_size()) {
        return (a->get_size() - b->get_size());
    }
    
    return memcmp(a->get_data(), b->get_data(), a->get_size());
} 

// MAIN function
int main(int argc, char *argv[])
{
	
    if(argc < 7) {
		std::cout << "Usage: " << argv[0]
                  << " dbname " // argv[1]
                  << " -[dDiqv] "  // argv[2]
                  << " signaturefile|#oflevels|level " // argv[3]
                  << " fanout|verifyqueryfile " // argv[4]
                  << " count " // argv[5]
                  << " irrpolydegree " // argv[6]
                  << " <metric=1|gcd=0> " // argv[7]
                  << " <num buffers> " // argv[8]
                  << " <dbdenv dir> " // argv[9]
                  << std::endl;
		exit(1);
	}

	char *cmd = argv[2];
	
    chordID ID;

	// pSim metric
	metric = argv[7] ? (bool) atoi(argv[7]) : false;
		
    // Create an environment first and it shld be shared by all
	// the logical databases
	DbEnv myEnv(0);
	
	if (DIRECTIO == true) {
		try {
            //myEnv.set_verbose(DB_DIRECT, 1);
			myEnv.set_flags(DB_DIRECT_DB, 1);
		}

		catch (DbException &e) {
            std::cout << e.what() << std::endl;
			exit(1);
		}
	}
    
    int numBuffers = argv[8] ? atoi(argv[8]) : NUM_BUFFERS;

    warnx << "Number of buffers: " << numBuffers << "\n";
    
	myEnv.set_cachesize(0, PAGE_SIZE * numBuffers, 0);
	myEnv.set_data_dir(".");

    // Cleanup only once
	if (!strcmp(cmd, "-d")) {
		char cmd1[32];
		sprintf(cmd1, "rm -rf %s/*", argv[9]);
		system(cmd1);
	}
    else if (!strcmp(cmd, "-i") || !strcmp(cmd, "-q")) { // Required to reset the buffer cache!!!
        char cmd1[32];
        sprintf(cmd1, "rm -rf %s/__db*", argv[9]);
        system(cmd1);
    }
    	
	try {
		myEnv.open(argv[9], DB_CREATE | DB_INIT_MPOOL, 0);
	}
	catch (DbException &e) {
        std::cerr << "Exception occurred: " << e.what() << std::endl;
		exit(1);
	}

    // handle for the berkeley db
    myDB = new Db(&myEnv, 0);
    myDB->set_pagesize(PAGE_SIZE);
    myDB->set_bt_compare(compare_ID);
    
	// Initialize node types
	LEAF = compute_hash(LEAFNODE, strlen(LEAFNODE));
	NONLEAF = compute_hash(NONLEAFNODE, strlen(NONLEAFNODE));
	ROOT = compute_hash(ROOTNODE, strlen(ROOTNODE));

	int maxCount = atoi(argv[5]);
	int irrPolyDegree = atoi(argv[6]);
	int randDelay = atoi(argv[4]);

	warnx << "Irreducible polynomial degree: " << irrPolyDegree << "\n";
	warnx << "Fan out: " << MAXENTRIES << "\n";
	
	// Time measurement
	double startTime = getgtod();
	seedForRand = (unsigned int) startTime;
	
	numReads = numWrites = dataReadSize = cacheHits = 0;

	// Compute the root node key for the tree
	chordID rootNodeID = compute_hash(ROOTNODEID, strlen(ROOTNODEID));
	warnx << "Root node id: " << rootNodeID << "\n";
        
	
    if (!strcmp(cmd, "-d")) {
		// Dump an index that has a root and a single leaf
        try {
            myDB->open(NULL, argv[1], SIGDB, DB_BTREE, DB_CREATE, 0);
        } catch(DbException &e) {
            std::cerr << "Failed to open DB object" << e.what() << argv[1] << std::endl;
            exit(1);
        }

        Interval leafInt;
		leafInt.ln = 0;
		leafInt.ld = 1;
		leafInt.rn = 1;
		leafInt.rd = 1;
		leafInt.level = 0;

		char tempBuf[128];
		sprintf(tempBuf, "%d.%d.%d", leafInt.level, leafInt.ln,
            leafInt.ld);
		chordID leafID;
		leafID = compute_hash(tempBuf, strlen(tempBuf));
		
		char *hdrVal;
		int hdrLen;

		strbuf lType;
		lType << LEAF;
		str leafType(lType);

		makeKeyValue(&hdrVal, hdrLen, leafType, leafInt, UPDATEHDR);

		// Leaf node
		DHTStatus stat = insertBDB(leafID, hdrVal, hdrLen);
		cleanup(hdrVal);

		Interval rootInt;
		rootInt.ln = 0;
		rootInt.ld = 1;
		rootInt.rn = 1;
		rootInt.rd = 1;
		rootInt.level = 1;

		strbuf rType;
		rType << ROOT;
		str rootType(rType);
		
		makeKeyValue(&hdrVal, hdrLen, rootType, rootInt, UPDATEHDR);
		stat = insertBDB(rootNodeID, hdrVal, hdrLen);
		cleanup(hdrVal);
		
		char *value;
		int valLen;

		std::vector<POLY> sig;
		sig.push_back(0x1);

		strbuf sBuf;
		sBuf << leafID;
		str leafIDStr(sBuf);
		
		makeKeyValue(&value, valLen, leafIDStr, sig, UPDATE);

		// Root node
		stat = insertBDB(rootNodeID, value, valLen);
		cleanup(value);
		
	}
	else if (!strcmp(cmd, "-q")) {
		double totalTime = 0;
		
		// Read the query signatures from a file
		// Read the DHT from root to leaf
		std::string queryFile(argv[3]);
		FILE *qfp = fopen(queryFile.c_str(), "r");
		assert(qfp);

		int count = 1;
        
        try {
            myDB->open(NULL, argv[1], SIGDB, DB_BTREE, DB_CREATE, 0);
        } catch(DbException &e) {
            std::cerr << "Failed to open DB object" << std::endl;
            exit(1);
        }
        		
		while (count <= maxCount) {

			// DON'T use readData to retrieve signatures from input files...
			// since the size filed uses POLY as a basic unit and not byte...
			// Format is <n = # of sigs><sig size><sig>... n times...
			int numSigs;
			if (fread(&numSigs, sizeof(numSigs), 1, qfp) != 1) {
				break;
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
				// free the allocated memory
				delete[] buf;
			}
			
			warnx << "******* Processing query " << count << " ********\n";
			numReads = numWrites = dataReadSize = cacheHits = 0;
			strbuf s;
			s << rootNodeID;
			str rootID(s);

			warnx << "Number of signatures: " << listOfSigs.size() << "\n";
			if ((int) listOfSigs.size() > MAXSIGSPERQUERY) {
				warnx << "Skipping this query...\n";
			}
			else {
				numDocs = 0;
				listOfVisitedNodes.clear();
				
				double beginQueryTime = getgtod();
				queryProcess(rootID, listOfSigs);
				double endQueryTime = getgtod();
				std::cout << "Query time: " << endQueryTime - beginQueryTime << std::endl;
				totalTime += (endQueryTime - beginQueryTime);
				warnx << "Num docs: " << numDocs << "\n";
			}
			
#ifdef _DEBUG_
			
#endif
			warnx << "Data read: " << dataFetched << "\n";
            warnx << "Data write: " << dataStored << "\n";
			warnx << "Num reads: " << numReads << "\n";
			//warnx << " num writes: " << numWrites << "\n";
			warnx << "Cache hits: " << cacheHits << "\n";
			warnx << "******** Finished query processing *********\n\n\n";
			count++;


			if (0) {
                sleep(1 + (int) ((double) randDelay * (rand() / (RAND_MAX + 1.0))));
			}
		}
		
		fclose(qfp);
		std::cout << "Time taken: " << totalTime << std::endl;
	}
	else if (!strcmp(cmd, "-v")) {
		// Read the query signatures from a file
		// Read the DHT from root to leaf
		std::string queryFile(argv[4]);
		FILE *qfp = fopen(queryFile.c_str(), "r");
		assert(qfp);

		int count = 1;
		while (count <= maxCount) {

			// DON'T use readData to retrieve signatures from input files...
			// since the size filed uses POLY as a basic unit and not byte...
			// Format is <n = # of sigs><sig size><sig>... n times...
			int numSigs;
			if (fread(&numSigs, sizeof(numSigs), 1, qfp) != 1) {
				break;
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
				// free the allocated memory
				delete[] buf;
			}
			
			//if ((int) listOfSigs.size() > MAXSIGSPERQUERY) {
			if (0) {
				warnx << "Skipping query verification...\n";
			}
			else {
				// Verify the query signature(s) with the data signatures!
				verifyProcess(argv[3], listOfSigs, count);
			}
			//warnx << "******** Finished verifying queries *********\n\n\n";
			count++;
			
		}

		// print the docMatchList
		for (std::map<int, std::vector<int> >::iterator itr = docMatchList.begin();
             itr != docMatchList.end(); itr++) {
			for (int i = 0; i < (int) itr->second.size(); i++) {
				std::cout << itr->second[i] << " ";
			}
			std::cout << "\n";
		}
		
		fclose(qfp);
	}
	else if (!strcmp(cmd, "-i")) {
		// Insert new signatures
		std::string dataFile(argv[3]);
		FILE *sigfp = fopen(dataFile.c_str(), "r");
        assert(sigfp);
        
        try {
            myDB->open(NULL, argv[1], SIGDB, DB_BTREE, DB_CREATE, 0);
        } catch(DbException &e) {
            std::cerr << "Failed to open DB object" << std::endl;
            exit(1);
        }
 
		int count = 1;
		while (count <= maxCount) {

			// DONT use readData to retrieve signatures from input files...
			// since the size filed uses POLY as a basic unit and not byte...
			// Read numSigs <it should be 1> for data signatures...
			int numSigs;
			if (fread(&numSigs, sizeof(int), 1, sigfp) != 1) {
				break;
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
			for (int i = 0; i < size; i++) {
				if (fread(&e, sizeof(POLY), 1, sigfp) != 1) {
					assert(0);
				}
				sig.push_back(e);
			}

			// Read the document id
			int docId;
			if (fread(&docId, sizeof(docId), 1, sigfp) != 1) {
				assert(0);
			}

            warnx << "**********************************************************\n";
			warnx << "*            Inserting document " << docId << " ********\n";
            warnx << "**********************************************************\n";
			// ROOT node id			
			strbuf s;
			s << rootNodeID;
			str rootID(s);
			DHTStatus status;

			warnx << "ROOT ID: " << rootID << "\n";
#ifdef _DEBUG_
			for (int l = 0; l < (int) sig.size(); l++) {
				warnx << sig[l] << " ";
			}
			warnx << "\n";
#endif
			while (1) {

                // Keep track of full nodes and their headers
                fullNodeIDs.clear();
                fullNodeInts.clear();
                
				status = insertDocument(rootID, sig, docId);

				if (status == REINSERT) {
					sleep(5);
					warnx << "Reinsert in progress...\n";
				}
				else {
					break;
				}
			}
			
			if (status == SUCC) {
				warnx << "***** Finished document insert successfully *******\n";
			}
			else if (status == FAIL) {
				warnx << "*+*+*+* Document insert FAILED *+*+*+*\n";
			}
			else if (status == FULL) {
				warnx << "*-*-*-* Index is full!!!! *-*-*-*\n";
				break;
			}
			
			count++;
			
			if (0) {
                sleep(1 + (int) ((double) randDelay * (rand() / (RAND_MAX + 1.0))));
			}
		}
		
		fclose(sigfp);
        warnx << "Data read: " << dataFetched << "\n";
        warnx << "Data write: " << dataStored << "\n";
		warnx << "Number of documents inserted: " << count - 1 << "\n";
		warnx << "Number of full nodes: " << cacheOfFullNodes.size() << "\n";
		double finishTime = getgtod();
        
		std::cout << "Time taken: " << finishTime - startTime << std::endl;
	}
	else {
        std::cerr << "Invalid option" << std::endl;
    }

    // Not opened for verification
    if (!strcmp(cmd, "-v")) {
        myDB->close(0);
    }
    delete myDB;
    
    std::cout << "------------------ Printing Statistics -----------------" << std::endl;
    myEnv.memp_stat_print(DB_STAT_CLEAR);
    std::cout << "--------------------------------------------------------" << std::endl;

    myEnv.close(0);
    //amain(); Asynchronous interface, requires probably exit()
    // from callbacks. eg. if (inflight == 0) exit(0);
    return 0;
}

// Verification process, the set of correct matches...
void verifyProcess(char *fileName, std::vector<std::vector<POLY> >& listOfSigs,
    int queryid)
{
	int numMatches = 0;
	FILE *sigfp = fopen(fileName, "r");

	std::vector<POLY> oldSig;
	std::vector<POLY> sig;
	
	int loopCount = 0;

	while (1) {
		int numSigs;
		if (fread(&numSigs, sizeof(int), 1, sigfp) != 1) {
			break;
		}
		assert(numSigs == 1);
		
		int size;
		
		if (fread(&size, sizeof(int), 1, sigfp) != 1) {
			assert(0);
		}

		// clear old sig
		sig.clear();
		POLY e;
		for (int i = 0; i < size; i++) {
			if (fread(&e, sizeof(POLY), 1, sigfp) != 1) {
				assert(0);
			}
			sig.push_back(e);
		}

		int docId;
		if (fread(&docId, sizeof(docId), 1, sigfp) != 1) {
			assert(0);
		}

		int dega = getDegree(sig);
		
		//warnx << "Deg a: " << dega << "\n";
		for (int j = 0; j < (int) listOfSigs.size(); j++) {
			std::vector<POLY> rem;
			// Test for divisibility!!!
			rem.clear();

			int degb = getDegree(listOfSigs[j]);

			//warnx << "Deg b: " << degb << "\n";
			
			if (dega >= degb) {
				//remainder(rem, sig, listOfSigs[j]);
				//double beginTime = getgtod();
				remainder(rem, sig, listOfSigs[j]);
				//double endTime = getgtod();
				//std::cerr << "REM time: " << endTime - beginTime << "\n";
			}

			if (rem.size() == 1 && rem[0] == (POLY) 0) {
				//warnx << "==== Found match ==== DOCID: " << docId << "\n";
				//warnx << docId << "\n";
				std::map<int, std::vector<int> >::iterator titr;
				titr = docMatchList.find(loopCount);
				
				if (titr != docMatchList.end()) {
					titr->second.push_back(queryid);
					
				}
				else {
					std::vector<int> e;
					e.push_back(queryid);
					docMatchList[loopCount] = e;
				}
				
				numMatches++;
				break;
			}
		}

		// If any document if not matched, just add an empty entry
		if (docMatchList.find(loopCount) == docMatchList.end()) {
			std::vector<int> e;
			docMatchList[loopCount] = e;
		}
		
		loopCount++;
	}

	fclose(sigfp);
}

// Computes the similarity between two signatures
int pSim(std::vector<POLY>& s1, std::vector<POLY>& s2,
    std::vector<POLY>& mygcd, bool isMetric)
{
	int retVal;
	if (isMetric) {
        std::vector<POLY> mylcm;
		mylcm.push_back((POLY) 0x1);
		lcm(mylcm, s1);
		lcm(mylcm, s2);
		double val = (double) (INT_MAX-1) * getDegree(mygcd) / getDegree(mylcm);
		retVal = (int) val;
	}
	else {
		retVal = getDegree(mygcd);
	}
	//warnx << "PSim: " << retVal << "\n";
	return retVal;
}

// Pick the best child
// Tie-breaking strategies not EMPLOYED YET!!!
str pickChild(vec<str>& nodeEntries, std::vector<POLY>& sig)
{
	std::vector<POLY> entrySig;
	std::vector<POLY> gcdPoly;
	int maxGCDDegree = -1;
	int sigDegree, bestChildSigDegree = -1;
	str bestChild = "NULL";
	str childKey;

	//warnx << "# of node entries for pick child: "
	//			<< nodeEntries.size() << "\n";
	
	// The first entry has the nodetype/interval
	for (int i = 1; i < (int) nodeEntries.size(); i++) {
		childKey = "";
		entrySig.clear();
		getKeyValue(nodeEntries[i].cstr(), childKey, entrySig);

		// degree of this signature
		sigDegree = getDegree(entrySig);
		
		// First compute GCD with the entry to be inserted
		gcdPoly.clear();
		
		// Compute GCD
		gcdSpecial(gcdPoly, entrySig, sig);
		
        int deg = pSim(entrySig, sig, gcdPoly, metric);
		//warnx << "Pick child degree: " << deg << "\n";
		if (deg > maxGCDDegree) {
			maxGCDDegree = deg;
			bestChild = childKey;
			bestChildSigDegree = sigDegree;
		}
		else if (deg == maxGCDDegree) {
			// Choose the smallest degree signature.
			// For split nodes, till they are fixed properly.
			// tie breaker
			if (sigDegree < bestChildSigDegree) {
				bestChild = childKey;
				bestChildSigDegree = sigDegree;
			}
		}
	}

	return bestChild;
}


// Split a node
void splitNode(vec<str>& entries,
    std::vector<int>& group1,
    std::vector<int>& group2,
    std::vector<POLY>& lcm1,
    std::vector<POLY>& lcm2)
{
	std::vector<std::vector<POLY> > sigList;
	vec<str> IDList;
	
	str ID, sigStr;
	std::vector<POLY> s;

    assert(entries.size() > 0);

	// Read in the signatures
	for (int i = 1; i < (int) entries.size(); i++) {
		getKeyValue(entries[i], ID, sigStr);
        //warnx << "Check id: " << ID << "\n";
		IDList.push_back(ID);
		s.clear();

		POLY *sigPtr = (POLY *) sigStr.cstr();
		for (int j = 0; j < (int) sigStr.len()/(int) sizeof(POLY); j++) {
			s.push_back(sigPtr[j]);
		}
		sigList.push_back(s);
	}

	std::multimap<int, int> sortedSigList;
	// Sort them based on degree
	for (int i = 0; i < (int) sigList.size(); i++) {
		int deg = getDegree(sigList[i]);
        //warnx << "Check degree: " << deg << "\n";
		sortedSigList.insert(std::pair<int, int>(deg, i));
	}
	
	int seed1 = -1, seed2 = -1;
	std::vector<POLY> gcdPoly;
	int minDeg = INT_MAX;
	
	// Find the two most dissimilar
	//for (int i = 0; i < sigList.size() - 1; i++) {
	std::multimap<int, int>::iterator endItrI = sortedSigList.end();
	endItrI--;
	
	for (std::multimap<int, int>::iterator itrI = sortedSigList.begin();
         itrI != endItrI; itrI++) {
		itrI++;
		std::multimap<int, int>::iterator itrJ = itrI;
		itrI--;
		
		for (; itrJ != sortedSigList.end(); itrJ++) {
			gcdPoly.clear();
			gcdSpecial(gcdPoly, sigList[itrI->second], sigList[itrJ->second]);

			//int deg = getDegree(gcdPoly);
			int deg = pSim(sigList[itrI->second], sigList[itrJ->second], gcdPoly,
                metric);
            //warnx << "Degree: " << deg << "\n";
			if (deg < minDeg) {
				seed1 = itrI->second;
				seed2 = itrJ->second;
				minDeg = deg;
			}
			
			if (deg < 0) break;
		}
	}

	group1.push_back(seed1+1);
	group2.push_back(seed2+1);

    //warnx << "SEED1 : " << seed1
    //      << "  SEED2 : " << seed2
    //      << "\n";
	lcm1 = sigList[seed1];
	lcm2 = sigList[seed2];

	std::vector<POLY> gcd1, gcd2;
	
	//for (int i = 0; i < sigList.size(); i++) {
	for (std::multimap<int, int>::iterator itr = sortedSigList.begin();
         itr != sortedSigList.end(); itr++) {
		if (itr->second == seed1 || itr->second == seed2) continue;

		gcd1.clear();
		gcd2.clear();
		
		gcdSpecial(gcd1, lcm1, sigList[itr->second]);
		gcdSpecial(gcd2, lcm2, sigList[itr->second]);

		//int deg1 = getDegree(gcd1);
		//int deg2 = getDegree(gcd2);
		int deg1 = pSim(lcm1, sigList[itr->second], gcd1, metric);
		int deg2 = pSim(lcm2, sigList[itr->second], gcd2, metric);

		if (deg1 > deg2) {
			lcm(lcm1, sigList[itr->second]);
			group1.push_back(itr->second+1);
		}
		else if (deg1 < deg2) {
			lcm(lcm2, sigList[itr->second]);
			group2.push_back(itr->second+1);
		}
		else {
			if (group1.size() <= group2.size()) {
				lcm(lcm1, sigList[itr->second]);
				group1.push_back(itr->second+1);
			}
			else {
				lcm(lcm2, sigList[itr->second]);
				group2.push_back(itr->second+1);
			}
		}		
	}

#ifdef _DEBUG_
	// Print the two groups
	warnx << "GROUP 1 \n";
	for (int i = 0; i < (int) group1.size(); i++) {
		warnx << group1[i] << " ";
	}

	warnx << "\n";
	
	warnx << "GROUP 2 \n";
	for (int i = 0; i < (int) group2.size(); i++) {
		warnx << group2[i] << " ";
	}

	warnx << "\n";
#endif
	return;
}

DHTStatus retrieveBDB(chordID ID, vec<str>& entries)
{
    // Read the key/value from berkeley db
    Dbt myKey;
    Dbt myValue;

    strbuf s;
	s << ID;
	str nodeID = str(s);

    //myKey.set_flags(DB_DBT_USERMEM);
    myKey.set_data(const_cast<void *>((const void *) nodeID.cstr()));
    myKey.set_size(nodeID.len());

    myValue.set_flags(DB_DBT_MALLOC);
    
    if (myDB->get(NULL, &myKey, &myValue, 0) != 0) {
        warnx << "Error reading node...\n";
        return FAIL;
    }

    entries = get_payload((char *) myValue.get_data(), myValue.get_size());
    free(myValue.get_data());
    return SUCC;
}

DHTStatus insertBDB(chordID ID, char *value, int valLen)
{
	warnx << "Storing key: " << ID << "\n";
	out = 0;
	insertError = false;
    insertStatus = SUCC;

    dataStored += valLen;
    

    // Read the key/value from berkeley db
    Dbt myKey;
    Dbt myValue;

    strbuf s;
	s << ID;
	str nodeID = str(s);

    //myKey.set_flags(DB_DBT_USERMEM);
    myKey.set_data(const_cast<void *>((const void *) nodeID.cstr()));
    myKey.set_size(nodeID.len());

    myValue.set_flags(DB_DBT_MALLOC);
    
    dhash_stat res;
    str updateVal;
    
    if (myDB->get(NULL, &myKey, &myValue, 0) == DB_NOTFOUND) {
        warnx << "Key does not exist\n";
        updateVal = adjust_data(ID, value, valLen, NULL, 0, res);
    }
    else {
        warnx << "VAL size: " << myValue.get_size() << " " << myValue.get_ulen() << "\n";
        updateVal = adjust_data(ID, value, valLen, (char *) myValue.get_data(),
            myValue.get_size(), res);

        free(myValue.get_data());
    }
    
    if (res == DHASH_NOTFIRST) {
        insertStatus = NOTFIRST;
    }

    // Now insert again
    //myKey.set_flags(DB_DBT_USERMEM);
    myKey.set_data(const_cast<void *>((const void *) nodeID.cstr()));
    myKey.set_size(nodeID.len());
    
    //myValue.set_flags(DB_DBT_USERMEM);
    myValue.set_data(const_cast<void *>((const void *) updateVal.cstr()));
    myValue.set_size(updateVal.len());
    
    warnx << "Value length: " << updateVal.len() << "\n";
        
    int ret = myDB->put(NULL, &myKey, &myValue, 0);
    
    warnx << "Ret val: " << ret << "\n";
    // Write to DBD
	if (insertError) {
		warnx << "Error during INSERT operation...\n";
		return FAIL;
	}
    
	return insertStatus;
}

// A node is adjusted based on the kind of operation requested
str adjust_data (chordID key, char *new_data, int new_len, char *prev_data, int prev_len,
    dhash_stat& resStatus)
{
   	vec<str> old_elems;
    

    if (prev_len > 0) {
        warnx << "PREV LEN: " << prev_len << "\n";
        old_elems = get_payload(prev_data, prev_len);
    }
    
    warnx << "Old elems...\n";
    
    str new_elems(new_data, new_len);

    warnx << "New elems...\n";
    
        
    InsertType type;
    memcpy(&type, new_elems.cstr(), sizeof(type));

    // get rid of the type field, and only have the key and value
    str newElem(new_elems.cstr()+sizeof(type), new_elems.len()-sizeof(type));

    // Updates or replaces a particular (sig,ptr) pair
    if (type == UPDATE || type == UPDATEIFPRESENT || type == REPLACESIG) {
        
        str newKey, childKey;
        std::vector<POLY> newSig, childSig;
        bool isUpdate = false;
        
        newKey = "";
        newSig.clear();

        getKeyValue(newElem.cstr(), newKey, newSig);
                
        // skip the header
        for (int i = 1; i < (int) old_elems.size(); i++) {
            childKey = "";
            childSig.clear();
			
            getKeyValue(old_elems[i].cstr(), childKey, childSig);
            
            if (childKey == newKey) {

                if (type == UPDATE  || type == UPDATEIFPRESENT) {
                    lcm(childSig, newSig);
                }
                else if (type == REPLACESIG) {
                    warnx << "Replacing entry in parent after split...\n";
                    childSig = newSig;
                }
                else {
                    assert(0);
                }
                
                isUpdate = true;
                
                char *value;
                int valLen;
                makeKeyValue(&value, valLen, childKey, childSig, NONE);
                old_elems[i] = str(value, valLen);
                cleanup(value);
                
                // Can break after this step, but will leave it like that.
            }
        }

        if (!isUpdate) {

            
            if (type != UPDATEIFPRESENT) {
                old_elems.push_back(newElem);
            }
            // For UPDATEIFPRESENT, don't add the entry -- because a split occurred
            // and the parent has moved to another node
        }

    }
    else if (type == UPDATEHDRLOCK || type == UPDATEHDR) { // header update
        
        warnx << "UPDATEHDR...\n";
        
        //warnx << "In update hdr...\n";
        if (old_elems.size() == 0) {
            old_elems.push_back(newElem);
        }
        else {
            // this is not the first peer, so relax and don't update
            // the database
            if (type == UPDATEHDRLOCK) {
                resStatus = DHASH_NOTFIRST;
            }
            else {
                str currKey, currValue;
                Interval currInt;
                getKeyValue(old_elems[0], currKey, currValue);
                getInterval(currValue, currInt);
                
                str newKey, newValue;
                Interval newInt;
                getKeyValue(newElem, newKey, newValue);
                getInterval(newValue, newInt);
				
				
                // Can do some verification like change of node types
                // type cannot change from leaf to non-leaf/root or
                // from non-leaf to leaf/root

                assert(newInt.ld == currInt.ld && newInt.ln == currInt.ln);
                
                // Update the interval
                if (newInt.rn * currInt.rd < newInt.rd * currInt.rn) {
                    currInt.rn = newInt.rn;
                    currInt.rd = newInt.rd;
                }
                
                // Only for the root, the level can change (increase)
                chordID nodeType;
                str2chordID(currKey, nodeType);

                if (nodeType == ROOT) {
                    //warnx << "CURR LEVEL: " << currInt.level << " NEW LEVEL: "
                    //<< newInt.level << "\n";
                    if (currInt.level < newInt.level) {
                        currInt.level = newInt.level;
                    }
                }
                
                char *hdrVal;
                int hdrLen;
                makeKeyValue(&hdrVal, hdrLen, currKey, currInt);
                
                // updating the header!!!
                old_elems[0] = str(hdrVal, hdrLen);
                cleanup(hdrVal);
            }
        }
    }
    else if (type == SPLIT) { // splitting a node -- add and remove entries

        warnx << "SPLIT...\n";
        
        if ((int) old_elems.size() > MAXENTRIES) {

            vec<str> update_elems;
            str childSigStr;
			str childKey;
            
            // Copy the header
            update_elems.push_back(old_elems[0]);
            
            // Skip the header
            for (int i = 1; i < (int) old_elems.size(); i++) {
                childKey = "";
                childSigStr = "";
                
                getKeyValue(old_elems[i].cstr(), childKey, childSigStr);
                
                const char *ptr = newElem.cstr();
                bool isPresent = false;
                                
                while (ptr < newElem.cstr() + newElem.len()) {
                    // Delete all those specified
                    int entryLen;
                    memcpy(&entryLen, ptr, sizeof(entryLen));
                    ptr += sizeof(entryLen);
                    str keyToDelete;
                    str sigToDelete;
                    
                    getKeyValue(ptr, keyToDelete, sigToDelete);
                    
                    //warnx << "Child key: " << childKey << " Key to delete: "
                    //      << keyToDelete << "\n";
                    
                    if (childKey == keyToDelete) {
                        if (sigToDelete != childSigStr) {
                            warnx << "We missed an update...Alert peer to refresh\n";
                        }
                        isPresent = true;
                        break;
                    }
                    
                    ptr += entryLen;
                }
                
                if (!isPresent) {
                    update_elems.push_back(old_elems[i]);
                }
            }
        
            //warnx << "Size of UPDATE ELEMS: " << update_elems.size() << "\n";
            str marshalled_block = marshal_block(update_elems);
			
            return marshalled_block;
        }
        else if (old_elems.size() == 1) {
            // Only header present -- 
            // Get all the keys and add them
            const char *ptr = newElem.cstr();
            int count = 0;
            while (ptr < newElem.cstr() + newElem.len()) {
                int entryLen;
                memcpy(&entryLen, ptr, sizeof(entryLen));
                ptr += sizeof(entryLen);
                str entryToAdd(ptr, entryLen);
                //warnx << "Entry to add: " << entryToAdd << "\n";
                // Add to existing list
                old_elems.push_back(entryToAdd);
                ptr += entryLen;
                count++;
            }
            //warnx << "Inserted into new node " << count << "\n";
        }
        
    }
    else if (type > 0) { // regular value append - used by iindex
        if ((int) old_elems.size() > MAXENTRIES) {
            resStatus = DHASH_FULL;
        }
        else {
            old_elems.push_back(new_elems);
        }
        //warnx << "Incorrect type...\n";
        //assert(0);
    }
    

	str marshalled_block = marshal_block(old_elems);
  
    return marshalled_block;
}


// split the root node with only one peer authorized to do it
DHTStatus performSplitRoot(chordID ID, Interval& oldInt)
{
    vec<str> entries;
	
    // now fetch the node
	out = 0;
	retrieveError = false;
	
	//dhash->retrieve(ID, DHASH_NOAUTH, wrap(fetch_cb));

	//while (out == 0) acheck();

    retrieveBDB(ID, entries);
    
	if (retrieveError) {
		warnx << "Failed to retrieve key during insertion...\n";
		return FAIL;
	}

	

    // Already split!!! so don't do anything more...
    if ((int) entries.size() <= MAXENTRIES) {
        return FAIL;
    }
    
	str nodeType, b;
	Interval nodeInt;
		
    getKeyValue(entries[0], nodeType, b);

	getInterval(b, nodeInt);

    // Test the old header seen and what is seen now...
    if (oldInt != nodeInt) {
        return FAIL;
    }

	// split the node
	group1.clear();
	group2.clear();
	lcm1.clear();
	lcm2.clear();

#ifdef _DEBUG_
	// Printing the node
	warnx << "------> ";
	for (int i = 0; i < (int) entries.size(); i++) {
		warnx << entries[i] << "        ";
	}
	warnx << "\n";
#endif
	
	splitNode(entries, group1, group2, lcm1, lcm2);

	// Original node ask it to delete the second part...
	// The new node, make sure it does not have the first part...
	// Then we can guarantee eventual correctness guarantee...
			
    // Key - already known
	// Format of value - <type=SPLIT><interval><group1><group2>
	Interval firstNodeInt, secondNodeInt, myInt;
	firstNodeInt.ln = 0;
	firstNodeInt.ld = 1;
	firstNodeInt.rn = 1;
	firstNodeInt.rd = 2;
	firstNodeInt.level = nodeInt.level;

	secondNodeInt.ln = 1;
	secondNodeInt.ld = 2;
	secondNodeInt.rn = 1;
	secondNodeInt.rd = 1;
	secondNodeInt.level = nodeInt.level;

    myInt.ln = nodeInt.ln;
    myInt.ld = nodeInt.ld;
    myInt.rn = nodeInt.rn;
    myInt.rd = nodeInt.rd;
    myInt.level = nodeInt.level + 1;

	
    // Key - new key from Interval -- includes level information
	// for uniqueness
	char tempBuf[128];
	sprintf(tempBuf, "%d.%d.%d", firstNodeInt.level, firstNodeInt.ln, firstNodeInt.ld);
	chordID firstNodeID;
	firstNodeID = compute_hash(tempBuf, strlen(tempBuf));
 	strbuf s;
	s << firstNodeID;
	str firstChildID = str(s);
			
	char *value;
	int valLen;

	char *hdrVal;
	int hdrLen;

    strbuf s1;
	s1 << NONLEAF;
	str nonLeafType(s1);
	makeKeyValue(&hdrVal, hdrLen, nonLeafType, firstNodeInt, UPDATEHDRLOCK);

    warnx << "-------------- Performing Root Split ------------------\n";
	// step 1: Create sibling node
		
	DHTStatus stat = insertBDB(firstNodeID, hdrVal, hdrLen);
	cleanup(hdrVal);

	if (stat == FAIL) return FAIL;

    // This peer has authority to finish the split
    if (stat != NOTFIRST) {
        // step 2: Insert contents to the newly created node...
        makeKeyValue(&value, valLen, entries, group1, SPLIT);

        stat = insertBDB(firstNodeID, value, valLen);
        cleanup(value);

        if (stat == FAIL) return FAIL;

        // step 3: Create the second node
        sprintf(tempBuf, "%d.%d.%d", secondNodeInt.level, secondNodeInt.ln, secondNodeInt.ld);
        chordID secondNodeID;
        secondNodeID = compute_hash(tempBuf, strlen(tempBuf));

        strbuf s2;
        s2 << secondNodeID;
        str secondChildID(s2);
        
        makeKeyValue(&hdrVal, hdrLen, nonLeafType, secondNodeInt, UPDATEHDR);
        
        		
        DHTStatus stat = insertBDB(secondNodeID, hdrVal, hdrLen);
        cleanup(hdrVal);
        
        if (stat == FAIL) return FAIL;

        // step 4: Insert contents to the newly created node...
        makeKeyValue(&value, valLen, entries, group2, SPLIT);

        stat = insertBDB(secondNodeID, value, valLen);
        cleanup(value);

        if (stat == FAIL) return FAIL;
        
        // step 5: Next delete all the contents from the original node
        std::vector<int> group3 = group1;
        for (int i = 0; i < (int) group2.size(); i++) {
            group3.push_back(group2[i]);
        }
        makeKeyValue(&value, valLen, entries, group3, SPLIT);
        
        stat = insertBDB(ID, value, valLen);
        cleanup(value);
        
        if (stat == FAIL) return FAIL;


        // step 6: Now add links to the two new children
        makeKeyValue(&value, valLen, firstChildID, lcm1, UPDATE);
        stat = insertBDB(ID, value, valLen);
        cleanup(value);
        if (stat == FAIL) return FAIL;
        
        makeKeyValue(&value, valLen, secondChildID, lcm2, UPDATE);
        stat = insertBDB(ID, value, valLen);
        cleanup(value);
        if (stat == FAIL) return FAIL;
        
        // step 7: Update the header of the original node
        makeKeyValue(&hdrVal, hdrLen, nodeType, myInt, UPDATEHDR);
        
        stat = insertBDB(ID, hdrVal, hdrLen);
        cleanup(hdrVal);
        if (stat == FAIL) return FAIL;
    }
    else if (stat == NOTFIRST) { // Will await header change
        int waitInterval = 1;
        vec<str> myNodeEntries;
        
        while (1) {
            sleep(waitInterval);
            out = 0;
           
            retrieveError = false;
            
            //dhash->retrieve(ID, DHASH_NOAUTH, wrap(fetch_cb));
            //while (out == 0) acheck();
            myNodeEntries.clear();
            
            retrieveBDB(ID, myNodeEntries);
            
            if (retrieveError) {
                warnx << "Unable to read node " << ID << "\n";
                return FAIL;
            }
            
           
            str a, b;
            Interval mynodeInt;

            //warnx << "# of node entries: " << myNodeEntries.size() << "\n";
            
            getKeyValue(myNodeEntries[0], a, b);
            
            getInterval(b, mynodeInt);

            // Check the header for change
            if (nodeInt == mynodeInt) {
                waitInterval *= 2;    
            }
            else {
                warnx << "------ Root header has changed ------\n";
                break;
            }
            
        }
    }

	return NODESPLIT;
}

// Perform non root node splits
DHTStatus performSplit(chordID ID, Interval& oldInt, chordID pID)
{
    vec<str> entries;
	
    // now fetch the node
	out = 0;
	retrieveError = false;
	
	//dhash->retrieve(ID, DHASH_NOAUTH, wrap(fetch_cb));

	//while (out == 0) acheck();

    retrieveBDB(ID, entries);
    
	if (retrieveError) {
		warnx << "Failed to retrieve key during insertion...\n";
		return FAIL;
	}

	
    // Already split!!! so don't do anything more...
    if ((int) entries.size() <= MAXENTRIES) {
        return FAIL;
    }
    
	str nodeType, b;
	Interval nodeInt;
		
    getKeyValue(entries[0], nodeType, b);

	getInterval(b, nodeInt);

    // Test the old header seen and what is seen now
    if (oldInt != nodeInt) {
        return FAIL;
    }

    // split the node
	group1.clear();
	group2.clear();
	lcm1.clear();
	lcm2.clear();

#ifdef _DEBUG_
	// Printing the node
	warnx << "------> ";
	for (int i = 0; i < (int) entries.size(); i++) {
		warnx << entries[i] << "        ";
	}
	warnx << "\n";
#endif
	
	splitNode(entries, group1, group2, lcm1, lcm2);

	// Original node ask it to delete the second part...
	// The new node, make sure it does not have the first part...
	// Then we can guarantee eventual correctness guarantee...
			
	// Key - already known
	// Format of value - <type=SPLIT><interval><group1><group2>
	Interval myInt, mySiblingInt;
	mySiblingInt.ln = nodeInt.ln + nodeInt.rn;
	mySiblingInt.ld = nodeInt.ld + nodeInt.rd;
	mySiblingInt.rn = nodeInt.rn;
	mySiblingInt.rd = nodeInt.rd;
	mySiblingInt.level = nodeInt.level;
			
	myInt.ln = nodeInt.ln;
	myInt.ld = nodeInt.ld;
	myInt.rn = mySiblingInt.ln;
	myInt.rd = mySiblingInt.ld;
    myInt.level = nodeInt.level;

	//warnx << "NODE LEVEL: " << nodeInt.level << "\n";
	
	// Key - new key from Interval -- includes level information
	// for uniqueness
	char tempBuf[128];
	sprintf(tempBuf, "%d.%d.%d", mySiblingInt.level, mySiblingInt.ln, mySiblingInt.ld);
	chordID siblingID;
	siblingID = compute_hash(tempBuf, strlen(tempBuf));
	strbuf s;
	s << siblingID;
	str newChildID = str(s);
			
	char *value;
	int valLen;

	char *hdrVal;
	int hdrLen;

	makeKeyValue(&hdrVal, hdrLen, nodeType, mySiblingInt, UPDATEHDRLOCK);

    //warnx << "Size group1: " << group1.size() << "\n";
    //warnx << "Size group2: " << group2.size() << "\n";
    
    warnx << "------ Performing Non Root Node Splitting -------\n";
    
	// step 1: Create sibling node
		
	DHTStatus stat = insertBDB(siblingID, hdrVal, hdrLen);
	cleanup(hdrVal);

	if (stat == FAIL) return FAIL;

    // This peer has authority to finish the split
    if (stat != NOTFIRST) {

        warnx << "I am the first!\n";
        
        // step 2: Insert contents to the newly created node...
        makeKeyValue(&value, valLen, entries, group2, SPLIT);

        stat = insertBDB(siblingID, value, valLen);
        cleanup(value);

        if (stat == FAIL) return FAIL;
        
        // step 3: Now update parent entry
        makeKeyValue(&value, valLen, newChildID, lcm2, UPDATE);
        DHTStatus stat = insertBDB(pID, value, valLen);
        cleanup(value);
        if (stat == FAIL) return FAIL;

        // step 3.1: Update the original parent entry
        strbuf o;
        o << ID;
        str myID = str(o);
        
        makeKeyValue(&value, valLen, myID, lcm1, REPLACESIG); 
        stat = insertBDB(pID, value, valLen);
        cleanup(value);
        
        if (stat == FAIL) return FAIL;
        

        // step 4: Next delete the moved contents from the original node
        makeKeyValue(&value, valLen, entries, group2, SPLIT);
        
        stat = insertBDB(ID, value, valLen);
        cleanup(value);
        
        if (stat == FAIL) return FAIL;
        
        // step 5: Update the header of the original node
        makeKeyValue(&hdrVal, hdrLen, nodeType, myInt, UPDATEHDR);
        
        stat = insertBDB(ID, hdrVal, hdrLen);
        cleanup(hdrVal);
        if (stat == FAIL) return FAIL;
    }
    else if (stat == NOTFIRST) { // Will await header change
        int waitInterval = 1;
        vec<str> myNodeEntries;
        
        while (1) {
            sleep(waitInterval);
            out = 0;
            nodeEntries.clear();
            retrieveError = false;
            
            //dhash->retrieve(ID, DHASH_NOAUTH, wrap(fetch_cb));
            
            //while (out == 0) acheck();
            myNodeEntries.clear();
            retrieveBDB(ID, myNodeEntries);
            
            if (retrieveError) {
                warnx << "Unable to read node " << ID << "\n";
                return FAIL;
            }
            
            warnx << "Number of entries: " << nodeEntries.size() << "\n";

            str a, b;
            Interval currInt;
           
            warnx << "# of node entries: " << myNodeEntries.size() << "\n";
            
            getKeyValue(myNodeEntries[0], a, b);
            
           
            getInterval(b, currInt);

            // Check the header for change
            if (nodeInt == currInt) {
                waitInterval *= 2;    
            }
            else {
                warnx << "---- Non Root Node Header has changed -----\n";
                break;
            }
            
        }
    }

	return SUCC;
}


// Inserts a document signature into a dynamic signature index.
// Format of each node: <type/interval><key/ptr><key/ptr>...
//
// A few scenarios are taken into consideration --
// a) Two clients (or more) read a full page and decide to split them
// b) A client thinks a page has space and tries to insert, then
//    in the mean time another client/s have already inserted keys and
//    made it full, then the node that stores it can return an error,
//    in which case, that node can be read again and verified and if it is
//    full it will be split!!! so works fine...
// c) if there is space and (a) and (b) are not true, then just insert!!!
//
// Note that we have an assumption that there is no merging and no deletion.
// Once a node is created, it is never deleted.
// Hence once two sets decide to part, they never come close!!!
// This will be exploited to make matters simple!!!
//
// Periodically each peer containing a node should perform the "REFRESH PHASE"
// where it quries it's child nodes and gets their latest lcm() and updates its
// signature.
//
// Semantics:
// (a) UPDATE - compute lcm with the existing key or adds a new one if absent
// (b) SPLIT - if node is full -- then delete the passed elements
//             if node is empty -- then add the passed elements
//             otherwise -- do nothing (do some verification may be)
// (c) UPDATEHDR - updates header -- if the level is at least as large as before and
//                                   the right interval boundary is at least as small
//                                   as before.
DHTStatus insertDocument(str& nodeID, std::vector<POLY>& sig, int docid)
{
	vec<str> myNodeEntries;
	chordID ID;
	str2chordID(nodeID, ID);

    // now fetch the node
	out = 0;
	retrieveError = false;
	
	//dhash->retrieve(ID, DHASH_NOAUTH, wrap(fetch_cb));

	//while (out == 0) acheck();

    retrieveBDB(ID, myNodeEntries);
    
	if (retrieveError) {
		warnx << "Failed to retrieve key during insertion...\n";
		return FAIL;
	}

	
	str a, b;
	Interval nodeInt;
	chordID nodeType;
	warnx << "# of node entries: " << myNodeEntries.size() << "\n";
	
	getKeyValue(myNodeEntries[0], a, b);

	str2chordID(a, nodeType);
	getInterval(b, nodeInt);

	DHTStatus status = FAIL;
	
	if (nodeType == LEAF) {
        warnx << "------------- VISITING Leaf Node --------------\n";
        
		// Full
		if ((int) myNodeEntries.size() > MAXENTRIES) {
            fullNodeIDs.push_back(ID);
            fullNodeInts.push_back(nodeInt);
            status = NODESPLIT;
		}
		else {
            // insert key
            warnx << " +++++ Inserting signature into leaf node +++++\n";
			char *value;
			int valLen;
			char docidstr[128];
			sprintf(docidstr, "%d", docid);
			str dockey(docidstr, strlen(docidstr));
			makeKeyValue(&value, valLen, dockey, sig, UPDATE);

			DHTStatus stat = insertBDB(ID, value, valLen);
			cleanup(value);
			if (stat == FAIL) return FAIL;
			status = SUCC;
		}
	}
	else if (nodeType == NONLEAF || nodeType == ROOT) {
		str childID = pickChild(myNodeEntries, sig);

        if (nodeType == ROOT) {
            warnx << "---------------- VISITING Root Node -----------------\n";
        }
        else {
            warnx << "---------------- VISITING Non Leaf Node -----------------\n";
        }
		assert(childID != "NULL");
		warnx << "===> Child picked for insertion: " << childID << "\n";
		
		status = insertDocument(childID, sig, docid);

        //warnx << "Finished lower level insert...\n";
        
		if (status == FAIL) {
			warnx << "Inserting failed...\n";
			return status;
		}
		else if (status == REINSERT) {
			return status;
		}
		else if (status == NODESPLIT) {
            if (nodeType != ROOT && (int) myNodeEntries.size() > MAXENTRIES) {
                fullNodeIDs.push_back(ID);
                fullNodeInts.push_back(nodeInt);
                //warnx << "Full size: " << fullNodeInts.size() << "\n";
                status = NODESPLIT;
            }
            else {

                // Split ROOT if reqd
                if (nodeType == ROOT && (int) myNodeEntries.size() > MAXENTRIES) {
                    performSplitRoot(ID, nodeInt);
                }
                
                // Go thro the list of full nodes and split it
                // Actually need to store the original headers
                chordID pID = ID;
                for (int i = 0; i < (int) fullNodeIDs.size(); i++) {
                    performSplit(fullNodeIDs[i], fullNodeInts[i], pID);
                    pID = fullNodeIDs[i];
                }
                status = REINSERT; // We will REINSERT from the root for now
            }
        }
        else if (status == SUCC) {

            warnx << "++++++ Updating parent node signature +++++ \n";
            char *value;
			int valLen;
			makeKeyValue(&value, valLen, childID, sig, UPDATEIFPRESENT);
            
			DHTStatus stat = insertBDB(ID, value, valLen);
			cleanup(value);
            
			if (stat == FAIL) return FAIL;
            
			status = SUCC;
        }
    }
	else {
		warnx << "Corrupt node header...\n";
		assert(0);
	}

	return status;
}

// Query processing stage.
void queryProcess(str &nodeID, std::vector<std::vector<POLY> >& listOfSigs)
{
	vec<str> entriesToProcess;
	chordID ID;
	str2chordID(nodeID, ID);

    out = 0;

    retrieveError = false;
	
    double beginRetrieveTime = getgtod();
    //dhash->retrieve(ID, DHASH_NOAUTH, wrap(fetch_cb));
	
    //while (out == 0) acheck();
    entriesToProcess.clear();
    retrieveBDB(ID, entriesToProcess);
    
    if (retrieveError) {
        warnx << "Unable to read node " << ID << "\n";
        return;
    }
    
    double endRetrieveTime = getgtod();
	
    warnx << "Key retrieved: " << ID << "\n";
    std::cout <<" Key retrieve time: "
              << endRetrieveTime - beginRetrieveTime << std::endl;
    
    warnx << "Number of entries: " << entriesToProcess.size() << "\n";
    
	
#ifdef _DEBUG_
	for (int i = 0; i < (int) nodeEntries.size(); i++) {
		dataReadSize += nodeEntries[i].len();
	}

#endif

	str a, b;
	Interval nodeInt;
	chordID nodeType;
	
	getKeyValue(entriesToProcess[0], a, b);

	str2chordID(a, nodeType);
	getInterval(b, nodeInt);
	
	std::vector<POLY> rem;
	std::vector<POLY> entrySig;
	str childKey;

	// Read the node and select those childptrs that are divided by
	// sig...
	for (int i = 1; i < (int) entriesToProcess.size(); i++) {
		entrySig.clear();
		getKeyValue(entriesToProcess[i].cstr(), childKey, entrySig);

		//warnx << "Child key: " << childKey << "\n";
		// Need to test for each signature in the listOfSigs...
		// An implicit OR predicate is assumed...
		int dega, degb;
		dega = getDegree(entrySig);
		//warnx << "Degree of entry: " << dega << "\n";	
#ifdef _DEBUG_
		warnx << "Deg a: " << dega << "\n";
		for (int j = 0; j < (int) entrySig.size(); j++) {
			warnx << entrySig[j] << " ";
		}
		warnx << "\n";
#endif
		
		std::vector<std::vector<POLY> > newListOfSigs;
		bool isDivides = false;
		for (int q = 0; q < (int) listOfSigs.size(); q++) {
			if (q == 0) {
				newListOfSigs = listOfSigs;
			}
			// Test for divisibility!!!
			rem.clear();
			degb = getDegree(listOfSigs[q]);
			//warnx << "Deg b: " << degb << "\n";
			//double beginRemTime = getgtod();
			if (dega >= degb) {
				remainder(rem, entrySig, listOfSigs[q]);
			}

			//double endRemTime = getgtod();
			//std::cout << "Division time: " << endRemTime - beginRemTime << "\n";
			
			if (rem.size() == 1 && rem[0] == (POLY) 0) {
				//warnx << "Break point: " << q << "\n";
				isDivides = true;
				break;
			}

			// Mark as not to be checked again
			newListOfSigs.erase(newListOfSigs.begin());
		}

		// If the division is successful, i.e., remainder = 0
		if (isDivides) {
			if (nodeType == LEAF) {
				numDocs++;
				//warnx << "==== Found match. Docid = " << childKey << " =====\n";
			}
			else {
				queryProcess(childKey, newListOfSigs);
			}
		}
	}

    return;
}


									 
// Compute the number of replicas based on the current level and
// maximum tree depth...
int getNumReplicas(int level, int maxTreeDepth)
{
	return (maxTreeDepth - level);
}

 
