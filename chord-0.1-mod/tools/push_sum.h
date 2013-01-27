// Author: Praveen Rao
#ifndef _PUSH_SUM_H_
#define _PUSH_SUM_H_

const char *NODEISFULL = "node.full";
const char *const ATTRSUFFIX = ".attr";

enum DHTStatus {
    SUCC = 0,
    FULL = 1,
    FAIL = 2,
    NODESPLIT = 3,
    REINSERT = 4,
    FIRST = 5,
    NOTFIRST = 6,
		CORRUPTHDR = 7,
		RETRY = 8
};



const int SLEEPTIME = 2;
const int MAXRETRIES = 5;
#ifdef _CS5590LD_
int MAXRETRIEVERETRIES = 2;
#else
const int MAXRETRIEVERETRIES = 2;
#endif

const int maxLeafEntries = 50;
const char *LEAFNODE = "node.leaf";
const char *NONLEAFNODE = "node.nonleaf";
const char *ROOTNODE = "node.root";
const char *ROOTNODEID = "root.node.id";

#endif
