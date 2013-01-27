// Author: Praveen Rao
#ifndef _CACHE_H_
#define _CACHE_H_
#include <vector>

// Cache size
int cacheSize;
int freeSpace;
int dataReadSize;

std::vector<std::pair<str, std::vector<str> > > nodeCache;
std::vector<std::pair<int, bool> > lruCache;

bool pinNode(int r)
{
  for (int i = 0; i < (int) lruCache.size(); i++) {
	if (lruCache[i].first == r) {
	  lruCache[i].second = true;
	  break;
	}
  }
  return true;
}


bool unpinNode(int r)
{
  for (int i = 0; i < (int) lruCache.size(); i++) {
	if (lruCache[i].first == r) {
	  lruCache[i].second = false;
	  break;
	}
  }
  return true;
}

bool moveToFront(int r)
{
  std::pair<int, bool> e;

  bool found = false;
  for (std::vector<std::pair<int, bool> >::iterator itr = lruCache.begin();
	   itr != lruCache.end(); itr++) {
	if (itr->first == r) {

	  // Delete from the list
	  e = *itr;
	  lruCache.erase(itr);
	  found = true;
	  break;
	}
  }

  assert(found);
  
  // Add to the front of the list
  lruCache.insert(lruCache.begin(), e);
  return true;
}

int getSize(std::vector<str>& e)
{
  int size = 0;
  for (int i = 0; i < (int) e.size(); i++) {
	size += e[i].len();
  }

  return size;
}

bool findReplacement(str& nodeID, std::vector<str>& e,
					 int& pinnedNodeId)
{
  bool found = false;

  int entrySize = getSize(e);
  //dataReadSize += entrySize;
  
  //warnx << "Entry size: " << entrySize 
  //	<< " free space: " << freeSpace 
  //	<< "\n";
  
  if (entrySize <= freeSpace) {
	str empty = "";
	for (int i = 0; i < (int) nodeCache.size(); i++) {
	  if (nodeCache[i].first == empty) {
		nodeCache[i].first = nodeID;
		nodeCache[i].second = e;
		lruCache.insert(lruCache.end(),
						std::pair<int, bool>(i, true));
		pinnedNodeId = i;
		found = true;
		break;
	  }
	}

	if (!found) {
	  nodeCache.push_back(std::pair<str, std::vector<str> >
						  (nodeID, e));
	  // Insert at front
	  lruCache.insert(lruCache.end(),
					  std::pair<int, bool>(nodeCache.size() - 1, true));
	  pinnedNodeId = nodeCache.size() - 1;
	  found = true;
	}

	moveToFront(pinnedNodeId);
	freeSpace -= entrySize;
  }
  else {

	int spaceFreed = freeSpace;
	std::vector<int> toDelete;
	for (int i = lruCache.size() - 1; i >= 0; i--) {
	  //warnx << "ID: " << i << "\n";
	  if (lruCache[i].second == false) {
		spaceFreed += getSize(nodeCache[lruCache[i].first].second);
		
		/*
		nodeCache[lruCache[i].first].first = nodeID;
		nodeCache[lruCache[i].first].second = e;
		lruCache[i].second = true;
		pinnedNodeId = lruCache[i].first;
		found = true;
		break;
		*/
		toDelete.push_back(i);
		//warnx << "Space obtained: " << spaceFreed << "\n";
	  }

	  // Found the required amount of free space.
	  if (spaceFreed >= entrySize) {
		found = true;
		break;
	  }
	}

	assert(found);

	// Now delete these entries
	for (int i = 0; i < (int) toDelete.size() - 1; i++) {
	  std::vector<str> v;
	  str empty = "";
	  nodeCache[lruCache[toDelete[i]].first].first = empty;
	  nodeCache[lruCache[toDelete[i]].first].second = v;

	  // remove the end elements
	  lruCache.erase(lruCache.end()-1);
	}
	
	//warnx << "LRU size: " << lruCache.size() << "\n";
	// Last elem
	int i = toDelete[toDelete.size()-1];
	
	nodeCache[lruCache[i].first].first = nodeID;
	nodeCache[lruCache[i].first].second = e;
	lruCache[i].second = true;
	pinnedNodeId = lruCache[i].first;
	
	// Move to front
	moveToFront(pinnedNodeId);

	// New amount of free space
	freeSpace = (spaceFreed - entrySize);
  }
  return found;
}

/*
bool findReplacement(str& nodeID, std::vector<str>& e,
					 int& pinnedNodeId)
{
  bool found = false;

  if ((int) lruCache.size() < cacheSize) {
	nodeCache.push_back(std::pair<str, std::vector<str> >
						(nodeID, e));
	// Insert at front
	lruCache.insert(lruCache.begin(),
					std::pair<int, bool>(nodeCache.size() - 1, true));
	pinnedNodeId = nodeCache.size() - 1;
	found = true;													  
  }
  else {
	for (int i = lruCache.size() - 1; i >= 0; i--) {
	  if (lruCache[i].second == false) {
		nodeCache[lruCache[i].first].first = nodeID;
		nodeCache[lruCache[i].first].second = e;
		lruCache[i].second = true;
		pinnedNodeId = lruCache[i].first;
		found = true;
		break;
	  }
	}

	assert(found);
	// Move to front
	moveToFront(pinnedNodeId);
  }
  return found;
}
*/


bool findInCache(str& nodeID, int& pinnedNodeId)
{
  for (int i = 0; i < (int) nodeCache.size(); i++) {
	if (nodeCache[i].first == nodeID) {
	  pinnedNodeId = i;
	  pinNode(pinnedNodeId);
	  moveToFront(pinnedNodeId);
	  return true;
	}
  }

  return false;
}
#endif
