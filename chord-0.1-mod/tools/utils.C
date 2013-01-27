// Author: Praveen Rao
#include <iostream>
#include <sys/time.h>
#include <assert.h>
#include <stdlib.h>
#include "retrievetypes.h"

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

int randomNumGen(int r)
{
	static unsigned int seed = (unsigned int) getgtod();
	int j;
	j = 1 + (int) (1.0 * r * rand_r(&seed)/(RAND_MAX+1.0));
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

void getKey(str& s, str& a)
{
	const char *ptr = s.cstr();
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

void makeSigData(str& sigdata, std::vector<std::vector<POLY> >& listOfSigs, enum RetrieveType type)
{
	unsigned int buf[1024];

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


// Return unique node ID
NODEID COMPUTE_NODEID()
{
    NODEID ret = nextNodeID;
    nextNodeID++;
    return ret;
}

// Covert str to NODEID
void str2NODEID(str& in, NODEID& id)
{
    //warnx << "NODESTR: " << in << "\n";
    sscanf(in.cstr(), "%d", &id);
    //std::cout << "NODEID : " << id << std::endl;
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
