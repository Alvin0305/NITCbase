#include "BPlusTree.h"

#include <cstring>
#include <iostream>

int BPlusTree::numOfComparisons = 0;

RecId BPlusTree::bPlusSearch(int relId, char attrName[ATTR_SIZE], Attribute attrVal, int op) {
  IndexId searchIndex;
  AttrCacheTable::getSearchIndex(relId, attrName, &searchIndex);

  AttrCatEntry attrCatEntry;
  AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);

  int block = -1, index = -1;

  if (searchIndex.block == -1 and searchIndex.index == -1) {
    // search is done for the first time
    block = attrCatEntry.rootBlock;
    index = 0;

    if (block == -1) {
      return RecId{-1, -1};
    }
  } else {
    // its not the first search => we continue from the last hit
    block = searchIndex.block;
    index = searchIndex.index + 1;

    // get the last hit leaf block
    IndLeaf leaf(block);

    HeadInfo leafHead;
    leaf.getHeader(&leafHead);

    // its the last entry in the leaf block, move to the next block
    if (index >= leafHead.numEntries) {
      block = leafHead.rblock;
      index = 0;

      if (block == -1) {
        return RecId{-1, -1};
      }
    }
  }

  // traversing the tree (internal index nodes to the leaf)
  // this part will be executed only once in a search since after that we will be using the linked list of leaves
  while (StaticBuffer::getStaticBlockType(block) == IND_INTERNAL) {
    IndInternal internalBlk(block);

    HeadInfo internalHeader;
    internalBlk.getHeader(&internalHeader);

    InternalEntry internalEntry;

    // if the operation is NE or LT or LE, we should always move to the left most leaf
    if (op == NE or op == LT or op == LE) {
      internalBlk.getEntry(&internalEntry, 0);
      block = internalEntry.lChild;
    } else {
      int entryIndex = 0;
      // loop through all the entries in the internal index block and find the path to proceed
      while (entryIndex < internalHeader.numEntries) {
        int ret = internalBlk.getEntry(&internalEntry, entryIndex);
        int cmpVal = compareAttrs(internalEntry.attrVal, attrVal, attrCatEntry.attrType);
        BPlusTree::numOfComparisons++;
        // if the operation is EQ and entry.value >= value => we need to go left
        // if the operation is GE and entry.value >= value => we need to go left
        // if the operation is GT and entry.value > value => we need to go left
        if ((op == EQ and cmpVal >= 0) or (op == GE and cmpVal >= 0) or (op == GT and cmpVal > 0)) {
          break;
        }
        
        entryIndex++;
      }

      // if the above loop breaks, then we take the left child
      // other wise we take the right child
      if (entryIndex < internalHeader.numEntries) {
        block = internalEntry.lChild;
      } else {
        block = internalEntry.rChild;
      }
    }
  }

  // scanning the leaf node
  while (block != -1) {
    IndLeaf leafBlk(block);

    HeadInfo leafHead;
    leafBlk.getHeader(&leafHead);

    Index leafEntry;
    // iterate through the entries in the leaf and find an entry which satisfies the condition
    while (index < leafHead.numEntries) {
      leafBlk.getEntry(&leafEntry, index);
      int cmpVal = compareAttrs(leafEntry.attrVal, attrVal, attrCatEntry.attrType);
      BPlusTree::numOfComparisons++;

      // if we get a satisfying record, set the search index and return the block and slot
      if ((op == EQ and cmpVal == 0) or (op == LE and cmpVal <= 0) or (op == LT and cmpVal < 0) or
          (op == GT and cmpVal > 0) or (op == GE and cmpVal >= 0) or (op == NE and cmpVal != 0)) {
        searchIndex.block = block;
        searchIndex.index = index;

        AttrCacheTable::setSearchIndex(relId, attrName, &searchIndex);
        return RecId{leafEntry.block, leafEntry.slot};
      } else if ((op == EQ or op == LE or op == LT) and cmpVal > 0) {
        return RecId{-1, -1};
      }
      index++;
    }
    if (op != NE) {
      break;
    }

    block = leafHead.rblock;
    index = 0;
  }

  // if we don't get a satisfying record, return {-1, -1}
  return RecId{-1, -1};
}