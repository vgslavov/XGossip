/*	$Id: utils.h,v 1.23 2012/03/15 16:53:14 vsfgd Exp vsfgd $	*/

// Author: Praveen Rao
#ifndef _UTILS_H_
#define _UTILS_H_

#include <string>
#include <vector>
#include <map>
#include <fstream>

#include "../tools/poly.h"
#include "../tools/nodeparams.h"
#include "../tools/retrievetypes.h"

//using namespace std;
 std::string cleanString(std::string &);
 double getgtod();
const int MAXKEYLEN = 1028;
const int PAGESIZE = 8192;
const int MAXBUFS = 2000;

// vsfgd
//const str GSOCK = "g-sock";

// dp244: all LSH code
class lsh {
	public:
	// constructor
	// k = number of elements in the signature
	// l = number of independent hash functions in each group (r rows)
	// m = number of groups (b bands)
	// n = random number seed
	lsh(int kc, int lc, int mc, int nc, int colc = 0, std::vector<POLY> irrnumbers, std::vector<int> hasha, std::vector<int> hashb) {
		k = kc;
		l = lc;
		m = mc;
		n = nc;
		col = colc;

		// loading poly's
		/*
		std::ifstream polystream;
		polystream.open(irrfile.c_str());
		POLY number;
		std::vector<POLY> numbers;
		while (polystream >> number) {
			numbers.push_back(number);
		}
		polystream.close();
		*/

		//randomly choose a poly
		srand(n);
		int range = irrnumbers.size() + 1;
		int rand_index = int((double)range*rand()/(RAND_MAX + 1.0));

		selected_poly = irrnumbers[rand_index];
		//irrpolyf = irrfile;
		//hashf = hashfile;

		// loading hash funcs
		highest = n;
		/*
		std::ifstream hashstream;
		hashstream.open(hashfile.c_str());
		int hashnum;
		randa.clear();
		randb.clear();
		int z = 0;
		while (hashstream >> hashnum) {
			++z;
			if (z <= l)
				randa.push_back(hashnum);
			else
				randb.push_back(hashnum);
		}
		assert(randa.size() == randb.size());
		assert((int)randa.size() == l);
		hashstream.close();
		*/
		irrnums.clear();
		randa.clear();
		randb.clear();
		irrnums = irrnumbers;
		randa = hasha;
		randb = hashb;
	}

	// destructor
	~lsh() {};
	std::vector<POLY> getHashCodeFindMod(std::vector<POLY>& S,POLY polNumber);
	std::vector<chordID> getHashCode(std::vector<POLY>& S);
	POLY isMinimum(std::vector<POLY>& v);
	POLY getIRRPoly();
        std::vector<POLY> getUniqueSet(std::vector<POLY> inputPols);
	// obsolete: instead give prime number when you create LSH object
	//int getPrimeNumberFromConfig(char* configFileName);

	// private variables
	private:
	int k;
	int l;
	int m;
	int n;
	int col;
        POLY selected_poly;
	//std::string irrpolyf;
	//std::string hashf;
	std::vector<POLY> irrnums;
	std::vector<int> randa;
	std::vector<int> randb;
	int highest;
};

enum OPTYPE {
    EQUAL = 0,
    GREATER = 1,
    LESS = 2,
    GREATEREQUAL = 3,
    LESSEQUAL = 4
};

const int MAXSIGSPERQUERY = 50;
#define NODEID int
// Distinct tags
const char *const TAGFILE = ".tags";
const char *const PATHTAGFILE = ".ptags";
const char *const TAGDEPTH = ".tdepth";
const char *const BEGINTAG = "--BEGINTAG--";
const char *const ENDTAG = "--ENDTAG--";
const char *const PATHFILE = ".path";
const char *const HISTFILE = ".hist";
const char *const TEXTHASHFILE = ".texthash";
const char *const VALFILE = ".value";

// Polynomial related
const POLY testPoly = (1 << MAXMODDEGREE);
const POLY ONEFLAG = 0xffffffff;
const POLY lowerBitsMask = ONEFLAG;

 POLY modularMultPoly(POLY, POLY, IRRPOLY);
 POLY findMod(void *, int, IRRPOLY);
 POLY remainder(std::vector<POLY> &, POLY);
	
 int getDegree(IRRPOLY);
 int getDegree(const std::vector<POLY>&);
 void multiplyPoly(std::vector<POLY>&, POLY);
 void multiplyPoly(std::vector<POLY>&, const std::vector<POLY>&, POLY);
 void multiplyPoly(std::vector<POLY>&, const std::vector<POLY>&, const std::vector<POLY>&);

 void addPoly(std::vector<POLY>&, const std::vector<POLY>&);

 void shiftLeft(std::vector<POLY>&, int);
 void shiftRight(std::vector<POLY>&, int);

 void remainder(std::vector<POLY>&, const std::vector<POLY>&, const std::vector<POLY>&);
 void gcd(std::vector<POLY>&, const std::vector<POLY>&, const std::vector<POLY>&);
 void gcdSpecial(std::vector<POLY>&, const std::vector<POLY>&, const std::vector<POLY>&);
 void lcm(std::vector<POLY>&, const std::vector<POLY>&);

// vsfgd
// copied from psi.C
struct CompareSig
{
	bool operator()(const std::vector<POLY>& s1, const std::vector<POLY>& s2) const
	{
		// Return TRUE if s1 < s2
		//warnx << "S1 size: " << s1.size() << " S2 size: " << s2.size() << "\n";
		if (s1.size() < s2.size()) return true;
		else if (s1.size() == s2.size()) {
			for (int i = 0; i < (int) s1.size(); i++) {
				if (s1[i] < s2[i]) return true;
				else if (s1[i] > s2[i]) return false;
			}
			return false;
		} else return false;
	}
};

// Interval for a node
struct Interval {
	int ln, ld, rn, rd;
	int level;
  int random;
	Interval() {
		ln = 0;
		ld = 1;
		rn = 1;
		rd = 1;
	}

    // == operator
    bool operator==(const Interval& testInt) {
        if (ln == testInt.ln && ld == testInt.ld &&
            rn == testInt.rn && rd == testInt.rd &&
            level == testInt.level) {
            return true;
        }
        else
            return false;
    }

    // != operator
    bool operator!=(const Interval& testInt) {
        if (ln == testInt.ln && ld == testInt.ld &&
            rn == testInt.rn && rd == testInt.rd &&
            level == testInt.level) {
            return false;
        }
        else
            return true;
    }

};

 str marshal_block(vec<str>&);
 vec<str> get_payload(char *, int);

 void getInterval(str&, Interval&);

 void getKeyValueSpecial(const char *, str&);

// vsfgd: push_sum
 void getKeyValue(str&, str&, double&, double&);
 void makeKeyValue(char **, int&, str&, double&, double& InsertType);
 void getKeyValue(const char *, str&, double&, double&);

// vsfgd: vxgossip/xgossip
 int getKeyValueLen(const char* buf);
 InsertType getKeyValueType(const char* buf);

// vsfgd: vxgossip/xgossip exec
 int getKeyValue(const char*, str&, str&, std::vector<std::vector<POLY> >&, std::vector<double>&, std::vector<double>&, int&, int);
 void makeKeyValue(char **, int&, str&, str&, std::map<std::vector<POLY>, std::vector<double>, CompareSig>&, int&, InsertType);
// with compression
void makeKeyValue(char **, int&, str&, str&, std::vector<POLY>&, std::vector<std::vector<unsigned char> >&, std::vector<double>&, std::vector<double>&, int&, InsertType);
int getKeyValue(const char*, str&, str&, std::vector<POLY>&, std::vector<std::vector<unsigned char> >&, int&, std::vector<double>&, std::vector<double>&, int&, int);
// with compression, long double
void makeKeyValue(char **, int&, str&, str&, std::vector<POLY>&, std::vector<std::vector<unsigned char> >&, std::vector<long double>&, std::vector<long double>&, int&, InsertType);
int getKeyValue(const char*, str&, str&, std::vector<POLY>&, std::vector<std::vector<unsigned char> >&, int&, std::vector<long double>&, std::vector<long double>&, int&, int);

// vsfgd: xgossip query
int getKeyValue(const char*, str&, str&, str&, std::vector<POLY>&, int&, int);
void makeKeyValue(char **, int&, str&, str&, str&, std::vector<POLY>&, int&, InsertType);

// vsfgd: xgossip init
 int getKeyValue(const char*, str&, str&, std::vector<POLY>&, double&, double&, int);
 void makeKeyValue(char **, int&, str&, str&, std::vector<POLY>&, double&, double&, InsertType);
// vsfgd: xgossip inform_team
 int getKeyValue(const char*, str&, int);
 void makeKeyValue(char **, int&, str&, std::vector<chordID>&, InsertType);

// vsfgd: gpsi (old)
 void makeKeyValue(char **, int&, str&, std::map<std::vector<POLY>, double, CompareSig>&, std::map<std::vector<POLY>, double, CompareSig>&, int&, InsertType);

 void getKeyValue(str&, str&, str&);
 void getKeyValue(const char *, str&, str&);
 void getKeyValue(const char*, str&, std::vector<POLY>&);
 void getDocid(const char *, std::vector<POLY>&, str&);

 void makeKeyValue(char **, int&, str&, std::vector<POLY>&, InsertType);
 void makeKeyValue(char **, int&, str&, Interval&, InsertType);
 void makeKeyValue(char **, int&, vec<str>&, std::vector<int>&, InsertType);
 void makeKeyValueSpecial(char **, int&, vec<str>&, std::vector<int>&, InsertType);
 void makeKeyValue(char **, int&, str&, Interval&);
 void makeKeyValue(char **, int&, str&, std::vector<POLY>&, Interval&,
    InsertType);
 void makeKeyValue(char **ptr, int& len, std::vector<POLY>& textHash,
    std::vector<POLY>& sig, int docId, InsertType type);
 void makeDocid(char **ptr, int *ptrlen, std::vector<POLY>& sig, char *docid);

 void cleanup(char *);
 std::string getString(str &);
 void readTags(FILE *, std::vector<std::string>&);

 void updateMBR(std::vector<POLY>&, std::vector<POLY>&);
 bool isValue(std::string& input);
void lcmSpecial(std::vector<POLY>& t, const std::vector<POLY>& s, std::vector<POLY>& g);
int pSimOpt(std::vector<POLY>&, std::vector<POLY>&, std::vector<POLY>&, bool);
int pSim(std::vector<POLY>&, std::vector<POLY>&,
	std::vector<POLY>&, bool);
str pickChild(vec<str>&, std::vector<POLY>&, bool);
str pickChildV(vec<str>&, std::vector<POLY>&);
int enlargement(std::vector<POLY>&, std::vector<POLY>&);
void makeSigData(str& sigdata, std::vector<std::vector<POLY> >& listOfSigs, 
	std::vector<POLY>& valSig, enum OPTYPE OP, enum RetrieveType type);
void makeSigData(str& sigdata, std::vector<std::vector<POLY> >& listOfSigs, enum RetrieveType type);
void makeSigData(str& sigdata, std::vector<std::vector<POLY> >& listOfSigs, std::vector<POLY>&, enum OPTYPE, enum RetrieveType type);
void makeSigData(str& sigdata, std::vector<POLY>& mySig, enum RetrieveType type);
int randomNumGen(int r);
int randomNumGenZ(int r);
bool isStrValue(std::string& input);

// raopr: all compression code

bool compressSignatures(std::vector<std::vector<POLY> >&, 
			std::vector<POLY>&, std::vector<std::vector<unsigned char> >&);
bool uncompressSignatures(std::vector<std::vector<POLY> >&, 
			  std::vector<POLY>&, std::vector<std::vector<unsigned char> >&, int);

void makeKeyValue(char **, std::vector<POLY>&, 
		  std::vector<std::vector<unsigned char> >&, int);

void getKeyValue(const char *, std::vector<POLY>&, 
		 std::vector<std::vector<unsigned char> >&, int&);

#endif
