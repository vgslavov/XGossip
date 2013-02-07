// Psi - Peer-to-peer Signature Index. Twig Query processing in P2P Environments.
// Author: Praveen Rao
// University of Arizona/University of Missouri-Kansas City
#define _CS5590LD_

#include "psi.h"
#include <chord.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <dhash_common.h>
#include <dhashclient.h>
#include <dhblock.h>
#include <dhblock_keyhash.h>
#include <sys/time.h>
#include <string>
#include <map>
#include <math.h>
#include <iostream>
#include "poly.h"
#include "../utils/utils.h"
#include <vector>
#include <map>
#include <utility>
#include "nodeparams.h"
#include "cache.h"
#include "retrievetypes.h"

#define _ELIMINATE_DUP_
// Change replicas in dhblock.C
#define _LIVE_  

dhashclient *dhash;
int out;

chordID maxID;
static const char* dhash_sock;
void retrieveDHT(chordID ID, int, str&, chordID guess = 0);
void retrieveDHTClosest(chordID ID, int, str&, chordID guess = 0);

DHTStatus insertDocument(str&, std::vector<POLY>&, char *, int, bool = false);
DHTStatus insertDocumentO(str&, std::vector<POLY>&, char *, int, bool = false);
DHTStatus insertDocumentP(str&, std::vector<POLY>&, char *, int, bool = false);

void splitNode(vec<str>&, std::vector<int>&, std::vector<int>&,
    std::vector<POLY>&);
void splitNodeV(vec<str>&, std::vector<int>&, std::vector<int>&,
    std::vector<POLY>&);

void getKey(str&, str&);
void getKeyValue(str&, str&, str&);
void makeKeyValue(char **, int&, str&, std::vector<POLY>&,
    InsertType);
void makeKeyValue(char **, int&, str&, Interval&,
    InsertType);
void makeKeyValue(char **, int&, vec<str>&, std::vector<int>&,
    InsertType);
DHTStatus insertDHT(chordID, char *, int, int = -1, int = MAXRETRIES, chordID = 0);

DHTStatus performSplit(chordID, Interval&, chordID, bool = false, chordID = 0);

DHTStatus performSplitRoot(chordID, Interval&, bool = false, chordID = 0);

DHTStatus createEmptyIndex(chordID, bool = false);
DHTStatus createEmptyIndexO(chordID, bool = false);

//void queryProcess(std::vector<str>&, std::vector<POLY>&);
//void queryProcess(str&, std::vector<std::vector<POLY> >&, int, int, int);
void queryProcess(str&, std::vector<std::vector<POLY> >&, str&);
void queryProcessO(str&, str&);
void queryProcessV(str &nodeID, std::vector<std::vector<POLY> >& listOfSigs,
    std::vector<POLY>& valList, enum OPTYPE OP, str&);
void queryProcessVO(str &nodeID, str&);

void verifyProcess(char *, std::vector<std::vector<POLY> >&, int);
int getNumReplicas(int, int);

void readValues(FILE *, std::multimap<POLY, std::pair<std::string, enum OPTYPE> >&);

std::string chooseIndex(std::string&, std::multimap<POLY, std::pair<std::string, enum OPTYPE> >&);

void getValArgs(std::multimap<POLY, std::pair<std::string, enum OPTYPE> >&,
    std::vector<POLY>&, enum OPTYPE& OP);
            
DHTStatus insertStatus;
vec<chordID> fullNodeIDs;
std::vector<Interval> fullNodeInts;
vec<chordID> fullNodeGuess;
std::string INDEXNAME;
char REPLICAID[128];
bool CLOSESTREPLICAS;

void pickReplica(char *out, int MAXREPLICAS);

static chordID ROOT, NONLEAF, LEAF;
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

//
dhash_stat retrieveStatus;
str failedNodeId;

// GUESS
chordID myGuess;

// Retrieve TIMEOUT
const int RTIMEOUT = 20;

// Fetch callback for NOAUTH...
void
fetch_cb (dhash_stat stat, ptr<dhash_block> blk, vec<chordID> path)
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
    out++;
}

void
store_cb (dhash_stat status, ptr<insert_info> i)
{

    if (status == DHASH_NOTFIRST) {
        insertStatus = NOTFIRST;
    }
    else if (status == DHASH_FULL) {
        insertStatus = FULL;
    }
    else if (status == DHASH_RETRYINSERT) {
        insertStatus = RETRY;
    }
    else if (status == DHASH_CORRUPTHDR) {
        insertStatus = CORRUPTHDR;
				// Trying again
        //insertError = true;
    }
    else if (status != DHASH_OK) {
        //warnx << "Failed to store key...\n";
        insertError = true;
    }
    else {
        numWrites++;
        //warnx << "Key stored successfully...\n";
    }
    out++;
}


int main(int argc, char *argv[]) {
	
    if(argc < 7) {
        std::cout << "Usage: " << argv[0]
                  << " sockname " // argv[1]
                  << " -[dD,iI,qQO,v,M,S,lL] "  // argv[2]
                  << " signaturefile " // argv[3]
                  << " delay " // argv[4]
                  << " count " // argv[5]
                  << " docidtype [0=string,>0=int] " // argv[6]
                  << " <metric=1|gcd=0> " // argv[7]
                  << " <CLOSESTREPLICAS: 0 or 1> " // argv[8]
                  << " <MAXREPLICAS> " // argv[9]
                  << std::endl;
        exit(1);
    }

    char *cmd = argv[2];
	
    if (!strcmp(cmd, "-v")) {
        dhash = NULL;
    }
    else {
				dhash_sock = argv[1];
        dhash = New dhashclient(dhash_sock);
    }
    chordID ID;

    // pSim metric
    metric = argv[7] ? (bool) atoi(argv[7]) : false;
		
    // Cache size
    cacheSize = 0;
    freeSpace = cacheSize;

    // Initialize node types
    LEAF = compute_hash(LEAFNODE, strlen(LEAFNODE));
    NONLEAF = compute_hash(NONLEAFNODE, strlen(NONLEAFNODE));
    ROOT = compute_hash(ROOTNODE, strlen(ROOTNODE));

	// Closest replica
	CLOSESTREPLICAS = argv[8] ? bool(atoi(argv[8])) : false;

    //MAX replicas
    int MAXREPLICAS = argv[9] ? atoi(argv[9]) : 1;

#ifdef _CS5590LD_
    const char *const CONFIG_FILE = "/home/umkc_rao1/raopr/psix.new/psix.config";
    FILE *fp = fopen(CONFIG_FILE, "r");
    if (fp) {
      char name[128];
      int value;
      while (1) {
        if (fscanf(fp, "%s %d", name, &value) == EOF) break;
        if (strcmp(name, "MAXRETRIEVERETRIES") == 0) {
          MAXRETRIEVERETRIES = value > MAXRETRIEVERETRIES ? value : MAXRETRIEVERETRIES;
          break;
        }
      }
      fclose(fp);
    }
    else {
      warnx << "Error opening " << CONFIG_FILE << "\n";
    }
    warnx << "MAXRETRIEVERETRIES: " << MAXRETRIEVERETRIES << "\n";
#endif

    int maxCount = atoi(argv[5]);
    int randDelay = atoi(argv[4]);
    int docidType = atoi(argv[6]);

    //warnx << "Irreducible polynomial degree: " << irrPolyDegree << "\n";
    warnx << "Fan out: " << MAXENTRIES << "\n";
    warnx << "Rand delay: " << randDelay << "\n";
	
		str temp = "ffffffffffffffffffffffffffffffffffffffff";
		str2chordID(temp, maxID);
		warnx << "MaxID: " << maxID << "\n";

		// Tag list
		std::map<std::string, bool> tagList;

    // Time measurement
    double startTime = getgtod();
    seedForRand = (unsigned int) startTime;
	
    numReads = numWrites = dataReadSize = cacheHits = 0;

    // Print the start time...
    system("date");
    // Compute the root node key for the tree
    chordID rootNodeID;

    if (!strcmp(cmd, "-d")) { // Single index
        // Dump an index that has a root and a single leaf
        INDEXNAME = "";
        std::string root = INDEXNAME + std::string(ROOTNODEID);
        rootNodeID = compute_hash(root.c_str(), root.length());
        warnx << "Root node id for index " << INDEXNAME.c_str() << " : " << rootNodeID << "\n";
  
        DHTStatus stat = createEmptyIndexO(rootNodeID);
        if (stat == FAIL) {
            warnx << "Creation of empty index failed...\n";
        }
    }
    else if (!strcmp(cmd, "-D")) { // Multiple indexes
        std::string elementFile = std::string(argv[3]);

        FILE *fp = fopen(elementFile.c_str(), "r");
        char elementName[128];
        
        if (fp) {
            while (1) {

                if (fscanf(fp, "%s", elementName) == EOF) break;

                std::string eName(elementName);
                
                // Do not create for attributes
                if (eName.find(ATTRSUFFIX, 0) != std::string::npos)
                    continue;
                                
                INDEXNAME = std::string(elementName);
                std::string root = INDEXNAME + std::string(ROOTNODEID);
                rootNodeID = compute_hash(root.c_str(), root.length());
                warnx << "Root node id for index " << INDEXNAME.c_str() << " : " << rootNodeID << "\n";
                
                DHTStatus stat = createEmptyIndexO(rootNodeID);
                if (stat == FAIL) {
                    warnx << "Creation of empty index failed...\n";
                }
                warnx << "Sleeping 10 seconds\n";
                
                sleep (10);
            }

            fclose(fp);
        }
    }
    else if (!strcmp(cmd, "-l")) { // Test for unique keys
        std::string elementFile = std::string(argv[3]);

        FILE *fp = fopen(elementFile.c_str(), "r");
        char elementName[128];
        
        if (fp) {
            while (1) {

                if (fscanf(fp, "%s", elementName) == EOF) break;

                int value[3] = {1,1,1};
                DHTStatus stat = insertDHT(compute_hash(elementName, strlen(elementName)), 
                    (char *) value, sizeof(value), -1, INT_MAX);
                if (stat == FAIL) {
                    warnx << "Insert failed...\n";
                }
                warnx << "Sleeping 20 seconds\n";
                
                sleep (20);
            }

            fclose(fp);
        }
    }
    else if (!strcmp(cmd, "-L")) { // Test for unique keys
        std::string elementFile = std::string(argv[3]);

        FILE *fp = fopen(elementFile.c_str(), "r");
        char elementName[128];
        
        if (fp) {
            while (1) {

                if (fscanf(fp, "%s", elementName) == EOF) break;

                str junk(""); 
                retrieveDHT(compute_hash(elementName, strlen(elementName)), RTIMEOUT, junk);         
                warnx << "Sleeping 10 seconds\n";
                
                sleep (10);
            }

            fclose(fp);
        }
    }
    else if (!strcmp(cmd, "-q")) {
        INDEXNAME = "";
        std::string root = INDEXNAME + std::string(ROOTNODEID);
        rootNodeID = compute_hash(root.c_str(), root.length());
        warnx << "Root node id for index " << INDEXNAME.c_str() << " : " << rootNodeID << "\n";
          
        double totalTime = 0;
        // Cache tree nodes
        nodeCache.clear();
        lruCache.clear();
        // Read the query signatures from a file
        // Read the DHT from root to leaf
        std::string queryFile(argv[3]);
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
			
            warnx << "******* Processing query " << count << " ********\n";
            numReads = numWrites = dataReadSize = cacheHits = 0;
            strbuf s;
            s << rootNodeID;
            str rootID(s);

            warnx << "Number of signatures generated for query: " << listOfSigs.size() << "\n";
            //if ((int) listOfSigs.size() > MAXSIGSPERQUERY) {
            if (0) {
                warnx << "Skipping this query...\n";
            }
            else {
                numDocs = 0;
                listOfVisitedNodes.clear();
				
                double beginQueryTime = getgtod();
                str junk("");
                queryProcess(rootID, listOfSigs, junk);
                
                
                double endQueryTime = getgtod();
                
                std::cerr << "Query time: " << endQueryTime - beginQueryTime << " secs\n";
                totalTime += (endQueryTime - beginQueryTime);
                warnx << "Num docs: " << numDocs << "\n";
            }
			
#ifdef _DEBUG_
			
#endif
            warnx << "Data read in bytes: " << dataFetched << "\n";
            warnx << "Data written in bytes: " << dataStored << "\n";
            warnx << "Number of DHT lookups: " << numReads << "\n";
            //warnx << " num writes: " << numWrites << "\n";
            warnx << "Cache hits: " << cacheHits << "\n";
            warnx << "******** Finished query processing *********\n\n\n";
            count++;

            if (count != maxCount + 1) {
                warnx << "Sleeping...\n";
								//sleep(1);
                //sleep(1 + (int) ((double) randDelay * (rand() / (RAND_MAX + 1.0))));
            }
        }
		
        fclose(qfp);
        std::cout << "Time taken: " << totalTime << " secs" << std::endl;
    }
    else if (!strcmp(cmd, "-Q")) {
                  
        double totalTime = 0;
        // Cache tree nodes
        nodeCache.clear();
        lruCache.clear();
        // Read the query signatures from a file
        // Read the DHT from root to leaf
        std::string queryFile(argv[3]);
        FILE *qfp = fopen(queryFile.c_str(), "r");
        assert(qfp);

        // Open the tags files too...
        std::string tagsFile = queryFile + std::string(TAGFILE);
		
        FILE *fpTags = fopen(tagsFile.c_str(), "r");
        assert(fpTags);

        // Open the tag depth file...
        std::string tagDepth = queryFile + std::string(TAGDEPTH);
        FILE *fpTagDepth = fopen(tagDepth.c_str(), "r");
        assert(fpTagDepth);

        // Open the value file...
        std::string valFile = queryFile + std::string(VALFILE);
        FILE *fpVal = fopen(valFile.c_str(), "r");
        assert(fpVal);

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
			
            warnx << "******* Processing query " << count << " ********\n";
            numReads = numWrites = dataReadSize = cacheHits = 0;

            // Read the distinct tags
            std::vector<std::string> distinctTags;
            distinctTags.clear();

            // Tag depth
            std::vector<std::string> tagDepth;
            tagDepth.clear();
			
            readTags(fpTags, distinctTags);
            readTags(fpTagDepth, tagDepth);

            // Pick the highest depth entry
            int maxDepth = -1;
            int maxDepthId = -1;
            for (int p = 0; p < (int) tagDepth.size(); p++) {
                if (atoi(tagDepth[p].c_str()) > maxDepth) {
                    maxDepth = atoi(tagDepth[p].c_str());
                    maxDepthId = p;
                }
            }
            

            // Read the values
            std::multimap<POLY, std::pair<std::string, enum OPTYPE> > valList;
            valList.clear();
            readValues(fpVal, valList);
            
            std::vector<POLY> valSig;
            valSig.clear();

            enum OPTYPE OP;

            // Choose an index to search
            INDEXNAME = chooseIndex(distinctTags[maxDepthId], valList);

            if (INDEXNAME != distinctTags[maxDepthId]) {
                getValArgs(valList, valSig, OP);
            }
                                                      
            // Pick a replica
            pickReplica(REPLICAID, MAXREPLICAS);

            std::string root = INDEXNAME + std::string(ROOTNODEID) +
                std::string(REPLICAID);
            rootNodeID = compute_hash(root.c_str(), root.length());
            warnx << "Root node id for index " << INDEXNAME.c_str() << " : " << rootNodeID << "\n";

            strbuf s;
            s << rootNodeID;
            str rootID(s);

            warnx << "Number of signatures generated for query: " << listOfSigs.size() << "\n";
            //if ((int) listOfSigs.size() > MAXSIGSPERQUERY) {
            if (0) {
                warnx << "Skipping this query...\n";
            }
            else {
                numDocs = 0;
                listOfVisitedNodes.clear();
				
                double beginQueryTime = getgtod();
                str junk("");
                // If not value do the usual stuff
                valSig.size() == 0 ? queryProcess(rootID, listOfSigs, junk) :
                    queryProcessV(rootID, listOfSigs, valSig, OP, junk);
                
                double endQueryTime = getgtod();
                std::cout << "Query time: " << endQueryTime - beginQueryTime << " secs" << std::endl;
                totalTime += (endQueryTime - beginQueryTime);
                warnx << "Num docs: " << numDocs << "\n";
            }
			
#ifdef _DEBUG_
			
#endif
            warnx << "Data read in bytes: " << dataFetched << "\n";
            warnx << "Data written in bytes: " << dataStored << "\n";
            warnx << "Number of DHT lookups: " << numReads << "\n";
            //warnx << " num writes: " << numWrites << "\n";
            warnx << "Cache hits: " << cacheHits << "\n";
            warnx << "******** Finished query processing *********\n\n\n";
            count++;

            if (count != maxCount + 1) {
                warnx << "Sleeping...\n";
								//sleep(1);
                //sleep(1 + (int) ((double) randDelay * (rand() / (RAND_MAX + 1.0))));
            }
        }
		
        fclose(qfp);
        fclose(fpTags);
        fclose(fpTagDepth);
        std::cout << "Time taken: " << totalTime << " secs" << std::endl;
    }
    else if (!strcmp(cmd, "-O")) {
                  
        double totalTime = 0;
        // Cache tree nodes
        nodeCache.clear();
        lruCache.clear();
        // Read the query signatures from a file
        // Read the DHT from root to leaf
        std::string queryFile(argv[3]);
        FILE *qfp = fopen(queryFile.c_str(), "r");
        assert(qfp);

        // Open the tags files too...
        std::string tagsFile = queryFile + std::string(TAGFILE);
		
        FILE *fpTags = fopen(tagsFile.c_str(), "r");
        assert(fpTags);

        // Open the tag depth file...
        std::string tagDepth = queryFile + std::string(TAGDEPTH);
        FILE *fpTagDepth = fopen(tagDepth.c_str(), "r");
        assert(fpTagDepth);

        // Open the value file...
        std::string valFile = queryFile + std::string(VALFILE);
        FILE *fpVal = fopen(valFile.c_str(), "r");
        assert(fpVal);

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
			
            warnx << "******* Processing query " << count << " ********\n";
            numReads = numWrites = dataReadSize = cacheHits = 0;

            // Read the distinct tags
            std::vector<std::string> distinctTags;
            distinctTags.clear();

            // Tag depth
            std::vector<std::string> tagDepth;
            tagDepth.clear();
			
            readTags(fpTags, distinctTags);
            readTags(fpTagDepth, tagDepth);

            // Pick the highest depth entry
            int maxDepth = -1;
            int maxDepthId = -1;
            for (int p = 0; p < (int) tagDepth.size(); p++) {
                if (atoi(tagDepth[p].c_str()) > maxDepth) {
                    maxDepth = atoi(tagDepth[p].c_str());
                    maxDepthId = p;
                }
            }
            

            // Read the values
            std::multimap<POLY, std::pair<std::string, enum OPTYPE> > valList;
            valList.clear();
            readValues(fpVal, valList);
            
            std::vector<POLY> valSig;
            valSig.clear();

            enum OPTYPE OP;

            // Choose an index to search
            INDEXNAME = chooseIndex(distinctTags[maxDepthId], valList);

            if (INDEXNAME != distinctTags[maxDepthId]) {
                getValArgs(valList, valSig, OP);
            }
                                                      
            // Pick a replica
            pickReplica(REPLICAID, MAXREPLICAS);

            std::string root = INDEXNAME + std::string(ROOTNODEID) +
                std::string(REPLICAID);
            rootNodeID = compute_hash(root.c_str(), root.length());
            warnx << "Root node id for index " << INDEXNAME.c_str() << " : " << rootNodeID << "\n";

            strbuf s;
            s << rootNodeID;
            str rootID(s);

            warnx << "Number of signatures generated for query: " << listOfSigs.size() << "\n";
            //if ((int) listOfSigs.size() > MAXSIGSPERQUERY) {
            if (0) {
                warnx << "Skipping this query...\n";
            }
            else {
                numDocs = 0;
                listOfVisitedNodes.clear();
				
                double beginQueryTime = getgtod();
                str junk("");
                str sigdata("");

								// Text and value has both use DIVISIBLEV
                valSig.size() == 0 ? makeSigData(sigdata, listOfSigs, DIVISIBLE) :
										makeSigData(sigdata, listOfSigs, valSig, OP, DIVISIBLEV);

                warnx << "Size of sigdata: " << sigdata.len() << "\n";
                // If not value do the usual stuff
                valSig.size() == 0 ? queryProcessO(rootID, sigdata) :
                    queryProcessVO(rootID, sigdata);
                
                double endQueryTime = getgtod();
                std::cout << "Query time: " << endQueryTime - beginQueryTime << " secs" << std::endl;
                totalTime += (endQueryTime - beginQueryTime);
                warnx << "Num docs: " << numDocs << "\n";
            }
			
#ifdef _DEBUG_
			
#endif
            warnx << "Data read in bytes: " << dataFetched << "\n";
            warnx << "Data written in bytes: " << dataStored << "\n";
            warnx << "Number of DHT lookups: " << numReads << "\n";
            //warnx << " num writes: " << numWrites << "\n";
            warnx << "Cache hits: " << cacheHits << "\n";
            warnx << "******** Finished query processing *********\n\n\n";
            count++;

            if (count != maxCount + 1) {
                warnx << "Sleeping...\n";
								//sleep(1);
                //sleep(1 + (int) ((double) randDelay * (rand() / (RAND_MAX + 1.0))));
            }
        }
		
        fclose(qfp);
        fclose(fpTags);
        fclose(fpTagDepth);
        std::cout << "Time taken: " << totalTime << " secs" << std::endl;
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
        INDEXNAME = "";
        std::string root = INDEXNAME + std::string(ROOTNODEID);
        rootNodeID = compute_hash(root.c_str(), root.length());
        warnx << "Root node id for index " << INDEXNAME.c_str() << " : " << rootNodeID << "\n";
  
        // Insert new signatures
        std::string dataFile(argv[3]);
        FILE *sigfp = fopen(dataFile.c_str(), "r");
        assert(sigfp);

        int count = 1;
        while (count <= maxCount) {

            // DONT use readData to retrieve signatures from input files...
            // since the size filed uses POLY as a basic unit and not byte...
            // Read numSigs <it should be 1> for data signatures...
            int numSigs;
            if (fread(&numSigs, sizeof(int), 1, sigfp) != 1) {
                break;
            }
            //warnx << "NUM sigs: " << numSigs << "\n";
            assert(numSigs == 1);
			
            int size;
			
            if (fread(&size, sizeof(int), 1, sigfp) != 1) {
                assert(0);
            }

            //warnx << "Signature size: " << size << "\n";

            std::vector<POLY> sig;
            sig.clear();
            POLY e;
            for (int i = 0; i < size; i++) {
                if (fread(&e, sizeof(POLY), 1, sigfp) != 1) {
                    assert(0);
                }
                sig.push_back(e);
            }

            char docId[128];
            // Read the document id
            if (docidType > 0) {
                int id;
                if (fread(&id, sizeof(id), 1, sigfp) != 1) {
                    assert(0);
                }
                sprintf(docId, "%d", id);
            }
            else {
                int len;
                // len of docid string includes \0
                if (fread(&len, sizeof(len), 1, sigfp) != 1) {
                    assert(0);
                }
                
                if (fread(&docId, len, 1, sigfp) != 1) {
                    assert(0);
                }
            }
            
            //warnx << "**********************************************************\n";
            //warnx << "            Inserting document with id " << docId << " \n";
            //warnx << "**********************************************************\n";
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
                
                status = insertDocument(rootID, sig, docId, strlen(docId));

                if (status == REINSERT) {
                    //sleep(5);
                    warnx << "Reinsert in progress..., #slept for 5 seconds...\n";
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
			
            if (count != maxCount + 1) {
                warnx << "Sleeping...\n";
								//sleep(1);
                //sleep(1 + (int) ((double) randDelay * (rand() / (RAND_MAX + 1.0))));
            }
        }
		
        fclose(sigfp);
        warnx << "Data read in bytes: " << dataFetched << "\n";
        warnx << "Data written in bytes: " << dataStored << "\n";
        warnx << "Number of documents inserted: " << count - 1 << "\n";
        //warnx << "Number of full nodes: " << cacheOfFullNodes.size() << "\n";
        double finishTime = getgtod();
		
        std::cerr << "Time taken: " << finishTime - startTime << " secs" << std::endl;
    }
    else if (!strcmp(cmd, "-P")) {
  
        // Insert new signatures
        std::string dataFile(argv[3]);
        FILE *sigfp = fopen(dataFile.c_str(), "r");
        assert(sigfp);

        // Open the tags files too...
        std::string tagsFile = dataFile + std::string(TAGFILE);
		

        FILE *fpTags = fopen(tagsFile.c_str(), "r");
        assert(fpTags);

        int count = 1;
        while (count <= maxCount) {

            // DONT use readData to retrieve signatures from input files...
            // since the size filed uses POLY as a basic unit and not byte...
            // Read numSigs <it should be 1> for data signatures...
            int numSigs;
            if (fread(&numSigs, sizeof(int), 1, sigfp) != 1) {
                break;
            }
            //warnx << "NUM sigs: " << numSigs << "\n";
            assert(numSigs == 1);
			
            int size;
			
            if (fread(&size, sizeof(int), 1, sigfp) != 1) {
                assert(0);
            }

            //warnx << "Signature size: " << size << "\n";

            std::vector<POLY> sig;
            sig.clear();
            POLY e;
            for (int i = 0; i < size; i++) {
                if (fread(&e, sizeof(POLY), 1, sigfp) != 1) {
                    assert(0);
                }
                sig.push_back(e);
            }

            char hostname[64];
            gethostname(hostname, sizeof(hostname));
#ifdef _LIVE_
						strcpy(hostname, "");
#endif

            char docId[128];
            // Read the document id
            if (docidType > 0) {
                int id;
                if (fread(&id, sizeof(id), 1, sigfp) != 1) {
                    assert(0);
                }
                // HACK: add the hostname
                sprintf(docId, "%d.%s#", id, hostname);
            }
            else {
                int len;
                // len of docid string includes \0
                if (fread(&len, sizeof(len), 1, sigfp) != 1) {
                    assert(0);
                }
                
                if (fread(&docId, len, 1, sigfp) != 1) {
                    assert(0);
                }
                // HACK: add the hostname
                strcat(docId, ".");
                strcat(docId, hostname);
                strcat(docId, "#");
            }
			
            //warnx << "**********************************************************\n";
            //warnx << "*            Inserting document " << docId << " ********\n";
            //     warnx << "**********************************************************\n";

            // Read the distinct tags
            std::vector<std::string> distinctTags;
            distinctTags.clear();

            readTags(fpTags, distinctTags);


            // Each H-index based on tagname!
            for (int ts = 0; ts < (int) distinctTags.size(); ts++) {
	
                // Do not insert for attributes
                if (distinctTags[ts].find(ATTRSUFFIX) != std::string::npos) {
                    continue;
                }
	

                INDEXNAME = distinctTags[ts];
	
                // Store replicas for robustness/fault-tolerance
                for (int r = 0; r < MAXREPLICAS; r++) {

                    sprintf(REPLICAID, "%d", r);

                    std::string root = INDEXNAME + std::string(ROOTNODEID) + 
                        std::string(REPLICAID);
                    warnx << "********* H-index chosen for inserting signature:  " << distinctTags[ts].c_str() << " ********** \n";
                    rootNodeID = compute_hash(root.c_str(), root.length());
                    //warnx << "Root node id for index " << INDEXNAME.c_str() << " : " << rootNodeID << "\n";
	  
                    // ROOT node id			
                    strbuf s;
                    s << rootNodeID;
                    str rootID(s);
                    DHTStatus status;
	  
                    //warnx << "ROOT ID: " << rootID << "\n";
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

												if (tagList.find(INDEXNAME) == tagList.end()) {
	   											status = createEmptyIndexO(rootNodeID); 
                          if (status == FAIL) {
                            warnx << "Creation of empty index failed...\n";
                            break;
                          }	
													else if (status == SUCC) {
														tagList[INDEXNAME] = true;
													}
												}
                        status = insertDocumentP(rootID, sig, docId, strlen(docId));
    
                        if (status == REINSERT) {
                            //sleep(5);
                            warnx << "Reinsert in progress... slept for 5 seconds...\n";
                        }
                        else {
                          break;
                        }
                    }
			
                    if (status == SUCC) {
                        warnx << "***** Insert into I-index successful *******\n";
                    }
                    else if (status == FAIL) {
                        warnx << "*+*+*+* Insert FAILED *+*+*+*\n";
                    }
                    else if (status == FULL) {
                        warnx << "*-*-*-* I-index node is full!!!! *-*-*-*\n";
                        break;
                    }
                }
            }
            count++;
			
            if (count != maxCount + 1) {
                warnx << "Sleeping...\n";
								//sleep(1);
                //sleep(1 + (int) ((double) randDelay * (rand() / (RAND_MAX + 1.0))));
            }
        }
		
        fclose(sigfp);
        fclose(fpTags);
        
        warnx << "Data read in bytes: " << dataFetched << "\n";
        warnx << "Data written in bytes: " << dataStored << "\n";
        warnx << "Number of documents inserted: " << count - 1 << "\n";
        //warnx << "Number of full nodes: " << cacheOfFullNodes.size() << "\n";
        double finishTime = getgtod();
        std::cerr << "Time taken to process XML tags: " << finishTime - startTime << " secs " << std::endl;
        startTime = finishTime;	
    }
    if (!strcmp(cmd, "-I") || !strcmp(cmd, "-IS")) {
  
        // Insert new signatures
        std::string dataFile(argv[3]);
        FILE *sigfp = fopen(dataFile.c_str(), "r");
        assert(sigfp);

        // Open the tags files too...
        std::string tagsFile = dataFile + std::string(TAGFILE);
		
        FILE *fpTags = fopen(tagsFile.c_str(), "r");
        assert(fpTags);

        int count = 1;
        while (count <= maxCount) {

            // DONT use readData to retrieve signatures from input files...
            // since the size filed uses POLY as a basic unit and not byte...
            // Read numSigs <it should be 1> for data signatures...
            int numSigs;
            if (fread(&numSigs, sizeof(int), 1, sigfp) != 1) {
                break;
            }
            //warnx << "NUM sigs: " << numSigs << "\n";
            assert(numSigs == 1);
			
            int size;
			
            if (fread(&size, sizeof(int), 1, sigfp) != 1) {
                assert(0);
            }

            //warnx << "Signature size: " << size << "\n";

            std::vector<POLY> sig;
            sig.clear();
            POLY e;
            for (int i = 0; i < size; i++) {
                if (fread(&e, sizeof(POLY), 1, sigfp) != 1) {
                    assert(0);
                }
                sig.push_back(e);
            }

            char hostname[64];
            gethostname(hostname, sizeof(hostname));
#ifdef _LIVE_
						strcpy(hostname, "");
#endif
            char docId[128];
            // Read the document id
            if (docidType > 0) {
                int id;
                if (fread(&id, sizeof(id), 1, sigfp) != 1) {
                    assert(0);
                }
                // HACK: add the hostname
                sprintf(docId, "%d.%s#", id, hostname);
            }
            else {
                int len;
                // len of docid string includes \0
                if (fread(&len, sizeof(len), 1, sigfp) != 1) {
                    assert(0);
                }
                
                if (fread(&docId, len, 1, sigfp) != 1) {
                    assert(0);
                }
                // HACK: add the hostname
                strcat(docId, ".");
                strcat(docId, hostname);
                strcat(docId, "#");
            }
			
            //warnx << "**********************************************************\n";
            //warnx << "*            Inserting document " << docId << " ********\n";
            //     warnx << "**********************************************************\n";

            // Read the distinct tags
            std::vector<std::string> distinctTags;
            distinctTags.clear();

            readTags(fpTags, distinctTags);


            // Each H-index based on tagname!
            for (int ts = 0; ts < (int) distinctTags.size(); ts++) {
	
                // Do not insert for attributes
                if (distinctTags[ts].find(ATTRSUFFIX) != std::string::npos) {
                    continue;
                }
	

                INDEXNAME = distinctTags[ts];
	
                // Store replicas for robustness/fault-tolerance
                for (int r = 0; r < MAXREPLICAS; r++) {

                    sprintf(REPLICAID, "%d", r);

                    std::string root = INDEXNAME + std::string(ROOTNODEID) + 
                        std::string(REPLICAID);
                    warnx << "********* H-index chosen for inserting signature:  " << distinctTags[ts].c_str() << " ********** \n";
                    rootNodeID = compute_hash(root.c_str(), root.length());
                    //warnx << "Root node id for index " << INDEXNAME.c_str() << " : " << rootNodeID << "\n";
	  
                    // ROOT node id			
                    strbuf s;
                    s << rootNodeID;
                    str rootID(s);
                    DHTStatus status;
	  
                    //warnx << "ROOT ID: " << rootID << "\n";
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
												fullNodeGuess.clear();
	    
                        status = insertDocumentO(rootID, sig, docId, strlen(docId));
	    
                        if (status == REINSERT) {
                            //sleep(5);
                            warnx << "Reinsert in progress... slept for 5 seconds...\n";
                        }
                        else if (status == FAIL && retrieveStatus == DHASH_NOENT) {
                            if (failedNodeId == rootID) {
																if (tagList.find(INDEXNAME) == tagList.end()) {
	   															DHTStatus stat = createEmptyIndexO(rootNodeID); 
                          				if (stat == FAIL) {
                            				warnx << "Creation of empty index failed...\n";
                            				break;
																	}
																	else if (stat == SUCC) {
																		tagList[INDEXNAME] = true;
																	}	
                          			}	
																/*
                                DHTStatus stat = createEmptyIndexO(rootNodeID);
                                if (stat == FAIL) {
                                    warnx << "Creation of empty index failed...\n";
                                    break;
                                }*/
																//sleep(2);
                            }
                            else {
                                warnx << "Unable to fetch index node at this time...\n";
                                break;
                            }
                        }
                        else {
                            break;
                        }
                    }
			
                    if (status == SUCC) {
												tagList[INDEXNAME] = true;
                        warnx << "***** Insert into H-index successful *******\n";
                    }
                    else if (status == FAIL) {
                        warnx << "*+*+*+* Insert FAILED *+*+*+*\n";
                    }
                    else if (status == FULL) {
                        warnx << "*-*-*-* H-index node is full!!!! *-*-*-*\n";
                        break;
                    }
                }
            }
            count++;
			
            if (count != maxCount + 1) {
                warnx << "Sleeping...\n";
								//sleep(1);
                //sleep(1 + (int) ((double) randDelay * (rand() / (RAND_MAX + 1.0))));
            }
        }
		
        fclose(sigfp);
        fclose(fpTags);
        
        warnx << "Data read in bytes: " << dataFetched << "\n";
        warnx << "Data written in bytes: " << dataStored << "\n";
        warnx << "Number of documents inserted: " << count - 1 << "\n";
        //warnx << "Number of full nodes: " << cacheOfFullNodes.size() << "\n";
        double finishTime = getgtod();
        std::cerr << "Time taken to process XML tags: " << finishTime - startTime << " secs " << std::endl;
        startTime = finishTime;	
    }
    if (!strcmp(cmd, "-S") || !strcmp(cmd, "-IS")) {
        // Insert new signatures
        std::string dataFile(argv[3]);
        FILE *sigfp = fopen(dataFile.c_str(), "r");
        assert(sigfp);

        std::string textHashFile = dataFile + std::string(TEXTHASHFILE);
        FILE *fpTextHash = fopen(textHashFile.c_str(), "r");

        std::string histFile = dataFile + std::string(HISTFILE);
        FILE *fpHist = fopen(histFile.c_str(), "r");

        assert(fpTextHash && fpHist);
                                                        
            
        int count = 1;
        while (count <= maxCount) {

            // DONT use readData to retrieve signatures from input files...
            // since the size filed uses POLY as a basic unit and not byte...
            // Read numSigs <it should be 1> for data signatures...
            int numSigs;
            if (fread(&numSigs, sizeof(int), 1, sigfp) != 1) {
                break;
            }
            //      warnx << "NUM sigs: " << numSigs << "\n";
            assert(numSigs == 1);
			
            int size;
			
            if (fread(&size, sizeof(int), 1, sigfp) != 1) {
                assert(0);
            }

            //warnx << "Signature size: " << size << "\n";

            std::vector<POLY> sig;
            sig.clear();
            POLY e;
            for (int i = 0; i < size; i++) {
                if (fread(&e, sizeof(POLY), 1, sigfp) != 1) {
                    assert(0);
                }
                sig.push_back(e);
            }

            char hostname[64];
            gethostname(hostname, sizeof(hostname));
#ifdef _LIVE_
						strcpy(hostname, "");
#endif
            char docId[128];
            // Read the document id
            if (docidType > 0) {
                int id;
                if (fread(&id, sizeof(id), 1, sigfp) != 1) {
                    assert(0);
                }
                // HACK: add the hostname
                sprintf(docId, "%d.%s#", id, hostname);
            }
            else {
                int len;
                // len of docid string includes \0
                if (fread(&len, sizeof(len), 1, sigfp) != 1) {
                    assert(0);
                }
                
                if (fread(&docId, len, 1, sigfp) != 1) {
                    assert(0);
                }
                // HACK: add the hostname
                strcat(docId, ".");
                strcat(docId, hostname);
                strcat(docId, "#");
            }

            //warnx << "**********************************************************\n";
            //warnx << "            Inserting values/text in the document with id " << docId << " \n";
            //     warnx << "**********************************************************\n";
            
            // Create a docid that contains the signature and the original docid
            char *myDocid;
            int myDocidLen;
            makeDocid(&myDocid, &myDocidLen, sig, docId);

            // First insert text hash
            // Format
            // #
            // POLY num <POLY>...
            int numPoly;
            fread(&numPoly, sizeof(numPoly), 1, fpTextHash);

            for (int i = 0; i < numPoly; i++) {
                POLY keyPoly;

                fread(&keyPoly, sizeof(keyPoly), 1, fpTextHash);

                int size;
                fread(&size, sizeof(size), 1, fpTextHash);

                POLY temp;
                std::vector<POLY> textHash;
                textHash.clear();
                
                for (int j = 0; j < size; j++) {
                    fread(&temp, sizeof(temp), 1, fpTextHash);

                    textHash.push_back(temp);
                }

                
                // Now use keyPoly as key, and (textHash, signature, docId) as value
                char polyStr[128];
                sprintf(polyStr, "%u", keyPoly);
                INDEXNAME = std::string(polyStr);
	
                for (int r = 0; r < MAXREPLICAS; r++) {
                    sprintf(REPLICAID, "%d", r);
                    std::string root = INDEXNAME + std::string(ROOTNODEID) +
                        std::string(REPLICAID);

                    rootNodeID = compute_hash(root.c_str(), root.length());
                    warnx << "********* H-index chosen for indexing text:  " << INDEXNAME.c_str() << " ********** \n";
                    //warnx << "Root node id for index " << INDEXNAME.c_str() << " : " << rootNodeID << "\n";
	  
                    // ROOT node id			
                    strbuf s;
                    s << rootNodeID;
                    str rootID(s);
                    DHTStatus status;
	  
                    //warnx << "ROOT ID: " << rootID << "\n";
	  
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
												fullNodeGuess.clear();
	    
                        //warnx << "inserting texthash...\n";
	   										// THis insert is similar to regular signature insert, isValue = false
                        status = insertDocumentO(rootID, textHash, myDocid, myDocidLen);
	    
                        if (status == REINSERT) {
                            //sleep(5);
                            warnx << "Reinsert in progress... slept for 5 seconds...\n";
                        }
                        else if (status == FAIL && retrieveStatus == DHASH_NOENT) {
                            if (failedNodeId == rootID) {
																if (tagList.find(INDEXNAME) == tagList.end()) {
	   															DHTStatus stat = createEmptyIndexO(rootNodeID); 
                          				if (stat == FAIL) {
                            				warnx << "Creation of empty index failed...\n";
                            				break;
																	}
																	else if (stat == SUCC) {
																		tagList[INDEXNAME] = true;
																	}	
                          			}	
                                /*DHTStatus stat = createEmptyIndexO(rootNodeID);
                                if (stat == FAIL) {
                                    warnx << "Creation of empty index failed...\n";
                                    break;
                                }*/
																//sleep(2);
                            }
                            else {
                                warnx << "Unable to fetch index node at this time...\n";
                                break;
                            }
                        }
                        else {
                            break;
                        }
                    }
	  
                    if (status == SUCC) {
											  tagList[INDEXNAME] = true;
                        warnx << "***** Indexed text successfully *******\n";
                    }
                    else if (status == FAIL) {
                        warnx << "*+*+*+* Text indexing FAILED *+*+*+*\n";
                    }
                    else if (status == FULL) {
                        warnx << "*-*-*-* Text index: H-Index node is full!!!! *-*-*-*\n";
                    }
                }
            }


            // Next insert value histogram
            // Format
            // #
            // POLY num <int> ...
            fread(&numPoly, sizeof(numPoly), 1, fpHist);

            for (int i = 0; i < numPoly; i++) {
                POLY keyPoly;

                fread(&keyPoly, sizeof(keyPoly), 1, fpHist);

                int size;
                fread(&size, sizeof(size), 1, fpHist);

                // Now use keyPoly as key, and (num-left-right-count, signature, docId) as value
                char polyStr[128];
                sprintf(polyStr, "%u", keyPoly);
                INDEXNAME = std::string(polyStr);
	
                for (int r = 0; r < MAXREPLICAS; r++) {
                    sprintf(REPLICAID, "%d", r);

                    warnx << "********* H-index chosen for indexing values:  " << INDEXNAME.c_str() << " ********** \n";
                    std::string root = INDEXNAME + std::string(ROOTNODEID) + 
                        std::string(REPLICAID);
                    rootNodeID = compute_hash(root.c_str(), root.length());
                    //warnx << "Root node id for index " << INDEXNAME.c_str() << " : " << rootNodeID << "\n";

                    // ROOT node id			
                    strbuf s;
                    s << rootNodeID;
                    str rootID(s);
                    DHTStatus status;

                    //warnx << "ROOT ID: " << rootID << "\n";
#ifdef _DEBUG_
                    for (int l = 0; l < (int) sig.size(); l++) {
                        warnx << sig[l] << " ";
                    }
                    warnx << "\n";
#endif

                    // For each value do an insert!!! expensive but will try to reduce it
                    // using histograms
                    std::vector<POLY> valueItem;

                    // For each value
                    for (int j = 0; j < size; j++) {
                        valueItem.clear();
                        float left, right;
                        int num;
                    
                        fread(&left, sizeof(left), 1, fpHist);
                        fread(&right, sizeof(right), 1, fpHist);
                        fread(&num, sizeof(num), 1, fpHist);
                        // Don't insert empty intervals.
                        if (num == 0) {
                            continue;
                        }
                        // Store as an interval [val,val]
                        valueItem.push_back((POLY) floor(left));
                        valueItem.push_back((POLY) ceil(right));
                    
                        while (1) {

                            // Keep track of full nodes and their headers
                            fullNodeIDs.clear();
                            fullNodeInts.clear();
														fullNodeGuess.clear();

                            //warnx << "inserting interval " << valueItem[0] << "," << valueItem[1] << "\n";
                    				// isValue is true for intervals    
                            status = insertDocumentO(rootID, valueItem, myDocid, myDocidLen, true);

                            if (status == REINSERT) {
                                //sleep(5);
                                warnx << "Reinsert in progress... slept for 5 seconds...\n";
                            }
                            else if (status == FAIL && retrieveStatus == DHASH_NOENT) {
                                if (failedNodeId == rootID) {
																	if (tagList.find(INDEXNAME) == tagList.end()) {
	   																DHTStatus stat = createEmptyIndexO(rootNodeID); 
                          					if (stat == FAIL) {
                            					warnx << "Creation of empty index failed...\n";
                            					break;
																		}
																		else if (stat == SUCC) {
																			tagList[INDEXNAME] = true;
																		}	
                          				}		
                                    /*DHTStatus stat = createEmptyIndexO(rootNodeID, true);
                                    if (stat == FAIL) {
                                        warnx << "Creation of empty index failed...\n";
                                        break;
                                    }*/
																	//sleep(2);
                                }
                                else {
                                    warnx << "Unable to fetch index node at this time...\n";
                                    break;
                                }
                            }
                            else {
                                break;
                            }
                        }
			
                        if (status == SUCC) {
														tagList[INDEXNAME] = true;
                            warnx << "***** Value indexed successfully *******\n";
                        }
                        else if (status == FAIL) {
                            warnx << "*+*+*+* Value indexing FAILED *+*+*+*\n";
                        }
                        else if (status == FULL) {
                            warnx << "*-*-*-* Value indexing: H-Index node is full!!!! *-*-*-*\n";
                        }
                    }
                }
            }

            // Is a combination of sig + docid
            cleanup(myDocid);
            
            count++;
			
            if (count != maxCount +  1) {
                warnx << "Sleeping...\n";
								//sleep(1);
                //sleep(1 + (int) ((double) randDelay * (rand() / (RAND_MAX + 1.0))));
            }
        }

        fclose(sigfp);
        fclose(fpTextHash);
        fclose(fpHist);
        
        warnx << "Data read in bytes: " << dataFetched << "\n";
        warnx << "Data written in bytes: " << dataStored << "\n";
        warnx << "Number of documents inserted: " << count - 1 << "\n";
        //warnx << "Number of full nodes: " << cacheOfFullNodes.size() << "\n";
        double finishTime = getgtod();
		
        std::cerr << "Time taken to process values/text: " << finishTime - startTime << " secs " << std::endl;
    }
    //double finishTime = getgtod();
    // Finish time
    system("date");
    warnx << "-------------------------------------------------- \n";
    warnx << "----------------- TASK COMPLETED ----------------- \n";	
    warnx << "-------------------------------------------------- \n";
    //std::cout << "Time taken: " << finishTime - startTime << std::endl;
    //amain(); Asynchronous interface, requires probably exit()
    // from callbacks. eg. if (inflight == 0) exit(0);
    return 0;
}

// Pick a replica -- Ideally should use network distance
void pickReplica(char *out, int MAXREPLICAS)
{
    int r = randomNumGen(MAXREPLICAS) - 1;
    sprintf(out, "%d", r);
    warnx << "Selected replica id: " << r << "\n";
    return;
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

// vsfgd: moved definition of CompareSig to utils/utils.h

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

#ifdef _ELIMINATE_DUP_
		std::map<std::vector<POLY>, std::vector<int>, CompareSig> uniqueSigList;
		std::map<int, std::map<std::vector<POLY>, std::vector<int>, CompareSig>::const_iterator > duplicateList;
	
    // Sort them based on degree
    for (int i = 0; i < (int) sigList.size(); i++) {
				std::map<std::vector<POLY>, std::vector<int> >::iterator itr = 
					uniqueSigList.find(sigList[i]);
				if (itr != uniqueSigList.end()) {
					itr->second.push_back(i+1);
				}
				else {
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
#else
    // Sort them based on degree
    for (int i = 0; i < (int) sigList.size(); i++) {
        int deg = getDegree(sigList[i]);
        //warnx << "Check degree: " << deg << "\n";
        sortedSigList.insert(std::pair<int, int>(deg, i));
    }
#endif
	
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
        }
        else {
            if (group1.size() <= group2.size()) {
                //lcm(lcm1, sigList[itr->second]);
            		lcm1 = testLCM1;
                group1.push_back(itr->second+1);
            }
            else {
                //lcm(lcm2, sigList[itr->second]);
            		lcm2 = testLCM2;
                group2.push_back(itr->second+1);
            }
        }		
    }

#ifdef _ELIMINATE_DUP_
		int group1Size = group1.size();
		int group2Size = group2.size();
		//warnx << "Group1 size: " << group1Size << "\n";

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

		//warnx << "Group2 size: " << group2Size << "\n";

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
		//warnx << "Sig list size: " << sigList.size() << " Total size: " << group1.size() + group2.size() << "\n";
		// Header is absent - one less
		assert(group1.size() + group2.size() == (size_t) MAXENTRIES);
#endif
    return;
}

// Split a node
void splitNodeV(vec<str>& entries,
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

    int max = -1;
    int seed1 = -1, seed2 = -1;
    std::vector<POLY> myMBR;
            
    // Pick the two that result in the maximum enlargement
    for (int i = 0; i < (int) sigList.size() - 1; i++) {
        for (int j = i; j < (int) sigList.size(); j++) {
            
            myMBR = sigList[i];
            updateMBR(myMBR, sigList[j]);
            int e = myMBR[1] - myMBR[0];
            
            if (e > max) {
                seed1 = i;
                seed2 = j;
            }
            else if (e == max) { // tie break???
                
            }
        }
    }
    
    group1.push_back(seed1+1);
    group2.push_back(seed2+1);

    lcm1 = sigList[seed1];
    lcm2 = sigList[seed2];

    // For the remaining ones...
    for (int i = 0; i < (int) sigList.size(); i++) {
        if (i == seed1 || i == seed2) continue;

        int deg1 = enlargement(lcm1, sigList[i]);
        int deg2 = enlargement(lcm2, sigList[i]);

        if (deg1 > deg2) {
            updateMBR(lcm1, sigList[i]);
            group1.push_back(i+1);
        }
        else if (deg1 < deg2) {
            updateMBR(lcm2, sigList[i]);
            group2.push_back(i+1);
        }
        else {
            if (group1.size() <= group2.size()) {
                updateMBR(lcm1, sigList[i]);
                group1.push_back(i+1);
            }
            else {
                updateMBR(lcm2, sigList[i]);
                group2.push_back(i+1);
            }
        }		
    }

    return;
}

DHTStatus insertDHT(chordID ID, char *value, int valLen, int randomNum,
    int STOPCOUNT, chordID guess)
{
    warnx << "Insert operation on key: " << ID << "\n";
    dataStored += valLen;
    int numTries = 0;
		
		// Make sure the guess is a successor of ID
   	if (guess < ID) {
			if (maxID - ID + guess < ID - guess) {
				// nothing	
			}
			else {
				guess = 0;
			}
		}
		else {
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
				}
				else {
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
    	
        // If UPDATEHDRLOCK is used, then need to handle the error
        //if (insertError && randomNum >= 0) 
        if (0) {
            warnx << "Testing random number in hdr\n";
            while (1) {
                /*nodeEntries.clear();
                // now fetch the node
                out = 0;
                retrieveError = false;
	
                dhash->retrieve(ID, DHASH_NOAUTH, wrap(fetch_cb));
	
                while (out == 0) acheck();
                */
                str junk(""); 
                retrieveDHT(ID, RTIMEOUT, junk);	
                if (retrieveError) {
                    // delete old dhash and create a new one -- to prevent any
                    // unfinished call backs...
                    //delete dhash;
                    //dhash = New dhashclient(dhash_sock);
                    //warnx << "Failed to retrieve key during insertion...\n";
                    // If node not yet created, then retry insertion
                    if (retrieveStatus == DHASH_NOENT) break;
	  
                    continue;
                }
	
                // NOT FIRST, so just do as before
                if (nodeEntries.size() > 1) {
                    //break;
                    insertError = false;
                    insertStatus = NOTFIRST;	
                    return insertStatus;
                }
	
                // Check the random number
                str nodeType, b;
                Interval nodeInt;
	
                getKeyValue(nodeEntries[0], nodeType, b);
	
                getInterval(b, nodeInt);
                // This peer is actually the first
                if (nodeInt.random == randomNum) {
                    warnx << "Random number matched.\n";
                    insertError = false;
                    return SUCC;
                }
	
            }
        }
         
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


// split the root node with only one peer authorized to do it
DHTStatus performSplitRoot(chordID ID, Interval& oldInt, bool isValue, chordID guess)
{
    vec<str> entries;
		//warnx << "Will try to split root...\n";
    
    str junk(""); 
    retrieveDHT(ID, RTIMEOUT, junk);

    if (retrieveError) {
        // delete old dhash and create a new one -- to prevent any
        // unfinished call backs...
        //delete dhash;
        //dhash = New dhashclient(dhash_sock);
        //warnx << "Failed to retrieve key during insertion...\n";
        return FAIL;
    }
    chordID IDGUESS = myGuess;

    // Copy it -- currently no other solution
    entries = nodeEntries;

    // Already split!!! so don't do anything more...
    if ((int) entries.size() <= MAXENTRIES) {
        return FAIL;
    }
    
		//warnx << "Successfully retrieved the full node...\n";
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
		double beginSplitTime = getgtod();	
    isValue ? splitNodeV(entries, group1, group2, lcm1, lcm2) : splitNode(entries, group1, group2, lcm1, lcm2);
		double endSplitTime = getgtod();

		std::cerr << "Finished grouping: " << endSplitTime - beginSplitTime << "\n";

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
    firstNodeInt.random = randomNumGen(RAND_MAX);

    secondNodeInt.ln = 1;
    secondNodeInt.ld = 2;
    secondNodeInt.rn = 1;
    secondNodeInt.rd = 1;
    secondNodeInt.level = nodeInt.level;
    secondNodeInt.random = randomNumGen(RAND_MAX);

    myInt.ln = nodeInt.ln;
    myInt.ld = nodeInt.ld;
    myInt.rn = nodeInt.rn;
    myInt.rd = nodeInt.rd;
    myInt.level = nodeInt.level + 1;
    myInt.random = nodeInt.random;

    // Key - new key from Interval -- includes level information
    // for uniqueness
    char tempBuf[128];
    sprintf(tempBuf, "%s.%s.%d.%d.%d", INDEXNAME.c_str(), REPLICAID, firstNodeInt.level, firstNodeInt.ln, firstNodeInt.ld);
    chordID firstNodeID;
    firstNodeID = compute_hash(tempBuf, strlen(tempBuf));
    strbuf s;
    s << firstNodeID;
    str firstChildID = str(s);
			
    char *value;
    int valLen;

    char *hdrVal;
    int hdrLen;

    strbuf s1, s2;

    // If root was leaf before, then its children should be leaf too
    if (nodeInt.level == 0) {
        s1 << LEAF;
        s2 << ROOT;
    }
    else {
        s1 << NONLEAF;
        s2 << ROOT;
    }
    
    str newNodeType(s1);
    str rootNodeType(s2);

    while (1) {
        makeKeyValue(&hdrVal, hdrLen, newNodeType, firstNodeInt, UPDATEHDRLOCK);
	  
        warnx << "-------------- Performing Root Split ------------------\n";
        // step 1: Create sibling node
	  
        DHTStatus stat = insertDHT(firstNodeID, hdrVal, hdrLen, firstNodeInt.random);
	 			//warnx << "HDRLOCK: " << hdrLen << " " << stat << "\n";
        cleanup(hdrVal);
        if (stat == FAIL || stat == FULL || stat == CORRUPTHDR || stat == RETRY) return FAIL;
	  
        // This peer has authority to finish the split
        if (stat != NOTFIRST) {
            //warnx << "I am first!\n";
            // step 2: Insert contents to the newly created node...
            makeKeyValue(&value, valLen, entries, group1, SPLIT);
	    
            stat = insertDHT(firstNodeID, value, valLen, -1, INT_MAX);
            cleanup(value);
	    
            if (stat == FAIL || stat == FULL || stat == CORRUPTHDR || stat == RETRY) return FAIL;
	    
            // step 3: Create the second node
            sprintf(tempBuf, "%s.%s.%d.%d.%d", INDEXNAME.c_str(), REPLICAID, secondNodeInt.level, secondNodeInt.ln, secondNodeInt.ld);
            chordID secondNodeID;
            secondNodeID = compute_hash(tempBuf, strlen(tempBuf));
	    
            strbuf s2;
            s2 << secondNodeID;
            str secondChildID(s2);
	    
            makeKeyValue(&hdrVal, hdrLen, newNodeType, secondNodeInt, UPDATEHDRLOCK);
	    
            DHTStatus stat = insertDHT(secondNodeID, hdrVal, hdrLen, secondNodeInt.random, INT_MAX);
            cleanup(hdrVal);
	    
            if (stat == FAIL || stat == FULL || stat == CORRUPTHDR || stat == RETRY) return FAIL;
            //assert(stat != NOTFIRST);
            if (stat == NOTFIRST) {
                warnx << "MAY BE TROUBLE\n";
								return REINSERT;
            }
					
            // step 4: Insert contents to the newly created node...
            makeKeyValue(&value, valLen, entries, group2, SPLIT);
	    
            stat = insertDHT(secondNodeID, value, valLen, -1, INT_MAX);
            cleanup(value);
	    
            if (stat == FAIL || stat == FULL || stat == CORRUPTHDR || stat == RETRY) return FAIL;

            // If splitting a ROOT at level = 0, this is required to avoid docids being
            // inserted into the node -- race condition
            if (nodeInt.level == 0) {
                nodeInt.random = -1; 
            	makeKeyValue(&hdrVal, hdrLen, nodeType, nodeInt, UPDATEHDR);
            	stat = insertDHT(ID, hdrVal, hdrLen, -1, INT_MAX, IDGUESS);
            	cleanup(hdrVal);
            	if (stat == FAIL || stat == FULL || stat == CORRUPTHDR || stat == RETRY) return FAIL;
            }

            // step 5: Next delete all the contents from the original node
            std::vector<int> group3 = group1;
            for (int i = 0; i < (int) group2.size(); i++) {
                group3.push_back(group2[i]);
            }
						warnx << "Size of group3: " << group3.size() << "\n";
            makeKeyValueSpecial(&value, valLen, entries, group3, SPLIT);
            //makeKeyValue(&value, valLen, entries, group3, SPLIT);
	    
            stat = insertDHT(ID, value, valLen, -1, INT_MAX, IDGUESS);
            cleanup(value);
	    
            if (stat == FAIL || stat == FULL || stat == CORRUPTHDR || stat == RETRY) return FAIL;
	    
	    
            // step 6: Now add links to the two new children
            makeKeyValue(&value, valLen, firstChildID, lcm1, isValue ? UPDATEVAL : UPDATE);
            stat = insertDHT(ID, value, valLen, -1, INT_MAX, IDGUESS);
            cleanup(value);
            if (stat == FAIL || stat == FULL || stat == CORRUPTHDR || stat == RETRY) return FAIL;
	    
            makeKeyValue(&value, valLen, secondChildID, lcm2, isValue ? UPDATEVAL : UPDATE);
            stat = insertDHT(ID, value, valLen, -1, INT_MAX, IDGUESS);
            cleanup(value);
            if (stat == FAIL || stat == FULL || stat == CORRUPTHDR || stat == RETRY) return FAIL;

            // step 7: Update the header of the original node
            //makeKeyValue(&hdrVal, hdrLen, nodeType, myInt, UPDATEHDR);
            //stat = insertDHT(ID, hdrVal, hdrLen, -1, INT_MAX, IDGUESS);
            //cleanup(hdrVal);
            //if (stat == FAIL || stat == FULL || stat == CORRUPTHDR || stat == RETRY) return FAIL;

						// Step 8: just verify that the header has actually changed!!!!
						vec<str> myNodeEntries;
						while (1) {
            	// step 7: Update the header of the original node
            	makeKeyValue(&hdrVal, hdrLen, nodeType, myInt, UPDATEHDR);
            	stat = insertDHT(ID, hdrVal, hdrLen, -1, INT_MAX, IDGUESS);
            	cleanup(hdrVal);
            	if (stat == FAIL || stat == FULL || stat == CORRUPTHDR || stat == RETRY) return FAIL;
                str junk(HDRONLY); 
                retrieveDHT(ID, RTIMEOUT, junk, IDGUESS);

                if (retrieveError) {
                    // delete old dhash and create a new one -- to prevent any
                    // unfinished call backs...
                    //delete dhash;
                    //dhash = New dhashclient(dhash_sock);
										continue;
                }

                // Copy it -- currently no other solution
                myNodeEntries.clear();
                myNodeEntries = nodeEntries;
	      
                str a, b;
                Interval mynodeInt;
	      
                //warnx << "# of node entries: " << myNodeEntries.size() << "\n";
	      
                getKeyValue(myNodeEntries[0], a, b);
	      
                getInterval(b, mynodeInt);
								//warnx << "MYNODEINT: " << mynodeInt.level << "-" << mynodeInt.ln
							//				<< "-" << mynodeInt.ld << "-" << mynodeInt.rn << "-" << mynodeInt.rd << "-" << mynodeInt.random << "\n";
							//	warnx << "MYINT: " << myInt.level << "-" << myInt.ln
							//				<< "-" << myInt.ld << "-" << myInt.rn << "-" << myInt.rd << "\n";
								if (mynodeInt == myInt) {
									warnx << "Header has really changed...\n";
									break;
								}
								return REINSERT;
								//sleep(2);
								//IDGUESS = 0;
						}
            break;
        }
        else if (stat == NOTFIRST) { // Will await header change
            int waitInterval = 1;
            vec<str> myNodeEntries;
            int attempts = 6;
            double blockingBeginTime = getgtod();
            while (attempts) {
                warnx << "Sleeping " << waitInterval << " seconds..\n";
                sleep(waitInterval);

                str junk(HDRONLY); 
                retrieveDHT(ID, RTIMEOUT, junk, IDGUESS);

                if (retrieveError) {
                    // delete old dhash and create a new one -- to prevent any
                    // unfinished call backs...
                    //delete dhash;
                    //dhash = New dhashclient(dhash_sock);
                    warnx << "Unable to read node " << ID << "\n";
                    return FAIL;
                }

                IDGUESS = myGuess;	      
                //warnx << "Number of entries: " << nodeEntries.size() << "\n";
	      
                // Copy it -- currently no other solution
                myNodeEntries.clear();
                myNodeEntries = nodeEntries;
	      
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
                attempts--;
            }
            double blockingEndTime = getgtod();
            std::cout << "Blocking time: " << blockingEndTime - blockingBeginTime << "\n";
            if (attempts == 0) {
                //continue; // RETRY to be first -- the peer that was first probably failed
                return FAIL;
            }
            else {
                break; // All well, so exit the loop
            }
        }
    }
	
    return NODESPLIT;
}

// Perform non root node splits
DHTStatus performSplit(chordID ID, Interval& oldInt, chordID pID, bool isValue, chordID guess)
{
    vec<str> entries;
		//warnx << "Will try to split nonroot...\n";
	
    str junk(""); 
    retrieveDHT(ID, RTIMEOUT, junk);

    if (retrieveError) {
        // delete old dhash and create a new one -- to prevent any
        // unfinished call backs...
        //delete dhash;
        //dhash = New dhashclient(dhash_sock);
        //warnx << "Failed to retrieve key during insertion...\n";
        return FAIL;
    }

    chordID IDGUESS = myGuess;

    // Copy it -- currently no other solution
    entries = nodeEntries;

    // Already split!!! so don't do anything more...
    if ((int) entries.size() <= MAXENTRIES) {
        return FAIL;
    }
		//warnx << "Successfully retrieved the full node...\n";
    
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
		double beginSplitTime = getgtod();	
    isValue ? splitNodeV(entries, group1, group2, lcm1, lcm2) : splitNode(entries, group1, group2, lcm1, lcm2);
		double endSplitTime = getgtod();

		std::cerr << "Finished grouping: " << endSplitTime - beginSplitTime << "\n";
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
    mySiblingInt.random = randomNumGen(RAND_MAX);
			
    myInt.ln = nodeInt.ln;
    myInt.ld = nodeInt.ld;
    myInt.rn = mySiblingInt.ln;
    myInt.rd = mySiblingInt.ld;
    myInt.level = nodeInt.level;
    myInt.random = nodeInt.random;

    //warnx << "NODE LEVEL: " << nodeInt.level << "\n";
	
    // Key - new key from Interval -- includes level information
    // for uniqueness
    char tempBuf[128];
    sprintf(tempBuf, "%s.%s.%d.%d.%d", INDEXNAME.c_str(), REPLICAID, mySiblingInt.level, mySiblingInt.ln, mySiblingInt.ld);
    chordID siblingID;
    siblingID = compute_hash(tempBuf, strlen(tempBuf));
    strbuf s;
    s << siblingID;
    str newChildID = str(s);
			
    char *value;
    int valLen;

    char *hdrVal;
    int hdrLen;


    //warnx << "Size group1: " << group1.size() << "\n";
    //warnx << "Size group2: " << group2.size() << "\n";
    
    warnx << "------ Performing Non Root Node Splitting -------\n";
    
    while (1) {
        // step 1: Create sibling node
		
        makeKeyValue(&hdrVal, hdrLen, nodeType, mySiblingInt, UPDATEHDRLOCK);
        DHTStatus stat = insertDHT(siblingID, hdrVal, hdrLen, mySiblingInt.random);
        cleanup(hdrVal);

        if (stat == FAIL || stat == FULL || stat == CORRUPTHDR || stat == RETRY) return FAIL;

        // This peer has authority to finish the split
        if (stat != NOTFIRST) {

            //warnx << "I am the first!\n";
        
            // step 2: Insert contents to the newly created node...
            makeKeyValue(&value, valLen, entries, group2, SPLIT);

            stat = insertDHT(siblingID, value, valLen, -1, INT_MAX);
            cleanup(value);

            if (stat == FAIL || stat == FULL || stat == CORRUPTHDR || stat == RETRY) return FAIL;
        
            // step 3: Now update parent entry
            makeKeyValue(&value, valLen, newChildID, lcm2, isValue ? UPDATEVAL : UPDATE);
            DHTStatus stat = insertDHT(pID, value, valLen, -1, INT_MAX);
            cleanup(value);
            if (stat == FAIL || stat == FULL || stat == CORRUPTHDR || stat == RETRY) return FAIL;

            // step 3.1: Update the original parent entry
            strbuf o;
            o << ID;
            str myID = str(o);
        
            makeKeyValue(&value, valLen, myID, lcm1, REPLACESIG); 
            stat = insertDHT(pID, value, valLen, -1, INT_MAX);
            cleanup(value);
        
            if (stat == FAIL || stat == FULL || stat == CORRUPTHDR || stat == RETRY) return FAIL;
        
            // step 4: Next delete the moved contents from the original node
            makeKeyValueSpecial(&value, valLen, entries, group2, SPLIT);
            //makeKeyValue(&value, valLen, entries, group2, SPLIT);
        
            stat = insertDHT(ID, value, valLen, -1, INT_MAX, IDGUESS);
            cleanup(value);
        
            if (stat == FAIL || stat == FULL || stat == CORRUPTHDR || stat == RETRY) return FAIL;
        
            // step 5: Update the header of the original node
            //makeKeyValue(&hdrVal, hdrLen, nodeType, myInt, UPDATEHDR);
            //stat = insertDHT(ID, hdrVal, hdrLen, -1, INT_MAX, IDGUESS);
            //cleanup(hdrVal);
            //if (stat == FAIL || stat == FULL || stat == CORRUPTHDR || stat == RETRY) return FAIL;
						// Step 8: just verify that the header has actually changed!!!!
						vec<str> myNodeEntries;
						while (1) {
            	// step 5: Update the header of the original node
            	makeKeyValue(&hdrVal, hdrLen, nodeType, myInt, UPDATEHDR);
            	stat = insertDHT(ID, hdrVal, hdrLen, -1, INT_MAX, IDGUESS);
            	cleanup(hdrVal);
            	if (stat == FAIL || stat == FULL || stat == CORRUPTHDR || stat == RETRY) return FAIL;
                str junk(HDRONLY); 
                retrieveDHT(ID, RTIMEOUT, junk, IDGUESS);

                if (retrieveError) {
                    // delete old dhash and create a new one -- to prevent any
                    // unfinished call backs...
                    //delete dhash;
                    //dhash = New dhashclient(dhash_sock);
										continue;
                }

                // Copy it -- currently no other solution
                myNodeEntries.clear();
                myNodeEntries = nodeEntries;
	      
                str a, b;
                Interval mynodeInt;
	      
                //warnx << "# of node entries: " << myNodeEntries.size() << "\n";
	      
                getKeyValue(myNodeEntries[0], a, b);
	      
                getInterval(b, mynodeInt);
								//warnx << "MYNODEINT: " << mynodeInt.level << "-" << mynodeInt.ln
							//				<< "-" << mynodeInt.ld << "-" << mynodeInt.rn << "-" << mynodeInt.rd << "\n";
							//	warnx << "MYINT: " << myInt.level << "-" << myInt.ln
							//				<< "-" << myInt.ld << "-" << myInt.rn << "-" << myInt.rd << "\n";
								
								if (mynodeInt == myInt) {
									warnx << "Header has really changed...\n";
									break;
								}
								return REINSERT;
								//sleep(2);
								//IDGUESS = 0;
						}

            break;
        }
        else if (stat == NOTFIRST) { // Will await header change
            int waitInterval = 1;
            vec<str> myNodeEntries;
            int attempts = 6; 
            double blockingBeginTime = getgtod();
            while (attempts) {
                warnx << "Sleeping " << waitInterval << " seconds...\n";
                sleep(waitInterval);
	      
                str junk(HDRONLY); 
                retrieveDHT(ID, RTIMEOUT, junk, IDGUESS);         
                if (retrieveError) {
                    // delete old dhash and create a new one -- to prevent any
                    // unfinished call backs...
                    //delete dhash;
                    //dhash = New dhashclient(dhash_sock);
                    warnx << "Unable to read node " << ID << "\n";
                    return FAIL;
                }
	      
                IDGUESS = myGuess;
                warnx << "Number of entries: " << nodeEntries.size() << "\n";
	      
                // Copy it -- currently no other solution
                myNodeEntries.clear();
                myNodeEntries = nodeEntries;
	      
                str a, b;
                Interval currInt;
	      
                //warnx << "# of node entries: " << myNodeEntries.size() << "\n";
	      
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
                attempts--;    
            }
            double blockingEndTime = getgtod();
            std::cout << "Blocking time: " << blockingEndTime - blockingBeginTime << "\n";
            if (attempts == 0) {
                //continue; // RETRY to be first -- the peer that was first probably failed...
                return FAIL;
            }
            else {
                break; // All well, so exit while loop
            }
        }
    }
    return SUCC;
}

// Retrieve a DHT key
void retrieveDHT(chordID keyID, int MAXWAIT, str& sigdata, chordID guess)
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
		}
		else {
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

// Retrieve a DHT key
void retrieveDHTClosest(chordID keyID, int MAXWAIT, str& sigdata, chordID guess)
{
    if (((keyID >> 32) << 32) == 0) {
        warnx << "Bad node id... Skipping retrieve...\n";
        retrieveError = true;
        return;
    }
 		guess = 0; 
    nodeEntries.clear();
    // now fetch the node
    out = 0;
    retrieveError = false;
  
		double beginTime = getgtod();
    ptr<option_block> options = New refcounted<option_block>();
    options->flags = DHASHCLIENT_ORDER_SUCCESSORS;
		if (guess > 0) {
			options->guess = guess;
			options->guess |= DHASHCLIENT_GUESS_SUPPLIED;
			// testing
		}
    dhash->retrieve(keyID, DHASH_NOAUTH, wrap(fetch_cb), sigdata, options);
    //double retrieveBegin = getgtod();
		int numTries = 0;
    while (out == 0) {
        acheck();
        //double retrieveEnd = getgtod();
        //if (retrieveEnd - retrieveBegin > MAXWAIT) {
        //    retrieveError = true;
        //    break;
        //}
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
DHTStatus insertDocument(str& nodeID, std::vector<POLY>& sig, char *docid, int docidLen,
    bool isValue)
{
    vec<str> myNodeEntries;
    chordID ID;
    str2chordID(nodeID, ID);

    str junk(""); 
    retrieveDHT(ID, RTIMEOUT, junk);

    if (retrieveError) {
        // delete old dhash and create a new one -- to prevent any
        // unfinished call backs...
        //delete dhash;
        //dhash = New dhashclient(dhash_sock);
        failedNodeId = nodeID;
        //warnx << "Failed to retrieve key during insertion...\n";
        return FAIL;
    }

    chordID IDGUESS = myGuess;

    // Copy it -- currently no other solution
    myNodeEntries = nodeEntries;

    str a, b;
    Interval nodeInt;
    chordID nodeType;
    //warnx << "# of node entries: " << myNodeEntries.size() << "\n";
	
    getKeyValue(myNodeEntries[0], a, b);

    str2chordID(a, nodeType);
    getInterval(b, nodeInt);

    DHTStatus status = FAIL;
	
    if (nodeType == LEAF || (nodeType == ROOT && nodeInt.level == 0)) {
        //warnx << "----- H-index name: " << INDEXNAME.c_str() << ", accessing leaf node -----\n";
        
        // Full
        if ((int) myNodeEntries.size() > MAXENTRIES) {

            if (nodeType == LEAF) {
                fullNodeIDs.push_back(ID);
                fullNodeInts.push_back(nodeInt);
                fullNodeGuess.push_back(IDGUESS);
                status = NODESPLIT;
            }
            else if (nodeType == ROOT) {
                // Split ROOT first time
                if (performSplitRoot(ID, nodeInt, isValue, IDGUESS) == FAIL) return FAIL;
                status = REINSERT;
            }
        }
        else {
            // insert key
            //warnx << " +++++ Inserting signature into leaf node +++++\n";
            char *value;
            int valLen;

            str dockey(docid, docidLen);
            makeKeyValue(&value, valLen, dockey, sig, isValue ? UPDATEVAL : UPDATE);
            DHTStatus stat = insertDHT(ID, value, valLen, -1, MAXRETRIES, IDGUESS);
            cleanup(value);
            if (stat != SUCC) return FAIL;
            status = SUCC;
        }
    }
    else if (nodeType == NONLEAF || nodeType == ROOT) {
        
        str childID = isValue ? pickChildV(myNodeEntries, sig) : pickChild(myNodeEntries, sig, metric);
        
        if (childID == "NULL") {
            warnx << "Cannot pick a child. Failing...\n";
            return FAIL;
        }
        if (nodeType == ROOT) {
            //warnx << "------ H-index name: " << INDEXNAME.c_str() << ", accessing root node  ------\n";
        }
        else {
            warnx << "------ H-index name: " << INDEXNAME.c_str() << ", accessing non-leaf node ------\n";
        }
        //assert(childID != "NULL");
        //warnx << "===> Child picked for insertion: " << childID << "\n";
		
        status = insertDocument(childID, sig, docid, docidLen, isValue);

        //warnx << "Finished lower level insert...\n";
        
        if (status == FAIL || status == FULL) {
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
                fullNodeGuess.push_back(IDGUESS);
                //warnx << "Full size: " << fullNodeInts.size() << "\n";
                status = NODESPLIT;
            }
            else {

                // Split ROOT if reqd
                if (nodeType == ROOT && (int) myNodeEntries.size() > MAXENTRIES) {
                    if (performSplitRoot(ID, nodeInt, isValue, IDGUESS) == FAIL) return FAIL;
                }
                
                // Go thro the list of full nodes and split it
                // Actually need to store the original headers
                chordID pID = ID;
                for (int i = 0; i < (int) fullNodeIDs.size(); i++) {
                    if (performSplit(fullNodeIDs[i], fullNodeInts[i], pID, isValue, fullNodeGuess[i]) == FAIL) return FAIL;
                    pID = fullNodeIDs[i];
                }
                status = REINSERT; // We will REINSERT from the root for now
            }
        }
        else if (status == SUCC) {

            //warnx << "++++++ Updating parent node signature +++++ \n";
            char *value;
            int valLen;
            makeKeyValue(&value, valLen, childID, sig, isValue ? UPDATEVALIFPRESENT : UPDATEIFPRESENT);
            
            DHTStatus stat = insertDHT(ID, value, valLen, -1, MAXRETRIES, IDGUESS);
            cleanup(value);
            
            if (stat == FAIL || stat == FULL || stat == CORRUPTHDR || stat == RETRY) return FAIL;
            
            status = SUCC;
        }
    }
    else {
        warnx << "Corrupt node header. Failing...\n";
        return FAIL;
    }

    return status;
}

// Optimized insertDocument
DHTStatus insertDocumentO(str& nodeID, std::vector<POLY>& sig, char *docid, int docidLen,
    bool isValue)
{
    vec<str> myNodeEntries;
    chordID ID;
    str2chordID(nodeID, ID);

    str junk(""); 
    isValue ? makeSigData(junk, sig, PICKCHILDV) : makeSigData(junk, sig, PICKCHILD);
    retrieveDHT(ID, RTIMEOUT, junk);

    if (retrieveError) {
        // delete old dhash and create a new one -- to prevent any
        // unfinished call backs...
        //delete dhash;
        //dhash = New dhashclient(dhash_sock);
        failedNodeId = nodeID;
        //warnx << "Failed to retrieve key during insertion...\n";
        return FAIL;
    }

    chordID IDGUESS = myGuess;

    // Copy it -- currently no other solution
    myNodeEntries = nodeEntries;

    str a, b;
    Interval nodeInt;
    chordID nodeType;
    //warnx << "# of node entries: " << myNodeEntries.size() << "\n";
	
    getKeyValue(myNodeEntries[0], a, b);

    str2chordID(a, nodeType);
    getInterval(b, nodeInt);

    DHTStatus status = FAIL;
    assert(myNodeEntries[1] == FULLNODE || myNodeEntries[1] == NOTFULLNODE);
	
    if (nodeType == LEAF || (nodeType == ROOT && nodeInt.level == 0)) {
        //warnx << "----- H-index name: " << INDEXNAME.c_str() << ", accessing leaf node -----\n";
        
        // Full
        if (myNodeEntries[1] == FULLNODE) {

            if (nodeType == LEAF) {
                fullNodeIDs.push_back(ID);
                fullNodeInts.push_back(nodeInt);
                fullNodeGuess.push_back(IDGUESS);
                status = NODESPLIT;
            }
            else if (nodeType == ROOT) {
                // Split ROOT first time
                if (performSplitRoot(ID, nodeInt, isValue, IDGUESS) == FAIL) return FAIL;
                status = REINSERT;
            }
        }
        else {
            // insert key
            //warnx << " +++++ Inserting signature into leaf node +++++\n";
            char *value;
            int valLen;

            str dockey(docid, docidLen);
            makeKeyValue(&value, valLen, dockey, sig, isValue ? UPDATEVAL : UPDATE);
            DHTStatus stat = insertDHT(ID, value, valLen, -1, MAXRETRIES, IDGUESS);
            cleanup(value);
            if (stat != SUCC) return FAIL;
            status = SUCC;
        }
    }
    else if (nodeType == NONLEAF || nodeType == ROOT) {
       	 
        str childID = myNodeEntries[2];
        
        if (childID == "NULL") {
            warnx << "Cannot pick a child. Failing...\n";
            return FAIL;
        }
        if (nodeType == ROOT) {
            //warnx << "------ H-index name: " << INDEXNAME.c_str() << ", accessing root node  ------\n";
        }
        else {
            warnx << "------ H-index name: " << INDEXNAME.c_str() << ", accessing non-leaf node ------\n";
        }
        //assert(childID != "NULL");
        //warnx << "===> Child picked for insertion: " << childID << "\n";
		
        status = insertDocumentO(childID, sig, docid, docidLen, isValue);

        //warnx << "Finished lower level insert...\n";
        
        if (status == FAIL || status == FULL) {
            warnx << "Inserting failed...\n";
            return status;
        }
        else if (status == REINSERT) {
            return status;
        }
        else if (status == NODESPLIT) {
            if (nodeType != ROOT && myNodeEntries[1] == FULLNODE) {
                fullNodeIDs.push_back(ID);
                fullNodeInts.push_back(nodeInt);
                fullNodeGuess.push_back(IDGUESS);
                //warnx << "Full size: " << fullNodeInts.size() << "\n";
                status = NODESPLIT;
            }
            else {

                // Split ROOT if reqd
                if (nodeType == ROOT && myNodeEntries[1] == FULLNODE) {
                    if (performSplitRoot(ID, nodeInt, isValue, IDGUESS) == FAIL) return FAIL;
                }
                
                // Go thro the list of full nodes and split it
                // Actually need to store the original headers
                chordID pID = ID;
                for (int i = 0; i < (int) fullNodeIDs.size(); i++) {
                    if (performSplit(fullNodeIDs[i], fullNodeInts[i], pID, isValue, fullNodeGuess[i]) == FAIL) return FAIL;
                    pID = fullNodeIDs[i];
                }
                status = REINSERT; // We will REINSERT from the root for now
            }
        }
        else if (status == SUCC) {

            //warnx << "++++++ Updating parent node signature +++++ \n";
            char *value;
            int valLen;
            makeKeyValue(&value, valLen, childID, sig, isValue ? UPDATEVALIFPRESENT : UPDATEIFPRESENT);
            
            DHTStatus stat = insertDHT(ID, value, valLen, -1, MAXRETRIES, IDGUESS);
            cleanup(value);
            
            if (stat == FAIL || stat == FULL || stat == CORRUPTHDR || stat == RETRY) return FAIL;
            
            status = SUCC;
        }
    }
    else {
        warnx << "Corrupt node header. Failing...\n";
        return FAIL;
    }

    return status;
}

// Optimized insertDocument
DHTStatus insertDocumentP(str& nodeID, std::vector<POLY>& sig, char *docid, int docidLen,
    bool isValue)
{
    chordID ID;
    str2chordID(nodeID, ID);

    DHTStatus status = FAIL;
	
    if (1) {
      // insert key
      //warnx << " +++++ Inserting signature into leaf node +++++\n";
      char *value;
      int valLen;

      str dockey(docid, docidLen);
      makeKeyValue(&value, valLen, dockey, sig, isValue ? JUSTAPPEND : JUSTAPPEND);
      DHTStatus stat = insertDHT(ID, value, valLen, -1, MAXRETRIES);
      cleanup(value);

			if (stat == CORRUPTHDR) return CORRUPTHDR;

      if (stat != SUCC) return FAIL;
      status = SUCC;
    }

    return status;
}

// Query processing stage.
void queryProcess(str &nodeID, std::vector<std::vector<POLY> >& listOfSigs, str& sigdata)
{
    vec<str> entriesToProcess;
    chordID ID;
    str2chordID(nodeID, ID);

    bool inCache;
    int pinnedNodeId;

    std::string nId = getString(nodeID);
    if (listOfVisitedNodes.find(nId) != listOfVisitedNodes.end()) {
        return;
    }

    // No cache used
    if (cacheSize == 0) {
        inCache = false;
    }
    else {
        inCache = findInCache(nodeID, pinnedNodeId);
    }
    
    // If not in cache, read from DHT
    if (inCache) {
        //warnx << "In cache...\n";
        cacheHits++;
        // Copy the entries
        for (int i = 0; i < (int) nodeCache[pinnedNodeId].second.size(); i++) {
            entriesToProcess.push_back(nodeCache[pinnedNodeId].second[i]);
        }

        listOfVisitedNodes[nId] = true;
    }
    else {
        double beginRetrieveTime = getgtod();
		
        if (CLOSESTREPLICAS) {
            retrieveDHTClosest(ID, RTIMEOUT, sigdata);
        }
        else {
            retrieveDHT(ID, RTIMEOUT, sigdata);
        }
	
        if (retrieveError) {
            delete dhash;
            dhash = New dhashclient(dhash_sock);
            warnx << "Unable to read node " << ID << "\n";
            return;
        }

        double endRetrieveTime = getgtod();
        listOfVisitedNodes[nId] = true;
		
        warnx << "Key retrieved: " << ID << "\n";
        std::cout <<" Key retrieve time: "
                  << endRetrieveTime - beginRetrieveTime << " secs" << std::endl;
		
        warnx << "Number of entries: " << nodeEntries.size() << "\n";
        entriesToProcess = nodeEntries;
        // If cache is used
        if (cacheSize > 0) {
            // store in cache
            std::vector<str> e;
            for (int i = 0; i < (int) entriesToProcess.size(); i++) {
                e.push_back(entriesToProcess[i]);
            }
            
            bool found = findReplacement(nodeID, e, pinnedNodeId);
            assert(found);
        }
    }
	
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
        //for (int q = 0; q < (int) listOfSigs.size(); q++) {
        for (int q = 0; q < MAXSIGSPERQUERY && q < (int) listOfSigs.size(); q++) {
            if (q == 0) {
                newListOfSigs = listOfSigs;
                // If we have used server side filtering
                if (sigdata.len() > 0) {
                    isDivides = true;
                    break;
                }
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
            if (nodeType == LEAF || (nodeType == ROOT && nodeInt.level == 0)) {
                numDocs++;
#ifdef _LIVE_
                warnx << "[Docid: " << childKey << "]\n";
#endif
            }
            else {
                queryProcess(childKey, newListOfSigs, sigdata);
            }
        }
    }

    // Unpin the page
    unpinNode(pinnedNodeId);
    return;
}

// Query processing stage. Optimized
void queryProcessO(str &nodeID, str& sigdata)
{
    vec<str> entriesToProcess;
    chordID ID;
    str2chordID(nodeID, ID);

    if (1) {
        //double beginRetrieveTime = getgtod();
		
        if (CLOSESTREPLICAS) {
            retrieveDHTClosest(ID, 30, sigdata);
        }
        else {
            retrieveDHT(ID, 30, sigdata);
        }
	
        if (retrieveError) {
            delete dhash;
            dhash = New dhashclient(dhash_sock);
            warnx << "Unable to read node " << ID << "\n";
            return;
        }

        //double endRetrieveTime = getgtod();
		
        //warnx << "Key retrieved: " << ID << "\n";
        //std::cout <<" Key retrieve time: "
        //          << endRetrieveTime - beginRetrieveTime << " secs" << std::endl;
		
        warnx << "Number of entries: " << nodeEntries.size() << "\n";
        entriesToProcess = nodeEntries;
    }
	
#ifdef _DEBUG_
		warnx << "SIZE: " << entriesToProcess.size() << "\n";
    for (int i = 0; i < (int) entriesToProcess.size(); i++) {
        warnx << entriesToProcess[i] << "\n";
    }
#endif

    str a, b;
    Interval nodeInt;
    chordID nodeType;
    getKeyValue(entriesToProcess[0], a, b);

    str2chordID(a, nodeType);
    getInterval(b, nodeInt);
	
    // Read the node and select those childptrs that are divided by
    // sig...
    for (int i = 1; i < (int) entriesToProcess.size(); i++) {
            if (nodeType == LEAF || (nodeType == ROOT && nodeInt.level == 0)) {
                numDocs++;
#ifdef _LIVE_
                warnx << "[Docid: " << entriesToProcess[i] << "]\n";
#endif
            }
            else {
                queryProcessO(entriesToProcess[i], sigdata);
            }
    }

    return;
}

// Query processing stage.
void queryProcessV(str &nodeID, std::vector<std::vector<POLY> >& listOfSigs,
    std::vector<POLY>& valSig, enum OPTYPE OP, str& sigdata)
{
    vec<str> entriesToProcess;
    chordID ID;
    str2chordID(nodeID, ID);

    bool inCache;
    int pinnedNodeId;

    std::string nId = getString(nodeID);
    if (listOfVisitedNodes.find(nId) != listOfVisitedNodes.end()) {
        return;
    }

    // No cache used
    if (cacheSize == 0) {
        inCache = false;
    }
    else {
        inCache = findInCache(nodeID, pinnedNodeId);
    }
    
    // If not in cache, read from DHT
    if (inCache) {
        //warnx << "In cache...\n";
        cacheHits++;
        // Copy the entries
        for (int i = 0; i < (int) nodeCache[pinnedNodeId].second.size(); i++) {
            entriesToProcess.push_back(nodeCache[pinnedNodeId].second[i]);
        }

        listOfVisitedNodes[nId] = true;
    }
    else {
        //double beginRetrieveTime = getgtod();
        str junk(""); 
		if (CLOSESTREPLICAS) {
			retrieveDHTClosest(ID, 30, junk);
		}
		else {
            retrieveDHT(ID, 30, junk);
		}

        if (retrieveError) {
            delete dhash;
            dhash = New dhashclient(dhash_sock);
            warnx << "Unable to read node " << ID << "\n";
            return;
        }

        //double endRetrieveTime = getgtod();
        listOfVisitedNodes[nId] = true;
		
        //warnx << "Key retrieved: " << ID << "\n";
        //std::cout <<" Key retrieve time: "
        //          << endRetrieveTime - beginRetrieveTime << " secs" << std::endl;
		
        warnx << "Number of entries: " << nodeEntries.size() << "\n";
        entriesToProcess = nodeEntries;
        // If cache is used
        if (cacheSize > 0) {
            // store in cache
            std::vector<str> e;
            for (int i = 0; i < (int) entriesToProcess.size(); i++) {
                e.push_back(entriesToProcess[i]);
            }
            
            bool found = findReplacement(nodeID, e, pinnedNodeId);
            assert(found);
        }
    }
	
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
    //std::cout << "VALSIGSIZE: " << valSig.size() << " OPERATOR: " << OP << std::endl;
    for (int i = 1; i < (int) entriesToProcess.size(); i++) {
        entrySig.clear();
        getKeyValue(entriesToProcess[i].cstr(), childKey, entrySig);

        //warnx << "Child key: " << childKey << "\n";
        // Need to test for each signature in the listOfSigs...
        // An implicit OR predicate is assumed...
#ifdef _DEBUG_
        warnx << "Deg a: " << dega << "\n";
        for (int j = 0; j < (int) entrySig.size(); j++) {
            warnx << entrySig[j] << " ";
        }
        warnx << "\n";
#endif
        bool isDivides = false;
        
        if (valSig.size() == 1) {
            if (OP != EQUAL) {
                warnx << "WARNING: there is support only for \"=\" operator.\n";
                return;
            }
            int dega, degb;
            dega = getDegree(entrySig);
            // Test for divisibility!!!
            rem.clear();
            degb = getDegree(valSig);
			
            if (dega >= degb) {
                remainder(rem, entrySig, valSig);
            }
			
            if (rem.size() == 1 && rem[0] == (POLY) 0) {
                isDivides = true;
            }
        }
        else if (valSig.size() == 2) {
            switch (OP) {
            case EQUAL:
                std::cout << "[" << entrySig[0] << "," << entrySig[1] << "]" << std::endl;
                std::cout << "[" << valSig[0] << "," << valSig[1] << "]" << std::endl;
                if (entrySig[0] <= valSig[0] && entrySig[1] >= valSig[1]) {
                    isDivides = true;
                }
                break;
            case GREATER:
                if (entrySig[1] <= valSig[0]) {
                    
                }
                else {
                    isDivides = true;
                }
                break;
            
            case GREATEREQUAL:
                if (entrySig[1] < valSig[0]) {

                }
                else {
                    isDivides = true;
                }
                break;
            case LESS:
                if (entrySig[0] >= valSig[1]) {

                }
                else {
                    isDivides = true;
                }
                break;
            case LESSEQUAL:
                if (entrySig[0] > valSig[1]) {

                }
                else {
                    isDivides = true;
                }
                break;
            }

        }
        
        // If the division is successful, i.e., remainder = 0
        if (isDivides) {
            if (nodeType == LEAF || (nodeType == ROOT && nodeInt.level == 0)) {
                std::vector<POLY> mySig, rem;
                int dega, degb;
                str myDocid;
                getDocid(childKey.cstr(), mySig, myDocid);
                dega = getDegree(mySig);

                for (int q = 0; q < MAXSIGSPERQUERY && q < (int) listOfSigs.size();
                     q++) {
                    // Test for divisibility!!!
                    rem.clear();
                    degb = getDegree(listOfSigs[q]);
                    if (dega >= degb) {
                        remainder(rem, mySig, listOfSigs[q]);
                    }

                    if (rem.size() == 1 && rem[0] == (POLY) 0) {
                        numDocs++;
#ifdef _LIVE_
                        warnx << "[Docid: " << myDocid << "]\n";
#endif
                        break;
                    }
                }
            }
            else {
                queryProcessV(childKey, listOfSigs, valSig, OP, sigdata);
            }
        }
    }

    // Unpin the page
    unpinNode(pinnedNodeId);
    return;
}

// Query processing stage.
void queryProcessVO(str &nodeID, str& sigdata)
{
    vec<str> entriesToProcess;
    chordID ID;
    str2chordID(nodeID, ID);

    if (1) {
        //double beginRetrieveTime = getgtod();
        
				if (CLOSESTREPLICAS) {
					retrieveDHTClosest(ID, 30, sigdata);
				}
				else {
            retrieveDHT(ID, 30, sigdata);
				}

        if (retrieveError) {
            delete dhash;
            dhash = New dhashclient(dhash_sock);
            warnx << "Unable to read node " << ID << "\n";
            return;
        }

        //warnx << "Key retrieved: " << ID << "\n";
        //std::cout <<" Key retrieve time: "
        //          << endRetrieveTime - beginRetrieveTime << " secs" << std::endl;
		
        warnx << "Number of entries: " << nodeEntries.size() << "\n";
        entriesToProcess = nodeEntries;
    }
	
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
	
    // Read the node and select those childptrs that are divided by
    // sig...
    for (int i = 1; i < (int) entriesToProcess.size(); i++) {
            if (nodeType == LEAF || (nodeType == ROOT && nodeInt.level == 0)) {
              numDocs++;
#ifdef _LIVE_
              warnx << "[Docid: " << entriesToProcess[i] << "]\n";
#endif
            }
            else {
                queryProcessVO(entriesToProcess[i], sigdata);
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

 
// Create an empty index
DHTStatus createEmptyIndex(chordID rootNodeID, bool isValue)
{
    warnx << "H-index does not exist -- so creating it.\n";
    Interval leafInt;
    leafInt.ln = 0;
    leafInt.ld = 1;
    leafInt.rn = 1;
    leafInt.rd = 1;
    leafInt.level = 0;
    leafInt.random = randomNumGen(RAND_MAX);

    char tempBuf[128];
    sprintf(tempBuf, "%s.%s.%d.%d.%d", INDEXNAME.c_str(), REPLICAID, leafInt.level, leafInt.ln,
        leafInt.ld);
    chordID leafID;
    leafID = compute_hash(tempBuf, strlen(tempBuf));
		
    char *hdrValLeaf;
    int hdrLenLeaf;

    strbuf lType;
    lType << LEAF;
    str leafType(lType);

    Interval rootInt;
    rootInt.ln = 0;
    rootInt.ld = 1;
    rootInt.rn = 1;
    rootInt.rd = 1;
    rootInt.level = 1;
    rootInt.random = randomNumGen(RAND_MAX);

    strbuf rType;
    rType << ROOT;
    str rootType(rType);
    char *hdrValRoot;
    int hdrLenRoot;
    DHTStatus stat;

    while (1) {
        // ROOT
        makeKeyValue(&hdrValRoot, hdrLenRoot, rootType, rootInt, UPDATEHDRLOCK);
        stat = insertDHT(rootNodeID, hdrValRoot, hdrLenRoot, rootInt.random);
        cleanup(hdrValRoot);
        if (stat == FAIL || stat == FULL || stat == CORRUPTHDR || stat == RETRY) return FAIL;

        // If I am first, then go for it
        if (stat != NOTFIRST) {
            // Leaf node
            makeKeyValue(&hdrValLeaf, hdrLenLeaf, leafType, leafInt, UPDATEHDRLOCK);
            stat = insertDHT(leafID, hdrValLeaf, hdrLenLeaf, leafInt.random, INT_MAX);
            cleanup(hdrValLeaf);
            if (stat == FAIL || stat == FULL || stat == CORRUPTHDR || stat == RETRY) return FAIL;
            //assert(stat != NOTFIRST);
            if (stat == NOTFIRST) {
                warnx << "MAY BE TROUBLE: " << stat << "\n";
            }

            char *value;
            int valLen;
    
            std::vector<POLY> sig;

            if (isValue) {
                sig.push_back(0x0);
                sig.push_back(0x0); // Interval [0,0]
            }
            else {
                sig.push_back(0x1);
            }
            strbuf sBuf;
            sBuf << leafID;
            str leafIDStr(sBuf);
	
            makeKeyValue(&value, valLen, leafIDStr, sig, isValue ? UPDATEVAL : UPDATE);
    
            // Root node
            stat = insertDHT(rootNodeID, value, valLen, -1, INT_MAX);
            cleanup(value);
    
            if (stat == FAIL || stat == FULL || stat == CORRUPTHDR || stat == RETRY) return FAIL;

            break;
        }
        else {
            int waitInterval = 1;

            int attempts = 6; 
            double blockingBeginTime = getgtod();
            while (attempts) {
                warnx << "Sleeping " << waitInterval << " seconds...\n";
                sleep(waitInterval);
                /*out = 0;
                  nodeEntries.clear();
                  retrieveError = false;
            
                  dhash->retrieve(rootNodeID, DHASH_NOAUTH, wrap(fetch_cb));
            
                  while (out == 0) acheck();
                */
                str junk(""); 
                retrieveDHT(rootNodeID, RTIMEOUT, junk);         
                if (retrieveError) {
                    // delete old dhash and create a new one -- to prevent any
                    // unfinished call backs...
                    //delete dhash;
                    //dhash = New dhashclient(dhash_sock);
                    warnx << "Unable to read node " << rootNodeID << "\n";
                    return FAIL;
                }
            
                warnx << "Number of entries: " << nodeEntries.size() << "\n";

                // Check if there is more than one entry -- basically a link to
                // leaf... then break...
                if (nodeEntries.size() < 2) {
                    waitInterval *= 2;    
                }
                else {
                    warnx << "---- Link to leaf has been added -----\n";
                    break;
                }
                attempts--;    
            }
            double blockingEndTime = getgtod();
            std::cout << "Blocking time: " << blockingEndTime - blockingBeginTime << "\n";
            if (attempts == 0) {
                continue; // RETRY to be first -- the peer that was first probably failed...
            }
            else {
                break; // All well, so exit while loop
            }
        }
    }

    return SUCC;
}

// Create an empty index
DHTStatus createEmptyIndexO(chordID rootNodeID, bool isValue)
{
    warnx << "H-index does not exist -- so creating it.\n";
    
    Interval rootInt;
    rootInt.ln = 0;
    rootInt.ld = 1;
    rootInt.rn = 1;
    rootInt.rd = 1;
    rootInt.level = 0;
    rootInt.random = randomNumGen(RAND_MAX);

    strbuf rType;
    rType << ROOT;
    str rootType(rType);
    char *hdrValRoot;
    int hdrLenRoot;
    DHTStatus stat;

    while (1) {
        // ROOT
        makeKeyValue(&hdrValRoot, hdrLenRoot, rootType, rootInt, UPDATEHDRLOCK);
        stat = insertDHT(rootNodeID, hdrValRoot, hdrLenRoot, rootInt.random);
        cleanup(hdrValRoot);
        if (stat == FAIL || stat == FULL || stat == CORRUPTHDR || stat == RETRY) return FAIL;

        // If I am first, then go for it
        if (stat != NOTFIRST) {
            
            // All done
            break;
        }
        else {
            int waitInterval = 1;

            int attempts = 6; 
            double blockingBeginTime = getgtod();
            while (attempts) {
                warnx << "Sleeping " << waitInterval << " seconds...\n";
                sleep(waitInterval);
                str junk(""); 
                retrieveDHT(rootNodeID, RTIMEOUT, junk);         
                if (retrieveError) {
                    // delete old dhash and create a new one -- to prevent any
                    // unfinished call backs...
                    //delete dhash;
                    //dhash = New dhashclient(dhash_sock);
                    warnx << "Unable to read node " << rootNodeID << "\n";
                    return FAIL;
                }
            
                warnx << "Number of entries: " << nodeEntries.size() << "\n";

                // Check if the header has been added
                // then break...
                if (nodeEntries.size() < 1) {
                    waitInterval *= 2;    
                }
                else {
                    warnx << "---- Link to leaf has been added -----\n";
                    break;
                }
                attempts--;    
            }
            double blockingEndTime = getgtod();
            std::cout << "Blocking time: " << blockingEndTime - blockingBeginTime << "\n";
            if (attempts == 0) {
                continue; // RETRY to be first -- the peer that was first probably failed...
            }
            else {
                break; // All well, so exit while loop
            }
        }
    }

    return SUCC;
}

void readValues(FILE *fp, std::multimap<POLY, std::pair<std::string, enum OPTYPE> >&valList)
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

std::string chooseIndex(std::string& tagName,
    std::multimap<POLY, std::pair<std::string, enum OPTYPE> >& valList)
{
    if (valList.size() == 0)
        return tagName;
    else {
        char buf[128];
        sprintf(buf, "%d", valList.begin()->first);
        return std::string(buf);
    }
}

// POLY, (val string, OP)
void getValArgs(std::multimap<POLY, std::pair<std::string, enum OPTYPE> >& valList,
    std::vector<POLY>& valSig, enum OPTYPE& OP)
{

    OP = valList.begin()->second.second;
    // if INT
    if (isStrValue(valList.begin()->second.first)) {
        valSig.push_back(atoi(valList.begin()->second.first.c_str()));
        valSig.push_back(atoi(valList.begin()->second.first.c_str()));
    }
    else {
        char *ptr = const_cast<char *>(valList.begin()->second.first.c_str());
        int len = valList.begin()->second.first.length();
        POLY m = findMod(ptr, len, valList.begin()->first);
        valSig.push_back(m);
    }
}
