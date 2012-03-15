/*	$Id: utils.C,v 1.46 2012/03/07 17:22:50 vsfgd Exp vsfgd $	*/

// Author: Praveen Rao
#include <iostream>
#include <sys/time.h>
#include <assert.h>
#include <stdlib.h>
#include <math.h>

#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include <chord.h>
#include <dhash_common.h>
#include <dhashclient.h>
#include <dhblock.h>

#include "utils.h"

bool isStrValue(std::string& input)
{
    for (int i = 0; i < (int) input.size(); i++) {
        if (input.at(i) > '9' || input.at(i) < '0') {
            return false;
        }
    }
    return true;
}



void readTags(FILE *fp, std::vector<std::string>& distinctTags)
{
	char buf[128];
	bool start = false, end = false;
	
	while (1) {
		if (fscanf(fp, "%s", buf) == EOF) break;

	  if (strcmp(buf, ENDTAG) == 0) {
			end = true;
			break;
		}
		else if (strcmp(buf, BEGINTAG) == 0) {
			start = true;
			continue;
		}
		else {
			distinctTags.push_back(std::string(buf));
		}
	}

	assert(start && end);
	return;
}

// Cleanup a string by deleting special characters
std::string cleanString(std::string &s)
{
	std::string res = s;

	bool flag = false;
	for (int i = (int) s.length()-1; i >= 0; i--) {
    switch(res[i]) {
		  case ' ':
			case '\t':
			case '\n':
		  case '\r':
				res.erase(i, 1);
				break;
		  default:
			  flag = true;
			
		}

		if (flag) {
      break;
		}
	}

	flag = false;
  for (int i = 0; i < (int) res.length(); ) {
    switch(res[i]) {
		  case ' ':
			case '\t':
			case '\n':
		  case '\r':
				res.erase(i, 1);
				break;
		  default:
			  flag = true;
			
		}

		if (flag) {
      break;
		}
	}

	return res;
}


double getgtod()
{
	static struct timeval gtod_time;
	gettimeofday(&gtod_time, NULL);
	return (double)gtod_time.tv_sec
		+ (double)gtod_time.tv_usec / CLOCKS_PER_SEC;
}


// Polynomial processing functions!
// Multiply the multiplicand with the multiplier
void multiplyPoly(std::vector<POLY>& multiplicand, POLY multiplier)
{
	unsigned long long product = 0;

	POLY x;
	unsigned long long y;

	for (int i = multiplicand.size() - 1; i >= 0; i--) {
		x = multiplier;
		y = multiplicand[i];
		while (x > 0) {
			if (x & 1 > (POLY) 0) {
				product = product ^ y;
			}
			x = x >> 1;
			y = y << 1;
		}
		// copy the lower bit set to the result
		multiplicand[i] = product & lowerBitsMask;
		product = product >> (sizeof(POLY) * 8);
	}
	
	if (product > 0) {
		multiplicand.insert(multiplicand.begin(), product & lowerBitsMask);
	}
}

// Polynomial processing functions!
// Multiply the multiplicand with the multiplier
void multiplyPoly(std::vector<POLY>& res, const std::vector<POLY>& multiplicand,
									POLY multiplier)
{
	unsigned long long product = 0;

	assert(res.size() == multiplicand.size());
	
	POLY x;
	unsigned long long y;

	for (int i = multiplicand.size() - 1; i >= 0; i--) {
		x = multiplier;
		y = multiplicand[i];
		while (x > 0) {
			if (x & 1 > (POLY) 0) {
				product = product ^ y;
			}
			x = x >> 1;
			y = y << 1;
		}
		// copy the lower bit set to the result
		//res.insert(res.begin(), product & lowerBitsMask);
		res[i] = product & lowerBitsMask;
		//multiplicand[i] = product & lowerBitsMask;
		product = product >> (sizeof(POLY) * 8);
	}
	
	if (product > 0) {
		//multiplicand.insert(multiplicand.begin(), product & lowerBitsMask);
		res.insert(res.begin(), product & lowerBitsMask);
	}
}

// Compute x * y mod z
POLY modularMultPoly(POLY x, POLY y, IRRPOLY z)
{
#ifdef _DEBUG_
	cout << "Args: " << x << " " << y << " " << z << endl;
#endif
	
	POLY res = 0;
	POLY x1 = x;

	POLY y1 = y;
	POLY modMask = (z ^ testPoly);

	while (y1 > (POLY) 0) {
		if (y1 & 1 > (POLY) 0) {
		  res = res ^ x1;
		}

		x1 = x1 << 1;

		// need casting
		if ((x1 & testPoly) > (POLY) 0) {
			x1 = (x1 ^ testPoly) ^ modMask;
		}
				
		y1 = y1 >> 1;
	}

	return res;
}

// Compute x mod z
POLY findMod(void *x, int len, IRRPOLY z)
{
	unsigned char *ptr = (unsigned char *) x;
 
	unsigned char bitMask = 1;
	int zDegree = getDegree(z);
#ifdef _DEBUG_
	cout << "POLY degree: " << zDegree << endl;
#endif
	POLY testMask = (1 << zDegree);
	POLY modMask = (z ^ (1 << zDegree));

	POLY res = 0;
	POLY resMask = modMask;
	int k = 0;
#ifdef _DEBUG_
	cout << "LENGTH: " << len << " Z: " << z << endl;
#endif

	POLY smallBitsMask = 1;
	for (int i = 0; i < len; i++) {
		for (int j = 0; j < 8; j++) {
			if (k >= zDegree) {
				if ((POLY) (*ptr & bitMask) > (POLY) 0) {
					res = res ^ resMask;
 				}
				resMask = resMask << 1;
				if ((POLY) (resMask & testMask) > (POLY) 0) {
					resMask = (resMask ^ testMask) ^ modMask;
				}
			}
			else {
				if ((POLY) (*ptr & bitMask) > (POLY) 0) {
					res = res | smallBitsMask;
				}
				smallBitsMask = smallBitsMask << 1;
			}
			k++;
			bitMask = bitMask << 1;
		}
			
		ptr += 1;
		bitMask = 1;
	}
#ifdef _DEBUG_
	cout << "TEXT mod: " << (char *) x << "=" << res << endl;
#endif
	return res;
}

// Return the remainder of x / y
POLY remainder(std::vector<POLY> &input, POLY z)
{
	POLY x; 
	POLY divisorMask = z;

	int degree = getDegree(z);
	while ((divisorMask & testPoly) == (POLY) 0) {
		divisorMask = divisorMask << 1;
	}

	int len = input.size();
	int i = 0;
	int bitNum = 0;
	int bitLimit = sizeof(POLY) * 8;
	int stopBitNum = bitLimit - degree;

	// Make sure that the MSW and the divisor are aligned with
	// their 1 bits together
	x = input[i];

	bool breakFlag = false;
	while (i < len - 1) {
		while (!(x & testPoly)) {
			x = x << 1;
			bitNum++;
			if (x == (POLY) 0 || bitNum == bitLimit) {
				bitNum = 0;
				i++;
				x = x ^ input[i];
				breakFlag = true;
				break;
			}
		}
			
		if (!breakFlag) {
			x = x ^ divisorMask;
		}
		else {
			breakFlag = false;
		}
	}
	
	while (i < len) {
		while (!(x & testPoly)) {
			x = x << 1;
			bitNum++;
			if (x == (POLY) 0 || bitNum == stopBitNum) {
				i++;
				break;
			}
		}

		if (i < len)
		  x = x ^ divisorMask; 
	}
#ifdef _DEBUG_
	cout << "Remainder = " << x << endl;
#endif
	return x;
}


// Returns the degree of the polynomial
int getDegree(POLY z)
{
	POLY mask = (1 << (sizeof(POLY) * 8 - 1));

	int i = sizeof(POLY) * 8 - 1;
	while (mask > (POLY) 0) {
		if ((mask & z) != (POLY) 0) {
			return i;
		}
		i--;
		mask = mask >> 1;
	}
	return i;
}


// Degree of poly
int getDegree(const std::vector<POLY>& myPoly)
{
	if (myPoly.size() == 0) {
		assert(0);
	}
	
	int deg = getDegree(myPoly[0]);
	return ((myPoly.size() - 1) * sizeof(POLY) * 8) + deg;
}

// Shift left
void shiftLeft(std::vector<POLY>& myPoly, int numBits)
{
	if (numBits == 0) return;
	if (numBits == sizeof(POLY) * 8) {
		myPoly.push_back((POLY) 0);
		return;
	}
	
	assert(numBits > 0 && numBits <= (int) sizeof(POLY) * 8);
	
#ifdef _DEBUG_
	// Output before shifting
	cout << "Before left shift: ";
	for (int i = 0; i < myPoly.size(); i++) {
		printf("%x ", myPoly[i]);
	}
	cout << endl;
#endif
	
	POLY mask = (ONEFLAG << (sizeof(POLY) * 8 - numBits));
	POLY prevmsb = 0;
	for (int i = myPoly.size()-1; i >= 0; i--) {
		POLY msb = myPoly[i] & mask;
		msb = msb >> (sizeof(POLY) * 8 - numBits);
		myPoly[i] = myPoly[i] << numBits;
		
		if (i < (int) myPoly.size()-1) {
			myPoly[i] |= prevmsb;
		}
		prevmsb = msb;
	}

	// extra bits
	if (prevmsb > (POLY) 0) {
		myPoly.insert(myPoly.begin(), prevmsb);
	}
	
#ifdef _DEBUG_
	// Output before shifting
	cout << "After left shift: ";
	for (int i = 0; i < myPoly.size(); i++) {
		printf("%x ", myPoly[i]);
	}
	cout << endl;
#endif
	
}

// Shift right
void shiftRight(std::vector<POLY>& myPoly, int numBits)
{
	if (numBits == 0 || numBits == sizeof(POLY) * 8) return;
	
	//assert(numBits > 0 && numBits < (int) sizeof(POLY) * 8);

#ifdef _DEBUG_
	// Output before shifting
	cout << "Before right shift: " << numBits << "bits ";
	for (int i = 0; i < myPoly.size(); i++) {
		printf("%x ", myPoly[i]);
	}
	cout << endl;
#endif
	
	POLY mask = (ONEFLAG >> (sizeof(POLY) * 8 - numBits));
	POLY prevlsb = 0;
	for (int i = 0; i < (int) myPoly.size(); i++) {	
		POLY lsb = myPoly[i] & mask;
		lsb = lsb << (sizeof(POLY) * 8 - numBits);
		myPoly[i] = myPoly[i] >> numBits;

		if (i > 0) {
			myPoly[i] |= prevlsb;
		}
		
		prevlsb = lsb;
		
	}

	if (prevlsb > (POLY) 0) {
		myPoly.push_back(prevlsb);
	}

	// Remove empty leading entries
	if (myPoly.size() > 0 && myPoly[0] == (POLY) 0) {
		myPoly.erase(myPoly.begin());
	}
#ifdef _DEBUG_
	// Output before shifting
	cout << "After right shift: ";
	for (int i = 0; i < myPoly.size(); i++) {
		printf("%x ", myPoly[i]);
	}
	cout << endl;
#endif
}

// Add a special check to have the larger degree poly on the left
// and smaller degree polynomial on the right.
void gcdSpecial(std::vector<POLY>& resPoly, const std::vector<POLY>& f, const std::vector<POLY>& g)
{
	int degf = getDegree(f);
	int degg = getDegree(g);
#ifdef _DEBUG_
	cout << "DEG f: " << degf
			 << " DEG g: " << degg
			 << endl;
#endif

	if (degf >= degg) {
		gcd(resPoly, f, g);
	}
	else {
		gcd(resPoly, g, f);
	}
}

// Compute GCD of two polynomials in GF(2)
void gcd(std::vector<POLY>& resPoly, const std::vector<POLY>& f, const std::vector<POLY>& g)
{
	if (g.size() == 1 && g[0] == (POLY) 0) {
		resPoly = f;
	}
	else {
		std::vector<POLY> r;
		remainder(r, f, g);
		gcd(resPoly, g, r);
	}
}

// Multiply two polynomials
void multiplyPoly(std::vector<POLY>& res, const std::vector<POLY>& f,
									const std::vector<POLY>& g)
{
	std::vector<POLY> partialProd;

	//cout << "prod size: " << partialProd.size() << endl;
	for (int i = 0; i < (int) g.size(); i++) {
		for (int j = 0; j < (int) f.size(); j++) {
			partialProd.push_back((POLY) 0);
		}
																								 
		multiplyPoly(partialProd, f, g[i]);
		addPoly(res, partialProd);

		if (i < (int) g.size() - 1) {
			shiftLeft(res, sizeof(POLY) * 8);
		}

		// clear the list
		partialProd.clear();
	}

}

// Add two polynomials
void addPoly(std::vector<POLY>& x, const std::vector<POLY>& y)
{
	int sizeX, sizeY;
	sizeX = x.size();
	sizeY = y.size();

	int i, j;
	
	for (i = sizeX - 1, j = sizeY - 1; i >= 0 && j >= 0; i--, j--) {
		x[i] ^= y[j];
	}

	for ( ; i < 0 && j >= 0; j--) {
		x.insert(x.begin(), y[j]);
	}
}

// Remainder of two polynomials
void remainder(std::vector<POLY>& r, const std::vector<POLY>& f, const std::vector<POLY>& g)
{
	assert(f.size() > 0 && g.size() > 0);
	
	int fdeg = getDegree(f[0]);
	int gdeg = getDegree(g[0]);

	std::vector<POLY> gtemp(g);
	
	if (fdeg > gdeg) {
		shiftLeft(gtemp, fdeg - gdeg);	
	}
	else {
		shiftRight(gtemp, gdeg - fdeg);
	}

	POLY bitMask = (1 << fdeg);

	int i = 0;
	int fFullDegree = getDegree(f);
	int gFullDegree = getDegree(g);

	/*
	if (fFullDegree < gFullDegree) {
		cout << "F deg: " << fFullDegree << "-" << "G deg: " << gFullDegree << endl;
		assert(0);
	}
	*/
	if (fFullDegree < gFullDegree) { // 2 % 5 = 2
		r = f;
		return;
  }
	//assert(fFullDegree >= gFullDegree);

	// r will contain the remainder eventually.
	r = f;

	for (int k = fFullDegree; k >= gFullDegree; k--) {
		if ((r[i] & bitMask) > (POLY) 0) {
			// Loop thro the polys and XOR each time...
			for (int j = 0; (i + j) < (int) r.size() && (j < (int) gtemp.size()); j++) {
				r[i + j] ^= gtemp[j];
			}

		}

		// Shift right
		// Can be optimized further...
		shiftRight(gtemp, 1);
		bitMask >>= 1;
		if (k % (sizeof(POLY) * 8) == 0) {
			i++;
			bitMask = (1 << (sizeof(POLY) * 8 - 1));
		}
	}

#ifdef _DEBUG_
	cout << "R[] : ";
	for (int i = 0; i < r.size(); i++) {
		cout << r[i] << " ";
	}
	cout << endl;
#endif
	
	// Remove leading empty entries
	int len = r.size() - 1;
	for (int j = 0; j < len; j++) {
		if (r[0] > (POLY) 0) {
			break;	
		}
		r.erase(r.begin());
	}
}

// Remainder of two polynomials
void quotient(std::vector<POLY>& q, const std::vector<POLY>& f, const std::vector<POLY>& g)
{
	assert(f.size() > 0 && g.size() > 0);

	std::vector<POLY> r;
	int fdeg = getDegree(f[0]);
	int gdeg = getDegree(g[0]);

	std::vector<POLY> gtemp(g);
	
	if (fdeg > gdeg) {
		shiftLeft(gtemp, fdeg - gdeg);	
	}
	else {
		shiftRight(gtemp, gdeg - fdeg);
	}

	POLY bitMask = (1 << fdeg);

	int i = 0;
	int fFullDegree = getDegree(f);
	int gFullDegree = getDegree(g);

	/*
	if (fFullDegree < gFullDegree) {
		cout << "F deg: " << fFullDegree << "-" << "G deg: " << gFullDegree << endl;
		assert(0);
	}
	*/
	assert(fFullDegree >= gFullDegree);

	// r will contain the remainder eventually.
	r = f;

	// setup the quotient
	q.clear();
	q.push_back((POLY) 0);

	for (int k = fFullDegree; k >= gFullDegree; k--) {
		shiftLeft(q, 1);
		if ((r[i] & bitMask) > (POLY) 0) {
			// Store quotient
			q[q.size()-1] |= (POLY) 1;
			//cout << "Setting bit..." << endl;
			// Loop thro the polys and XOR each time...
			for (int j = 0; (i + j) < (int) r.size() && (j < (int) gtemp.size()); j++) {
				r[i + j] ^= gtemp[j];
			}
		}
		
		// Shift right
		// Can be optimized further...
		shiftRight(gtemp, 1);
		bitMask >>= 1;
		if (k % (sizeof(POLY) * 8) == 0) {
			i++;
			bitMask = (1 << (sizeof(POLY) * 8 - 1));
		}
	}

#ifdef _DEBUG_
	cout << "R[] : ";
	for (int i = 0; i < r.size(); i++) {
		cout << r[i] << " ";
	}
	cout << endl;
#endif

	/*
	// Remove leading empty entries
	int len = r.size() - 1;
	for (int j = 0; j < len; j++) {
		if (r[0] > (POLY) 0) {
			break;	
		}
		r.erase(r.begin());
	}
	*/
}

// Compute the LCM of two polynomials
void lcm(std::vector<POLY>& t, const std::vector<POLY>& s)
{
	if (t.size() == 1 && s.size() == 1 && t[0] == 0x1 && s[0] == 0x1) {
		return;
	}
	
	std::vector<POLY> g;
	gcdSpecial(g, t, s);

	std::vector<POLY> q;
	int degs = getDegree(s);
	int degg = getDegree(g);

	if (degs >= degg) {
		quotient(q, s, g);
	}
	else {
		quotient(q, g, s);
	}

#ifdef _DEBUG_
	int qDeg = getDegree(q);
	cout << "Quotient degree: " << qDeg << endl;

	cout << "Before LCM degree: " << getDegree(t) << endl;
	cout << "Before LCM: ";
	for (int i = 0; i < t.size(); i++) {
		cout << t[i] << " ";
	}
	cout << endl;
#endif
	
	std::vector<POLY> res;
	multiplyPoly(res, t, q);

	t = res;
	
#ifdef _DEBUG_
	cout << "LCM degree: " << getDegree(t) << endl;
	cout << "LCM: ";
	for (int i = 0; i < t.size(); i++) {
		cout << t[i] << " ";
	}
	cout << endl;

#endif
	return;
}

// range [1 to r]
int randomNumGen(int r)
{
	static unsigned int seed = (unsigned int) getgtod();
	int j;
	j = 1 + (int) (1.0 * r * rand_r(&seed)/(RAND_MAX+1.0));
	return j;
}

// range [0 to r-1]
int randomNumGenZ(int r)
{
	static unsigned int seed = (unsigned int) getgtod();
	int j;
	j = 0 + (int) (1.0 * r * rand_r(&seed)/(RAND_MAX+1.0));
	return j;
}

// More operations
void getInterval(str& s, Interval& interval)
{
    int *ptr = (int *) s.cstr();
    interval.ln = ptr[0];
    interval.ld = ptr[1];
    interval.rn = ptr[2];
    interval.rd = ptr[3];
    interval.level = ptr[4];
    interval.random = ptr[5];
    return;
}

std::string getString(str &s)
{
	const char *ptr = s.cstr();
	int len = s.len();
	char *r = new char[len+1];
	assert(r);
  memcpy(r, ptr, len);
	r[len] = '\0';
	std::string ret(r);
	delete[] r;
	return ret;
}

// format: <size><key><size><value>
void getKeyValueSpecial(const char *ptr, str& a)
{
	int keySize;
	memcpy(&keySize, ptr, sizeof(keySize));
	ptr += sizeof(keySize);

	str tempStrKey(ptr, keySize);
	a = tempStrKey;

	return;
}

// format: <size><key><size><value>
void getKeyValue(str& s, str& a, str& b)
{
	const char *ptr = s.cstr();
	int keySize, valSize;
	memcpy(&keySize, ptr, sizeof(keySize));
	ptr += sizeof(keySize);

	a = str(ptr, keySize);

    ptr += keySize;

	memcpy(&valSize, ptr, sizeof(valSize));
	ptr += sizeof(valSize);

	b = str(ptr, valSize);
	return;
}

// vsfgd: push_sum
// format: <size><key><size><value1><size><value2>
void getKeyValue(str& s, str& a, double& b, double& c)
{
	const char *ptr = s.cstr();
	int keySize, valSize;
	memcpy(&keySize, ptr, sizeof(keySize));
	ptr += sizeof(keySize);

	a = str(ptr, keySize);

	ptr += keySize;

	// copy value1
	memcpy(&valSize, ptr, sizeof(valSize));
	ptr += sizeof(valSize);
	//b = str(ptr, valSize);
	memcpy(&b, ptr, sizeof(b));
	ptr += sizeof(b);

	// copy value2
	memcpy(&valSize, ptr, sizeof(valSize));
	ptr += sizeof(valSize);
	memcpy(&c, ptr, sizeof(c));

	return;
}

// format: <size><key><size><value>
void getKeyValue(const char *ptr, str& a, str& b)
{
	int keySize, valSize;
	memcpy(&keySize, ptr, sizeof(keySize));
	ptr += sizeof(keySize);

	str tempStrKey(ptr, keySize);
	a = tempStrKey;

	ptr += keySize;

	memcpy(&valSize, ptr, sizeof(valSize));
	ptr += sizeof(valSize);

	str tempStrVal(ptr, valSize);
	b = tempStrVal;
	return;
}

// vsfgd: push_sum
// format: <size><key><size><value1><size><value2>
void getKeyValue(const char *ptr, str& a, double& b, double& c)
{
	int keySize, valSize;
	memcpy(&keySize, ptr, sizeof(keySize));
	ptr += sizeof(keySize);

	str tempStrKey(ptr, keySize);
	a = tempStrKey;

	ptr += keySize;

	// copy value1
	memcpy(&valSize, ptr, sizeof(valSize));
	ptr += sizeof(valSize);
	//str tempStrVal(ptr, valSize);
	//b = tempStrVal;
	memcpy(&b, ptr, sizeof(b));
	ptr += sizeof(b);

	// copy value2
	memcpy(&valSize, ptr, sizeof(valSize));
	ptr += sizeof(valSize);
	memcpy(&c, ptr, sizeof(c));

	return;
}

// format: <size><key>
void getKeyValue(const char *ptr, str& a)
{
	int keySize;
	memcpy(&keySize, ptr, sizeof(keySize));
	ptr += sizeof(keySize);

	str tempStrKey(ptr, keySize);
	a = tempStrKey;

	return;
}

// Given a buffer of the format <size><key><size><sig>
void getKeyValue(const char* buf, str& key, std::vector<POLY>& value)
{
	const char *keyPtr;
	int keySize;
	memcpy(&keySize, buf, sizeof(keySize));
	keyPtr = buf + sizeof(keySize);

	key = str(keyPtr, keySize);

	const char *valPtr;
	int valLen;
	valPtr = keyPtr + keySize;

	memcpy(&valLen, valPtr, sizeof(valLen));
	valPtr += sizeof(valLen);

	const POLY *ptr = (POLY *) valPtr;

	for (int i = 0; i < valLen/(int) sizeof(POLY); i++) {
		value.push_back(ptr[i]);
	}

	return;
}

// vsfgd: vxgossip
int getKeyValueLen(const char* buf)
{
	const char *lenPtr;
	InsertType tmp;
	int len;
	lenPtr = buf + sizeof(tmp);
	memcpy(&len, lenPtr, sizeof(len));
	return len;
}

// vsfgd: vxgossip
InsertType getKeyValueType(const char* buf)
{
	InsertType msgType;
	//warnx << "getKeyValueType: before memcpy\n";
	memcpy(&msgType, buf, sizeof(msgType));
	//warnx << "getKeyValueType: after memcpy\n";
	return msgType;
}

// vsfgd: vxgossip/xgossip exec (compression)
// format:
// <msgtype><msglen><keysize><key><teamidsize><teamid><seq>
// <numSigs><compressedsiglistlen><POLY><unsigned char>...
// <freq><weight>...
int getKeyValue(const char* buf, str& key, str& teamid, std::vector<POLY>& compressedList, std::vector<std::vector<unsigned char> >& outBitmap, int& numSigs, std::vector<double>& freqList, std::vector<double>& weightList, int& seq, int recvlen)
{
	// copy msgtype
	InsertType msgType;
	memcpy(&msgType, buf, sizeof(msgType));

	// copy len
	const char *lenPtr;
	int len;
	lenPtr = buf + sizeof(msgType);
	memcpy(&len, lenPtr, sizeof(len));
	//warnx << "getKeyValue: msglen: " << len << "\n";

	// XXX: InsertType
	//if ((recvlen + (int)sizeof(int)) != len) {
	if (recvlen != len) {
		//warnx << "getKeyValue: recvlen: " << recvlen
		//	<< " len: "<< len << "\n";
		warnx << "getKeyValue: len doesn't match\n";
		return -1;
	}

	// copy keysize
	const char *keySizePtr;
	int keySize;
	keySizePtr = lenPtr + sizeof(len);
	memcpy(&keySize, keySizePtr, sizeof(keySize));

	// copy key
	const char *keyPtr;
	keyPtr = keySizePtr + sizeof(keySize);
	key = str(keyPtr, keySize);
	//assert(key);

	// copy teamidsize
	const char *teamidSizePtr;
	int teamidSize;
	teamidSizePtr = keyPtr + keySize;
	memcpy(&teamidSize, teamidSizePtr, sizeof(teamidSize));

	// copy teamid
	const char *teamidPtr;
	teamidPtr = teamidSizePtr + sizeof(teamidSize);
	teamid = str(teamidPtr, teamidSize);

	// copy seq
	const char *seqPtr;
	seqPtr = teamidPtr + teamidSize;
	memcpy(&seq, seqPtr, sizeof(seq));

	// copy numSigs
	const char *numSigsPtr;
	numSigsPtr = seqPtr + sizeof(seq);
	memcpy(&numSigs, numSigsPtr, sizeof(numSigs));

	// copy compressedListLen
	const char *compressedListPtr;
	int compressedListLen;
	compressedListPtr = numSigsPtr + sizeof(numSigs);
	memcpy(&compressedListLen, compressedListPtr, sizeof(compressedListLen));

	// FUCKING pointers!
	compressedListPtr += sizeof(compressedListLen);

	for (int i = 0; i < compressedListLen; i++) {
		compressedList.push_back(*((POLY *) compressedListPtr));
		compressedListPtr += sizeof(POLY);

		std::vector<unsigned char> e;
		e.clear();

		for (int j = 0; j < (int) ceil(numSigs/8.0); j++) {
			e.push_back(*((unsigned char *) compressedListPtr));
			compressedListPtr += sizeof(unsigned char);
		}

		outBitmap.push_back(e);
	}

	std::vector<POLY> sig;
	sig.clear();
	double freq;
	double weight;
	const char *freqPtr = NULL;
	const char *weightPtr = NULL;

	freqPtr = compressedListPtr;
	for (int i = 0; i < numSigs; i++) {
		// copy freq
		memcpy(&freq, freqPtr, sizeof(freq));
		freqList.push_back(freq);

		// copy weight
		weightPtr = freqPtr + sizeof(freq);
		memcpy(&weight, weightPtr, sizeof(weight));
		weightList.push_back(weight);

		freqPtr = weightPtr + sizeof(weight);
	}
	return 0;
}

// vsfgd: vxgossip/xgossip exec
// format: <msgtype><totallen><keysize><key><teamidsize><teamid><seq><siglistlen><sigsize><sig><freq><weight>...
int getKeyValue(const char* buf, str& key, str& teamid, std::vector<std::vector<POLY> >& sigList, std::vector<double>& freqList, std::vector<double>& weightList, int& seq, int recvlen)
{
	// copy msgtype
	InsertType msgType;
	memcpy(&msgType, buf, sizeof(msgType));

	// copy len
	const char *lenPtr;
	int len;
	lenPtr = buf + sizeof(msgType);
	memcpy(&len, lenPtr, sizeof(len));
	//warnx << "getKeyValue: msglen: " << len << "\n";

	// XXX: InsertType
	//if ((recvlen + (int)sizeof(int)) != len) {
	if (recvlen != len) {
		//warnx << "getKeyValue: recvlen: " << recvlen
		//	<< " len: "<< len << "\n";
		warnx << "getKeyValue: len doesn't match\n";
		return -1;
	}

	// copy keysize
	const char *keySizePtr;
	int keySize;
	keySizePtr = lenPtr + sizeof(len);
	memcpy(&keySize, keySizePtr, sizeof(keySize));

	// copy key
	const char *keyPtr;
	keyPtr = keySizePtr + sizeof(keySize);
	key = str(keyPtr, keySize);
	//assert(key);

	// copy teamidsize
	const char *teamidSizePtr;
	int teamidSize;
	teamidSizePtr = keyPtr + keySize;
	memcpy(&teamidSize, teamidSizePtr, sizeof(teamidSize));

	// copy teamid
	const char *teamidPtr;
	teamidPtr = teamidSizePtr + sizeof(teamidSize);
	teamid = str(teamidPtr, teamidSize);

	// copy seq
	const char *seqPtr;
	seqPtr = teamidPtr + teamidSize;
	memcpy(&seq, seqPtr, sizeof(seq));

	// copy sigListLen
	const char *sigListPtr;
	int sigListLen;
	sigListPtr = seqPtr + sizeof(seq);
	memcpy(&sigListLen, sigListPtr, sizeof(sigListLen));

	sigListPtr += sizeof(sigListLen);
	
	std::vector<POLY> sig;
	sig.clear();

	double freq;
	double weight;
	const POLY *ptr;
	const char *freqPtr = NULL;
	const char *weightPtr = NULL;
	int sigLen;

	for (int i = 0; i < sigListLen; i++) {
		// copy sigLen
		memcpy(&sigLen, sigListPtr, sizeof(sigLen));
#ifdef _DEBUG_
		warnx << "getKeyValue: sigLen: " << sigLen << "\n";
#endif

		// copy sig
		sigListPtr += sizeof(sigLen);
		ptr = (POLY *) sigListPtr;
		for (int j = 0; j < sigLen/(int) sizeof(POLY); j++) {
			sig.push_back(ptr[j]);
		}
		sigList.push_back(sig);
		sig.clear();

		// copy freq
		// check if sigLen is valid?
		freqPtr = sigListPtr + sigLen;
		//assert(freqPtr);
		memcpy(&freq, freqPtr, sizeof(freq));
		freqList.push_back(freq);

		// copy weight
		weightPtr = freqPtr + sizeof(freq);
		memcpy(&weight, weightPtr, sizeof(weight));
		weightList.push_back(weight);

		sigListPtr = weightPtr + sizeof(weight);
	}
	return 0;
}

// vsfgd: xgossip init
// format: <msgtype><msglen><keysize><key><teamidsize><teamid><sigsize><sig><freq><weight>
int getKeyValue(const char* buf, str& key, str& teamid, std::vector<POLY>& sig, double& freq, double& weight, int recvlen)
{
	// copy msgtype
	InsertType msgType;
	memcpy(&msgType, buf, sizeof(msgType));

	// copy len
	const char *lenPtr;
	int len;
	lenPtr = buf + sizeof(msgType);
	memcpy(&len, lenPtr, sizeof(len));
	//warnx << "getKeyValue: msglen: " << len << "\n";

	// XXX: InsertType
	//if ((recvlen + (int)sizeof(int)) != len) {
	if (recvlen != len) {
		//warnx << "getKeyValue: recvlen: " << recvlen
		//	<< " len: "<< len << "\n";
		warnx << "getKeyValue: len doesn't match\n";
		return -1;
	}

	// copy keysize
	const char *keySizePtr;
	int keySize;
	keySizePtr = lenPtr + sizeof(len);
	memcpy(&keySize, keySizePtr, sizeof(keySize));

	// copy key
	const char *keyPtr;
	keyPtr = keySizePtr + sizeof(keySize);
	key = str(keyPtr, keySize);

	// copy teamidsize
	const char *teamidSizePtr;
	int teamidSize;
	teamidSizePtr = keyPtr + keySize;
	memcpy(&teamidSize, teamidSizePtr, sizeof(teamidSize));

	// copy teamid
	const char *teamidPtr;
	teamidPtr = teamidSizePtr + sizeof(teamidSize);
	teamid = str(teamidPtr, teamidSize);

	// copy siglen
	const char *sigLenPtr;
	int sigLen;
	sigLenPtr = teamidPtr + teamidSize;
	memcpy(&sigLen, sigLenPtr, sizeof(sigLen));

	sigLenPtr += sizeof(sigLen);

	const POLY *ptr = (POLY *) sigLenPtr;

	for (int i = 0; i < sigLen/(int) sizeof(POLY); i++) {
		sig.push_back(ptr[i]);
	}
	
	// copy freq
	const char *freqPtr;
	freqPtr = sigLenPtr + sigLen;
	memcpy(&freq, freqPtr, sizeof(freq));

	// copy weight
	const char *weightPtr;
	weightPtr = freqPtr + sizeof(freq);
	memcpy(&weight, weightPtr, sizeof(weight));

	return 0;
}

// vsfgd: xgossip query
// format: <msgtype><msglen><keysize><key><teamidsize><teamid><dtdsize><dtd><sigsize><qid>
int getKeyValue(const char* buf, str& key, str& teamid, str& dtd, std::vector<POLY>& sig, int& qid, int recvlen)
{
	// copy msgtype
	InsertType msgType;
	memcpy(&msgType, buf, sizeof(msgType));

	// copy len
	const char *lenPtr;
	int len;
	lenPtr = buf + sizeof(msgType);
	memcpy(&len, lenPtr, sizeof(len));
	//warnx << "getKeyValue: msglen: " << len << "\n";

	// XXX: InsertType
	//if ((recvlen + (int)sizeof(int)) != len) {
	if (recvlen != len) {
		//warnx << "getKeyValue: recvlen: " << recvlen
		//	<< " len: "<< len << "\n";
		warnx << "getKeyValue: len doesn't match\n";
		return -1;
	}

	// copy keysize
	const char *keySizePtr;
	int keySize;
	keySizePtr = lenPtr + sizeof(len);
	memcpy(&keySize, keySizePtr, sizeof(keySize));

	// copy key
	const char *keyPtr;
	keyPtr = keySizePtr + sizeof(keySize);
	key = str(keyPtr, keySize);

	// copy teamidsize
	const char *teamidSizePtr;
	int teamidSize;
	teamidSizePtr = keyPtr + keySize;
	memcpy(&teamidSize, teamidSizePtr, sizeof(teamidSize));

	// copy teamid
	const char *teamidPtr;
	teamidPtr = teamidSizePtr + sizeof(teamidSize);
	teamid = str(teamidPtr, teamidSize);

	// copy dtdsize
	const char *dtdSizePtr;
	int dtdSize;
	dtdSizePtr = teamidPtr + teamidSize;
	memcpy(&dtdSize, dtdSizePtr, sizeof(dtdSize));

	// copy dtd
	const char *dtdPtr;
	dtdPtr = dtdSizePtr + sizeof(dtdSize);
	dtd = str(dtdPtr, dtdSize);

	// copy siglen
	const char *sigLenPtr;
	int sigLen;
	sigLenPtr = dtdPtr + dtdSize;
	memcpy(&sigLen, sigLenPtr, sizeof(sigLen));

	sigLenPtr += sizeof(sigLen);

	const POLY *ptr = (POLY *) sigLenPtr;

	for (int i = 0; i < sigLen/(int) sizeof(POLY); i++) {
		sig.push_back(ptr[i]);
	}
	
	// copy freq
	const char *qidPtr;
	qidPtr = sigLenPtr + sigLen;
	memcpy(&qid, qidPtr, sizeof(qid));

	return 0;
}

// vsfgd: xgossip inform_team?
// format: <msgtype><msglen><keysize><key>
int getKeyValue(const char* buf, str& key, int recvlen)
{
	// copy msgtype
	InsertType msgType;
	memcpy(&msgType, buf, sizeof(msgType));

	// copy len
	const char *lenPtr;
	int len;
	lenPtr = buf + sizeof(msgType);
	memcpy(&len, lenPtr, sizeof(len));
	//warnx << "getKeyValue: msglen: " << len << "\n";

	// XXX: InsertType
	//if ((recvlen + (int)sizeof(int)) != len) {
	if (recvlen != len) {
		//warnx << "getKeyValue: recvlen: " << recvlen
		//	<< " len: "<< len << "\n";
		warnx << "getKeyValue: len doesn't match\n";
		return -1;
	}

	// copy keysize
	const char *keySizePtr;
	int keySize;
	keySizePtr = lenPtr + sizeof(len);
	memcpy(&keySize, keySizePtr, sizeof(keySize));

	// copy key
	const char *keyPtr;
	keyPtr = keySizePtr + sizeof(keySize);
	key = str(keyPtr, keySize);

	return 0;
}

void makeSigData(str& sigdata, std::vector<std::vector<POLY> >& listOfSigs, 
	std::vector<POLY>& valSig, enum OPTYPE OP, enum RetrieveType type)
{
	unsigned int buf[2048];

	// Header
	buf[0] = type;
	buf[1] = 0;

	int index = 2;
	
  // Limit the size 
	int i;
	for (i = 0; i < (int) listOfSigs.size(); i++) {

		if (index + 1 + (int) listOfSigs[i].size() > (int) sizeof(buf)) {
			break;
		}

	  buf[index] = listOfSigs[i].size();	
		index++;
		for (int j = 0; j < (int) listOfSigs[i].size(); j++) {
			buf[index] = listOfSigs[i][j];
			index++;
		}
	}

	buf[1] = i;

	// Now add valSig and OP
	buf[index] = valSig.size();
	index++;
	for (int j = 0; j < (int) valSig.size(); j++) {
		buf[index] = valSig[j];
		index++;
	}
	buf[index] = OP;

	sigdata = str((const char *) buf, index * sizeof(unsigned int));
}

void makeSigData(str& sigdata, std::vector<std::vector<POLY> >& listOfSigs, enum RetrieveType type)
{
	unsigned int buf[2048];

	// Header
	buf[0] = type;
	buf[1] = 0;

	int index = 2;
	
  // Limit the size 
	int i;
	for (i = 0; i < (int) listOfSigs.size(); i++) {

		if (index + 1 + (int) listOfSigs[i].size() > (int) sizeof(buf)) {
			break;
		}

	  buf[index] = listOfSigs[i].size();	
		index++;
		for (int j = 0; j < (int) listOfSigs[i].size(); j++) {
			buf[index] = listOfSigs[i][j];
			index++;
		}
	}

	buf[1] = i;
	sigdata = str((const char *) buf, index * sizeof(unsigned int));
}

void makeSigData(str& sigdata, std::vector<POLY>& mySig, enum RetrieveType type)
{
	unsigned int buf[2048];

	// Header
	buf[0] = type;
	buf[1] = 0;

	int index = 2;
	
		if (index + 1 + (int) mySig.size() > (int) sizeof(buf)) {
			return;
		}

	  buf[index] = mySig.size();	
		index++;
		for (int j = 0; j < (int) mySig.size(); j++) {
			buf[index] = mySig[j];
			index++;
		}

	buf[1] = 1;
	sigdata = str((const char *) buf, index * sizeof(unsigned int));
}

void makeKeyValue(char **ptr, int& len, str& key, std::vector<POLY>& sig,
									InsertType type)
{
	int keyLen = key.len();
  //warnx << "LENGTH: ++++ " << keyLen << "\n";
	int valLen = sig.size() * sizeof(POLY);
	
	if (type == NONE) {
		len = sizeof(int) + keyLen + sizeof(int) + valLen;
	}
	else {
		len = sizeof(int) + sizeof(int) + keyLen + sizeof(int) + valLen;
	}
	
	*ptr = new char[len];
	assert(ptr);
  char *buf = *ptr;

	if (type != NONE) {
		// Copy type
		memcpy(buf, &type, sizeof(type));
		buf += sizeof(type);
	}

	// Copy key
	memcpy(buf, &keyLen, sizeof(keyLen));
	buf += sizeof(keyLen);
	memcpy(buf, key.cstr(), keyLen);
	buf += keyLen;

	// Copy value
	memcpy(buf, &valLen, sizeof(valLen));
	buf += sizeof(valLen);

	POLY *sigPtr = (POLY *) buf;
	for (int i = 0; i < (int) sig.size(); i++) {
		sigPtr[i] = sig[i];
	}
	return;
}

// vsfgd: vxgossip/xgossip exec (compression)
// format:
// <msgtype><msglen><keysize><key><teamidsize><teamid><seq>
// <siglistlen><compressedsiglistlen><POLY><unsigned char>...
// <freq><weight>...
void makeKeyValue(char **ptr, int& len, str& key, str& teamid, std::vector<POLY>& compressedList, std::vector<std::vector<unsigned char> >& outBitmap, std::vector<double>& freqList, std::vector<double>& weightList, int& seq, InsertType type)
{
	int keyLen = key.len();
	int teamidLen = teamid.len();
	int compressedListLen = compressedList.size();
	int sigListLen = freqList.size();

	assert(freqList.size() == weightList.size());
	
	// msgtype + msglen + keysize + key + teamidsize + teamid + seq
	len = sizeof(int) + sizeof(int) + sizeof(int) + keyLen + sizeof(int) + teamidLen + sizeof(int);

	// sigListLen + compressedListLen + compressedListLen * (POLY + sigListLen)
	len += sizeof(int) + sizeof(int) + (int) compressedListLen * (sizeof(POLY) + (int) ceil(sigListLen/8.0));
	//len += sizeof(int) + sizeof(int) + (int) compressedListLen * (sizeof(POLY) + sizeof(unsigned char));

	// freqList + weightList: (2 * sigListLen * weight|freq)
	len += (2 * sigListLen * sizeof(double));

	// XXX: includes 4 bytes for type
	warnx << "makeKeyValue:\ntxmsglen: " << len
	      << " txlistlen: " << sigListLen
	      << " txclistlen: " << compressedListLen << "\n";
	// TODO: New vs new?
	*ptr = New char[len];
	//*ptr = new char[len];
	assert(ptr);
	char *buf = *ptr;

	// copy type
	memcpy(buf, &type, sizeof(type));
	buf += sizeof(type);

	// copy len
	// XXX: is sizeof(type) always equal to sizeof(int)?
	//len = len - sizeof(int);
	//warnx << "makeKeyValue: len (msg): " << len << "\n";
	memcpy(buf, &len, sizeof(len));
	buf += sizeof(len);

	// copy key
	memcpy(buf, &keyLen, sizeof(keyLen));
	buf += sizeof(keyLen);
	memcpy(buf, key.cstr(), keyLen);
	buf += keyLen;

	// copy teamid
	memcpy(buf, &teamidLen, sizeof(teamidLen));
	buf += sizeof(teamidLen);
	memcpy(buf, teamid.cstr(), teamidLen);
	buf += teamidLen;

	// copy seq
	memcpy(buf, &seq, sizeof(seq));
	buf += sizeof(seq);

	// copy sigListLen
	memcpy(buf, &sigListLen, sizeof(sigListLen));
	buf += sizeof(sigListLen);

	// copy compressedListLen
	memcpy(buf, &compressedListLen, sizeof(compressedListLen));
	buf += sizeof(compressedListLen);

	// copy compressedList
	for (int i = 0; i < compressedListLen; i++) {
		memcpy(buf, &compressedList[i], sizeof(POLY));
		buf += sizeof(POLY);

		// copy outBitmap
		for (int j = 0; j < (int) outBitmap[i].size(); j++) {
			memcpy(buf, &outBitmap[i][j], sizeof(unsigned char));
			buf += sizeof(unsigned char);
		}
	}

	// copy freqList and weightList
	for (int i = 0; i < sigListLen; i++) {
		memcpy(buf, &freqList[i], sizeof(double));
		buf += sizeof(double);
		memcpy(buf, &weightList[i], sizeof(double));
		buf += sizeof(double);
	}

	return;
}

// vsfgd: vxgossip/xgossip exec
// format: <msgtype><msglen><keysize><key><teamidsize><teamid><seq><siglistlen><sigsize><sig><freq><weight>...
void makeKeyValue(char **ptr, int& len, str& key, str& teamid, std::map<std::vector<POLY>, std::vector<double>, CompareSig>& sigList, int& seq, InsertType type)
{
	int keyLen = key.len();
	int teamidLen = teamid.len();
	//warnx << "LENGTH: ++++ " << keyLen << "\n";
	
	// msgtype + msglen + keysize + key + teamidsize + teamid + seq + siglistlen
	len = sizeof(int) + sizeof(int) + sizeof(int) + keyLen + sizeof(int) + teamidLen + sizeof(int) + sizeof(int);

	int sigListLen = sigList.size();
	int sigLen;
	std::vector<POLY> sig;
	sig.clear();

	// + sigListLen *
	for (std::map<std::vector<POLY>, std::vector<double> >::iterator itr = sigList.begin();
	     itr != sigList.end(); itr++) {

		sig = itr->first;
		sigLen = sig.size() * sizeof(POLY);
		//warnx << "sigLen: " << sigLen << "\n";
		// (sigsize + sig + freq + weight)
		// FUCKING DOUBLE!
		len += (sizeof(int) + sigLen + sizeof(double) + sizeof(double));
		sig.clear();
		//warnx << "len (after sig n): " << len << "\n";
	}
	// XXX: includes 4 bytes for type
	warnx << "makeKeyValue:\ntxmsglen: " << len
	      << " txlistlen: " << sigListLen << "\n";
	// TODO: New vs new?
	*ptr = New char[len];
	//*ptr = new char[len];
	assert(ptr);
	char *buf = *ptr;

	// copy type
	memcpy(buf, &type, sizeof(type));
	buf += sizeof(type);

	// copy len
	// XXX: is sizeof(type) always equal to sizeof(int)?
	//len = len - sizeof(int);
	//warnx << "makeKeyValue: len (msg): " << len << "\n";
	memcpy(buf, &len, sizeof(len));
	buf += sizeof(len);

	// copy key
	memcpy(buf, &keyLen, sizeof(keyLen));
	buf += sizeof(keyLen);
	memcpy(buf, key.cstr(), keyLen);
	buf += keyLen;

	// copy teamid
	memcpy(buf, &teamidLen, sizeof(teamidLen));
	buf += sizeof(teamidLen);
	memcpy(buf, teamid.cstr(), teamidLen);
	buf += teamidLen;

	// copy seq
	memcpy(buf, &seq, sizeof(seq));
	buf += sizeof(seq);

	// copy sigListLen
	memcpy(buf, &sigListLen, sizeof(sigListLen));
	buf += sizeof(sigListLen);

	double freq;
	double weight;
	POLY *sigPtr = (POLY *) buf;
	for (std::map<std::vector<POLY>, std::vector<double> >::iterator itr = sigList.begin(); itr != sigList.end(); itr++) {

		sig = itr->first;
		// DO NOT halve f and w
		freq = itr->second[0];
		weight = itr->second[1];

		// copy sigLen
		sigLen = sig.size() * sizeof(POLY);
		memcpy(buf, &sigLen, sizeof(sigLen));
		buf += sizeof(sigLen);
#ifdef _DEBUG_
		warnx << "makeKeyValue: sigLen: " << sigLen << "\n";
#endif

		// copy sig
		sigPtr = (POLY *) buf;
		for (int i = 0; i < (int) sig.size(); i++) {
			sigPtr[i] = sig[i];
			// is this better?
			//buf += sizeof(POLY);
		}
		buf += sigLen;

		// copy freq
		memcpy(buf, &freq, sizeof(freq));
		buf += sizeof(freq);

		// copy weight
		memcpy(buf, &weight, sizeof(weight));
		buf += sizeof(weight);

		sig.clear();
	}
	//buf += sigListLen;

	return;
}

// vsfgd: gpsi (old)
// format: <type><total len><keysize><key><seq><siglistlen><sigsize><sig><freq><weight>...
void makeKeyValue(char **ptr, int& len, str& key, std::map<std::vector<POLY>, double, CompareSig>& sigList, std::map<std::vector<POLY>, double, CompareSig>& weightList, int& seq, InsertType type)
{
	int keyLen = key.len();
	//warnx << "LENGTH: ++++ " << keyLen << "\n";
	
	// type + total len + keysize + key + seq + siglistlen
	len = sizeof(int) + sizeof(int) + sizeof(int) + keyLen + sizeof(int) + sizeof(int);

	int sigListLen = sigList.size();
	int sigLen;
	std::vector<POLY> sig;
	sig.clear();

	// + sigListLen *
	for (std::map<std::vector<POLY>, double >::iterator itr = sigList.begin();
	     itr != sigList.end(); itr++) {

		sig = itr->first;
		//freq = itr->second;
		sigLen = sig.size() * sizeof(POLY);
		//warnx << "sigLen: " << sigLen << "\n";
		// (sigsize + sig + freq + weight)
		// you f*ing double
		len += (sizeof(int) + sigLen + sizeof(double) + sizeof(double));
		sig.clear();
		//warnx << "len (after sig n): " << len << "\n";
	}
	// XXX: includes 4 bytes for type
	warnx << "makeKeyValue: len (allocated): " << len << "\n";
	warnx << "makeKeyValue: sigListLen: " << sigListLen << "\n";
	// TODO: New vs new
	*ptr = New char[len];
	//*ptr = new char[len];
	assert(ptr);
	char *buf = *ptr;

	// copy type
	memcpy(buf, &type, sizeof(type));
	buf += sizeof(type);

	// copy len
	// XXX: is sizeof(type) always equal to sizeof(int)?
	//len = len - sizeof(int);
	//warnx << "makeKeyValue: len (msg): " << len << "\n";
	memcpy(buf, &len, sizeof(len));
	buf += sizeof(len);

	// copy key
	memcpy(buf, &keyLen, sizeof(keyLen));
	buf += sizeof(keyLen);
	memcpy(buf, key.cstr(), keyLen);
	buf += keyLen;

	// copy seq
	memcpy(buf, &seq, sizeof(seq));
	buf += sizeof(seq);

	// copy sigListLen
	memcpy(buf, &sigListLen, sizeof(sigListLen));
	buf += sizeof(sigListLen);

	double freq;
	double weight;
	POLY *sigPtr = (POLY *) buf;
	std::map<std::vector<POLY>, double >::iterator itrW = weightList.begin();
	for (std::map<std::vector<POLY>, double >::iterator itr = sigList.begin();
	     itr != sigList.end(); itr++) {

		/*
		if (itrW == weightList.end()) {
			warnx << "weightList is over before sigList\n";
			return;
		}
		*/
		sig = itr->first;
		freq = itr->second / 2;
		weight = itrW->second / 2;
		++itrW;

		// copy sigLen
		sigLen = sig.size() * sizeof(POLY);
		memcpy(buf, &sigLen, sizeof(sigLen));
		buf += sizeof(sigLen);
#ifdef _DEBUG_
		warnx << "makeKeyValue: sigLen: " << sigLen << "\n";
#endif

		// copy sig
		sigPtr = (POLY *) buf;
		for (int i = 0; i < (int) sig.size(); i++) {
			sigPtr[i] = sig[i];
			// is this better?
			//buf += sizeof(POLY);
		}
		buf += sigLen;

		// copy freq
		memcpy(buf, &freq, sizeof(freq));
		buf += sizeof(freq);

		// copy weight
		memcpy(buf, &weight, sizeof(weight));
		buf += sizeof(weight);

		sig.clear();
	}
	//buf += sigListLen;

	return;
}

// vsfgd: xgossip inform_team
// format: <msgtype><msglen><keysize><key><chordID>
void makeKeyValue(char **ptr, int& len, str& key, std::vector<chordID>& minhash, InsertType type)
{
	int keyLen = key.len();

	// msgtype + msglen + keysize + key + chordID
	len = sizeof(int) + sizeof(int) + sizeof(int) + keyLen + sizeof(chordID);

	warnx << "makeKeyValue: len (allocated): " << len << "\n";
	// TODO: New vs new?
	*ptr = New char[len];
	//*ptr = new char[len];
	assert(ptr);
	char *buf = *ptr;

	// Copy msgtype
	memcpy(buf, &type, sizeof(type));
	buf += sizeof(type);

	// Copy msglen
	memcpy(buf, &len, sizeof(len));
	buf += sizeof(len);

	// Copy key
	memcpy(buf, &keyLen, sizeof(keyLen));
	buf += sizeof(keyLen);
	memcpy(buf, key.cstr(), keyLen);
	buf += keyLen;

	// Copy chordID
	chordID ID = minhash[0];
	memcpy(buf, &ID, sizeof(ID));
}

// vsfgd: xgossip init
// format: <msgtype><msglen><keysize><key><teamidsize><teamid><sigsize><sig><freq><weight>
void makeKeyValue(char **ptr, int& len, str& key, str& teamid, std::vector<POLY>& sig, double& freq, double& weight, InsertType type)
{
	int keyLen = key.len();
	int teamidLen = teamid.len();
	//warnx << "LENGTH: ++++ " << keyLen << "\n";
	int sigLen = sig.size() * sizeof(POLY);
	
	// msgtype + msglen + keysize + key + teamidsize + teamid + sigsize + sig + freq + weight
	len = sizeof(int) + sizeof(int) + sizeof(int) + keyLen + sizeof(int) + teamidLen + sizeof(int) + sigLen + sizeof(double) + sizeof(double);
	
	// TODO: New vs new?
	*ptr = New char[len];
	//*ptr = new char[len];
	assert(ptr);
	char *buf = *ptr;

	// Copy msgtype
	memcpy(buf, &type, sizeof(type));
	buf += sizeof(type);

	// Copy msglen
	memcpy(buf, &len, sizeof(len));
	buf += sizeof(len);

	// Copy key
	memcpy(buf, &keyLen, sizeof(keyLen));
	buf += sizeof(keyLen);
	memcpy(buf, key.cstr(), keyLen);
	buf += keyLen;

	// Copy teamid
	memcpy(buf, &teamidLen, sizeof(teamidLen));
	buf += sizeof(teamidLen);
	memcpy(buf, teamid.cstr(), teamidLen);
	buf += teamidLen;

	// Copy siglen
	memcpy(buf, &sigLen, sizeof(sigLen));
	buf += sizeof(sigLen);

	POLY *sigPtr = (POLY *) buf;
	for (int i = 0; i < (int) sig.size(); i++) {
		sigPtr[i] = sig[i];
	}
	buf += sigLen;

	// Copy freq
	memcpy(buf, &freq, sizeof(freq));
	buf += sizeof(freq);

	// Copy weight
	memcpy(buf, &weight, sizeof(weight));
	buf += sizeof(weight);

	return;
}

// vsfgd: xgossip query
// format: <msgtype><msglen><keysize><key><teamidsize><teamid><dtd><sigsize><sig><queryid>
void makeKeyValue(char **ptr, int& len, str& key, str& teamid, str& dtd, std::vector<POLY>& sig, int& qid, InsertType type)
{
	int keyLen = key.len();
	int teamidLen = teamid.len();
	int dtdLen = dtd.len();
	//warnx << "LENGTH: ++++ " << keyLen << "\n";
	int sigLen = sig.size() * sizeof(POLY);
	
	// msgtype + msglen + keysize + key + teamidsize + teamid + dtdsize + dtd + sigsize + sig + qid
	len = sizeof(int) + sizeof(int) + sizeof(int) + keyLen + sizeof(int) + teamidLen + sizeof(int) + dtdLen + sizeof(int) + sigLen + sizeof(int);
	
	// TODO: New vs new?
	*ptr = New char[len];
	//*ptr = new char[len];
	assert(ptr);
	char *buf = *ptr;

	// Copy msgtype
	memcpy(buf, &type, sizeof(type));
	buf += sizeof(type);

	// Copy msglen
	memcpy(buf, &len, sizeof(len));
	buf += sizeof(len);

	// Copy key
	memcpy(buf, &keyLen, sizeof(keyLen));
	buf += sizeof(keyLen);
	memcpy(buf, key.cstr(), keyLen);
	buf += keyLen;

	// Copy teamid
	memcpy(buf, &teamidLen, sizeof(teamidLen));
	buf += sizeof(teamidLen);
	memcpy(buf, teamid.cstr(), teamidLen);
	buf += teamidLen;

	// Copy dtd
	memcpy(buf, &dtdLen, sizeof(dtdLen));
	buf += sizeof(dtdLen);
	memcpy(buf, dtd.cstr(), dtdLen);
	buf += dtdLen;

	// Copy siglen
	memcpy(buf, &sigLen, sizeof(sigLen));
	buf += sizeof(sigLen);

	POLY *sigPtr = (POLY *) buf;
	for (int i = 0; i < (int) sig.size(); i++) {
		sigPtr[i] = sig[i];
	}
	buf += sigLen;

	// Copy qid
	memcpy(buf, &qid, sizeof(qid));
	buf += sizeof(qid);

	return;
}

void makeKeyValue(char **ptr, int& len, str& key, std::vector<POLY>& sig,
    Interval& myInt, InsertType type)
{
	int keyLen = key.len();
	int valLen = sig.size() * sizeof(POLY);
	
	if (type == NONE) {
		len = sizeof(int) + sizeof(Interval) + keyLen + sizeof(int) + valLen;
	}
	else {
		len = sizeof(int) + sizeof(int) + sizeof(Interval) + keyLen + sizeof(int) + valLen;
	}
	
	*ptr = new char[len];
	assert(ptr);
    char *buf = *ptr;

	if (type != NONE) {
		// Copy type
		memcpy(buf, &type, sizeof(type));
		buf += sizeof(type);
	}

    // Copy header interval
    memcpy(buf, &myInt, sizeof(myInt));
    buf += sizeof(myInt);

	// Copy key
	memcpy(buf, &keyLen, sizeof(keyLen));
	buf += sizeof(keyLen);
	memcpy(buf, key.cstr(), keyLen);
	buf += keyLen;

	// Copy value
	memcpy(buf, &valLen, sizeof(valLen));
	buf += sizeof(valLen);

	POLY *sigPtr = (POLY *) buf;
	for (int i = 0; i < (int) sig.size(); i++) {
		sigPtr[i] = sig[i];
	}
	return;
}

void makeKeyValue(char **ptr, int& len, std::vector<POLY>& textHash,
    std::vector<POLY>& sig, int docId, InsertType type)
{
    int textHashSize = textHash.size();
    int sigSize = sig.size();
    
    len = sizeof(int) + sizeof(textHashSize) + textHashSize * sizeof(POLY)
        +  sizeof(sigSize) + sigSize * sizeof(POLY) + sizeof(docId);
	
	*ptr = new char[len];
	assert(ptr);
    char *buf = *ptr;

	if (type != NONE) {
		// Copy type
		memcpy(buf, &type, sizeof(type));
		buf += sizeof(type);
	}

    // Copy textHash
    memcpy(buf, &textHashSize, sizeof(textHashSize));
    buf += sizeof(textHashSize);

    POLY *sigPtr = (POLY *) buf;
	for (int i = 0; i < (int) textHash.size(); i++) {
		sigPtr[i] = textHash[i];
	}

    buf += textHashSize * sizeof(POLY);

    // Copy signature
    memcpy(buf, &sigSize, sizeof(sigSize));
    buf += sizeof(sigSize);

    sigPtr = (POLY *) buf;
	for (int i = 0; i < (int) sig.size(); i++) {
		sigPtr[i] = sig[i];
	}

    buf += sigSize * sizeof(POLY);

    // Finally docid
    memcpy(buf, &docId, sizeof(docId));
        
    return;
}

void makeKeyValue(char **ptr, int& len, str& key, Interval& val,
									InsertType type)
{
	int keyLen = key.len();
	int valLen = sizeof(val);
	len = sizeof(int) + sizeof(int) + keyLen + sizeof(int) + valLen;
	*ptr = new char[len];
	assert(ptr);
    char *buf = *ptr;
	
	// Copy type
	memcpy(buf, &type, sizeof(type));
	buf += sizeof(type);

	// Copy key
	memcpy(buf, &keyLen, sizeof(keyLen));
	buf += sizeof(keyLen);
	memcpy(buf, key.cstr(), keyLen);
	buf += keyLen;

	// Copy value
	memcpy(buf, &valLen, sizeof(valLen));
	buf += sizeof(valLen);

    memcpy(buf, &val, sizeof(val));

	return;
}

// vsfgd: push_sum
// format: <type><size><key><size><value1><size><value2>
void makeKeyValue(char **ptr, int& len, str& key, double& val1, double& val2, InsertType type)
{
	int keyLen = key.len();
	int valLen1 = sizeof(val1);
	int valLen2 = sizeof(val2);
	// typelen + size + keylen + size + valuelen1 + size + valuelen2
	len = sizeof(int) + sizeof(int) + keyLen + sizeof(int) + valLen1 + sizeof(int) + valLen2;
	*ptr = new char[len];
	assert(ptr);
	char *buf = *ptr;
	
	// Copy type
	memcpy(buf, &type, sizeof(type));
	buf += sizeof(type);

	// Copy key
	memcpy(buf, &keyLen, sizeof(keyLen));
	buf += sizeof(keyLen);
	memcpy(buf, key.cstr(), keyLen);
	buf += keyLen;

	// Copy value1
	memcpy(buf, &valLen1, sizeof(valLen1));
	buf += sizeof(valLen1);
	memcpy(buf, &val1, sizeof(val1));
	buf += sizeof(val1);

	// Copy value2
	memcpy(buf, &valLen2, sizeof(valLen2));
	buf += sizeof(valLen2);
	memcpy(buf, &val2, sizeof(val2));

	return;
}

void makeKeyValue(char **ptr, int& len, str& key, Interval& val)
{
	int keyLen = key.len();
	int valLen = sizeof(val);
	len = sizeof(int) + keyLen + sizeof(int) + valLen;
	*ptr = new char[len];
	assert(ptr);
    char *buf = *ptr;
	
	// Copy key
	memcpy(buf, &keyLen, sizeof(keyLen));
	buf += sizeof(keyLen);
	memcpy(buf, key.cstr(), keyLen);
	buf += keyLen;

	// Copy value
	memcpy(buf, &valLen, sizeof(valLen));
	buf += sizeof(valLen);

  memcpy(buf, &val, sizeof(val));

	return;
}

// Format: <type><len><strval><len><strval>...
void makeKeyValue(char **ptr, int& len, vec<str>& entries, std::vector<int>& ids,
									InsertType type)
{
	len = sizeof(int);
	
	//warnx << "MAKEKEYVALUE \n";
	for (int i = 0; i < (int) ids.size(); i++) {
		if (ids[i] < 0) continue;
		len += (entries[ids[i]].len() + sizeof(int));
		//warnx << entries[ids[i]] << " ";
	}
	//warnx << "\n";
	
	*ptr = new char[len];
	assert(ptr);
  char *buf = *ptr;
	
	// Copy type
	memcpy(buf, &type, sizeof(type));
	buf += sizeof(type);

	for (int i = 0; i < (int) ids.size(); i++) {
		if (ids[i] < 0) continue;
		
		int size = entries[ids[i]].len();
		memcpy(buf, &size, sizeof(size));
		buf += sizeof(size);
		memcpy(buf, entries[ids[i]].cstr(), size);
		buf += size;
	}
	return;
}

// Format: <type><len><strval><len><strval>...
void makeKeyValueSpecial(char **ptr, int& len, vec<str>& entries, std::vector<int>& ids,
									InsertType type)
{
	len = sizeof(int);

	for (int i = 0; i < (int) ids.size(); i++) {
		if (ids[i] < 0) continue;

		str keyEntry = "", valEntry = "";
		getKeyValue(entries[ids[i]].cstr(), keyEntry, valEntry);
		// an extra size field is required for backward compatibility
		len += (sizeof(int) + keyEntry.len() + sizeof(int));
	}
	
	*ptr = new char[len];
	assert(ptr);
  char *buf = *ptr;
	
	// Copy type
	memcpy(buf, &type, sizeof(type));
	buf += sizeof(type);

	for (int i = 0; i < (int) ids.size(); i++) {
		if (ids[i] < 0) continue;
		
		str keyEntry = "", valEntry = "";
		getKeyValue(entries[ids[i]].cstr(), keyEntry, valEntry);

		int size = keyEntry.len() + sizeof(int);
		// Extra size for backward compatibility
		memcpy(buf, &size, sizeof(size));
		buf += sizeof(size);

		// Actual size of key
		size = keyEntry.len();
		memcpy(buf, &size, sizeof(size));
		buf += sizeof(size);

		memcpy(buf, keyEntry.cstr(), size);
		buf += size;
	}
	return;
}

// Free allocated memory
void cleanup(char *ptr)
{
	delete[] ptr;
}

// Convert a vector of str's to one str
str marshal_block(vec<str>& elems)
{
    int len = 0;
    for (int i = 0; i < (int) elems.size(); i++) {
        len += elems[i].len();
        len += sizeof(int);
    }

    char *tempbuf = (char *) malloc(sizeof(char) * len);

    assert(tempbuf);
    char *ptr = tempbuf;
    
    for (int i = 0; i < (int) elems.size(); i++) {
        int myLen = elems[i].len();
        //warnx << "MY LEN: " << myLen << "\n";
        memcpy(ptr, &myLen, sizeof(myLen));
        memcpy(ptr+sizeof(myLen), elems[i].cstr(), myLen);
        ptr += (sizeof(myLen) + myLen);
    }

    str res(tempbuf, len);

    free(tempbuf);
    return res;
}

// Convert one string to a vector of str's
vec<str> get_payload(char *input, int length)
{
    vec<str> res;

    char *ptr = input;

    while (ptr < input + length) {
        int myLen;
        memcpy((char *) &myLen, ptr, sizeof(myLen));
        //warnx << "entry len: " << myLen << "\n";
        str e(ptr+sizeof(myLen), myLen);
        res.push_back(e);
        ptr += (sizeof(myLen) + myLen);
    }
    
    return res;
}


// Concatenate sig and docid
void makeDocid(char **input, int *inputlen, std::vector<POLY>& sig, char *docid)
{
    int len = sizeof(int) + sizeof(POLY) * sig.size() + sizeof(int)
        + strlen(docid);
    *inputlen = len;
    *input = new char[len];
    assert(*input);
    char *ptr = *input;

    int size = sig.size();
    memcpy(ptr, &size, sizeof(size));
    ptr += sizeof(size);
    
    POLY *buf = (POLY *) ptr;

    for (int i = 0; i < (int) sig.size(); i++) {
        buf[i] = sig[i];
        ptr += sizeof(POLY);
    }

    size = strlen(docid);
    memcpy(ptr, &size, sizeof(size));
    ptr += sizeof(size);
    memcpy(ptr, docid, size);

    return;
}

// extract the sig and docid
void getDocid(const char *buf, std::vector<POLY>& sig, str& docid)
{
    int size;
    const char *temp = buf;
    memcpy(&size, temp, sizeof(size));
    temp += sizeof(size);
    
    POLY *ptr = (POLY *) temp;
    
    for (int i = 0; i < size; i++) {
        sig.push_back(ptr[i]);
    }

    temp += (sig.size() * sizeof(POLY));
    
    memcpy(&size, temp, sizeof(size));
    docid = str(temp+sizeof(size), size);
    return;
}


// MBR
void updateMBR(std::vector<POLY>& src, std::vector<POLY>& e)
{
    if (e[0] < src[0])
        src[0] = e[0];

    if (e[1] > src[1])
        src[1] = e[1];
}

// Computes the similarity between two signatures: SMALLER score ==> more dissimilar
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

// Compute the LCM of two polynomials
void lcmSpecial(std::vector<POLY>& t, const std::vector<POLY>& s, std::vector<POLY>& g)
{
      if (t.size() == 1 && s.size() == 1 && t[0] == 0x1 && s[0] == 0x1) {
              return;
      }    

      //std::vector<POLY> g;
      //gcdSpecial(g, t, s);

      std::vector<POLY> q;
      int degs = getDegree(s);
      int degg = getDegree(g);

      if (degs >= degg) {
              quotient(q, s, g);
      }    
      else {
              quotient(q, g, s);
      }    

#ifdef _DEBUG_
      int qDeg = getDegree(q);
      cout << "Quotient degree: " << qDeg << endl;

      cout << "Before LCM degree: " << getDegree(t) << endl;
      cout << "Before LCM: ";
      for (int i = 0; i < t.size(); i++) {
              cout << t[i] << " "; 
      }    
      cout << endl;
#endif

      std::vector<POLY> res; 
      multiplyPoly(res, t, q);

      t = res; 
#ifdef _DEBUG_
      cout << "LCM degree: " << getDegree(t) << endl;
      cout << "LCM: ";
      for (int i = 0; i < t.size(); i++) {
              cout << t[i] << " ";
      }
      cout << endl;

#endif
      return;
}

// Computes the similarity between two signatures: SMALLER score ==> more dissimilar
int pSimOpt(std::vector<POLY>& s1, std::vector<POLY>& s2,
    std::vector<POLY>& mygcd, bool isMetric)
{

        int retVal;
        if (isMetric) {
        				lcmSpecial(s1, s2, mygcd);
                double val = (double) (INT_MAX-1) * getDegree(mygcd) / getDegree(s1);
                retVal = (int) val; 
        }
        else {
                retVal = getDegree(mygcd);
        }
        return retVal;
}

// Pick the best child
// Tie-breaking strategies not EMPLOYED YET!!!
str pickChild(vec<str>& nodeEntries, std::vector<POLY>& sig, bool ismetric)
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
		
        int deg = pSim(entrySig, sig, gcdPoly, ismetric);
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

// For values
str pickChildV(vec<str>& nodeEntries, std::vector<POLY>& sig)
{
    std::vector<POLY> entrySig;
    int max = -1;
    POLY bestChildSize = 0;
    str bestChild = "NULL";
    str childKey;

    //warnx << "# of node entries for pick child: "
    //			<< nodeEntries.size() << "\n";
	
    // The first entry has the nodetype/interval
    for (int i = 1; i < (int) nodeEntries.size(); i++) {
        childKey = "";
        entrySig.clear();
        getKeyValue(nodeEntries[i].cstr(), childKey, entrySig);

        int e = enlargement(entrySig, sig);
        
        if (e > max) {
            max = e;
            bestChild = childKey;
            bestChildSize = entrySig[1] - entrySig[0];
        }
        else if (e == max) {
            // Choose the smallest degree signature.
            // For split nodes, till they are fixed properly.
            // tie breaker
            if ((entrySig[1] - entrySig[0]) < bestChildSize) {
                bestChild = childKey;
                bestChildSize = entrySig[1] - entrySig[0];
            }
        }
    }

    return bestChild;
}

// Enlargement of intervals
int enlargement(std::vector<POLY>& entrySig, std::vector<POLY>& sig)
{
	std::vector<POLY> myMBR = entrySig;
	updateMBR(myMBR, sig);

	return ((myMBR[1]-myMBR[0]) - (entrySig[1]-entrySig[0]));
}

// dp244: all LSH code

std::vector <POLY>
lsh::getUniqueSet(std::vector<POLY> inputPols)
{
	std::vector<POLY> outputPols = inputPols;
	std::vector<POLY> irr;
	std::vector<POLY> polManipulate;
	std::vector<POLY> polNums;

	for (unsigned int i = 0; i < outputPols.size(); i++) {
		polNums.push_back(outputPols[i]);
	}

	std::vector<POLY> temp_d;
	std::vector<POLY> result;
	std::vector<POLY>::iterator it;
	it = unique(polNums.begin(), polNums.end());
	polNums.resize(it - polNums.begin());
	int count[polNums.size()];
	for (unsigned int i = 0; i < polNums.size(); i++) {
		count[i] = 0;
		for (unsigned int j = 0; j < outputPols.size(); j++) {
			if (!(polNums[i] < outputPols[j])&&
			    !(polNums[i] > outputPols[j])) {
				if (count[i] > 0) {
					/* for the time being, this operation is withheld*/
					polManipulate.push_back(outputPols[j]);
					temp_d.push_back(irrnums[count[i]-1]);
					//temp_d.push_back(irr[count[i]-1]);
					//multiplyPoly(result,polManipulate,irr[count[i]-1]);
					multiplyPoly(result,polManipulate,temp_d);
					temp_d.clear();
					outputPols[j] = result[0];
					polManipulate.clear();
					result.clear();
				}
				count[i]++;
			}
		}
	}

	return outputPols;
}

POLY
lsh::getIRRPoly()
{
	return selected_poly;
}

// determine min element value of a vector
POLY
lsh::isMinimum(std::vector<POLY>& v)
{
	POLY v1 = v[0];

	for (unsigned int i = 0; i < v.size(); i++) {
		if (v[i] < v1) {
			v1 = v[i];
			//warnx<<" my V "<<v[i]<<"\n";
		}
	}
	//warnx<<" min "<<v1<<"\n";
	return v1;
}

// obsolete: instead give prime number when you create LSH object
/*
int
lsh::getPrimeNumberFromConfig(char* configFileName)
{
	int prime = -999;
	int number = -999;
	std::ifstream conTxt;

	conTxt.open(configFileName);
	if (!conTxt) {
		warnx << "Config file not Found. Exiting .... " << "\n";
		exit(0);
	}

	while (conTxt >> prime) number = prime;

	conTxt.close();
	return number;
}
*/

// function to compute hashCode
std::vector<chordID>
lsh::getHashCode(std::vector<POLY>& S)
{
	//std::vector<bool> isWhat;
	//std::vector<POLY> S1;
	std::vector<POLY> h;
	std::vector<POLY> min_hash;
	std::vector<std::string> pre_hash;
	std::vector<chordID> hash;

	/*
	// random number generation using seeds from constructor argument "n"
	srand(n); 
	int random_integer_a; 
	int random_integer_b; 
	// initialize 
	int lowest_a = 1, highest_a = -9;
	int lowest_b = 0, highest_b = -9;
	// get prime number
	//highest_a = getPrimeNumberFromConfig("lsh.config");
	//highest_b = getPrimeNumberFromConfig("lsh.config");
	//highest_a = 1122941;
	//highest_b = 1122941;
	highest_a = highest_b = n;

	if (highest_a <= 0) {
		warnx << "Inappropriate prime number. Exiting .... " << "\n";
		exit(0);
	}

	if (highest_b <= 0) {
		warnx << "Inappropriate prime number. Exiting .... " << "\n";
		exit(0);
	}

	int range_a = (highest_a - lowest_a) + 1;
	int range_b = (highest_b - lowest_b) + 1;
	*/

	// group from ctor arg m
	int f = 0;
	for (int ik = 0; ik < m; ik++) {
		// loop over how many rand number we want (from ctor argument l)
		for (int j = 0; j < l; j++) {
			// loop over S which is read from external file sig.txt
			for (int i = 0; i < k; i++) {
				//warnx << "randa[" << f << "]: " << randa[f] << "\n";
				//warnx << "randb[" << f << "]: " << randb[f] << "\n";
				POLY htmp = (randa[f] * S[i] + randb[f]) % highest;
				//warnx << "h[" << i << "]: " << htmp << "\n";
				h.push_back(htmp);
			}
			//warnx << "h.size(): " << h.size() << "\n";
			POLY min = isMinimum(h);
			//warnx << "min of h: " << min << "\n";
			min_hash.push_back(min);	
			h.clear();
			++f;
		}
		std::string temp;
		std::stringstream ss;
		for (unsigned int ijk = 0; ijk < min_hash.size(); ijk++) {
		      ss.str("");
		      ss << min_hash[ijk];
		      temp += ss.str();
		}
		//warnx << "min_hash.size(): " << min_hash.size() << "\n";
		
		ss.str("");
		pre_hash.push_back(temp);
		temp.clear();
		min_hash.clear();
		// only one column is needed
		if (col != 0) break;
	}
	//warnx << "pre_hash.size(): " << pre_hash.size() << "\n";
	//warnx << "f: " << f << "\n";

	// now compute hash
	chordID ID;
	const char *p;
	int len;
	//warnx << "hash IDs:\n";
	for (unsigned int ih = 0; ih < pre_hash.size(); ih++) {
		p = pre_hash[ih].c_str();
		len = strlen(p);
		ID = compute_hash(p, len);
		//warnx << ID << "\n";
		hash.push_back(ID);
		//txt << str1 << "\n";
		//char storage[1024];
		//char *buf = &storage[0];
		//mpz_get_raw(buf,sha1::hashsize,&ID);
		//warnx<<buf<<" "<<"\n";
	}
	//warnx << "hash.size(): " << hash.size() << "\n";
	//txt.close();
	//hash is a vector of type chordID with m number of IDs
	return hash;
}

std::vector<POLY>
lsh::getHashCodeFindMod(std::vector<POLY>& S, POLY polNumber)
{
	//std::vector<bool> isWhat;
	//std::vector<POLY> S1;
	std::vector<POLY> h;
	std::vector<POLY> min_hash;
	std::vector<std::string> pre_hash;
	std::vector<POLY> hash;

	/*
	// random number generation using seeds from constructor argument "n"
	srand(n); 
	int random_integer_a; 
	int random_integer_b; 
	// initialize 
	int lowest_a = 1, highest_a = -9;
	int lowest_b = 0, highest_b = -9;
	// get prime number
	//highest_a = getPrimeNumberFromConfig("lsh.config");
	//highest_b = getPrimeNumberFromConfig("lsh.config");
	//highest_a = 1122941;
	//highest_b = 1122941;
	highest_a = highest_b = n;

	if (highest_a <= 0) {
		warnx << "Inappropriate prime number. Exiting .... " << "\n";
		exit(0);
	}

	if (highest_b <= 0) {
		warnx << "Inappropriate prime number. Exiting .... " << "\n";
		exit(0);
	}

	int range_a = (highest_a - lowest_a) + 1;
	int range_b = (highest_b - lowest_b) + 1;
	*/

	// group from ctor arg m
	int f = 0;
	for (int ik = 0; ik < m; ik++) {
		// loop over how many rand number we want (from ctor argument l)
		for (int j = 0; j < l; j++) {
			// loop over S which is read from external file sig.txt
			for (int i = 0; i < k; i++) {
				POLY htmp = (randa[f] * S[i] + randb[f]) % highest;
				//warnx << "h[k]: " << htmp << "\n";
				h.push_back(htmp);
			}
			//warnx << "h.size(): " << h.size() << "\n";
			POLY min = isMinimum(h);
			//warnx << "min of h: " << min << "\n";
			min_hash.push_back(min);	
			h.clear();
			++f;
		}
		std::string temp;
		std::stringstream ss;
		for (unsigned int ijk = 0; ijk < min_hash.size(); ijk++) {
		      ss.str("");
		      ss << min_hash[ijk];
		      temp += ss.str();
		}
		//warnx << "min_hash.size(): " << min_hash.size() << "\n";
		
		ss.str("");
		pre_hash.push_back(temp);
		temp.clear();
		min_hash.clear();
	}
	//warnx << "pre_hash.size(): " << pre_hash.size() << "\n";

	// now compute hash
	// this is temporary
	//std::ofstream txt;
	//txt.open("prehash1.txt");
	POLY ID;
	const char *p;
	int len;
	//warnx << "hash IDs:\n";
	for (unsigned int ih = 0; ih < pre_hash.size(); ih++) {
		p = pre_hash[ih].c_str();
		len = strlen(p);
		ID = findMod((char *)p, len, polNumber);
		//warnx << ID << "\n";
		hash.push_back(ID);
		//txt << str1 << "\n";
		//char storage[1024];
		//char *buf = &storage[0];
		//mpz_get_raw(buf,sha1::hashsize,&ID);
		//warnx<<buf<<" "<<"\n";
	}
	//warnx << "hash.size(): " << hash.size() << "\n";
	//txt.close();
	//hash is a vector of type chordID with m number of IDs
	return hash;
}

// raopr: all compression code

bool
compressSignatures(std::vector<std::vector<POLY> >& inputSig, 
			std::vector<POLY>& outputSig, std::vector<std::vector<unsigned char> >& outputBitmap)
{
  
  // find # of sigs to compress
  int numSigs = inputSig.size();
  if (numSigs == 0) {
    std::cout << "Nothing to compress..." << std::endl;
    return false;
  }

  POLY minPoly;
  std::vector<int> index;
  
  // init the indexes
  for (int i = 0; i < numSigs; i++) {
    index.push_back(0);
    if (inputSig[i].size() == 0) {
      std::cout << "At least one signature is empty." << std::endl;
      return false;
    }
  }

  // Create an empty bitmap
  std::vector<unsigned char> ZEROVECTOR;
  for (int j = 0; j < ceil(numSigs/8.0); j++) {
      ZEROVECTOR.push_back((unsigned char)0x0);
  }

  // Create the bitmaps
  while (1) {
    // Check the loop break out condition
    bool eov = true;
    for (int i = 0; i < numSigs; i++) {
      if (index[i] < (int) inputSig[i].size()) {
	eov = false;
	break;
      }
    }

    // We are done if we have scanned all the sigs
    if (eov == true) break;

    minPoly = (POLY) 0xffffffff;
    // First find the min from all the sigs
    for (int i = 0; i < numSigs; i++) {
      if (index[i] < (int) inputSig[i].size()) {
	if (inputSig[i][index[i]] < minPoly) {
	  minPoly = inputSig[i][index[i]];
	}
      }
      //else {
      //std::cout << "Reached EOF for " << i << std::endl;
      //}
    }
  
    //std::cout << "MIN poly: " << minPoly << std::endl;

    // Push the minPoly to the output signature
    outputSig.push_back(minPoly);
    // Push an zero vector
    outputBitmap.push_back(ZEROVECTOR);

    // Now loop through all the signatures and increment the indexes and
    // update the bitmaps

    for (int i = 0; i < numSigs; i++) {
      // If it matches minPoly
      //if (inputSig[i][index[i]] == minPoly) {
      // very hard to find bug! (dr. rao found it)
      if (index[i] < (int) inputSig[i].size() && inputSig[i][index[i]] == minPoly) {
	// Adjust the bitmap
	int bitmapid = i / 8;
	
	unsigned char mask = (unsigned char) 0x80 >> (i % 8);

	//warnx << "Bitmapid: " << bitmapid << " mask: " << mask <<  " POLY: " << inputSig[i][index[i]] << "\n";
	// do a bitwise OR using the mask
	outputBitmap[outputBitmap.size() - 1][bitmapid] = 
	  outputBitmap[outputBitmap.size() - 1][bitmapid] | mask;
	
	// move to the next element
	index[i]++;
      }
    }
  }

  return true;
}

bool
uncompressSignatures(std::vector<std::vector<POLY> >& outputSig, 
			  std::vector<POLY>& inputSig, std::vector<std::vector<unsigned char> >& inputBitmap, int numSigs)
{
  
  // Initialize 
  for (int i = 0; i < numSigs; i++) {
    std::vector<POLY> e;
    outputSig.push_back(e);
  }

  // Now extract the items
  for (int i = 0; i < (int) inputSig.size(); i++) {
    for (int j = 0; j < numSigs; j++) {
      unsigned char mask = 0x80 >> (j % 8);
      int bitmapid = j / 8;
      if (inputBitmap[i][bitmapid] & mask) {
	outputSig[j].push_back(inputSig[i]);
      }
    }
  }
  return true;
}

// format: <numSigs><inputSigLen><POLY=inputSig[i]><vector<unsigned char>=inputBitmap[i]>
void
makeKeyValue(char **inptr, std::vector<POLY>& inputSig, 
		  std::vector<std::vector<unsigned char> >& inputBitmap, int numSigs)
{
  int len = sizeof(numSigs) + sizeof(int) + (int) inputSig.size() * (sizeof(POLY) + (int) ceil(numSigs/8.0));
  unsigned char *ptr = new unsigned char[len];
  *inptr = (char *) ptr;

  // store the num of signatures
  memcpy(ptr, &numSigs, sizeof(numSigs));
  ptr += sizeof(numSigs);

  // store the len of the aligned multiset
  int alen = (int) inputSig.size();
  memcpy(ptr, &alen, sizeof(alen));
  ptr += sizeof(alen);

  // store the aligned multiset
  for (int i = 0; i < alen; i++) {
    memcpy(ptr, &inputSig[i], sizeof(POLY));
    ptr += sizeof(POLY);

    // bitmap
    for (int j = 0; j < (int) inputBitmap[i].size(); j++) {
      memcpy(ptr, &inputBitmap[i][j], sizeof(unsigned char));
      ptr += sizeof(unsigned char);
    }
  }

  return;
}

void
getKeyValue(const char *buf, std::vector<POLY>& outputSig, 
		 std::vector<std::vector<unsigned char> >& outputBitmap, int& numSigs)
{
  const char *ptr = buf;
  memcpy(&numSigs, ptr, sizeof(numSigs));
  ptr += sizeof(numSigs);

  int alen;
  memcpy(&alen, ptr, sizeof(alen));
  ptr += sizeof(alen);

  printf("Num sigs: %d Alen %d\n", numSigs, alen);

  for (int i = 0; i < alen; i++) {
    outputSig.push_back(*((POLY *) ptr));
    ptr += sizeof(POLY);
    
    std::vector<unsigned char> e;
    e.clear();

    for (int j = 0; j < (int) ceil(numSigs/8.0); j++) {
      e.push_back(*((unsigned char *) ptr));
      ptr += sizeof(unsigned char);
    }
    
    outputBitmap.push_back(e);
  }
  return;
}
