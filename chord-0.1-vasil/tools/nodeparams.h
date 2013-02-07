// Author: Praveen Rao
#ifndef _NODEPARAMS_H_
#define _NODEPARAMS_H_

#include <chord.h>
// index node related defs
const int MAXENTRIES = 500;
//chordID LEAF, NONLEAF, ROOT;

const char* const FULLNODE = "FULL";
const char* const NOTFULLNODE = "NOTFULL";
const char* const HDRONLY = "HDRONLY";

enum InsertType {
  UPDATE = -1,
  SPLIT = -2,
  UPDATEHDRLOCK = -3,
  UPDATEHDR = -4,
  NONE = -5,
  REPLACE = -6,
  SPLITSPECIAL = -7,
  REPLACESIG = -8,
  UPDATEIFPRESENT = -9,
  JUSTAPPEND = -10,
  UPDATEVAL = -11,
  UPDATEVALIFPRESENT = -12,
  QUERYX = -13,
  QUERYS = -14,
  VXGOSSIP = -15,
  INITGOSSIP = -16,
  INFORMTEAM = -17,
  XGOSSIP = -18,
  INVALID = -19,
  XGOSSIPC = -20,
  QUERYXP = -21,
  VXGOSSIPC = -22,
  BCAST = -23,
  BCASTC = -24
};

#endif
