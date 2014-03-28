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
#include <float.h>
#include <math.h>

#include <dirent.h>
#include <libgen.h>
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

static char rcsid[] = "v1.135";
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
chordID thisID;
static const char *dsock;
static const char *gsock;
static char *hashfile;
static char *killfile;
static char *idsfile;
static char *irrpolyfile;
static char *initfile;
static char *logfile;
static char *rootdir = "empty";
int myhostnum = -1;
std::vector<chordID> allIDs;
std::vector<POLY> irrnums;
std::vector<int> hasha;
std::vector<int> hashb;
std::vector<POLY> gproxysig;
double proxyminsim = 0;
double proxyavgsim = 0;
int lshseed = 0;
int plist = 0;
int gflag = 0;
int Qflag = 0;
int uflag = 0;
int xpathqueryid = -1;
int peers = 0;
int teamsize = 5;
// TODO: make cmd line arg
int ninstances = 1;
double errorlimit = 100;
int freqbyx = 1;
int totalrxmsglen = 0;
int totaltxmsglen = 0;
int roundrxmsglen = 0;
int roundtxmsglen = 0;
bool initphase = false;
bool bcast = false;
bool vanilla = false;
bool gossipdone = false;
bool churn = false;
bool killme = false;
bool compress = false;

// by default, accept msgs from any round
// (different from consuming failed inserts!)
bool accept_anyround = true;
// by default, consume msgs which failed to send
bool consume_failed_msgs = true;

// paper:k, slides:bands
int mgroups = 10;
// paper:l, slides:rows
int lfuncs = 16;

// sig, id
typedef std::multimap<std::vector<POLY>, int, CompareSig> sig2idmulti;
sig2idmulti queryMap;

// sig, ids
//typedef std::map<std::vector<POLY>, std::vector<int>, CompareSig> sig2ids;
//sig2ids querysig2qids;
//sig2ids sig2qids;

// id, sigs
typedef std::map<int, std::vector<std::vector<POLY> > > id2sigs;
id2sigs qid2querysig;
id2sigs qid2sigs;

// id, chordIDs
typedef std::map<int, std::vector<chordID> > id2chordIDs;
id2chordIDs qid2teamIDs;

// id, string
typedef std::map<int, std::vector<std::string> > id2strings;
id2strings qid2dtd;
id2strings qid2txt;

// strings, sig
typedef std::map<std::string, std::vector<std::vector<POLY> > > string2sigs;
string2sigs dtd2sigs;
string2sigs dtd2proxysigs;

// sig, (<freq,weight> | <avg,avg,...>)
typedef std::map<std::vector<POLY>, std::vector<double>, CompareSig> mapType;
// sig, (<freq,weight> | <avg,avg,...>)
typedef std::map<std::vector<POLY>, std::vector<long double>, CompareSig> lmapType;

typedef std::vector<mapType> vecomap;
typedef std::vector<lmapType> lvecomap;
// use only for storing the sigs after reading them from files:
// - during init phases
// - when querying
// store in:
// [0]: regular sigs
// [1]: proxy sigs
vecomap allT;
lvecomap lallT;

// list of sig2avg results
vecomap allsig2avg;

// map id of sig2avg to qid
typedef std::map<int, int> id2id;
id2id qid2id;

// vector of vector of maps for each team
// init phase uses 1st vecomap only
typedef std::vector<vecomap> totalvecomap;
totalvecomap totalT;

// map teamID to team's list index in totalT
// (store index in [0], use more only for Mflag with multiple init files)
typedef std::map<chordID, std::vector<int> > teamid2totalT;
teamid2totalT teamindex;

typedef std::map<chordID, int> teamidfreq;
teamidfreq teamidminhash;

// map chordIDs/POLYs to sig indices
typedef std::map<chordID, std::vector<int> > chordID2sig;
typedef std::map<POLY, std::vector<int> > poly2sig;

void acceptconn(int);
void addqid(int, std::vector<POLY>);
bool addteamID(int, chordID);
void add2querymap(std::vector<std::vector<POLY> >, std::vector<std::string>, std::vector<std::string>);
// XGossip
void add2vecomapx(std::vector<std::vector <POLY> >, std::vector<double>, std::vector<double>, chordID);
// VanillaXGossip
void add2vecomapv(std::vector<std::vector <POLY> >, std::vector<double>, std::vector<double>);
int getdir(std::string, std::vector<std::string>&);
void calcfreq(std::vector<std::vector <POLY> >, bool, bool);
void lcalcfreq(std::vector<std::vector <POLY> >, bool, bool);
void calcfreqM(std::vector<std::vector <POLY> >, std::vector<double>, std::vector<double>);
void calcteamids(std::vector <chordID>);
void delspecial(int);
void dividefreq(vecomap&, int, int, int);
void doublefreqgroup(int, mapType);
chordID findteamid(int);
bool fixnan(double, double);
void informteam(chordID, chordID, std::vector<POLY>);
void initgossiprecv(chordID, chordID, std::vector<POLY>, double, double);
DHTStatus insertDHT(chordID, char *, int, double &, int = MAXRETRIES, chordID = 0);
std::string itostring(int);
void retrieveDHT(chordID ID, int, str&, chordID guess = 0);
std::vector<POLY> inverse(std::vector<POLY>);
void listengossip(void);
//void *listengossip(void *);
void loginitstate(FILE *);
int loadinitstate(FILE *);
int loadresults(FILE *, InsertType &);
int lshsig(vecomap, int, InsertType, int, int);
int lshquery(sig2idmulti, int, int, InsertType, int);
int vanillaquery(sig2idmulti, int, InsertType, int);
int make_team(chordID, chordID, std::vector<chordID> &);
void mergelists(vecomap &);
void mergelists_nogossip(vecomap &);
void mergeinit();
void mergebyteamid();
void multiplyfreq(vecomap&, int, int, int);
char *netstring_decode(FILE *);
void netstring_encode(const char *, FILE *);
void printdouble(std::string, double);
void lprintdouble(std::string, long double);
int printlist(vecomap, int, int, bool hdr = true);
int lprintlist(lvecomap, int, int, bool hdr = true);
int printlistall(int seq = -1);
int lprintlistall(int seq = -1);
void printteamids();
void printteamidfreq();
chordID randomID(void);
void readgossip(int);
void readquery(std::string, std::vector<std::vector <POLY> > &, std::vector<std::string> &);
void readsig(std::string, std::vector<std::vector <POLY> > &, bool);
void readValues(FILE *, std::multimap<POLY, std::pair<std::string, enum OPTYPE> > &);
std::vector<chordID> run_lsh(std::vector<POLY>);
int run_query(int, std::vector<POLY>, int &, double &, double &, double &, InsertType, chordID = 0, bool = false);
bool sig2str(std::vector<POLY>, str &);
bool string2sig(std::string, std::vector<POLY> &);
bool sigcmp(std::vector<POLY>, std::vector<POLY>);
bool samesig(std::vector<POLY>, std::vector<POLY>);
int set_inter(std::vector<POLY>, std::vector<POLY>, bool &);
int set_inter(std::vector<chordID>, std::vector<chordID>, bool &);
int set_inter_noskip(std::vector<POLY>, std::vector<POLY>, bool &);
int set_uni(std::vector<POLY>, std::vector<POLY>, bool &);
double signcmp(std::vector<POLY>, std::vector<POLY>, bool &);
void tokenize(const std::string &, std::vector<std::string> &, const std::string &);
void usage(void);
bool validsig(std::vector<POLY>);
void vecomap2vec(vecomap, int, std::vector<std::vector <POLY> > &, std::vector<double> &, std::vector<double> &);
void lvecomap2vec(lvecomap, int, std::vector<std::vector <POLY> > &, std::vector<long double> &, std::vector<long double> &);

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
std::vector<int> rxqseq;
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

int
lshquery(sig2idmulti queryMap, int queryid, int intval = -1, InsertType msgtype = INVALID, int col = 0)
{
    std::vector<std::vector<chordID> > matrix;
    std::vector<chordID> minhash;
    std::vector<chordID> buckets;
    std::vector<POLY> querysig;
    std::vector<POLY> lshquerysig;
    std::vector<POLY> regularsig;
    std::vector<POLY> lshregularsig;
    std::string dtd;
    lsh *myLSH = NULL;
    chordID ID;
    DHTStatus status;
    std::vector<chordID> team;
    chordID teamID;
    char *value;
    int valLen, n, range, randcol, qid;
    double beginTime, endTime, instime, totalinstime;
    bool querysigfound = false;
    str sigbuf;

    matrix.clear();
    minhash.clear();
    querysig.clear();
    lshquerysig.clear();
    regularsig.clear();
    lshregularsig.clear();
    n = 0;
    totalinstime = 0;
    for (sig2idmulti::iterator itr = queryMap.begin(); itr != queryMap.end(); itr++) {
        querysig = itr->first;
        qid = itr->second;

        if (queryid == qid) {
            querysigfound = true;
        } else if (queryid != -1) {
            // don't send the query, still looking for correct qid
            continue;
        }

        id2strings::iterator ditr = qid2dtd.find(qid);
        if (ditr != qid2dtd.end()) {
            // TODO: assume 1 dtd per qid
            dtd = ditr->second.back();
        } else {
            warnx << "lshquery: no DTD found for qid: " << qid << "\n";
        }

        // use querysig or regularsig to generate teamIDs
        if (msgtype == QUERYX || msgtype == QUERYS) {
            sig2str(querysig, sigbuf);
            warnx << "running LSH on querysig: " << sigbuf << "\n";
            beginTime = getgtod();    
            myLSH = new lsh(querysig.size(), lfuncs, mgroups, lshseed, col, irrnums, hasha, hashb);
            endTime = getgtod();    
            printdouble("lshquery: lsh time: ", endTime - beginTime);
            warnx << "\n";

            // convert multiset to set
            if (uflag == 1) {
                //sig2str(querysig, sigbuf);
                //warnx << "multiset: " << sigbuf << "\n";
                lshquerysig = myLSH->getUniqueSet(querysig);
                //sig2str(lshquerysig, sigbuf);
                //warnx << "set: " << sigbuf << "\n";
                minhash = myLSH->getHashCode(lshquerysig);
            } else {
                minhash = myLSH->getHashCode(querysig);
            }

            sort(minhash.begin(), minhash.end());

            /*
            team.clear();
            for (int i = 0; i < (int)minhash.size(); i++) {
                teamID = minhash[i];
                warnx << "teamID: " << teamID << "\n";
                // TODO: check return status
                make_team(NULL, teamID, team);
                range = team.size();
                // randomness verified
                //randcol = randomNumGenZ(range-1);
                randcol = randomNumGenZ(range);
                ID = team[randcol];
                warnx << "ID in randcol " << randcol << ": " << ID << "\n";
            }
            */
        // use proxy sig to generate teamIDs
        } else if (msgtype == QUERYXP) {
            string2sigs::iterator sitr = dtd2proxysigs.find(dtd);
            if (sitr != dtd2proxysigs.end()) {
                // TODO: assume 1 sig per DTD
                regularsig = sitr->second.back();
            } else {
                warnx << "lshquery: no proxy sig found for DTD: " << dtd.c_str() << "\n";
            }

            sig2str(regularsig, sigbuf);
            warnx << "running LSH on regularsig: " << sigbuf << "\n";
            beginTime = getgtod();    
            myLSH = new lsh(regularsig.size(), lfuncs, mgroups, lshseed, col, irrnums, hasha, hashb);
            endTime = getgtod();    
            printdouble("lshquery: lsh time: ", endTime - beginTime);
            warnx << "\n";

            // convert multiset to set
            if (uflag == 1) {
                //sig2str(regularsig, sigbuf);
                //warnx << "multiset: " << sigbuf << "\n";
                lshregularsig = myLSH->getUniqueSet(regularsig);
                //sig2str(lshregularsig, sigbuf);
                //warnx << "set: " << sigbuf << "\n";
                minhash = myLSH->getHashCode(lshregularsig);
            } else {
                minhash = myLSH->getHashCode(regularsig);
            }

            sort(minhash.begin(), minhash.end());
        }

        //warnx << "minhash.size(): " << minhash.size() << "\n";
        /*
        if (plist == 1) {
            warnx << "sig" << n << ": ";
            warnx << "minhash IDs: " << "\n";
            for (int i = 0; i < (int)minhash.size(); i++) {
                warnx << minhash[i] << "\n";
            }
        }
        */

        //calcteamids(minhash);

        if ((Qflag == 1 || gflag == 1) && msgtype != INVALID) {
            team.clear();
            beginTime = getgtod();    
            for (int i = 0; i < (int)minhash.size(); i++) {
                teamID = minhash[i];
                warnx << "teamID: " << teamID << "\n";
                // TODO: check return status
                make_team(NULL, teamID, team);
                range = team.size();
                // randomness verified
                //randcol = randomNumGenZ(range-1);
                randcol = randomNumGenZ(range);
                ID = team[randcol];
                warnx << "ID in randcol " << randcol << ": " << ID << "\n";
                strbuf t;
                strbuf p;
                t << ID;
                p << team[0];
                str key(t);
                str strteamID(p);
                str dtdstr(dtd.c_str());
                warnx << "inserting " << msgtype << ":\n";
                makeKeyValue(&value, valLen, key, strteamID, dtdstr, querysig, qid, msgtype);
                totaltxmsglen += valLen;
                status = insertDHT(ID, value, valLen, instime, MAXRETRIES);
                totalinstime += instime;
                cleanup(value);

                // do not exit if insert FAILs!
                if (status != SUCC) {
                    // TODO: do I care?
                    warnx << "error: insert FAILed\n";
                } else {
                    warnx << "insert SUCCeeded\n";
                }
            }

            // don't forget to clear team list!
            team.clear();
            endTime = getgtod();
            printdouble("lshquery: insert time (only): ", totalinstime);
            totalinstime = 0;
            warnx << "\n";
            printdouble("lshquery: insert time (+others): ", endTime - beginTime);
            warnx << "\n";

            warnx << "sleeping (lshquery)...\n";
            initsleep = 0;
            delaycb(intval, 0, wrap(initsleepnow));
            while (initsleep == 0) acheck();
        }
        // needed?
        minhash.clear();
        delete myLSH;
        ++n;

        // done, sent the single qid specified
        if (querysigfound == true)
            break;
    }
    return 0;
}

int
vanillaquery(sig2idmulti queryMap, int intval = -1, InsertType msgtype = INVALID, int col = 0)
{
    std::vector<POLY> querysig;
    std::string dtd;
    chordID ID;
    DHTStatus status;
    char *value;
    int valLen, n, qid;
    double beginTime, endTime, instime, totalinstime;
    str sigbuf;

    querysig.clear();
    n = 0;
    totalinstime = 0;
    for (sig2idmulti::iterator itr = queryMap.begin(); itr != queryMap.end(); itr++) {
        querysig = itr->first;
        qid = itr->second;
        id2strings::iterator ditr = qid2dtd.find(qid);
        if (ditr != qid2dtd.end()) {
            // TODO: assume 1 dtd per qid
            dtd = ditr->second.back();
        } else {
            warnx << "vanillaquery: no DTD found for qid: " << qid << "\n";
        }

        if ((Qflag == 1 || gflag == 1) && msgtype != INVALID) {
            beginTime = getgtod();    
            // send to random peer
            ID = make_randomID();
            strbuf z;
            z << ID;
            str key(z);
            str dtdstr(dtd.c_str());
            warnx << "inserting " << msgtype << ":\n";
            // teamID doesn't matter in Vanilla
            makeKeyValue(&value, valLen, key, key, dtdstr, querysig, qid, msgtype);
            totaltxmsglen += valLen;
            status = insertDHT(ID, value, valLen, instime, MAXRETRIES);
            totalinstime += instime;
            cleanup(value);

            // do not exit if insert FAILs!
            if (status != SUCC) {
                // TODO: do I care?
                warnx << "error: insert FAILed\n";
            } else {
                warnx << "insert SUCCeeded\n";
            }

            endTime = getgtod();
            printdouble("vanillaquery: insert time (only): ", totalinstime);
            totalinstime = 0;
            warnx << "\n";
            printdouble("vanillaquery: insert time (+others): ", endTime - beginTime);
            warnx << "\n";

            warnx << "sleeping (vanillaquery)...\n";
            initsleep = 0;
            delaycb(intval, 0, wrap(initsleepnow));
            while (initsleep == 0) acheck();
        }
        ++n;
    }
    return 0;
}

// TODO: verify
int
lshsig(vecomap teamvecomap, int intval = -1, InsertType msgtype = INVALID, int listnum = 0, int col = 0)
{
    std::vector<std::vector<chordID> > matrix;
    std::vector<chordID> minhash;
    std::vector<chordID> buckets;
    std::vector<POLY> sig;
    //std::vector<POLY> sig1;
    //std::vector<POLY> sig2;
    std::vector<POLY> lshsig;
    chordID ID;
    DHTStatus status;
    char *value;
    int valLen, n, range, randcol;
    double freq, weight, beginTime, endTime, instime, totalinstime;
    str sigbuf;

    matrix.clear();
    minhash.clear();
    sig.clear();
    lshsig.clear();
    n = 0;
    totalinstime = 0;
    for (mapType::iterator itr = teamvecomap[listnum].begin(); itr != teamvecomap[listnum].end(); itr++) {
        sig = itr->first;
        freq = itr->second[0];
        weight = itr->second[1];

        // will hold last two signatures;
        /*
        if (n % 2 == 0)
            sig1 = sig;
        else
            sig2 = sig;
        */

        beginTime = getgtod();    
        lsh *myLSH = new lsh(sig.size(), lfuncs, mgroups, lshseed, col, irrnums, hasha, hashb);
        endTime = getgtod();    
        printdouble("lshsig: lsh time: ", endTime - beginTime);
        warnx << "\n";
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

        sort(minhash.begin(), minhash.end());

        //warnx << "minhash.size(): " << minhash.size() << "\n";
        /*
        if (plist == 1) {
            warnx << "sig" << n << ": ";
            warnx << "minhash IDs: " << "\n";
            for (int i = 0; i < (int)minhash.size(); i++) {
                warnx << minhash[i] << "\n";
            }
        }
        */

        //calcteamids(minhash);

        if ((Qflag == 1 || gflag == 1) && msgtype != INVALID) {
            std::vector<chordID> team;
            chordID teamID;
            team.clear();
            beginTime = getgtod();    
            for (int i = 0; i < (int)minhash.size(); i++) {
                teamID = minhash[i];
                warnx << "teamID: " << teamID << "\n";
                // TODO: check return status
                make_team(NULL, teamID, team);
                range = team.size();
                // randomness verified
                //randcol = randomNumGenZ(range-1);
                randcol = randomNumGenZ(range);
                ID = team[randcol];
                warnx << "ID in randcol " << randcol << ": " << ID << "\n";
                strbuf t;
                strbuf p;
                t << ID;
                p << team[0];
                str key(t);
                str teamID(p);
                warnx << "inserting " << msgtype << ":\n";
                sig2str(sig, sigbuf);
                warnx << "txsig: " << sigbuf;
                printdouble(" ", freq);
                printdouble(" ", weight);
                warnx << "\n";
                makeKeyValue(&value, valLen, key, teamID, sig, freq, weight, msgtype);
                totaltxmsglen += valLen;
                status = insertDHT(ID, value, valLen, instime, MAXRETRIES);
                totalinstime += instime;
                cleanup(value);

                // do not exit if insert FAILs!
                if (status != SUCC) {
                    // TODO: do I care?
                    warnx << "error: insert FAILed\n";
                } else {
                    warnx << "insert SUCCeeded\n";
                }
            }
            // don't forget to clear team list!
            team.clear();
            endTime = getgtod();
            printdouble("lshsig: insert time (only): ", totalinstime);
            totalinstime = 0;
            warnx << "\n";
            printdouble("lshsig: insert time (+others): ", endTime - beginTime);
            warnx << "\n";

            warnx << "sleeping (lshsig)...\n";
            initsleep = 0;
            delaycb(intval, 0, wrap(initsleepnow));
            while (initsleep == 0) acheck();
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

    //if (plist == 1) printteamidfreq();

    return 0;
}

// obsolete: use lshsig instead
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
    double freq, weight, instime;
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
        int randcol = randomNumGenZ(range);
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
            totaltxmsglen += valLen;
            status = insertDHT(ID, value, valLen, instime, MAXRETRIES);
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

int
main(int argc, char *argv[])
{
    bool usedummy = false;
    bool useproxy = false;
    int Gflag, Lflag, lflag, rflag, Sflag, sflag, Oflag, zflag, vflag, Hflag, dflag, jflag, mflag, Iflag, Eflag, Pflag, Dflag, Mflag, Fflag, fflag, xflag, yflag, Zflag, Uflag, iflag, aflag, eflag;
    int ch, gintval, initintval, waitintval, listenintval, nids, rounds, valLen, logfd;
    int listnum;
    int qid;
    int bstrapport = 0;
    int sessionlen = 0;
    int sessionrounds = 0;
    int killroundrange = 0;
    int killround = 0;
    double beginTime = 0.0;
    double beginreadTime = 0.0;
    double begingossipTime, endgossipTime, endTime, endreadTime, instime;
    char *value;
    struct stat statbuf;
    time_t rawtime;
    chordID ID;
    chordID teamID;
    std::vector<chordID> team;
    DHTStatus status;
    InsertType msgtype = NONE;
    MergeType mergetype = OTHER;
    str sigbuf;
    std::string dtd;
    std::string initdir;
    std::string proxysigdir;
    std::string sigdir;
    std::string xpathdir;
    std::string xpathtxtdir;
    std::string xpathtxtquery;
    std::string inmergetype;
    std::string cmd;
    std::vector<std::string> initfiles;
    std::vector<std::string> proxysigfiles;
    std::vector<std::string> sigfiles;
    std::vector<std::string> xpathfiles;
    std::vector<std::string> xpathtxtfiles;
    std::vector<std::string> dtdList;
    std::vector<std::vector<POLY> > proxysigList;
    std::vector<std::vector<POLY> > sigList;
    std::vector<double> freqList;
    std::vector<double> weightList;
    std::vector<long double> lfreqList;
    std::vector<long double> lweightList;
    std::vector<std::vector<POLY> > queryList;
    std::vector<std::string> querytxtList;
    std::vector<POLY> sig;
    std::vector<POLY> proxysig;
    std::vector<POLY> querysig;
    std::vector<POLY> compressedList;
    std::vector<std::vector<unsigned char> > outBitmap;


    Gflag = Lflag = lflag = rflag = Sflag = sflag = Oflag = zflag = vflag = Hflag = dflag = jflag = mflag = Eflag = Iflag = Pflag = Dflag = Mflag = Fflag = fflag = xflag = yflag = Zflag = Uflag = iflag = aflag = eflag = 0;
    gintval = waitintval = listenintval = nids = 0;
    initintval = -1;
    rounds = -1;
    rxqseq.clear();
    rxseq.clear();
    txseq.clear();
    // init txseq/rxqseq: txseq/rxqseq.back() segfaults!
    txseq.push_back(0);
    rxqseq.push_back(0);
    // init dummysig: segfault!
    dummysig.clear();
    dummysig.push_back(1);

    // parse arguments
    while ((ch = getopt(argc, argv, "A:a:B:bCcD:d:Ee:F:f:G:gHhIi:Jj:K:k:L:lMmN:n:O:o:P:pQq:R:rS:s:T:t:U:uVvW:w:X:x:y:Z:z")) != -1)
        switch(ch) {
        case 'A':
            bstrapport = strtol(optarg, NULL, 10);
            break;
        case 'a':
            aflag = 1;
            xpathtxtdir = optarg;
            break;
        case 'B':
            mgroups = strtol(optarg, NULL, 10);
            break;
        case 'b':
            bcast = true;
            break;
        case 'R':
            lfuncs = strtol(optarg, NULL, 10);
            break;
        case 'C':
            compress = true;
            break;
        case 'c':
            churn = true;
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
        case 'e':
            eflag = 1;
            xpathtxtquery = optarg;
            break;
        case 'F':
            Fflag = 1;
            hashfile = optarg;
            break;
        case 'f':
            fflag = 1;
            killfile = optarg;
            break;
        case 'I':
            Iflag = 1;
            break;
        case 'i':
            iflag = 1;
            idsfile = optarg;
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
        case 'J':
            consume_failed_msgs = false;
            break;
        case 'j':
            jflag = 1;
            irrpolyfile = optarg;
            break;
        case 'K':
            killroundrange = strtol(optarg, NULL, 10);
            break;
        case 'k':
            sessionrounds = strtol(optarg, NULL, 10);
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
        case 'N':
            ninstances = strtol(optarg, NULL, 10);
            break;
        case 'n':
            nids = strtol(optarg, NULL, 10);
            break;
        case 'O':
            Oflag = 1;
            proxysigdir = optarg;
            break;
        case 'o':
            rounds = strtol(optarg, NULL, 10);
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
        case 'U':
            Uflag = 1;
            // TODO: check if valid chordID
            str2chordID(optarg, thisID);
            break;
        case 'u':
            uflag = 1;
            break;
        case 'W':
            listenintval = strtol(optarg, NULL, 10);
            break;
        case 'w':
            waitintval = strtol(optarg, NULL, 10);
            break;
        case 'X':
            freqbyx = strtol(optarg, NULL, 10);
            break;
        case 'x':
            xflag = 1;
            xpathdir = optarg;
            break;
        case 'y':
            yflag = 1;
            inmergetype = optarg;
            break;
        case 'Z':
            Zflag = 1;
            teamsize = strtol(optarg, NULL, 10);
            break;
        case 'z':
            zflag = 1;
            break;
        case 'V':
            vanilla = true;
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

    // print version
    if (vflag == 1) {
        warnx << rcsid << "\n";
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

    if (uflag == 1) fatal << "don't try to convert multiset to set when using LSH!\n";

    // generate random chordIDs and -Z teams for each chordID
    if (zflag == 1 && nids != 0) {
        std::vector<chordID> IDs;
        for (int i = 0; i < nids; i++) {
            strbuf t;
            ID = make_randomID();
            //warnx << "ID: " << ID << "\n";
            IDs.push_back(ID);
        }
        sort(IDs.begin(), IDs.end());
        warnx << "sorted IDs:\n";
        for (int i = 0; i < (int)IDs.size(); i++) {
            warnx << "ID: " << IDs[i] << "\n";
        }
        warnx << "teams:\n";
        for (int i = 0; i < (int)IDs.size(); i++) {
            warnx << "team_" << i << ":\n";
            make_team(NULL, IDs[i], team);
        }

        //double dtype = 0.0556211;
        double dtype = 1;
        //long double ldtype = 0.0556211;
        long double ldtype = 1;

        printdouble("dtype: ", dtype);
        warnx << " sizeof(dtype): " << sizeof(dtype) << "\n";
        printdouble("ldtype: ", ldtype);
        warnx << " sizeof(ldtype): " << sizeof(ldtype) << "\n";
        std::cout << "DBL_MIN: " << DBL_MIN << "\n";
        std::cout << "DBL_MAX: " << DBL_MAX << "\n";
        for (int i = 0; i < rounds; i++) {
            warnx << "round " << i;
            dtype /= 2;
            ldtype /= 2;
            //std::cout << " c:dtype: " << dtype;
            printdouble(" p:dtype: ", dtype);
            lprintdouble(" lp:dtype: ", dtype);
            //std::cout << " c:ldtype: " << ldtype;
            printdouble(" p:ldtype: ", ldtype);
            lprintdouble(" lp:ldtype: ", ldtype);
            double nan = dtype / dtype;
            double inf = ldtype / dtype;
            printdouble(" p:nan: ", nan);
            printdouble(" p:inf: ", inf);
            if (isnan(nan)) {
                printdouble(" isnan(dtype/dtype): ", nan);
            }
            if (isinf(inf)) {
                printdouble(" isinf(ldtype/dtype): ", inf);
            }
            warnx << "\n";
        }

        return 0;
    }

    // TODO: make sure only one flag is set at a time
    // at least one action: read, gossip, LSH, query or listen
    if (rflag == 0 && gflag == 0 && lflag == 0 && Hflag == 0 && Mflag == 0 && Qflag == 0 && churn == false) usage();

    // sockets are required when listening or gossiping
    if ((gflag == 1 || lflag == 1) && (Gflag == 0 || Sflag == 0)) usage();

    // dhash socket is required when querying
    if (Qflag == 1 && Sflag == 0) usage();

    if (churn == true && (gintval == 0 || peers == 0 || ninstances == 0 || bstrapport == 0 || rounds == -1 || sessionrounds == 0 || listenintval == 0)) usage();

    // sig or xpath are required when querying
    if (Qflag == 1 && (sflag == 0 && xflag == 0)) usage();

    if (xflag == 1 && aflag == 0) usage();

    // TODO: handle sflag & Pflag
    if (gflag == 1 && gintval == 0) usage();

    // merge type and directory of files to be merged is required
    if (Mflag == 1 && (Dflag == 0 || yflag == 0 || Zflag == 0)) usage();

    if (yflag == 1) {
        if (strcmp(inmergetype.c_str(), "init") == 0)
            mergetype = INIT;   
        else if (strcmp(inmergetype.c_str(), "results") == 0)
            mergetype = RESULTS;    
        else
            usage();
    }

    // action H (for testing)
    if ((Hflag == 1) && (dflag == 0 || jflag == 0 || Fflag == 0)) usage();

    // option H (for gossiping)
    // TODO: handle sflag & Pflag & Fflag
    if ((Hflag == 1 && gflag == 1) && (waitintval == 0 || initintval == -1) &&
        (Eflag == 1 || Iflag == 1))
        usage();

    // sigs/queries are required when listening or reading
    // TODO: add Oflag
    if ((lflag == 1 || rflag == 1) && (sflag == 0 && xflag == 0)) usage();

    // set up log file: log.gpsi
    if (Lflag == 1) {
        // overwrite existing log file
        logfd = open(logfile, O_CREAT|O_WRONLY|O_TRUNC, 0644);
        if (logfd < 0) fatal << "can't open log file " << logfile << "\n";
        lseek(logfd, 0, SEEK_END);
        errfd = logfd;

        // set root dir
        rootdir = dirname(logfile);
        myhostnum = strtol(basename(rootdir), NULL, 10);
    }

    time(&rawtime);
    warnx << "start ctime: " << ctime(&rawtime);
    warnx << "start sincepoch: " << time(&rawtime) << "\n";

    // set up init phase file: log.init
    FILE *initfp = NULL;
    std::string acc;
    if (Pflag == 1) {
        // overwrite in init phase or reading
        if (Iflag == 1 || rflag == 1)
            acc = "w+";
        // read-only in exec phase
        else
            acc = "r";

        if ((initfp = fopen(initfile, acc.c_str())) == NULL) {
            fatal << "can't open init file " << initfile << "\n";
        }
    }

    // check if this proc should be killed
    if (fflag == 1) {
        if (stat(killfile, &statbuf) != 0)
            fatal << "'" << killfile << "' does not exist" << "\n";

        std::ifstream killstream;
                killstream.open(killfile);
                int hostnum;
                while (killstream >> hostnum) {
            if (hostnum == myhostnum) killme = true;
                }
                killstream.close();

    }

    // set up hash file: hash.dat
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
        // exec phase OR
        // querying?
        } else {
            //acc = "r";
            //if ((hashfp = fopen(hashfile, acc.c_str())) == NULL) {
            //  fatal << "can't read hash file " << hashfile << "\n";
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

    // load irreducible polys in memory
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

    // load chordIDs in memory
    if (iflag == 1) {
        if (stat(idsfile, &statbuf) != 0)
            fatal << "'" << idsfile << "' does not exist" << "\n";

        allIDs.clear();
        std::ifstream idstream;
                idstream.open(idsfile);
                std::string a;
        chordID b;
                while (idstream >> a) {
            str z(a.c_str());
            str2chordID(z, b);
            allIDs.push_back(b);
                }
                idstream.close();

        warnx << "chordIDs loaded: " << allIDs.size() << "\n";
        /*
        warnx << "before shuffle:\n";
        for (int i = 0; i < (int)allIDs.size(); i++)
            warnx << allIDs[i] << "\n";
        */

        // shuffle all the IDs
        random_shuffle(allIDs.begin(), allIDs.end());

        warnx << "IDs after shuffle:\n";
        for (int i = 0; i < (int)allIDs.size(); i++)
            warnx << allIDs[i] << "\n";
    }

    // connect to Chord socket when: listening, gossiping, querying
    if (gflag == 1 || lflag == 1 || Qflag == 1) {
        if (stat(dsock, &statbuf) != 0)
            fatal << "'" << dsock << "' does not exist" << "\n";
        dhash = New dhashclient(dsock);
    }

    // read signatures in memory
    if ((rflag == 1 || gflag == 1 || Hflag == 1 || lflag == 1 || Qflag == 1 || Mflag == 1) && (sflag == 1 || Oflag == 1)) {
        usedummy = false;
        useproxy = false;

        // read regular sigs
        if (sflag == 1) {
            beginreadTime = getgtod();    
            getdir(sigdir, sigfiles);
            sigList.clear();

            // insert dummy
            // the polynomial "1" has a degree 0
            // don't add dummy when querying, gossiping using LSH, or merging results?
            if (Hflag == 0 && Qflag == 0 && Mflag == 0) {
                usedummy = true;
                sigList.push_back(dummysig);
            }

            warnx << "reading signatures from files...\n";
            for (unsigned int i = 0; i < sigfiles.size(); i++) {
                readsig(sigfiles[i], sigList, useproxy);
            }
            warnx << "calculating frequencies...\n";
            beginTime = getgtod();    
            warnx << "sigList.size() (all): " << sigList.size() << "\n";
            calcfreq(sigList, usedummy, useproxy);
            // long double
            //lcalcfreq(sigList, usedummy, useproxy);
            endTime = getgtod();    
            printdouble("calcfreq time (+sorting): ", endTime - beginTime);
            warnx << "\n";
            if (freqbyx > 1) {
                // don't multiply weight
                multiplyfreq(allT, 0, freqbyx, 1);
            }

            if (plist == 1) {
                // both init phases use allT
                printlist(allT, 0, -1);
                // long double
                //lprintlist(lallT, 0, -1);
                // print DTD names and how many sigs/DTD
                for (string2sigs::iterator itr = dtd2sigs.begin(); itr != dtd2sigs.end(); itr++) {
                    warnx << "DTD: " << itr->first.c_str();
                    warnx << " sigs: " << itr->second.size() << "\n";
                    //sig2str(itr->second.back(), sigbuf);
                    //warnx << " sig: " << sigbuf << "\n";
                }
            }
        }

        // read proxy sigs
        if (Oflag == 1) {
            getdir(proxysigdir, proxysigfiles);
            proxysigList.clear();
            usedummy = false;
            useproxy = true;

            warnx << "reading proxy signatures from files...\n";
            for (unsigned int i = 0; i < proxysigfiles.size(); i++) {
                readsig(proxysigfiles[i], proxysigList, useproxy);
            }
            warnx << "calculating frequencies...\n";
            warnx << "proxysigList.size() (all): " << proxysigList.size() << "\n";
            calcfreq(proxysigList, usedummy, useproxy);

            if (plist == 1) {
                // both init phases use allT
                printlist(allT, 1, -1);
                // print DTD names and how many sigs/DTD
                for (string2sigs::iterator itr = dtd2proxysigs.begin(); itr != dtd2proxysigs.end(); itr++) {
                    warnx << "DTD: " << itr->first.c_str();
                    warnx << " proxy sigs: " << itr->second.size() << "\n";
                    //sig2str(itr->second.back(), sigbuf);
                    //warnx << " sig: " << sigbuf << "\n";
                }
            }

        }

        // verify compress/uncompress/makeKeyValue/getKeyValue work
        /*
        if (compress == 1) {
            compressedList.clear();
            outBitmap.clear();
            sigList.clear();
            lfreqList.clear();
            lweightList.clear();
            lvecomap2vec(lallT, 0, sigList, lfreqList, lweightList);

            int sigbytesize = 0;
            for (int i = 0; i < (int)sigList.size(); i++) {
                sigbytesize += (sigList[i].size() * sizeof(POLY));
            }

            warnx << "sigList size (bytes): " << sigbytesize << "\n";
            warnx << "sigList.size() (unique): " << sigList.size() << "\n";

            compressSignatures(sigList, compressedList, outBitmap);

            int compressedsize = compressedList.size() * sizeof(POLY);
            warnx << "compressedList size (bytes): " << compressedsize << "\n";
            warnx << "compressedList.size(): " << compressedList.size() << "\n";

            int bitmapsize = 0;
            for (int i = 0; i < (int)outBitmap.size(); i++) {
                bitmapsize += (outBitmap[i].size() * sizeof(unsigned char));
            }

            warnx << "outBitmap size (bytes): " << bitmapsize << "\n";
            warnx << "outBitmap.size(): " << outBitmap.size() << "\n";

            warnx << "total compressed size (list+bitmap): " << compressedsize + bitmapsize << "\n";

            //for (int i = 0; i < (int) compressedList.size(); i++) {
            //  printf("%u\n", compressedList[i]);
            //  for (int j = 0; j < ceil(sigList.size() / 8.0); j++) {
            //      //warnx << "--> " << outBitmap[i][j] << "\n";
            //      printf("--> %x\n", outBitmap[i][j]);
            //  }
            //}

            ID = make_randomID();
            strbuf t;
            t << ID;
            str key(t);
            str teamID(t);
            txseq.push_back(777);
            warnx << "key: " << key << "\n";
            warnx << "teamID: " << teamID << "\n";
            warnx << "seq: " << txseq.back() << "\n";
            warnx << "numSigs: " << lfreqList.size() << "\n";
            warnx << "compressedList.size(): " << compressedList.size() << "\n";
            makeKeyValue(&value, valLen, key, teamID, compressedList, outBitmap, lfreqList, lweightList, txseq.back(), XGOSSIPC);

            // clear and init global txseq list!
            txseq.clear();
            txseq.push_back(0);

            int seq, numSigs;
            str newkey;
            str newteamID;
            compressedList.clear();
            outBitmap.clear();
            lfreqList.clear();
            lweightList.clear();

            int ret = getKeyValue(value, newkey, newteamID, compressedList, outBitmap, numSigs, lfreqList, lweightList, seq, valLen);
            if (ret == -1) warnx << "error: getKeyValue failed\n";
            warnx << "key: " << newkey << "\n";
            warnx << "teamID: " << newteamID << "\n";
            warnx << "seq: " << seq << "\n";
            warnx << "numSigs: " << numSigs << "\n";
            warnx << "compressedList.size(): " << compressedList.size() << "\n";

            //int numSigs = sigList.size();
            std::vector<std::vector<POLY> > newsigList;
            newsigList.clear();
            uncompressSignatures(newsigList, compressedList, outBitmap, numSigs);

            warnx << "after uncompress:\n";
            assert(newsigList.size() == sigList.size());
            bool multi;
            double simi;
            for (int i = 0; i < (int) newsigList.size(); i++) {
                simi = signcmp(sigList[i], newsigList[i], multi);
                if (simi != 1) {
                    int ninter = set_inter_noskip(sigList[i], newsigList[i], multi);
                    warnx << "warning: uncompressed sig differs ";
                    printdouble("sim: ", simi);
                    warnx << " common-POLYs: " << ninter;
                    warnx << " sig-org.size: " << sigList[i].size();
                    warnx << " sig-unc.size: " << newsigList[i].size();
                    warnx << "\n";
                    sig2str(sigList[i], sigbuf);
                    warnx << "sig-org[" << i << "]: " << sigbuf << "\n";
                    sig2str(newsigList[i], sigbuf);
                    warnx << "sig-unc[" << i << "]: " << sigbuf << "\n";
                } else {
                    printdouble("sim: ", simi);
                    warnx << " ";
                    sig2str(newsigList[i], sigbuf);
                    warnx << "sig[" << i << "]: " << sigbuf << "\n";
                }
            }
        }
        */

        // create log.init of sigs (why?)
        /*
        if (Pflag == 1) {
            warnx << "writing " << initfile << "...\n";
            loginitstate(initfp);
            fclose(initfp);
        }
        */
        endreadTime = getgtod();    
        printdouble("read time: ", endreadTime - beginreadTime);
        warnx << "\n";
    }

    // reading or querying using XPath files
    if ((rflag == 1 || Qflag == 1) && xflag == 1 && aflag == 1) {
        getdir(xpathdir, xpathfiles);
        getdir(xpathtxtdir, xpathtxtfiles);

        warnx << "reading queries from txt files...\n";
        for (unsigned int i = 0; i < xpathtxtfiles.size(); i++) {
            if (stat(xpathtxtfiles[i].c_str(), &statbuf) != 0)
                fatal << "'" << xpathtxtfiles[i].c_str() << "' does not exist" << "\n";

            warnx << "querytxtfile: " << xpathtxtfiles[i].c_str() << "\n";
            std::ifstream querystream;
            querystream.open(xpathtxtfiles[i].c_str());
            std::string txt;
            while (querystream >> txt) {
                querytxtList.push_back(txt);
            }
            warnx << "NUM queries: " << querytxtList.size() << "\n";
            querystream.close();
        }

        queryList.clear();
        dtdList.clear();
        warnx << "reading queries from sig files...\n";
        for (unsigned int i = 0; i < xpathfiles.size(); i++) {
            readquery(xpathfiles[i], queryList, dtdList);
        }
        add2querymap(queryList, querytxtList, dtdList);
        /*
        warnx << "calculating frequencies...\n";
        beginTime = getgtod();    
        calcfreq(queryList);
        endTime = getgtod();    
        printdouble("calcfreq time (+sorting): ", endTime - beginTime);
        warnx << "\n";
        */
        std::string xpathtxt;
        if (plist == 1) {
            // both init phases use allT
            //printlist(allT, 0, -1);
            warnx << "queries:\n";
            for (sig2idmulti::iterator qitr = queryMap.begin(); qitr != queryMap.end(); qitr++) {
                querysig = qitr->first;
                qid = qitr->second;

                dtd = "";
                id2strings::iterator ditr = qid2dtd.find(qid);
                if (ditr != qid2dtd.end()) {
                    if (ditr->second.size() != 1) {
                        warnx << "more than 1 DTD per QID\n";
                    }
                    dtd = ditr->second.back();
                } else {
                    warnx << "DTD missing\n";
                }

                xpathtxt = "";
                id2strings::iterator titr = qid2txt.find(qid);
                if (titr != qid2txt.end()) {
                    if (titr->second.size() != 1) {
                        warnx << "more than 1 txt query per QID\n";
                    }
                    xpathtxt = titr->second.back();
                } else {
                    warnx << "txt query missing\n";
                }

                sig2str(querysig, sigbuf);
                warnx << "QID: " << qid << " DTD: " << dtd.c_str() << " QUERYSIG: " << sigbuf << " XPATH: " << xpathtxt.c_str() << "\n";
            }
            /*
            for (unsigned int i = 0; i < queryList.size(); i++) {
                sig2str(queryList[i], sigbuf);
                warnx << "query" << i << ": " << sigbuf << "\n";
            }
            */
        }
    }

    if (eflag == 1) {
        warnx << "xpath query string: " << xpathtxtquery.c_str() << "\n";
        std::string xpathtxt;
        for (id2strings::iterator itr = qid2txt.begin(); itr != qid2txt.end(); itr++) {
            qid = itr->first;
            xpathtxt = itr->second[0];
            if (itr->second.size() > 1)
                warnx << "more than 1 txt query per QID\n";

            if (xpathtxt == xpathtxtquery) {
                warnx << "found query: " << xpathtxt.c_str();
                warnx << " QID: " << qid << "\n";
                xpathqueryid = qid;
                break;
            }
        }
    }

    // run xpath queries on sigs locally
    //if (sflag == 1 && xflag == 1) {
    if (rflag == 1 && sflag == 1 && xflag == 1) {
        int sigmatches = 0;
        double totalfreq = 0;
        int tind = 0;
        double minsim = 0;
        double avgsim = 0;
        //int ninter;
        //bool multi;
        // store all sigs in allT[allnum]
        //int allnum = 0;
        // store results sigs in allT[resultsnum]
        //int resultsnum = 1;
        //mapType tmpvecomap;
        //allT.push_back(tmpvecomap);

        // from now on, work only with totalT
        totalT.push_back(allT);

        warnx << "running xpath queries locally...\n";
        warnx << "qid,dtd,querysig,querytxt,sigmatches,totalfreq,minsim,avgsim\n";
        for (sig2idmulti::iterator qitr = queryMap.begin(); qitr != queryMap.end(); qitr++) {
            querysig = qitr->first;
            qid = qitr->second;

            // doesn't matter if it's QUERYX or QUERYXP
            run_query(tind, querysig, sigmatches, totalfreq, minsim, avgsim, QUERYX);

            /*
            // find if the query sig is a subset of any of the sigs
            for (mapType::iterator sitr = allT[allnum].begin(); sitr != allT[allnum].end(); sitr++) {
                ninter = set_inter_noskip(qitr->first, sitr->first, multi);
                //warnx << "ninter: " << ninter << ", querysig.size(): " << queryList[i].size() << "\n";
                if (ninter == (int)qitr->first.size()) {
                    //if (Qflag == 1) {
                        //allT[resultsnum][qitr->first][0] += sitr->second[0];
                        //allT[resultsnum][qitr->first][1] += sitr->second[1];
                    //}
                    ++sigmatches;
                    // weight is always 1
                    totalfreq += sitr->second[0];
                    //totalavg += (sitr->second[0]/sitr->second[1]);
                    if (plist == 1) {
                        //warnx << "superset sig found: ";
                        sig2str(sitr->first, sigbuf);
                        warnx << "supersetsig: " << sigbuf;
                        printdouble(" f: ", sitr->second[0]);
                        printdouble(" w: ", sitr->second[1]);
                        printdouble(" avg: ", sitr->second[0]/sitr->second[1]);
                        warnx << "\n";
                        //if (multi == 1) warnx << "superset sig is a multiset\n";
                    }
                }
            }
            */

            dtd = "";
            id2strings::iterator ditr = qid2dtd.find(qid);
            if (ditr != qid2dtd.end()) {
                if (ditr->second.size() != 1) {
                    warnx << "more than 1 DTD per QID\n";
                }
                dtd = ditr->second.back();
            } else {
                warnx << "DTD missing\n";
            }

            std::string xpathtxt = "";
            id2strings::iterator titr = qid2txt.find(qid);
            if (titr != qid2txt.end()) {
                if (titr->second.size() != 1) {
                    warnx << "more than 1 txt query per QID\n";
                }
                xpathtxt = titr->second.back();
            } else {
                warnx << "txt query missing\n";
            }

            warnx << qid << ",";
            warnx << dtd.c_str() << ",";
            sig2str(querysig, sigbuf);
            warnx << sigbuf << ",";
            warnx << xpathtxt.c_str() << ",";
            warnx << sigmatches << ",";
            printdouble("", totalfreq);
            warnx << ",";
            printdouble("", minsim);
            warnx << ",";
            printdouble("", avgsim);
            warnx << ",\n";

            /*
            warnx << "qid: " << qitr->second;
            warnx << " querysig: " << sigbuf;
            warnx << " sigmatches: " << sigmatches;
            printdouble(" totalfreq: ", totalfreq);
            printdouble(" minsim: ", minsim);
            printdouble(" avgsim: ", avgsim);
            warnx << "\n";
            */
            sigmatches = 0;
            totalfreq = 0;
        }
    }

    warnx << "rcsid: " << rcsid << "\n";
    warnx << "root dir: " << rootdir << "\n";
    warnx << "my host num: " << myhostnum << "\n";
    warnx << "will be killed: ";
    if (killme == true) warnx << "yes\n";
    else warnx << "no\n";
    warnx << "host: " << host << "\n";
    warnx << "pid: " << getpid() << "\n";
    warnx << "peers: " << peers << "\n";
    warnx << "instances: " << ninstances << "\n";
    printdouble("peers/instance: ", peers / ninstances);
    warnx << "\n";
    warnx << "loseed: " << loseed << "\n";
    warnx << "mgroups: " << mgroups << "\n";
    warnx << "lfuncs: " << lfuncs << "\n";
    warnx << "teamsize: " << teamsize << "\n";
    warnx << "total rounds: " << rounds << "\n";
    warnx << "round length: " << gintval << "\n";
    warnx << "kill round range: " << killroundrange << "\n";
    // pick random round to kill itself
    if (killroundrange != 0) {
        srand(loseed);
        // TODO: fix this mess
        int range;
        int max = killroundrange;
        int min;
        // b/w 1 and 10
        if (max == 10) {
            min = 1;
        // b/w 11 and 20
        } else if (max == 20) {
            min  = 11;
        } else {
            fatal << "wrong kill range\n";
        }
        range = max - min + 1;
        killround = rand() % range + min;
        warnx << "kill round: " << killround << "\n";
    }
    if (Uflag == 1) warnx << "myID: " << thisID << "\n";
    warnx << "compression: ";
    if (compress) warnx << "yes\n";
    else warnx << "no\n";
    warnx << "churn: ";
    if (churn == true) {
        warnx << "yes\n";
        warnx << "session rounds: " << sessionrounds << "\n";
        sessionlen = gintval * sessionrounds;
        warnx << "session length: " << sessionlen << "\n";
        warnx << "bootstrap port: " << bstrapport << "\n";
    } else
        warnx << "no\n";

    if (churn == true) {
        //std::string myuser = getlogin();
        std::string myuser = "ec2-user";
        std::string myhome = "/home/" + myuser;
        std::string scripts = myhome + "/bin";
        std::string churnlsdroot = myhome + "/tmp.churn.lsd";
        // for large # of peers on EC2
        //std::string churnlsdroot = "/media/ephemeral0/tmp.churn.lsd";

        // pick random round to join
        srand(loseed);
        //int randround = randomNumGenZ(rounds);
        int randround = rand() % rounds + 1;
        warnx << "join round: " << randround << "\n";

        // wait for all other peers to start exec phase
        warnx << "listen interval: " << listenintval << "\n";
        warnx << "waiting to start exec phase (" << listenintval << "s)...\n";
        initsleep = 0;
        delaycb(listenintval, 0, wrap(initsleepnow));
        while (initsleep == 0) acheck();

        // wait to join in randround
        warnx << "waiting to join Chord (" << randround * gintval << "s)...\n";
        gossipsleep = 0;
        delaycb(randround * gintval, 0, wrap(gossipsleepnow));
        while (gossipsleep == 0) acheck();

        // join Chord
        int step = 5;
        int gap = 100;
        int lastport = peers * step + bstrapport + gap;

        // find next available port
        std::string sbport = itostring(bstrapport);
        std::string slport = itostring(lastport);
        std::string myroot = churnlsdroot + "/" + slport;
        while (stat(myroot.c_str(), &statbuf) == 0) {
            lastport += 5;
            slport = itostring(lastport);
            myroot = churnlsdroot + "/" + slport;
        }

        mkdir(myroot.c_str(), 0775);

        warnx << "joining Chord...\n";
        cmd = scripts + "/join_chord.sh " + sbport + " " + slport + " " + myroot;
        warnx << cmd.c_str() << "\n";
        system(cmd.c_str());

        // wait sessionlen in Chord
        warnx << "waiting in Chord (" << sessionlen << "s)...\n";
        gossipsleep = 0;
        delaycb(sessionlen, 0, wrap(gossipsleepnow));
        while (gossipsleep == 0) acheck();

        // leave
        warnx << "leaving Chord...\n";
        cmd = scripts + "/leave_chord.sh " + myroot;
        warnx << cmd.c_str() << "\n";
        system(cmd.c_str());

        return 0;
    }

    std::vector<std::vector<POLY> > pmatrix;

    // VanillaXGossip init phase ends?
    // VanillaXGossip exec phase starts?
    // start using totalT BEFORE listening
    if (gflag == 1 && Hflag == 0) {
        vanilla = true;
        // from now on, work only with totalT
        totalT.push_back(allT);
    }

    // listen
    if (gflag == 1 || lflag == 1) {
        time(&rawtime);
        warnx << "listen ctime: " << ctime(&rawtime);
        warnx << "listen sincepoch: " << time(&rawtime) << "\n";
        // enter init phase
        if (Iflag == 1) initphase = true;

        warnx << "listening for gossip...\n";
        listengossip();     
        warnx << "listen interval: " << listenintval << "\n";
        initsleep = 0;
        delaycb(listenintval, 0, wrap(initsleepnow));
        while (initsleep == 0) acheck();
    }

    // XGossip init phase and "action H" (not gossiping)
    if (Hflag == 1 && Iflag == 1) {
        warnx << "initgossipsend...\n";
        warnx << "interval b/w inserts: " << initintval << "\n";

        beginTime = getgtod();    
    
        // InitGossipSend: use findMod()
        if (mflag == 1) {
            // T_m is 1st list
            lshpoly(allT, pmatrix, initintval, INITGOSSIP);
            pmatrix.clear();
        // InitGossipSend: use compute_hash()
        } else {
            lshsig(allT, initintval, INITGOSSIP);
        }

        endTime = getgtod();    
        printdouble("xgossip init phase time: ", endTime - beginTime);
        warnx << "\n";

        if (gflag == 1) {
            warnx << "wait interval: " << waitintval << "\n";
            warnx << "waiting for all peers to finish init phase...\n";

            waitsleep = 0;
            delaycb(waitintval, 0, wrap(waitsleepnow));
            while (waitsleep == 0) acheck();

            printteamids();
            warnx << "writing " << initfile << "...\n";
            loginitstate(initfp);
            fclose(initfp);
            /*
            if (plist == 1) {
                printlistall();
            }
            */
            warnx << "openfd: " << openfd << "\n";
            warnx << "closefd: " << closefd << "\n";
        }

        // exit init phase
        initphase = false;
    }

    // Broadcast exec phase
    if (bcast == true) {
        warnx << "broadcast exec...\n";
        warnx << "broadcast interval: " << gintval << "\n";
        //warnx << "interval b/w inserts: " << initintval << "\n";
        time(&rawtime);
        warnx << "start exec ctime: " << ctime(&rawtime);
        warnx << "start exec sincepoch: " << time(&rawtime) << "\n";

        begingossipTime = getgtod();    

        // needed?
        srandom(loseed);

        for (int i = 0; i < (int)allIDs.size(); i++) {
            // skip myID
            if (allIDs[i] == thisID) {
                warnx << "skipping myID: " << thisID << "\n";
                continue;
            }
            warnx << "inserting ";
            if (compress) warnx << "BCASTC:\n";
            else warnx << "BCAST:\n";

            ID = allIDs[i];
            strbuf z;
            z << ID;
            str key(z);
            warnx << "ID" << i << ": " << ID;
            warnx << " txseq: " << txseq.back() << "\n";

            if (compress) {
                compressedList.clear();
                outBitmap.clear();
                sigList.clear();
                freqList.clear();
                weightList.clear();
                lfreqList.clear();
                lweightList.clear();
                vecomap2vec(totalT[0], 0, sigList, freqList, weightList);
                //vecomap2vec(totalT[0], 0, sigList, lfreqList, lweightList);

                /*
                int sigbytesize = 0;
                warnx << "sigs before compression:\n";
                for (int i = 0; i < (int)sigList.size(); i++) {
                    sigbytesize += (sigList[i].size() * sizeof(POLY));
                    //sig2str(sigList[i], sigbuf);
                    //warnx << "sig[" << i << "]: " << sigbuf << "\n";
                }

                warnx << "sigList size (bytes): " << sigbytesize << "\n";
                warnx << "sigList.size() (unique): " << sigList.size() << "\n";
                */
                compressSignatures(sigList, compressedList, outBitmap);

                /*
                int compressedsize = compressedList.size() * sizeof(POLY);
                warnx << "compressedList size (bytes): " << compressedsize << "\n";
                warnx << "compressedList.size(): " << compressedList.size() << "\n";

                int bitmapsize = 0;
                for (int i = 0; i < (int)outBitmap.size(); i++) {
                    bitmapsize += (outBitmap[i].size() * sizeof(unsigned char));
                }

                warnx << "outBitmap size (bytes): " << bitmapsize << "\n";
                warnx << "outBitmap.size(): " << outBitmap.size() << "\n";

                warnx << "total compressed size (list+bitmap): " << compressedsize + bitmapsize << "\n";
                */

                // after merging, everything is stored in totalT[0][0]
                makeKeyValue(&value, valLen, key, key, compressedList, outBitmap, freqList, weightList, txseq.back(), BCASTC);
            } else {
                makeKeyValue(&value, valLen, key, key, totalT[0][0], txseq.back(), BCAST);
            }

            totaltxmsglen += valLen;
            roundtxmsglen += valLen;
            status = insertDHT(ID, value, valLen, instime, MAXRETRIES);
            cleanup(value);

            // do not exit if insert FAILs!
            if (status != SUCC) {
                warnx << "error: insert FAILed\n";
            } else {
                warnx << "insert SUCCeeded\n";
            }

            warnx << "roundtxmsglen: " << roundtxmsglen;
            warnx << " txseq: " << txseq.back() << "\n";
            roundtxmsglen = 0;
            txseq.push_back(txseq.back() + 1);
            warnx << "sleeping (broadcast)...\n";
            gossipsleep = 0;
            delaycb(gintval, 0, wrap(gossipsleepnow));
            while (gossipsleep == 0) acheck();
        }
        // TODO: merge all lists
        warnx << "totalT.size(): " << totalT.size() << "\n";
        for (int i = 0; i < (int)totalT.size(); i++) {
            warnx << "totalT[i].size(): " << totalT[i].size() << "\n";
        }
        printteamids();
        warnx << "done broadcasting\n";
        time(&rawtime);
        warnx << "stop exec ctime: " << ctime(&rawtime);
        warnx << "stop exec sincepoch: " << time(&rawtime) << "\n";
        endgossipTime = getgtod();    

        printdouble("broadcast exec phase time: ", endgossipTime - begingossipTime);
        warnx << "\n";
    // VanillaXGossip exec phase
    } else if (gflag == 1 && Hflag == 0) {
        warnx << "vanillaxgossip exec...\n";
        warnx << "gossip interval: " << gintval << "\n";
        warnx << "interval b/w inserts: " << initintval << "\n";
        time(&rawtime);
        warnx << "start exec ctime: " << ctime(&rawtime);
        warnx << "start exec sincepoch: " << time(&rawtime) << "\n";

        begingossipTime = getgtod();    

        // needed?
        srandom(loseed);

        while (1) {
            // by default, gossip indefinitely (unless "rounds" is set)
            // print this only once
            if (txseq.back() == rounds && gossipdone == false) {
                warnx << "txseq = " << rounds << "\n";
                warnx << "totaltxmsglen: " << totaltxmsglen << "\n";
                warnx << "done gossiping\n";
                gossipdone = true;
                time(&rawtime);
                warnx << "stop exec ctime: " << ctime(&rawtime);
                warnx << "stop exec sincepoch: " << time(&rawtime) << "\n";
                endgossipTime = getgtod();    

                printdouble("vanillaxgossip exec phase time: ", endgossipTime - begingossipTime);
                warnx << "\n";

                // don't break because you have to keep merging the received lists!
                //break;
                // don't exit since each peer will reach a particular round at a different time
                //return 0;
            }

            ID = make_randomID();
            strbuf z;
            z << ID;
            str key(z);
            beginTime = getgtod();    

            if (gossipdone == false) {
                // store everything in totalT[0]
                mergelists(totalT[0]);
            } else {
                // don't divide freq and weight by 2
                mergelists_nogossip(totalT[0]);
            }
            endTime = getgtod();    
            printdouble("merge lists time: ", endTime - beginTime);
            warnx << "\n";

            // don't print lists after gossip is done
            // (but print lists right after gossiping is done at "rounds")
            if (plist == 1 && (gossipdone == false || txseq.back() == rounds)) {
                printlist(totalT[0], 0, txseq.back());
                // TODO
                //lprintlist(totalT[0], 0, txseq.back());
            }

            if (gossipdone == false) {
                warnx << "inserting ";
                if (compress) warnx << "VXGOSSIPC:\n";
                else warnx << "VXGOSSIP:\n";

                warnx << "txseq: " << txseq.back() << "\n";
                warnx << "txID: " << ID << "\n";

                if (compress) {
                    compressedList.clear();
                    outBitmap.clear();
                    sigList.clear();
                    freqList.clear();
                    weightList.clear();
                    lfreqList.clear();
                    lweightList.clear();
                    vecomap2vec(totalT[0], 0, sigList, freqList, weightList);
                    //vecomap2vec(totalT[0], 0, sigList, lfreqList, lweightList);

                    int sigbytesize = 0;
                    warnx << "sigs before compression:\n";
                    for (int i = 0; i < (int)sigList.size(); i++) {
                        sigbytesize += (sigList[i].size() * sizeof(POLY));
                        //sig2str(sigList[i], sigbuf);
                        //warnx << "sig[" << i << "]: " << sigbuf << "\n";
                    }

                    warnx << "sigList size (bytes): " << sigbytesize << "\n";
                    warnx << "sigList.size() (unique): " << sigList.size() << "\n";
                    compressSignatures(sigList, compressedList, outBitmap);

                    int compressedsize = compressedList.size() * sizeof(POLY);
                    warnx << "compressedList size (bytes): " << compressedsize << "\n";
                    warnx << "compressedList.size(): " << compressedList.size() << "\n";

                    int bitmapsize = 0;
                    for (int i = 0; i < (int)outBitmap.size(); i++) {
                        bitmapsize += (outBitmap[i].size() * sizeof(unsigned char));
                    }

                    warnx << "outBitmap size (bytes): " << bitmapsize << "\n";
                    warnx << "outBitmap.size(): " << outBitmap.size() << "\n";

                    warnx << "total compressed size (list+bitmap): " << compressedsize + bitmapsize << "\n";

                    // after merging, everything is stored in totalT[0][0]
                    makeKeyValue(&value, valLen, key, key, compressedList, outBitmap, freqList, weightList, txseq.back(), VXGOSSIPC);
                } else {
                    makeKeyValue(&value, valLen, key, key, totalT[0][0], txseq.back(), VXGOSSIP);
                }

                totaltxmsglen += valLen;
                roundtxmsglen += valLen;
                status = insertDHT(ID, value, valLen, instime, MAXRETRIES);
                cleanup(value);

                // do not exit if insert FAILs!
                if (status != SUCC) {
                    warnx << "error: insert FAILed\n";
                    // to preserve mass conservation:
                    // "send" msg to yourself (double freq)
                    if (consume_failed_msgs)
                        multiplyfreq(totalT[0], 0, 2, 2);
                } else {
                    warnx << "insert SUCCeeded\n";
                }
            }
            warnx << "roundtxmsglen: " << roundtxmsglen;
            warnx << " txseq: " << txseq.back() << "\n";
            roundtxmsglen = 0;
            txseq.push_back(txseq.back() + 1);
            warnx << "sleeping (vanillaxgossip)...\n";
            gossipsleep = 0;
            delaycb(gintval, 0, wrap(gossipsleepnow));
            while (gossipsleep == 0) acheck();
        }
    // XGossip exec phase
    } else if (Hflag == 1 && Eflag == 1) {
        // load sigs and teams from log.init into memory
        if (Pflag == 1) {
            warnx << "state: loading from init file...\n";
            // TODO: what if some peer starts gossiping before this?
            loadinitstate(initfp);
            if (plist == 1) {
                printlistall();
            }
        } else if (sflag == 1) {
            warnx << "state: signature files\n";
        } else if (Iflag != 1 || Eflag != 1) {
            fatal << "state: none available\n";
        } else {
            warnx << "state: init phase\n";
        }

        warnx << "xgossip exec...\n";
        warnx << "gossip interval: " << gintval << "\n";
        warnx << "interval b/w inserts: " << initintval << "\n";
        time(&rawtime);
        warnx << "start exec ctime: " << ctime(&rawtime);
        warnx << "start exec sincepoch: " << time(&rawtime) << "\n";

        begingossipTime = getgtod();    

        // needed?
        //srandom(loseed);

        while (1) {
            // by default, gossip indefinitely (unless "rounds" is set)
            // print this only once
            if (txseq.back() == rounds && gossipdone == false) {
                warnx << "txseq = " << rounds << "\n";
                warnx << "totaltxmsglen: " << totaltxmsglen << "\n";
                warnx << "done gossiping\n";
                gossipdone = true;
                time(&rawtime);
                warnx << "stop exec ctime: " << ctime(&rawtime);
                warnx << "stop exec sincepoch: " << time(&rawtime) << "\n";
                endgossipTime = getgtod();    

                printdouble("xgossip exec phase time: ", endgossipTime - begingossipTime);
                warnx << "\n";

                // don't break because you have to keep merging the received lists!
                //break;
                // don't exit since each peer will reach a particular round at a different time!
                //return 0;
            }

            beginTime = getgtod();    
            warnx << "totalT.size(): " << totalT.size() << "\n";
            for (int i = 0; i < (int)totalT.size(); i++) {
                warnx << "merging totalT[" << i << "]\n";
                if (gossipdone == false) {
                    mergelists(totalT[i]);
                } else {
                    // don't divide freq and weight by 2
                    mergelists_nogossip(totalT[i]);
                }
            }
            endTime = getgtod();    
            printdouble("merge lists time: ", endTime - beginTime);
            warnx << "\n";
            //delspecial(0);
            // don't print lists after gossip is done
            // (but print lists right after gossiping is done at "rounds")
            if (plist == 1 && (gossipdone == false || txseq.back() == rounds)) {
                printlistall(txseq.back());
            }
        
            if (gossipdone == false) {
                if (mflag == 1) {
                    fatal << "not implemented\n";
                // use compute_hash()
                } else {
                    for (int i = 0; i < (int)totalT.size(); i++) {
                        // TODO: needed?
                        if (gflag == 1) {
                            warnx << "inserting ";
                            if (compress) warnx << "XGOSSIPC:\n";
                            else warnx << "XGOSSIP:\n";

                            warnx << "txseq: " << txseq.back() << "\n";

                            teamID = findteamid(i);
                            if (teamID == NULL) {
                                warnx << "xgossip: teamID is NULL\n";
                                continue;
                            }
                            warnx << "teamID(" << i << "): " << teamID << "\n";

                            // TODO: check return status
                            make_team(NULL, teamID, team);
                            int range = team.size();
                            // randomness verified
                            int randcol = randomNumGenZ(range);
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

                            if (compress) {
                                compressedList.clear();
                                outBitmap.clear();
                                sigList.clear();
                                freqList.clear();
                                weightList.clear();
                                vecomap2vec(totalT[i], 0, sigList, freqList, weightList);

                                int sigbytesize = 0;
                                warnx << "sigs before compression:\n";
                                for (int i = 0; i < (int)sigList.size(); i++) {
                                    sigbytesize += (sigList[i].size() * sizeof(POLY));
                                    //sig2str(sigList[i], sigbuf);
                                    //warnx << "sig[" << i << "]: " << sigbuf << "\n";
                                }

                                warnx << "sigList size (bytes): " << sigbytesize << "\n";
                                warnx << "sigList.size() (unique): " << sigList.size() << "\n";
                                compressSignatures(sigList, compressedList, outBitmap);

                                int compressedsize = compressedList.size() * sizeof(POLY);
                                warnx << "compressedList size (bytes): " << compressedsize << "\n";
                                warnx << "compressedList.size(): " << compressedList.size() << "\n";

                                int bitmapsize = 0;
                                for (int i = 0; i < (int)outBitmap.size(); i++) {
                                    bitmapsize += (outBitmap[i].size() * sizeof(unsigned char));
                                }

                                warnx << "outBitmap size (bytes): " << bitmapsize << "\n";
                                warnx << "outBitmap.size(): " << outBitmap.size() << "\n";

                                warnx << "total compressed size (list+bitmap): " << compressedsize + bitmapsize << "\n";

                                makeKeyValue(&value, valLen, key, teamID, compressedList, outBitmap, freqList, weightList, txseq.back(), XGOSSIPC);
                            } else {
                                makeKeyValue(&value, valLen, key, teamID, totalT[i][0], txseq.back(), XGOSSIP);
                            }

                            totaltxmsglen += valLen;
                            roundtxmsglen += valLen;

                            status = insertDHT(ID, value, valLen, instime, MAXRETRIES);
                            cleanup(value);
                            // don't forget to clear team list!
                            team.clear();

                            // do not exit if insert FAILs!
                            if (status != SUCC) {
                                warnx << "error: insert FAILed\n";
                                // to preserve mass conservation:
                                // "send" msg to yourself
                                // (double freq)
                                if (consume_failed_msgs)
                                    multiplyfreq(totalT[i], 0, 2, 2);
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
                    }
                }
            }
            warnx << "roundtxmsglen: " << roundtxmsglen;
            warnx << " txseq: " << txseq.back() << "\n";
            roundtxmsglen = 0;
            txseq.push_back(txseq.back() + 1);

            if ((killme == true) && (killround == txseq.back())) {
                warnx << "killing Chord processes...\n";
                cmd = "kill -ABRT `cat " + std::string(rootdir) + "/pid.syncd`";
                system(cmd.c_str());
                cmd = "kill -ABRT `cat " + std::string(rootdir) + "/pid.lsd`";
                system(cmd.c_str());
                cmd = "kill -ABRT `cat " + std::string(rootdir) + "/pid.adbd`";
                system(cmd.c_str());
                warnx << "exiting gpsi...\n";
                return 0;
            }

            warnx << "sleeping (xgossip)...\n";
            gossipsleep = 0;
            delaycb(gintval, 0, wrap(gossipsleepnow));
            while (gossipsleep == 0) acheck();
        }
    } else if (Qflag == 1) {
        //initintval = 1;
        warnx << "interval b/w inserts: " << initintval << "\n";
        warnx << "querying ";
        beginTime = getgtod();
        // VanillaXGossip
        if (vanilla == true) {
            // xpath: querysig
            if (xflag == 1 && Oflag == 0) {
                warnx << "using xpath...\n";
                vanillaquery(queryMap, initintval, QUERYX);
            // sig: regularsig
            } else {
                warnx << "using sig...\n";
                warnx << "not implemented\n";
                //sig(allT, initintval, QUERYS);
            }
        // XGossip
        } else {
            // xpath: LSH(proxysig)
            if (xflag == 1 && Oflag == 1) {
                warnx << "using xpath, LSH(proxysig)...\n";
                if (eflag == 1) {
                    // use XPath text query
                    lshquery(queryMap, xpathqueryid, initintval, QUERYXP);
                } else {
                    lshquery(queryMap, -1, initintval, QUERYXP);
                }
            // xpath: LSH(querysig)
            } else if (xflag == 1 && Oflag == 0) {
                warnx << "using xpath, LSH(querysig)...\n";
                lshquery(queryMap, xpathqueryid, initintval, QUERYX);
            // sig: LSH(regularsig)
            } else {
                warnx << "using sig, LSH(sig)...\n";
                lshsig(allT, initintval, QUERYS);
            }
        }
        endTime = getgtod();    
        printdouble("query time: ", endTime - beginTime - initintval);
        warnx << "\n";

        return 0;
    } else if (Mflag == 1) {
        int range;
        int randcol;
        int avgid;
        warnx << "action: merging ";
        getdir(initdir, initfiles);
        acc = "r";
        if (mergetype == INIT) {
            warnx << "init files...\n";
            int nlists = 0;
            int zerofiles = 0;
            int totallists = 0;
            warnx << "loading files...\n";
            beginTime = getgtod();    
            for (unsigned int i = 0; i < initfiles.size(); i++) {
                //warnx << "file: " << initfiles[i].c_str() << "\n";
                if ((initfp = fopen(initfiles[i].c_str(), acc.c_str())) == NULL) {
                    warnx << "can't open init file" << initfiles[i].c_str() << "\n";
                    continue;
                }
                nlists = loadinitstate(initfp);
                if (nlists == 0) ++zerofiles;
                totallists += nlists;
                fclose(initfp);
            }
            endTime = getgtod();    
            printdouble("loading files time: ", endTime - beginTime);
            warnx << "\n";

            beginTime = getgtod();    
            warnx << "total lists added: " << totallists << "\n";
            warnx << "total 0-sized init files: " << zerofiles << "\n";
            warnx << "totalT.size(): " << totalT.size() << "\n";
            if (plist == 1) {
                warnx << "teamids (before merging)\n";
                printteamids();
                warnx << "lists (before merging)\n";
                printlistall();
            }

            // do not do it! why?
            // put everything in allT[0]
            /*
            warnx << "merging by teamID into allT[0]...\n";
            mergebyteamid();

            if (plist == 1) {
                warnx << "teamids (after merging by teamID)\n";
                printteamids();
                warnx << "lists (after merging by teamID)\n";
                printlistall();
            }
            */

            allT.clear();
            // put everything in allT[0]
            warnx << "merging everything into allT[0]...\n";
            mergeinit();

            if (plist == 1) {
                printlist(allT, 0, -1);
            }

            /*
            for (int i = 0; i < (int)totalT.size(); i++) {
                mergelists(totalT[i]);
            }
            */
            endTime = getgtod();    
            printdouble("merge init lists time: ", endTime - beginTime);
            warnx << "\n";
            /*
            if (plist == 1) {
                warnx << "teamids (after merging)\n";
                printteamids();
                warnx << "lists (after merging)\n";
                printlistall();
            }
            */
        } else if (mergetype == RESULTS) {
            bool multi = false;
            int estsigmatches = 0;
            int truesigmatches = 0;
            int ninter = 0;
            int proxylshmisses = 0;
            int querylshmisses = 0;
            int qid;
            double totalninter = 0;
            double estfreq = 0;
            double estmaxfreq = 0;
            double truefreq = 0;
            double relerror = 0;
            double maxrelerror = 0;
            //double estcursim;
            //double estminsim = 1;
            double trueminsim = 0;
            double trueavgsim = 0;
            std::string dtd;
            std::vector<chordID> proxylshIDs;
            std::vector<chordID> querylshIDs;
            std::vector<chordID> resultlshIDs;
            proxysig.clear();
            querysig.clear();
            proxylshIDs.clear();
            querylshIDs.clear();
            resultlshIDs.clear();
            //mapType uniqueSigList;
            //allT.clear();
            //allT.push_back(uniqueSigList);

            warnx << "results files...\n";
            warnx << "loading files...\n";
            beginTime = getgtod();    
            for (unsigned int i = 0; i < initfiles.size(); i++) {
                //warnx << "file: " << initfiles[i].c_str() << "\n";
                if ((initfp = fopen(initfiles[i].c_str(), acc.c_str())) == NULL) {
                    warnx << "can't open results file" << initfiles[i].c_str() << "\n";
                    continue;
                }
                warnx << initfiles[i].c_str() << "\n";
                if (loadresults(initfp, msgtype) == -1) warnx << "loadresults failed\n";
                fclose(initfp);
            }
            endTime = getgtod();    
            printdouble("loading files time: ", endTime - beginTime);
            warnx << "\n";

            if (msgtype == QUERYX || msgtype == QUERYXP) {
                // from now on, work only with totalT
                totalT.push_back(allT);
                int tind = 0;
                // TODO: make cmd line arg?
                bool lshmisses = true;

                warnx << "calculating xpath query estimates and true results...\n";
                warnx << "qid,querysig,sigmatches: est,sigmatches: true,sigmatches: rel error (%),freq (*teamsize): est,freq max (*teamsize): est,freq: true,freq: rel error (%),freq max: rel error (%),freq (*instances): true,freq (*instances): rel error (%),proxy lsh misses,query lsh misses,avg inter,query minsim,query avgsim,proxy minsim,proxy avgsim,query minsim > proxy minsim,teamIDs\n";
                for (id2sigs::iterator qitr = qid2querysig.begin(); qitr != qid2querysig.end(); qitr++) {
                    if (qitr->second.size() > 1)
                        warnx << "multiple queries for the same qid\n";

                    qid = qitr->first;
                    querysig = qitr->second[0];

                    // find DTD
                    id2strings::iterator ditr = qid2dtd.find(qid);
                    if (ditr != qid2dtd.end()) {
                        // TODO: assume 1 dtd per qid?
                        dtd = ditr->second.back();
                        //warnx << "found DTD\n";
                    } else {
                        // TODO: what to do?
                        warnx << "no DTD found for qid: " << qid << "\n";
                        dtd.clear();
                    }

                    if (msgtype == QUERYXP) {
                        // find proxy sig
                        string2sigs::iterator pitr = dtd2proxysigs.find(dtd);
                        if (pitr != dtd2proxysigs.end()) {
                            // TODO: assume 1 sig per DTD?
                            proxysig = pitr->second.back();
                            //warnx << "found proxysig\n";
                        } else {
                            // TODO: what to do?
                            warnx << "no proxy sig found for DTD: " << dtd.c_str() << "\n";
                            proxysig.clear();
                        }
                        proxylshIDs.clear();
                        proxylshIDs = run_lsh(proxysig);
                        //warnx << "run_lsh ran succuessfully\n";
                    } else if (msgtype == QUERYX) {
                        querylshIDs.clear();
                        querylshIDs = run_lsh(querysig);
                    }

                    // find sigs which match this qid
                    id2sigs::iterator sitr = qid2sigs.find(qid);
                    if (sitr != qid2sigs.end()) {
                        //warnx << "found sigs for qid: " << qid << "\n";
                        // add up avg of different sigs and pick a random avg for same one
                        for (int i = 0; i < (int)sitr->second.size(); i++) {
                            // find id corresponding to this qid in allsig2avg
                            id2id::iterator iditr = qid2id.find(qid);
                            if (iditr != qid2id.end()) {
                                avgid = iditr->second;
                                //warnx << "found id for qid: " << qid << "\n";
                            } else {
                                warnx << "qid2id: can't find qid: " << qid << "\n";
                                continue;
                            }

                            // find vector<avg> of the sig in the sig2avg map for this qid
                            mapType::iterator saitr = allsig2avg[avgid].find(sitr->second[i]);
                            if (saitr != allsig2avg[avgid].end()) {
                                //warnx << "found avg\n";
                                range = saitr->second.size();
                                // TODO: verify range
                                // pick random avg for that sig
                                randcol = randomNumGenZ(range);
                                //warnx << "sig2avg: range: " << range << " randcol: " << randcol << "\n";
                                estfreq += saitr->second[randcol];

                                // find max freq
                                double maxavg = 0;
                                for (int j = 0; j < (int)saitr->second.size(); j++) {
                                    if (maxavg < saitr->second[j])  
                                        maxavg = saitr->second[j];
                                }
                                estmaxfreq += maxavg;
                                ++estsigmatches;
                            } else {
                                warnx << "allsig2avg: where is the sig\n";
                                continue;
                            }

                            // find minimum similarity b/w query and results
                            /*
                            estcursim = signcmp(querysig, sitr->second[i], multi);
                            if (estminsim > estcursim)
                                estminsim = estcursim;
                            */

                            // run lsh on sig result
                            if (lshmisses == true) {
                                resultlshIDs.clear();
                                resultlshIDs = run_lsh(sitr->second[i]);

                                if (msgtype == QUERYXP) {
                                    // TODO: use set_inter_noskip() instead?
                                    ninter = set_inter(proxylshIDs, resultlshIDs, multi);
                                    totalninter += ninter;
                                    //warnx << "ninter: " << ninter << " multi: " << multi << "\n";
                                    if (ninter == 0) {
                                        //warnx << "lsh proxy sig miss\n";
                                        ++proxylshmisses;
                                    }
                                } else if (msgtype == QUERYX) {
                                    ninter = set_inter(querylshIDs, resultlshIDs, multi);
                                    totalninter += ninter;
                                    if (ninter == 0) {
                                        //warnx << "lsh query sig miss\n";
                                        ++querylshmisses;
                                    }
                                }
                            }
                        }
                    } else {
                        //warnx << "qid2sigs: can't find sigs for qid: " << qid << "\n";
                    }

                    // qid
                    warnx << qid << ",";

                    // querysig
                    sig2str(querysig, sigbuf);
                    warnx << sigbuf << ",";

                    // get true result
                    truesigmatches = 0;
                    truefreq = 0;
                    trueminsim = 0;
                    trueavgsim = 0;

                    if (msgtype == QUERYXP) {
                        useproxy = true;
                        gproxysig = proxysig;
                    }

                    // be careful with default arguments! (specify teamID)
                    run_query(tind, querysig, truesigmatches, truefreq, trueminsim, trueavgsim, msgtype, 0, useproxy);

                    // est sigmatches: don't multiply by teamsize!
                    warnx << estsigmatches << ",";

                    // true sigmatches: don't multiply by the number of instances!
                    warnx << truesigmatches << ",";

                    // sigmatches rel error
                    if (truesigmatches != 0) {
                        relerror = (double)(fabs((double)truesigmatches - estsigmatches) / truesigmatches) * 100;
                    } else {
                        relerror = 0;
                    }
                    printdouble("", relerror);
                    warnx << ",";

                    // est freq
                    estfreq *= teamsize;
                    printdouble("", estfreq);
                    warnx << ",";

                    // est max freq
                    estmaxfreq *= teamsize;
                    printdouble("", estmaxfreq);
                    warnx << ",";

                    // true totalfreq: don't multiply by # of instances!
                    printdouble("", truefreq);
                    warnx << ",";

                    // freq rel error
                    if (truefreq != 0) {
                        relerror = fabs(truefreq - estfreq) / truefreq * 100;
                    } else {
                        relerror = 0;
                    }
                    printdouble("", relerror);
                    warnx << ",";

                    // max freq rel error
                    if (truefreq != 0) {
                        maxrelerror = fabs(truefreq - estmaxfreq) / truefreq * 100;
                    } else {
                        maxrelerror = 0;
                    }
                    printdouble("", maxrelerror);
                    warnx << ",";

                    // true totalfreq using instances
                    truefreq *= ninstances;
                    printdouble("", truefreq);
                    warnx << ",";

                    // freq rel error
                    if (truefreq != 0) {
                        relerror = fabs(truefreq - estfreq) / truefreq * 100;
                    } else {
                        relerror = 0;
                    }
                    printdouble("", relerror);
                    warnx << ",";

                    // proxy lsh misses
                    warnx << proxylshmisses << ",";

                    // query lsh misses
                    warnx << querylshmisses << ",";

                    // avg inter
                    printdouble("", totalninter / mgroups);
                    warnx << ",";

                    // est minsim 
                    //printdouble("", estminsim);
                    //warnx << ",";

                    // query minsim 
                    printdouble("", trueminsim);
                    warnx << ",";

                    // query avgsim 
                    printdouble("", trueavgsim);
                    warnx << ",";

                    // proxy minsim 
                    printdouble("", proxyminsim);
                    warnx << ",";

                    // proxy avgsim 
                    printdouble("", proxyavgsim);
                    warnx << ",";

                    // query minsim > proxy minsim
                    if (trueminsim > proxyminsim)
                        warnx << "y,";
                    else
                        warnx << "n,";

                    // print teamIDs
                    id2chordIDs::iterator titr = qid2teamIDs.find(qid);
                    if (titr != qid2teamIDs.end()) {
                        //warnx << "qid2teamIDs: qid: " << qid << " teamIDs: " << titr->second.size() << "\n";
                        for (int i = 0; i < (int)titr->second.size(); i++ ) {
                            warnx << titr->second[i] << ",";
                        }
                    } else {
                        warnx << "qid2teamIDs: can't find teamIDs for qid: " << qid << "\n";
                    }

                    warnx << "\n";

                    // print LSH() output when error is high
                    if (relerror > errorlimit) {
                        if (msgtype == QUERYXP) {
                            warnx << proxylshIDs.size() << ",";
                            for (int i = 0; i < (int)proxylshIDs.size(); i++) {
                                warnx << proxylshIDs[i] << ",";
                            }
                        } else if (msgtype == QUERYX) {
                            warnx << querylshIDs.size() << ",";
                            for (int i = 0; i < (int)querylshIDs.size(); i++) {
                                warnx << querylshIDs[i] << ",";
                            }
                        }
                        warnx << "\n";

                        warnx << resultlshIDs.size() << ",";
                        for (int i = 0; i < (int)resultlshIDs.size(); i++) {
                            warnx << resultlshIDs[i] << ",";
                        }
                        warnx << "\n";
                    }

                    //estminsim = 0;
                    estsigmatches = 0;
                    estfreq = 0;
                    estmaxfreq = 0;
                    proxylshmisses = 0;
                    querylshmisses = 0;
                    totalninter = 0;
                    // very important!
                    proxylshIDs.clear();
                    querylshIDs.clear();
                    resultlshIDs.clear();
                }
            // TODO:
            // decide where to put querysigs and sigs: allT[?], totalT[?][?]
            // call run_query()
            } else if (msgtype == QUERYS) {
                listnum = 0;
                int sid = 1;
                warnx << "sig query results...\n";
                warnx << "id,querysig,freq: est,freq (*teamsize): est\n";
                for (mapType::iterator itr = allT[listnum].begin(); itr != allT[listnum].end(); itr++) {
                    warnx << sid << ",";
                    sig2str(itr->first, sigbuf);
                    warnx << sigbuf << ",";
                    estfreq = itr->second[0]/itr->second[1];
                    printdouble("", estfreq);
                    warnx << ",";
                    printdouble("", estfreq * teamsize);
                    warnx << "\n";
                    ++sid;
                }
            } else {
                warnx << "error: invalid msgtype\n";
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
insertDHT(chordID ID, char *value, int valLen, double &instime, int STOPCOUNT, chordID guess)
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
        instime = endTime - beginTime;
        printdouble("key insert time: ", instime);
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

// TODO: verify xgossip part
void
readgossip(int fd)
{
    strbuf buf;
    strbuf totalbuf;
    str key;
    str keyteamid;
    str dtdstr;
    str sigbuf;
    chordID ID;
    chordID teamID;
    std::vector<std::vector<POLY> > sigList;
    std::vector<double> freqList;
    std::vector<double> weightList;
    InsertType msgtype;
    std::vector<POLY> sig;
    std::vector<POLY> querysig;
    std::vector<POLY> compressedList;
    std::vector<std::vector<unsigned char> > outBitmap;
    double freq, weight, totalavg, minsim, avgsim;
    int n, msglen, recvlen, nothing, tind, ninter, qid, sigmatches;
    int ret, seq;
    int numSigs;
    bool multi;

    ninter = msglen = recvlen = nothing = 0;
    minsim = avgsim = 0;
    multi = 0;
    qid = -1;
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
    if (msgtype == VXGOSSIP || msgtype == VXGOSSIPC || msgtype == XGOSSIP || msgtype == XGOSSIPC || msgtype == BCAST || msgtype == BCASTC) {
        // discard msg if in init phase
        if (initphase == true) {
            warnx << "warning: phase=init, msgtype=" << msgtype << "\n";
            warnx << "warning: msg discarded\n";
            return;
        } else if (msgtype == BCAST) {
            warnx << "BCAST";
            ret = getKeyValue(gmsg.cstr(), key, keyteamid, sigList, freqList, weightList, seq, recvlen);
        } else if (msgtype == BCASTC) {
            warnx << "BCASTC";
            ret = getKeyValue(gmsg.cstr(), key, keyteamid, compressedList, outBitmap, numSigs, freqList, weightList, seq, recvlen);
            uncompressSignatures(sigList, compressedList, outBitmap, numSigs);
        } else if (msgtype == VXGOSSIP) {
            warnx << "VXGOSSIP";
            ret = getKeyValue(gmsg.cstr(), key, keyteamid, sigList, freqList, weightList, seq, recvlen);
        } else if (msgtype == VXGOSSIPC) {
            warnx << "VXGOSSIPC";
            ret = getKeyValue(gmsg.cstr(), key, keyteamid, compressedList, outBitmap, numSigs, freqList, weightList, seq, recvlen);
            uncompressSignatures(sigList, compressedList, outBitmap, numSigs);
        } else if (msgtype == XGOSSIP) {
            warnx << "XGOSSIP";
            ret = getKeyValue(gmsg.cstr(), key, keyteamid, sigList, freqList, weightList, seq, recvlen);
        } else {
            warnx << "XGOSSIPC";
            ret = getKeyValue(gmsg.cstr(), key, keyteamid, compressedList, outBitmap, numSigs, freqList, weightList, seq, recvlen);
            uncompressSignatures(sigList, compressedList, outBitmap, numSigs);
        }

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

        if (msgtype == BCAST || msgtype == BCASTC || msgtype == XGOSSIP || msgtype == XGOSSIPC) warnx << " teamID: " << keyteamid;

        warnx  << "\n";

        warnx << "freqList.size(): " << freqList.size() << "\n";;
        warnx << "weightList.size(): " << weightList.size() << "\n";

        // count rounds off by one
        n = seq - txseq.back();
        if (n < 0) {
            warnx << "warning: rxseq<txseq n: " << abs(n) << "\n";
            if (accept_anyround == false) {
                if (abs(n) != 1) {
                    warnx << "warning: msg discarded\n";
                    return;
                }
            }
        } else if (n > 0) {
            warnx << "warning: rxseq>txseq n: " << n << "\n";
            if (accept_anyround == false) {
                if (n != 1) {
                    warnx << "warning: msg discarded\n";
                    return;
                }
            }
        }

#ifdef _DEBUG_
        for (int i = 0; i < (int) sigList.size(); i++) {
            sig2str(sigList[i], sigbuf);
            //if (validsig(sigList[i]) == false) warnx << "warning: invalid signature\n";
            warnx << "sig[" << i << "]: " << sigbuf << "\n";
        }
        for (int i = 0; i < (int) freqList.size(); i++) {
            warnx << "freq[" << i << "]: ";
            printdouble("", freqList[i]);
            warnx << "\n";
        }
        for (int i = 0; i < (int) weightList.size(); i++) {
            warnx << "weight[" << i << "]: ";
            printdouble("", weightList[i]);
            warnx << "\n";
        }
#endif

        rxseq.push_back(seq);
        str2chordID(keyteamid, teamID);
        if (msgtype == VXGOSSIP || msgtype == VXGOSSIPC) {
            add2vecomapv(sigList, freqList, weightList);
        } else if (msgtype == BCAST || msgtype == BCASTC) {
            // TODO: addvecomapx() and merge
        // both XGossip and Broadcast use teamID
        } else {
            add2vecomapx(sigList, freqList, weightList, teamID);
        }
    } else if (msgtype == INITGOSSIP || msgtype == INFORMTEAM) {
        // discard msg if not in init phase
        if (initphase == false) {
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
        sig2str(sig, sigbuf);
        warnx << "rxsig: " << sigbuf;
        printdouble(" ", freq);
        printdouble(" ", weight);
        warnx << "\n";
        str2chordID(key, ID);
        str2chordID(keyteamid, teamID);
        initgossiprecv(ID, teamID, sig, freq, weight);
    } else if (msgtype == QUERYS || msgtype == QUERYX || msgtype == QUERYXP) {
        querysig.clear();
        if (msgtype == QUERYS) {
            warnx << "QUERYS";
            // TODO: do we need the freq/weight of the query sig?
            // TODO: add dtd
            ret = getKeyValue(gmsg.cstr(), key, keyteamid, querysig, freq, weight, recvlen);
        } else if (msgtype == QUERYX) {
            warnx << "QUERYX";
            ret = getKeyValue(gmsg.cstr(), key, keyteamid, dtdstr, querysig, qid, recvlen);
        } else {
            warnx << "QUERYXP";
            ret = getKeyValue(gmsg.cstr(), key, keyteamid, dtdstr, querysig, qid, recvlen);
        }

        if (ret == -1) {
            warnx << "error: getKeyValue failed\n";
            fdcb(fd, selread, NULL);
            close(fd);
            ++closefd;
            return;
        }

        warnx << " rxID: " << key;
        warnx << " teamID: " << keyteamid << "\n";
        sig2str(querysig, sigbuf);

        // VanillaXGgossip
        if (vanilla == true) {
            warnx << "queryresult:";
            warnx << " vanilla"; 
            warnx << " qid: " << qid << " querysig: " << sigbuf;
            warnx << " DTD: " << dtdstr;
            warnx << " teamID: " << teamID << "\n";
            sigmatches = 0;
            totalavg = 0;
            // Vanilla uses index 0
            tind = 0;
            run_query(tind, querysig, sigmatches, totalavg, minsim, avgsim, msgtype);
            warnx << "queryresultend:";
            warnx << " rxqseq: " << rxqseq.back();
            warnx << " sigmatches: " << sigmatches;
            printdouble(" totalavg: ", totalavg);
            warnx << "\n";
            rxqseq.push_back(rxqseq.back() + 1);
        // XGossip
        } else {
            // find if list for team exists
            str2chordID(keyteamid, teamID);
            teamid2totalT::iterator teamitr = teamindex.find(teamID);
            if (teamitr != teamindex.end()) {
                tind = teamitr->second[0];
                warnx << "queryresult:";
                warnx << " teamID-found"; 
                //warnx << "teamID found at teamindex[" << tind << "]";
                warnx << " qid: " << qid << " querysig: " << sigbuf;
                warnx << " DTD: " << dtdstr;
                warnx << " teamID: " << teamID << "\n";
                sigmatches = 0;
                totalavg = 0;
                run_query(tind, querysig, sigmatches, totalavg, minsim, avgsim, msgtype);
                warnx << "queryresultend:";
                warnx << " rxqseq: " << rxqseq.back();
                warnx << " sigmatches: " << sigmatches;
                printdouble(" totalavg: ", totalavg);
                warnx << "\n";
                rxqseq.push_back(rxqseq.back() + 1);
            } else {
                // don't run query if this peer is not a part of the team
                /*
                warnx << "queryresult:";
                warnx << " teamID NOT found";
                warnx << " qid: " << qid << " querysig: " << sigbuf;
                warnx << " teamID: " << teamID << "\n";
                */

                // run query even if this peer is not a part of the team
                warnx << "queryresult: ";
                warnx << "teamID-NOT-found";
                warnx << " qid: " << qid << " querysig: " << sigbuf;
                warnx << " DTD: " << dtdstr;
                warnx << " teamID: " << teamID << "\n";
                sigmatches = 0;
                totalavg = 0;
                for (int i = 0; i < (int)totalT.size(); i++) {
                    teamID = findteamid(i);
                    run_query(i, querysig, sigmatches, totalavg, minsim, avgsim, msgtype, teamID);
                }

                warnx << "queryresultend:";
                warnx << " rxqseq: " << rxqseq.back();
                warnx << " sigmatches: " << sigmatches;
                printdouble(" totalavg: ", totalavg);
                warnx << "\n";
                rxqseq.push_back(rxqseq.back() + 1);
            }

            // TODO: search other lists (not yet merged) for other sigs?
            /*
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
            */
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

// don't init sigmatches and totalavg!
int
run_query(int tind, std::vector<POLY> querysig, int &sigmatches, double &totalavg, double &minsim, double &avgsim, InsertType msgtype, chordID teamID, bool useproxy)
{
    bool multi;
    int ninter;
    double cursim;
    str sigbuf;

    // init to max value possible!
    minsim = 1;
    proxyminsim = 1;

    avgsim = 0;
    proxyavgsim = 0;

    // search for exact sig match
    if (msgtype == QUERYS) {
        mapType::iterator sigitr = totalT[tind][0].find(querysig);
        // exact signature found
        if (sigitr != totalT[tind][0].end()) {
            ++sigmatches;
            totalavg += (sigitr->second[0]/sigitr->second[1]);
            if (plist == 1) {
                sig2str(sigitr->first, sigbuf);
                warnx << "exactsig: " << sigbuf;
                printdouble(" f: ", sigitr->second[0]);
                printdouble(" w: ", sigitr->second[1]);
                printdouble(" avg: ", sigitr->second[0]/sigitr->second[1]);
                // teamID is set only when running queries for "teamID-NOT-found"
                if (teamID != 0) warnx << " teamID: " << teamID;
                warnx << "\n";
            }
        }
    // search for superset
    } else if (msgtype == QUERYX || msgtype == QUERYXP) {
        // find if the query sig is a subset of any of the sigs
        for (mapType::iterator itr = totalT[tind][0].begin(); itr != totalT[tind][0].end(); itr++) {
            ninter = set_inter_noskip(querysig, itr->first, multi);
            //warnx << "ninter: " << ninter << ", querysig.size(): " << querysig.size() << "\n";
            if (ninter == (int)querysig.size()) {
                ++sigmatches;
                totalavg += (itr->second[0]/itr->second[1]);

                // find minimum similarity b/w query and results
                cursim = signcmp(querysig, itr->first, multi);
                avgsim += cursim;
                if (minsim > cursim)
                    minsim = cursim;

                // find minimum similarity b/w proxy sig and results
                if (useproxy == true) {
                    cursim = signcmp(gproxysig, itr->first, multi);
                    proxyavgsim += cursim;
                    if (proxyminsim > cursim)
                        proxyminsim = cursim;
                }

                if (plist == 1) {
                    sig2str(itr->first, sigbuf);
                    warnx << "supersetsig: " << sigbuf;
                    printdouble(" f: ", itr->second[0]);
                    printdouble(" w: ", itr->second[1]);
                    printdouble(" avg: ", itr->second[0]/itr->second[1]);
                    // teamID is set only when running queries for "teamID-NOT-found"
                    if (teamID != 0) warnx << " teamID: " << teamID;
                    warnx << "\n";
                    //if (multi == 1) warnx << "superset sig is a multiset\n";
                }
            }
        }
    } else {
        warnx << "invalid msgtype: " << msgtype << "\n";
        return -1;
    }


    avgsim /= sigmatches;
    proxyavgsim /= sigmatches;

    return 0;
}

std::vector<chordID>
run_lsh(std::vector<POLY> sig)
{
    int col = 0;
    //int range, randcol;
    double beginTime, endTime;
    std::vector<POLY> lshsig;
    std::vector<chordID> minhash;

    beginTime = getgtod();    
    lsh *myLSH = new lsh(sig.size(), lfuncs, mgroups, lshseed, col, irrnums, hasha, hashb);
    endTime = getgtod();    
    //printdouble("run_lsh: lsh time: ", endTime - beginTime);
    //warnx << "\n";
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

    sort(minhash.begin(), minhash.end());

    //warnx << "minhash.size(): " << minhash.size() << "\n";
    /*
    if (plist == 1) {
        warnx << "sig" << n << ": ";
        warnx << "minhash IDs: " << "\n";
        for (int i = 0; i < (int)minhash.size(); i++) {
            warnx << minhash[i] << "\n";
        }
    }
    */

    //calcteamids(minhash);

    /*
    std::vector<chordID> team;
    chordID teamID, ID;
    team.clear();
    for (int i = 0; i < (int)minhash.size(); i++) {
        teamID = minhash[i];
        lshIDs.push_back(teamID);
        //warnx << "teamID: " << teamID << "\n";
        // TODO: check return status
        make_team(NULL, teamID, team);
        range = team.size();
        // randomness verified
        //randcol = randomNumGenZ(range-1);
        randcol = randomNumGenZ(range);
        ID = team[randcol];
        //warnx << "ID in randcol " << randcol << ": " << ID << "\n";
        lshIDs.push_back(ID);
    }
    */

    delete myLSH;
    return minhash;
}

void
addqid(int qid, std::vector<POLY> querysig)
{
    // map querysig to qid:
    // store querysig in 1st position
    id2sigs::iterator qitr = qid2querysig.find(qid);
    if (qitr != qid2querysig.end()) {
        if (samesig(querysig, qitr->second[0]) != 1)
            warnx << "loadresults: different querysigs for the same qid\n";
    } else {
        qid2querysig[qid].push_back(querysig);
    }
}

bool
addteamID(int qid, chordID teamID)
{
    bool teamIDfound = 0;

    // map teamID to qid
    id2chordIDs::iterator titr = qid2teamIDs.find(qid);
    if (titr != qid2teamIDs.end()) {
        for (int i = 0; i < (int)titr->second.size(); i++) {
            if (teamID == titr->second[i]) {
                warnx << "loadresults: teamID already present\n";
                teamIDfound = 1;
                break;
            }
        }
        if (teamIDfound != 1) titr->second.push_back(teamID);
        teamIDfound = 0;
    } else {
        qid2teamIDs[qid].push_back(teamID);
    }

    return teamIDfound;
}

int
loadresults(FILE *initfp, InsertType &msgtype)
{
    chordID teamID;
    std::vector<POLY> sig;
    std::vector<POLY> querysig;
    std::vector<std::string> tokens;
    mapType sig2avg;
    std::string linestr;
    std::string dtd;
    str sigbuf;
    bool sigfound = 0;
    int listnum = 0;
    int n = 0;
    int sigmatches, qid, avgid;
    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    double freq, weight, avg, totalavg;

    qid = 0;
    while ((read = getline(&line, &len, initfp)) != -1) {
        // skip 1st line
        /*
        if (n == 0) {
            ++n;
            continue;
        }
        */
        linestr.clear();
        linestr.assign(line);
        tokens.clear();
        tokenize(linestr, tokens, ": ");
        sig.clear();
        int toksize = tokens.size();
        if (strcmp(tokens[0].c_str(), "rxmsglen") == 0) {
            // TODO: record other fields?
            if (strcmp(tokens[3].c_str(), "QUERYS") == 0) {
                msgtype = QUERYS;
            } else if (strcmp(tokens[3].c_str(), "QUERYX") == 0) {
                msgtype = QUERYX;
            } else if (strcmp(tokens[3].c_str(), "QUERYXP") == 0) {
                msgtype = QUERYXP;
            } else {
                warnx << "loadresults: error parsing\n";
                return -1;
            }
        } else if (strcmp(tokens[0].c_str(), "queryresult") == 0) {
            qid = strtol(tokens[3].c_str(), NULL, 10);
            querysig.clear();
            string2sig(tokens[5], querysig);
            dtd = tokens[7];
            // strip "\n" from teamID
            std::string t = tokens[toksize-1].substr(0,tokens[toksize-1].size()-1);
            str z(t.c_str());
            str2chordID(z, teamID);
            sig2str(querysig, sigbuf);
            //warnx << "qid: " << qid << " querysig: " << sigbuf << " DTD: " << dtd.c_str() << " teamID: " << teamID << "\n";
            if (msgtype == QUERYX || msgtype == QUERYXP) {
                addqid(qid, querysig);
                // TODO: check return
                addteamID(qid, teamID);
                // add DTD
                id2strings::iterator itr = qid2dtd.find(qid);
                if (itr != qid2dtd.end()) {
                    //warnx << "loadresults: qid already exists\n";
                } else {
                    qid2dtd[qid].push_back(dtd);
                }
            } else if (msgtype == QUERYS) {
                // TODO: associate querysig with an id
            }
        // only found in QUERYX msgs
        } else if (strcmp(tokens[0].c_str(), "supersetsig") == 0) {
            sig.clear();
            string2sig(tokens[1], sig);
            freq = strtod(tokens[3].c_str(), NULL);
            weight = strtod(tokens[5].c_str(), NULL);
            avg = strtod(tokens[7].c_str(), NULL);
            sig2str(sig, sigbuf);

            //addqid2id(qid, sig, avg);

            // XXX: assume that qid is still valid?
            id2id::iterator iditr = qid2id.find(qid);
            if (iditr != qid2id.end()) {
                avgid = iditr->second;
                // add sig and avg OR add avg to existing sig
                mapType::iterator saitr = allsig2avg[avgid].find(sig);
                if (saitr != allsig2avg[avgid].end()) {
                    saitr->second.push_back(avg);
                } else {
                    allsig2avg[avgid][sig].push_back(avg);
                }

            } else {
                // TODO: verfiy that it's the correct index
                avgid = allsig2avg.size();
                qid2id[qid] = avgid;
                sig2avg[sig].push_back(avg);
                allsig2avg.push_back(sig2avg);
            }

            // map sig to qid
            id2sigs::iterator sitr = qid2sigs.find(qid);
            if (sitr != qid2sigs.end()) {
                for (int i = 0; i < (int)sitr->second.size(); i++) {
                    if (samesig(sig, sitr->second[i]) == 1) {
                        //warnx << "loadresults: sig already present\n";
                        sigfound = 1;
                        break;
                    }
                }
                if (sigfound != 1)
                    sitr->second.push_back(sig);
                sigfound = 0;
            } else {
                qid2sigs[qid].push_back(sig);
            }
        // only found in QUERYS msgs
        } else if (strcmp(tokens[0].c_str(), "exactsig") == 0) {
            sig.clear();
            // TODO: don't strip "," in future versions
            std::string exst = tokens[1].substr(0,tokens[1].size()-1);
            string2sig(exst, sig);
            exst = tokens[3].substr(0,tokens[3].size()-1);
            freq = strtod(exst.c_str(), NULL);
            exst = tokens[5].substr(0,tokens[5].size()-1);
            weight = strtod(exst.c_str(), NULL);
            // TODO: discard avg?
            // avg doesn't have trailing ","
            avg = strtod(tokens[7].c_str(), NULL);
            sig2str(sig, sigbuf);

            mapType::iterator itr = allT[listnum].find(sig);
            if (itr != allT[listnum].end()) {
                itr->second[0] += freq;
                itr->second[1] += weight;
            } else {
                allT[listnum][sig].push_back(freq);
                allT[listnum][sig].push_back(weight);
            }
        // TODO: record queryresultend data?
        } else if (strcmp(tokens[0].c_str(), "queryresultend") == 0) {
            sigmatches = strtol(tokens[4].c_str(), NULL, 10);
            totalavg = strtod(tokens[6].c_str(), NULL);
            //warnx << "queryresultend: sigmatches: " << sigmatches;
            //printdouble(" totalavg: ", totalavg);
            //warnx << "\n";
        } else {
            warnx << "loadresults: error parsing\n";
            return -1;
        }
        ++n;
    }
            /*
            // list index
            tind = strtol(tokens[1].c_str(), NULL, 10);
            // strip "\n" from chordID
            std::string t = tokens[toksize-1].substr(0,tokens[toksize-1].size()-1);
            str z(t.c_str());
            str2chordID(z, teamID);
            warnx << "teamID(" << tind << ":" << tsize + tind << "): "
                  << teamID << "\n";
            teamindex[teamID].push_back(tsize + tind);
            // done with previous team, add its vecomap
            // (but don't do it for the very first index line)
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

    if (n != 0) {
        tmpvecomap.push_back(uniqueSigList);
        totalT.push_back(tmpvecomap);
    }
    */

    if (line) free(line);
    //warnx << "loadresults: teams added: " << n << "\n";

    return n;
}

int
loadinitstate(FILE *initfp)
{
    mapType uniqueSigList;
    vecomap tmpvecomap;
    chordID teamID;
    std::vector<POLY> sig;
    std::vector<std::string> tokens;
    std::string linestr;
    int tind, tsize;
    int n = 0;
    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    double freq, weight;

    uniqueSigList.clear();
    tmpvecomap.clear();
    tsize = totalT.size();
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
            warnx << "teamID(" << tind << ":" << tsize + tind << "): "
                  << teamID << "\n";
            teamindex[teamID].push_back(tsize + tind);
            // done with previous team, add its vecomap
            // (but don't do it for the very first index line)
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

    if (n != 0) {
        tmpvecomap.push_back(uniqueSigList);
        totalT.push_back(tmpvecomap);
    }

    if (line) free(line);
    warnx << "loadinitstate: teams added: " << n << "\n";

    return n;
}

void
loginitstate(FILE *initfp)
{
    std::vector<POLY> sig;
    sig.clear();
    str sigbuf;
    chordID ID;
    // merged list is first
    int listnum = 0;
    double freq, weight;

    for (int i = 0; i < (int)totalT.size(); i++) {
        ID = findteamid(i);
        if (ID == NULL) {
            warnx << "loginitstate: teamID is NULL\n";
            continue;
        }
        strbuf z;
        z << ID;
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
        // separate by / in case dir arg doesn't have a trailing slash
        files.push_back(dir + "/" + std::string(dirp->d_name));
    }
    closedir(dp);
    return 0;
}

// copied from psi.C
void
readquery(std::string queryfile, std::vector<std::vector <POLY> > &queryList, std::vector<std::string> &dtdList)
{
    std::vector<std::string> tokens;
    double startTime, finishTime;
    FILE *qfp;

    startTime = getgtod();
    // open queries
    warnx << "queryfile: " << queryfile.c_str() << "\n";
        qfp = fopen(queryfile.c_str(), "r");
        assert(qfp);
    tokens.clear();
    tokenize(queryfile, tokens, "/");
    // remove ".qry" extension
    std::string file = tokens.back();
    tokens.clear();
    tokenize(file, tokens, ".");
    std::string dtd;
    for (unsigned int i = 0; i < tokens.size() - 1; i++) {
        dtd += tokens[i];
        // don't put a dot at the end of the DTD name
        if (i != tokens.size() - 2)
            dtd += ".";
    } 

    while (1) {
        // DON'T use readData to retrieve signatures from input files...
        // since the size filed uses POLY as a basic unit and not byte...
        // Format is <n = # of sigs><sig size><sig>... n times...
        int numSigs;
        if (fread(&numSigs, sizeof(numSigs), 1, qfp) != 1) {
            warnx << "numSigs: " << numSigs << "\n";
            break;
        }
        warnx << "NUM sigs: " << numSigs << "\n";
        assert(numSigs > 0);

        for (int t = 0; t < numSigs; t++) {
            POLY *buf;
            int size;
            if (fread(&size, sizeof(int), 1, qfp) != 1) {
                assert(0);
            }
            warnx << "Signature size: " << size * sizeof(POLY) << " bytes\n";
                
            buf = new POLY[size];
            assert(buf);
            if (fread(buf, sizeof(POLY), size, qfp) != (size_t) size) {
                assert(0);
            }
                    
            std::vector<POLY> sig;
            sig.clear();
            warnx << "Query signature (sorted): ";
            for (int i = 0; i < size; i++) {
                // XXX: discard "1"s
                if (buf[i] == 1) continue;
                sig.push_back(buf[i]);
            }
            sort(sig.begin(), sig.end());
            str sigbuf;
            sig2str(sig, sigbuf);
            warnx << sigbuf << "\n";
            queryList.push_back(sig);
            dtdList.push_back(dtd);

            // free the allocated memory
            delete[] buf;
        }
            
        /*
        warnx << "******* Processing query " << count << " ********\n";
        numReads = numWrites = dataReadSize = cacheHits = 0;
        strbuf s;
        s << rootNodeID;
        str rootID(s);

        //if ((int) listOfSigs.size() > MAXSIGSPERQUERY) {
        if (0) {
            warnx << "Skipping this query...\n";
        } else {
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
        */
    }

    fclose(qfp);
    warnx << "readquery: Size of query list: " << queryList.size() << "\n";
    finishTime = getgtod();
}

// TODO: work w/ 0-sized sigs
void
readsig(std::string sigfile, std::vector<std::vector <POLY> > &sigList, bool useproxy)
{
    std::vector<std::string> tokens;
    double startTime, finishTime;
    FILE *sigfp;

    startTime = getgtod();
    // open signatures
    warnx << "sigfile: " << sigfile.c_str() << "\n";
    sigfp = fopen(sigfile.c_str(), "r");
    // change to if?
    assert(sigfp);
    tokenize(sigfile, tokens, "/");
    // remove ".$NUM.sig" extension
    std::string file = tokens.back();
    tokens.clear();
    tokenize(file, tokens, ".");
    std::string dtd;
    for (unsigned int i = 0; i < tokens.size() - 2; i++) {
        dtd += tokens[i];
        // don't put a dot at the end of the DTD name
        if (i != tokens.size() - 3)
            dtd += ".";
    }
    // TODO: add maxcount arg?

    // DONT use readData to retrieve signatures from input files...
    // since the size filed uses POLY as a basic unit and not byte...
    // Read numSigs <it should be 1> for data signatures...
    int numSigs;
    if (fread(&numSigs, sizeof(int), 1, sigfp) != 1) {
        //warnx << "numSigs: " << numSigs << "\n";
    }
    //warnx << "NUM sigs: " << numSigs;
    assert(numSigs == 1);

    int size;

    if (fread(&size, sizeof(int), 1, sigfp) != 1) {
        assert(0);
        warnx << "\ninvalid signature\n";
    }
    //warnx << ", Signature size: " << size * sizeof(POLY) << " bytes\n";

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
    // map dtd to sig
    if (useproxy == false) {
        string2sigs::iterator itr = dtd2sigs.find(dtd);
        if (itr != dtd2sigs.end()) {
            itr->second.push_back(sig);
        } else {
            dtd2sigs[dtd].push_back(sig);
        }
    } else {
        string2sigs::iterator itr = dtd2proxysigs.find(dtd);
        if (itr != dtd2proxysigs.end()) {
            itr->second.push_back(sig);
        } else {
            dtd2proxysigs[dtd].push_back(sig);
        }
    }

    //warnx << "readsig: Size of sig list: " << sigList.size() << "\n";
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
    str sigbuf;

    dummysig.clear();
    dummysig.push_back(1);
    hldsig.clear();
    hldsig.push_back(0);

    // find if list for team exists
    teamid2totalT::iterator teamitr = teamindex.find(teamID);
    if (teamitr != teamindex.end()) {
        warnx << "teamID found\n";
        tind = teamitr->second[0];
    } else {
        warnx << "teamID NOT found: adding new vecomap\n";
        warnx << "totalT.size() [before]: " << totalT.size() << "\n";
        tind = totalT.size();
        // don't add 1, since it's 0-based
        teamindex[teamID].push_back(tind);
        // add empty vecomap otherwise a push_back() below will fail
        totalT.push_back(tmpveco);
    }
    warnx << "tind: " << tind << "\n";
    warnx << "totalT.size(): " << totalT.size() << "\n";
    warnx << "teamindex.size(): " << teamindex.size() << "\n";
    warnx << "totalT[tind].size(): " << totalT[tind].size() << "\n";

    if (totalT[tind].size() == 0) {
        uniqueSigList[hldsig].push_back(0);
        uniqueSigList[hldsig].push_back(1);
        totalT[tind].push_back(uniqueSigList);
    }

    mapType::iterator sigitr = totalT[tind][listnum].find(sig);
    mapType::iterator dummysigitr = totalT[tind][listnum].find(dummysig);

    // s is regular and T_h[s] exists
    if ((sig != dummysig) && (sigitr != totalT[tind][listnum].end())) {
        warnx << "initgossiprecv: update freq\n";
        sig2str(sig, sigbuf);
        warnx << "initgossiprecv: sig: " << sigbuf;
        printdouble(" cur-f: ", sigitr->second[0]);
        printdouble(" cur-w: ", sigitr->second[1]);
        warnx << "\n";
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
    double instime = 0;

    warnx << "teamID: " << teamID << "\n";
    idindex = make_team(myID, teamID, team);
    
    if (idindex == -1) {
        warnx << "myID not in team\n";
        return;
    } else if (idindex == ((int)team.size() - 1)) {
        warnx << "i am last, wrap around to first\n";
        nextID = team[0];
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
    totaltxmsglen += valLen;
    warnx << "inserting INFORMTEAM:\n";
    warnx << "myID: " << myID << " nextID: " << nextID << " teamID: " << teamID << "\n";
    status = insertDHT(nextID, value, valLen, instime, MAXRETRIES);
    cleanup(value);

    if (status != SUCC) {
        // TODO: do I care?
        warnx << "error: insert FAILed\n";
    } else {
        warnx << "insert SUCCeeded\n";
    }
}

// add teamIDs generated by LSH to teamidminhash
void
calcteamids(std::vector<chordID> minhash)
{
    teamidfreq::iterator teamitr;

    for (int i = 0; i < (int)minhash.size(); i++) {
        teamitr = teamidminhash.find(minhash[i]);
        if (teamitr != teamidminhash.end()) {
            warnx << "teamID found in teamidminhash\n";
            teamitr->second += 1;
        } else {
            warnx << "teamID NOT found in teamidminhash\n";
            teamidminhash[minhash[i]] = 1;
        }
    }
}

// find teamID of totalT[tix]
// TODO: inefficient?
chordID
findteamid(int tix)
{
    teamid2totalT::iterator itr;
    for (itr = teamindex.begin(); itr != teamindex.end(); itr++) {
        for (int i = 0; i < (int)itr->second.size(); i++) {
            if (itr->second[i] == tix) {
                return itr->first;
            }
        }
    }
    
    return NULL;
}

// return -1 if myID is not in team
// verify
int
make_team(chordID myID, chordID teamID, std::vector<chordID> &team)
{
    int idindex = -1;
    chordID curID;

    if (myID == teamID) {
        //warnx << "myID == teamID, index = 0\n";
        idindex = 0;
    }

    // size of ring
    bigint rngmax = (bigint (1) << 160)  - 1;
    bigint arclen = rngmax / teamsize;
    //warnx << "arclen: " << arclen << "\n";
    chordID b = 1;
    b <<= NBIT;
    team.clear();
    team.push_back(teamID);
    curID = teamID;
    //warnx << "ID_0: " << teamID << "\n";
    // start at 1 because of team id
    for (int j = 1; j < teamsize; j++) {
        curID += arclen;
        // wraparound
        if (curID >= b) curID -= b;
        //warnx << "ID_" << j << ": " << curID << "\n";
        if (myID == curID) {
            //warnx << "myID == curID, index = " << j << "\n";
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
        //  isig.push_back(sig[i]);
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

bool
validsig(std::vector<POLY> sig)
{
    if (sig.size() <= 0) return false;

    for (int i = 0; i < (int)sig.size(); i++)
        if (sig[i] == 0)
            return false;

    return true;
}

void
add2querymap(std::vector<std::vector<POLY> > queryList, std::vector<std::string> querytxtList, std::vector<std::string> dtdList)
{
    int qid = 0;
    warnx << "add2querymap: queryList.size(): " << queryList.size() << "\n";
    warnx << "add2querymap: querytxtList.size(): " << querytxtList.size() << "\n";
    assert(queryList.size() == dtdList.size());
    //assert(queryList.size() == querytxtList.size());

    for (int i = 0; i < (int)queryList.size(); i++) {
        queryMap.insert(std::pair<std::vector<POLY>, int>(queryList[i], qid));
        id2strings::iterator itr = qid2dtd.find(qid);
        if (itr != qid2dtd.end()) {
            warnx << "add2querymap: qid already exists in qid2dtd\n";
        } else {
            qid2dtd[qid].push_back(dtdList[i]);
        }

        id2strings::iterator itr2 = qid2txt.find(qid);
        if (itr2 != qid2txt.end()) {
            warnx << "add2querymap: qid already exists in qid2txt\n";
        } else {
            qid2txt[qid].push_back(querytxtList[i]);
        }

        ++qid;
    }
    warnx << "queries added: " << qid << "\n";
}

// verified
// use allT only in init phases
void
calcfreq(std::vector<std::vector<POLY> > sigList, bool usedummy, bool proxysigs)
{
    mapType uniqueSigList;
    int i = 0;

    if (usedummy == 1) {
        // dummy's freq is 0 and weight is 1
        uniqueSigList[sigList[0]].push_back(0);
        uniqueSigList[sigList[0]].push_back(1);
        // skip dummy
        i = 1;
    }

    for (; i < (int)sigList.size(); i++) {
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
    if (proxysigs == false) {
        allT.push_back(uniqueSigList);
        warnx << "calcfreq: setsize: " << allT[0].size() << "\n";
    } else {
        // XXX: ugly!
        if (allT.size() == 0) {
            mapType tmpSigList;
            allT.push_back(tmpSigList);
        }
        allT.push_back(uniqueSigList);
        warnx << "calcfreq: setsize: " << allT[1].size() << "\n";
    }
}

// verified
// use allT only in init phases
void
lcalcfreq(std::vector<std::vector<POLY> > sigList, bool usedummy, bool proxysigs)
{
    lmapType uniqueSigList;
    int i = 0;

    if (usedummy == 1) {
        // dummy's freq is 0 and weight is 1
        uniqueSigList[sigList[0]].push_back(0);
        uniqueSigList[sigList[0]].push_back(1);
        // skip dummy
        i = 1;
    }

    for (; i < (int)sigList.size(); i++) {
        lmapType::iterator itr = uniqueSigList.find(sigList[i]);
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
    if (proxysigs == false) {
        lallT.push_back(uniqueSigList);
        warnx << "calcfreq: setsize: " << lallT[0].size() << "\n";
    } else {
        // XXX: ugly!
        if (lallT.size() == 0) {
            lmapType tmpSigList;
            lallT.push_back(tmpSigList);
        }
        lallT.push_back(uniqueSigList);
        warnx << "calcfreq: setsize: " << lallT[1].size() << "\n";
    }
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

// xgossip
void
add2vecomapx(std::vector<std::vector<POLY> > sigList, std::vector<double> freqList, std::vector<double> weightList, chordID teamID)
{
    int tind;
    mapType uniqueSigList;
    vecomap tmpvecomap;
    tind = 0;

    warnx << "add2vecomap\n";

    for (int i = 0; i < (int) sigList.size(); i++) {
        uniqueSigList[sigList[i]].push_back(freqList[i]);
        uniqueSigList[sigList[i]].push_back(weightList[i]);
    }

    // find if list for team exists
    teamid2totalT::iterator teamitr = teamindex.find(teamID);
    if (teamitr != teamindex.end()) {
        tind = teamitr->second[0];
        warnx << "teamID found at teamindex[" << tind << "]\n";
        totalT[tind].push_back(uniqueSigList);
        warnx << "totalT[tind].size(): " << totalT[tind].size() << "\n";
    } else {
        warnx << "warning: teamID NOT found: " << teamID << "\n";

        // Broadcast only
        if (bcast == true) {
            warnx << "teamID NOT found: adding new vecomap\n";
            warnx << "totalT.size() [before]: " << totalT.size() << "\n";
            tmpvecomap.push_back(uniqueSigList);
            totalT.push_back(tmpvecomap);
            tind = totalT.size() - 1;
            teamindex[teamID].push_back(tind);
        }
    }

    warnx << "tind: " << tind << "\n";
    warnx << "totalT.size(): " << totalT.size() << "\n";
    warnx << "teamindex.size(): " << teamindex.size() << "\n";
}

// vanillaxgossip
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

// if nan or inf, init num to 1st operand
// (i.e. don't perform operation)
bool
fixnan(double num, double op1)
{
    if (isnan(num)) {
        printdouble("isnan: ", num);
        printdouble(", ", op1);
        warnx << "\n";
        num = op1;
        return true;
    } else if (isinf(num)) {
        printdouble("isinf: ", num);
        printdouble(", ", op1);
        warnx << "\n";
        num = op1;
        return true;
    }

    return false;
}

// s1 and s2 are assumed to be sorted
bool
samesig(std::vector<POLY> s1, std::vector<POLY> s2)
{
    if (s1.size() != s2.size()) {
        return false;
    } else {
        for (int i = 0; i < (int) s1.size(); i++)
            if (s1[i] != s2[i]) return false;
    }

    return true;
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
// works with multisets (DOES NOT skip duplicates)
// complexity:
// at most, performs 2*(count1+count2)-1 comparisons or applications of comp
// (where countX is the distance between firstX and lastX)
int
set_inter_noskip(std::vector<POLY> s1, std::vector<POLY> s2, bool &multi)
{
    std::vector<POLY>::iterator s1itr = s1.begin();
    std::vector<POLY>::iterator s2itr = s2.begin();
    POLY s1prev, s2prev;
    int n, s1skip, s2skip;

    //warnx << "\n";
    s1prev = s2prev = n = s1skip = s2skip = 0;
    while (s1itr != s1.end() && s2itr != s2.end()) {
        // DON'T skip duplicates, just count them
        if (s1prev == *s1itr) {
            //warnx << "skipping duplicate\n";
            ++s1skip;
            //s1prev = *s1itr;
            //++s1itr;
        } else if (s2prev == *s2itr) {
            //warnx << "skipping duplicate\n";
            ++s2skip;
            //s2prev = *s2itr;
            //++s2itr;
        }

        if (*s1itr < *s2itr) {
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

// TODO: implement using templates (to handle both vectors of POLYs and vectors of chordIDs)
// return number of elements in the intersection of s1 and s2
// s1 and s2 are assumed to be sorted
// logic copied from set_intersection() in <algorithm>
// works with multisets (skips duplicates)
// complexity:
// at most, performs 2*(count1+count2)-1 comparisons or applications of comp
// (where countX is the distance between firstX and lastX)
int
set_inter(std::vector<chordID> s1, std::vector<chordID> s2, bool &multi)
{
    std::vector<chordID>::iterator s1itr = s1.begin();
    std::vector<chordID>::iterator s2itr = s2.begin();
    chordID s1prev, s2prev;
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

void
mergebyteamid()
{
    std::vector<POLY> sig;
    int tix, tixsize, mergeix;
    int listnum = 0;

    sig.clear();
    mergeix = 0;
    for (teamid2totalT::iterator itr = teamindex.begin(); itr != teamindex.end(); itr++) {
        warnx << "teamID: " << itr->first;
        tixsize = itr->second.size();
        warnx << " tixsize: " << tixsize;
        for (int i = 0; i < tixsize; i++) {
            tix = itr->second[i];
            if (mergeix == 0) {
                mergeix = tix;
                warnx << " mergeix: " << mergeix;
            } else {
                for (mapType::iterator titr = totalT[tix][listnum].begin(); titr != totalT[tix][listnum].end(); titr++) {
                    sig = titr->first;
                    mapType::iterator mitr = totalT[mergeix][listnum].find(sig);
                    if (mitr != totalT[mergeix][listnum].end()) {
                        // update freq only!
                        mitr->second[0] += titr->second[0];
                        // don't touch weight (it's 1 already)
                        //mitr->second[1] += titr->second[1];
                    } else {
                        totalT[mergeix][listnum][sig].push_back(titr->second[0]);
                        // set weight to 1 because there is no gossiping
                        totalT[mergeix][listnum][sig].push_back(1);
                    }

                }
            }
        }
        itr->second.clear();
        itr->second.push_back(mergeix);
        mergeix = 0;
        warnx << "\n";
    }
    warnx << "teamindex.size(): " << teamindex.size() << "\n";
}

// put everything in allT[0]
void
mergeinit()
{
    mapType uniqueSigList;
    int listnum = 0;

    allT.clear();
    allT.push_back(uniqueSigList);

    for (int i = 0; i < (int)totalT.size(); i++) {
        // TODO: do we always store sigs in totalT[i][0]?
        for (mapType::iterator titr = totalT[i][listnum].begin(); titr != totalT[i][listnum].end(); titr++) {
            mapType::iterator aitr = allT[listnum].find(titr->first);
            if (aitr != allT[listnum].end()) {
                aitr->second[0] += titr->second[0];
                // don't touch weight
                //aitr->second[1] += titr->second[1];
            } else {
                allT[listnum][titr->first].push_back(titr->second[0]);
                // set weight to 1 because there is no gossiping
                allT[listnum][titr->first].push_back(1);
            }
        }
    }
}


// fixed: distinguishes b/w Vanilla and XGossip!
void
mergelists(vecomap &teamvecomap)
{
    double sumf, sumw, tmp;
    str sigbuf;
    std::vector<POLY> minsig;
    std::vector<POLY> tmpsig;
    minsig.clear();

    warnx << "merging:\n";
    
    if (vanilla == true) warnx << "vanilla: 1 dummy\n";
    else warnx << "xgossip: 2 dummies\n";

    int n = teamvecomap.size();
    warnx << "initial teamvecomap.size(): " << n << "\n";
    if (n == 1) {
        dividefreq(teamvecomap, 0, 2, 2);
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
        //tmpsig = citr[i]->first;
        //sig2str(tmpsig, sigbuf);
        //warnx << "1st dummy: " << sigbuf << "\n";
        // SKIP 1ST DUMMY (both vanilla and xgossip)
        ++citr[i];
        //tmpsig = citr[i]->first;
        //sig2str(tmpsig, sigbuf);
        //warnx << "2nd dummy: " << sigbuf << "\n";
        // SKIP 2ND DUMMY (only xgossip)
        if (vanilla == false) ++citr[i];
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
                tmp = sumw + teamvecomap[i][dummysig][1];
                fixnan(tmp, sumw);
                sumw = tmp;
            }
            //printdouble("new local dummy sumw/2: ", sumw/2);
            //warnx << "\n";
            tmp = sumw/2;
            fixnan(tmp, sumw);
            teamvecomap[0][dummysig][1] = tmp;
            //warnx << "mergelists: breaking from loop\n";
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
            // check for end of map first and then compare to minsig!
            if ((citr[i] != teamvecomap[i].end()) && (citr[i]->first == minsig)) {
                //warnx << "minsig found in T_" << i << "\n";
                tmp = sumf + citr[i]->second[0];
                fixnan(tmp, sumf);
                sumf = tmp;
                tmp = sumw + citr[i]->second[1];
                fixnan(tmp, sumw);
                sumw = tmp;
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
                tmp = sumf + teamvecomap[i][dummysig][0];
                fixnan(tmp, sumf);
                sumf = tmp;
                tmp = sumw + teamvecomap[i][dummysig][1];
                fixnan(tmp, sumw);
                sumw = tmp;
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
            //warnx << "T_0: updating sums of minsig\n";
            tmp = sumf/2;
            fixnan(tmp, sumf);
            teamvecomap[0][minsig][0] = tmp;
            tmp = sumw/2;
            fixnan(tmp, sumw);
            teamvecomap[0][minsig][1] = tmp;
            // XXX: see above XXX
            ++citr[0];
        } else {
            /*
            warnx << "T_0: no minsig in T_0";
            if (citr[0] == teamvecomap[0].end()) {
                warnx << " (list ended)";
            }
            warnx << "\n";
            */
            // insert new sig
            //warnx << "T_0: inserting minsig...\n";
            tmp = sumf/2;
            fixnan(tmp, sumf);
            teamvecomap[0][minsig].push_back(tmp);
            tmp = sumw/2;
            fixnan(tmp, sumw);
            teamvecomap[0][minsig].push_back(tmp);

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

// use only after gossiping is done!
void
mergelists_nogossip(vecomap &teamvecomap)
{
    double sumf, sumw;
    str sigbuf;
    std::vector<POLY> minsig;
    std::vector<POLY> tmpsig;
    minsig.clear();

    warnx << "merging (no gossip):\n";
    
    if (vanilla == true) warnx << "vanilla: 1 dummy\n";
    else warnx << "xgossip: 2 dummies\n";

    int n = teamvecomap.size();
    warnx << "initial teamvecomap.size(): " << n << "\n";
    if (n == 1) {
        // don't divide!
        //dividefreq(teamvecomap, 0, 2, 2);
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
        //tmpsig = citr[i]->first;
        //sig2str(tmpsig, sigbuf);
        //warnx << "1st dummy: " << sigbuf << "\n";
        // SKIP 1ST DUMMY (both vanilla and xgossip)
        ++citr[i];
        //tmpsig = citr[i]->first;
        //sig2str(tmpsig, sigbuf);
        //warnx << "2nd dummy: " << sigbuf << "\n";
        // SKIP 2ND DUMMY (only xgossip)
        if (vanilla == false) ++citr[i];
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
            //printdouble("new local dummy sumw/2: ", sumw/2);
            //warnx << "\n";

            // don't divide!
            //teamvecomap[0][dummysig][1] = sumw/2;

            //warnx << "mergelists: breaking from loop\n";
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
            // check for end of map first and then compare to minsig!
            if ((citr[i] != teamvecomap[i].end()) && (citr[i]->first == minsig)) {
                //warnx << "minsig found in T_" << i << "\n";
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
            //warnx << "T_0: updating sums of minsig\n";

            // don't divide!
            //teamvecomap[0][minsig][0] = sumf/2;
            //teamvecomap[0][minsig][1] = sumw/2;

            // XXX: see above XXX
            ++citr[0];
        } else {
            /*
            warnx << "T_0: no minsig in T_0";
            if (citr[0] == teamvecomap[0].end()) {
                warnx << " (list ended)";
            }
            warnx << "\n";
            */
            // insert new sig
            //warnx << "T_0: inserting minsig...\n";

            // don't divide!
            //teamvecomap[0][minsig].push_back(sumf/2);
            //teamvecomap[0][minsig].push_back(sumw/2);

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
        dividefreq(allT, 0, 2, 2);
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
            allT[0][dummysig][1] = sumw/2;
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
multiplyfreq(vecomap &teamvecomap, int listnum, int fby, int wby)
{
    double freq, weight, tmp;

    mapType::iterator itr = teamvecomap[listnum].begin();
    // skip dummysig if multiplying freq to increase the total number of sigs
    // don't skip it if unsuccessful insert (fby == wby)
    if ((itr->first == dummysig) && (fby != wby)) {
        warnx << "multiplyfreq: skipping dummysig\n";
        ++itr;
    }

    // TODO: will it bomb if teamvecomap only contains dummysig?
    for (; itr != teamvecomap[listnum].end(); itr++) {
        freq = itr->second[0];
        tmp = freq * fby;
        fixnan(tmp, freq);
        itr->second[0] = tmp;
        weight = itr->second[1];
        tmp = weight * wby;
        fixnan(tmp, weight);
        itr->second[1] = tmp;
    }
    warnx << "multiplyfreq(" << fby << ", " << wby << "): setsize: " << teamvecomap[listnum].size() << "\n";
}

void
dividefreq(vecomap &teamvecomap, int listnum, int fby, int wby)
{
    double freq, weight, tmp;

    for (mapType::iterator itr = teamvecomap[listnum].begin(); itr != teamvecomap[listnum].end(); itr++) {

        freq = itr->second[0];
        tmp = freq / fby;
        fixnan(tmp, freq);
        itr->second[0] = tmp;

        weight = itr->second[1];
        tmp = weight / wby;
        fixnan(tmp, weight);
        itr->second[1] = tmp;
    }
    warnx << "dividefreq(" << fby << ", " << wby << "): setsize: " << teamvecomap[listnum].size() << "\n";
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

void
vecomap2vec(vecomap teamvecomap, int listnum, std::vector<std::vector <POLY> >&sigList, std::vector<double> &freqList, std::vector<double> &weightList)
{
    for (mapType::iterator itr = teamvecomap[listnum].begin(); itr != teamvecomap[listnum].end(); itr++) {
        sigList.push_back(itr->first);
        freqList.push_back(itr->second[0]);
        weightList.push_back(itr->second[1]);
    }
}

void
lvecomap2vec(lvecomap teamvecomap, int listnum, std::vector<std::vector <POLY> >&sigList, std::vector<long double> &freqList, std::vector<long double> &weightList)
{
    for (lmapType::iterator itr = teamvecomap[listnum].begin(); itr != teamvecomap[listnum].end(); itr++) {
        sigList.push_back(itr->first);
        freqList.push_back(itr->second[0]);
        weightList.push_back(itr->second[1]);
    }
}

std::string
itostring(int num)
{
    std::ostringstream oss;
    oss << num;
    return oss.str();
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
lprintdouble(std::string fmt, long double num)
{
    std::ostringstream oss;
    std::string ss;

    oss << num;
    ss = oss.str();
    warnx << fmt.c_str() << ss.c_str();
}

void
printteamidfreq()
{
    warnx << "printteamidfreq:\n";

    for (teamidfreq::iterator itr = teamidminhash.begin(); itr != teamidminhash.end(); itr++) {
        warnx << "teamID: " << itr->first;
        warnx << " freq: " << itr->second;
        warnx << "\n";
    }
    warnx << "teamidminhash.size(): " << teamidminhash.size() << "\n";
}

void
printteamids()
{
    double avglists, avgsigspteamid;
    int nlists, high, low, tixsize, sigspteamid, totalsigspteamid;

    totalsigspteamid = 0;
    nlists = 0;
    // set to extreme values
    high = 0;
    low = teamsize;

    warnx << "printteamids:\n";

    for (teamid2totalT::iterator itr = teamindex.begin(); itr != teamindex.end(); itr++) {
        warnx << "teamID: " << itr->first;
        tixsize = itr->second.size();
        nlists += tixsize;
        if (low > tixsize) low = tixsize;
        if (high < tixsize) high = tixsize;
        warnx << " tixsize: " << tixsize;
        warnx << " tix: ";
        sigspteamid = 0;
        for (int i = 0; i < tixsize; i++) {
            sigspteamid += totalT[itr->second[i]][0].size();
            warnx << itr->second[i] << ", ";
        }
        while (tixsize < teamsize) {
            warnx << "na, ";
            ++tixsize;  
        }
        warnx << "\n";
        totalsigspteamid += sigspteamid;
        warnx << "sigspteamid: " << sigspteamid;
        warnx << " teamID: " << itr->first;
        warnx << "\n";
    }
    warnx << "teamindex.size(): " << teamindex.size() << "\n";
    warnx << "total sigs/teamID: " << totalsigspteamid << "\n";
    avgsigspteamid = (double)totalsigspteamid / (double)teamindex.size();
    printdouble("avg sigs/teamID: ", avgsigspteamid);
    warnx << "\n";
    avglists = (double)nlists / (double)teamindex.size();
    printdouble("avg lists/teamID: ", avglists);
    warnx << "\n";
    warnx << "lowest # of lists/teamID: " << low << "\n";
    warnx << "highest # of lists/teamID: " << high << "\n";
}

// print all lists and teamIDs
int
printlistall(int seq)
{
    chordID teamID;
    int totalsigs = 0;

    for (int i = 0; i < (int)totalT.size(); i++) {
        teamID = findteamid(i);
        if (teamID == NULL) {
            warnx << "printlistall: teamID is NULL\n";
            continue;
        }
        warnx << "list T_0: txseq: " << seq
              << " teamID(i=" << i << "): " << teamID
              << " len: " << totalT[i][0].size() << "\n";
        totalsigs += printlist(totalT[i], 0, seq, false);
    }

    warnx << "totalsigs: " << totalsigs << "\n";
    return totalsigs;
}

// print all lists and teamIDs
int
lprintlistall(int seq)
{
    chordID teamID;
    int totalsigs = 0;

    for (int i = 0; i < (int)totalT.size(); i++) {
        teamID = findteamid(i);
        if (teamID == NULL) {
            warnx << "lprintlistall: teamID is NULL\n";
            continue;
        }
        warnx << "list T_0: txseq: " << seq
              << " teamID(i=" << i << "): " << teamID
              << " len: " << totalT[i][0].size() << "\n";
        totalsigs += printlist(totalT[i], 0, seq, false);
        // TODO
        //totalsigs += lprintlist(totalT[i], 0, seq, false);
    }

    warnx << "totalsigs: " << totalsigs << "\n";
    return totalsigs;
}

int
printlist(vecomap teamvecomap, int listnum, int seq, bool hdr)
{
    bool multi;
    int n, nmulti, ixsim, setsize, totalsigs;
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
    totalsigs = n = nmulti = ixsim = 0;

    // print header
    if (hdr == true) {
        warnx << "list T_" << listnum << ": txseq: " << seq
              << " len: " << teamvecomap[listnum].size() << "\n";
    }

    warnx << "hdrB: sig freq weight avg avg*peers avg*teamsize cmp2prev multi size\n";
    for (mapType::iterator itr = teamvecomap[listnum].begin(); itr != teamvecomap[listnum].end(); itr++) {
        sig = itr->first;
        freq = itr->second[0];
        weight = itr->second[1];
        ++totalsigs;
        avg = freq / weight;
        sumavg += avg;
        sumsum += (avg * peers);
        sig2str(sig, sigbuf);
        warnx << "sig" << n << ": " << sigbuf;
        printdouble(" ", freq);
        printdouble(" ", weight);
        printdouble(" ", avg);
        printdouble(" ", avg * peers);
        printdouble(" ", avg * teamsize);
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
        warnx << " " << multi;
        warnx << " " << sig.size() * sizeof(POLY) << "\n";
        ++n;
        prevsig = sig;
    }
    warnx << "hdrE: sig freq weight avg avg*peers avg*teamsize cmp2prev multi size\n";
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

    // subtract dummies
    return totalsigs - 2;
}

int
lprintlist(lvecomap teamvecomap, int listnum, int seq, bool hdr)
{
    bool multi;
    int n, nmulti, ixsim, setsize, totalsigs;
    long double freq, weight, avg;
    long double sumavg, sumsum;
    long double avgsim, highsim, cursim;
    std::vector<POLY> sig;
    std::vector<POLY> prevsig;
    sig.clear();
    prevsig.clear();
    str sigbuf;

    sumavg = sumsum = 0;
    avgsim = highsim = cursim = 0;
    totalsigs = n = nmulti = ixsim = 0;

    // print header
    if (hdr == true) {
        warnx << "list T_" << listnum << ": txseq: " << seq
              << " len: " << teamvecomap[listnum].size() << "\n";
    }

    warnx << "hdrB: sig freq weight avg avg*peers avg*teamsize cmp2prev multi size\n";
    for (lmapType::iterator itr = teamvecomap[listnum].begin(); itr != teamvecomap[listnum].end(); itr++) {
        sig = itr->first;
        freq = itr->second[0];
        weight = itr->second[1];
        ++totalsigs;
        avg = freq / weight;
        sumavg += avg;
        sumsum += (avg * peers);
        sig2str(sig, sigbuf);
        warnx << "sig" << n << ": " << sigbuf;
        lprintdouble(" ", freq);
        lprintdouble(" ", weight);
        lprintdouble(" ", avg);
        lprintdouble(" ", avg * peers);
        lprintdouble(" ", avg * teamsize);
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
        lprintdouble(" ", cursim);
        warnx << " " << multi;
        warnx << " " << sig.size() * sizeof(POLY) << "\n";
        ++n;
        prevsig = sig;
    }
    warnx << "hdrE: sig freq weight avg avg*peers avg*teamsize cmp2prev multi size\n";
    lprintdouble("printlist: sumavg: ", sumavg);
    lprintdouble(" multisetsize: ", sumsum);
    // subtract dummy (what about 2 dummies?)
    setsize = teamvecomap[listnum].size() - 1;
    warnx << " setsize: " << setsize;
    warnx << " highsim@sig" << ixsim;
    lprintdouble(": ", highsim);
    // 1st sig doesn't have sim
    if ((setsize - 1) == 0)
        avgsim = 0;
    else
        avgsim /= (setsize - 1);
    lprintdouble(" avgsim: ", avgsim);
    warnx << " multisets: " << nmulti << "\n";

    // subtract dummies
    return totalsigs - 2;
}

void
usage(void)
{
    warn << "Usage: " << __progname << " [-h] [actions...] [options...]\n\n";
    /*
    warn << "EXAMPLES:\n\n";
    warn << "Send signature query:\n";
    warn << "\t" << __progname << " -Q -S dhash-sock -s sigdir -F hashfile -d 1122941 -j irrpoly-deg9.dat -B 5 -R 10 -Z 4\n\n";
    warn << "Send xpath query:\n";
    warn << "\t" << __progname << " -Q -S dhash-sock -x xpathdir -F hashfile -d 1122941 -j irrpoly-deg9.dat -B 10 -R 16\n\n";
    warn << "LSH on sig dir:\n";
    warn << "\t" << __progname << " -H -I -s sigdir -F hashfile -d 1122941 -j irrpoly-deg9.dat -B 10 -R 16 -p\n\n";
    warn << "Generate init file for sigdir:\n";
    warn << "\t" << __progname << " -r -s sigdir -P initfile\n\n";
    warn << "VanillaXGossip:\n";
    warn << "\t" << __progname << " -S dhash-sock -G g-sock -L log.gpsi -s sigdir -g -t 120 -q 165 -p\n\n";
    warn << "XGossip:\n";
    warn << "\t" << __progname << " -S dhash-sock -G g-sock -L log.gpsi -s sigdir -g -t 120 -T 1 -w 900\n"
         << "\t     -H -d 1122941 -j irrpoly-deg9.dat -B 50 -R 10 -I -E -P initfile -p\n\n";
    warn << "Broadcast:\n";
    warn << "[TODO]\n";
    warn << "XGossip exec (churn peers):\n";
    warnx << "\t" << __progname << " -c -L log.gpsi -t 240 -A 9555 -q 1000 -N 20 -o 20 -k 5 -W 300\n\n";
    */

    warn << "OPTIONS:\n\n"
             << "\tACTIONS (optional):\n"
         << "   -C      compress signatures (XGossip exec only)\n"
         << "   -J      don't consume msgs which failed to send\n"
                    "\t\t\t(affects mass conservation)\n"
         << "   -H      generate chordIDs/POLYs using LSH when gossiping\n"
                    "\t\t\t(requires -g, -s, -d, -j, -I, -w, -F)\n"
         << "       -m      use findMod instead of compute_hash\n"
                    "\t\t\t(vector of POLYs instead of chordIDs)\n"
         << "       -p      verbose (print list of signatures)\n"
         << "(  -u      make POLYs unique (convert multiset to set))\n"
                    "\t\t\t(don't use)\n\n"

             << "\tDIRECTORIES:\n"
         << "   -a      <dir with xpath txt files>\n"
         << "   -D      <dir with files to be merged>\n"
         << "   -O      <dir with proxy sig>\n"
         << "   -s      <dir with sigs>\n"
         << "   -x      <dir with xpath sig files>\n\n"

             << "\tFILES:\n"
         << "       -F      <hash funcs file>\n"
         << "       -f      <killed hosts file>\n"
         << "       -i      <chordIDs file>\n"
         << "       -j      <irrpoly file>\n"
         << "   -L      <log file>\n"
         << "       -P      <init phase file>\n"
                    "\t\t\t(after XGossip init state is complete)\n\n"

             << "\tTEXT:\n"
         << "   -e      <xpath query string>\n\n"

             << "\tNUMBERS:\n"
         << "   -A      <bootstrap port>\n"
         << "   -B      <bands for LSH>\n"
                    "\t\t\t(a.k.a. m groups)\n"
         << "   -d      <random prime number for LSH seed>\n"
         << "   -K      <range of rounds to kill itself>\n"
                    "\t\t\t(simulate failures: random killing)\n"
         << "   -k      <session rounds for churn peers>\n"
                    "\t\t\t(simulate churn: join/leave)\n"
         << "       -N      <how many instances/nodes>\n"
         << "       -n      <how many chordIDs>\n"
         << "       -o      <how many rounds>\n"
         << "       -q      <estimate of # of peers in DHT>\n"
         << "   -R      <rows for LSH>\n"
                    "\t\t\t(a.k.a. l hash functions)\n"
         << "   -U      <my chordID>\n"
         << "   -X      <how many times>\n"
                    "\t\t\t(multiply freq of sigs by)\n"
         << "   -Z      <team size>\n\n"

             << "\tPHASES/TYPES:\n"
         << "   -E      exec phase of XGossip (requires -H)\n"
         << "   -I      init phase of XGossip (requires -H)\n"
         << "   -y      <init | results>\n"
                    "\t\t\t(type for merging)\n\n"

             << "\tSOCKETS:\n"
         << "   -G      <gossip socket>\n"
         << "   -S      <dhash socket>\n\n"

             << "\tTIMES:\n"
         << "       -T      <how often>\n"
                    "\t\t\t(interval between inserts in XGossip)\n"
         << "       -t      <how often>\n"
                    "\t\t\t(gossip interval)\n"
         << "       -W      <how long>\n"
                                        "\t\t\t(wait interval before init/exec phase starts)\n"
         << "       -w      <how long>\n"
                    "\t\t\t(wait interval after XGossip init phase is done)\n\n"

         << "ACTIONS:\n"
         << "   -b      broadcast: no gossip\n"
                    "\t\t\t(use baseline broadcast protocol instead)\n"
         << "   -c      simulate churn: discard messages (requires -t, -A, -q, -N, -o, -W, -k)\n"
         << "   -g      gossip (requires -S, -G, -s, -t)\n"
         << "   -H      generate chordIDs/POLYs using LSH (requires  -s, -d, -j, -F)\n"
                    "\t\t\t(XGossip)\n"
         << "   -l      listen for gossip (requires -S, -G, -s)\n"
         << "   -M      merge {results | init states} (requires -D, -y, -Z)\n"
         << "   -Q      send query (requires -S, -d, -j, -s or -x)\n"
         << "   -r      read signatures (requires -s and/or -x)\n"
         << "   -V      Vanilla (when querying)\n"
         << "   -v      print version\n"
         << "   -z      generate random chordID (requires -n)\n";
    exit(0);
}
