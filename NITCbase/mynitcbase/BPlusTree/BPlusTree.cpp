#include "BPlusTree.h"

#include <cstring>
#include <iostream>

int BPlusTree::numOfComparisons = 0;

// =============================== Stage 11 ===============================
RecId BPlusTree::bPlusSearch(int relId, char attrName[ATTR_SIZE], Attribute attrVal, int op) {
  IndexId searchIndex;
  int ret = AttrCacheTable::getSearchIndex(relId, attrName, &searchIndex);
  if (ret != SUCCESS) {
    printf("Search index not available\n");
    exit(FAILURE);
  }

  AttrCatEntry attrCatEntry;
  ret = AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);
  if (ret != SUCCESS) {
    printf("Failed to get attr cat entry\n");
    exit(FAILURE);
  }

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

// =============================== Stage 11 ===============================
int BPlusTree::bPlusCreate(int relId, char attrName[ATTR_SIZE]) {
  // we cannot create a bplus tree for relcat and attrcat
  if (relId == RELCAT_RELID or relId == ATTRCAT_RELID) {
    return E_NOTPERMITTED;
  }

  AttrCatEntry attrCatEntry;
  int ret = AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);
  if (ret != SUCCESS) {
    return ret;
  }

  // if the attribute is already indexed, return SUCCESS
  if (attrCatEntry.rootBlock != -1) {
    return SUCCESS;
  }

  // if we fail to get a leaf node, return
  IndLeaf rootBlockBuffer;
  int rootBlock = rootBlockBuffer.getBlockNum();
  if (rootBlock == E_DISKFULL) {
    return E_DISKFULL;
  }

  // update the rootblock of the attribute in the attributeCache
  attrCatEntry.rootBlock = rootBlock;
  AttrCacheTable::setAttrCatEntry(relId, attrName, &attrCatEntry);

  RelCatEntry relCatEntry;
  RelCacheTable::getRelCatEntry(relId, &relCatEntry);

  int block = relCatEntry.firstBlk;
  int numOfSlotsPerBlk = relCatEntry.numSlotsPerBlk;
  int numAttrs = relCatEntry.numAttrs;
  int offset = attrCatEntry.offset;

  // insert all the blocks of the relation into the bplus tree
  while (block != -1) {
    RecBuffer currentBlock(block);

    // get the slotmap of the block
    unsigned char slotMap[numOfSlotsPerBlk];
    currentBlock.getSlotMap(slotMap);

    // for each occupied slot, insert it into the bplus tree
    for (int slot = 0; slot < numOfSlotsPerBlk; slot++) {
      if (slotMap[slot] == SLOT_OCCUPIED) {
        Attribute record[numAttrs];
        currentBlock.getRecord(record, slot);

        RecId recId{block, slot};
        ret = BPlusTree::bPlusInsert(relId, attrName, record[offset], recId);

        if (ret == E_DISKFULL) {
          return E_DISKFULL;
        }
      }
    }

    // move to the right block
    HeadInfo header;
    currentBlock.getHeader(&header);
    block = header.rblock;
  }

  return SUCCESS;
}

// =============================== Stage 11 ===============================
int BPlusTree::bPlusDestroy(int rootBlockNum) {
  if (rootBlockNum < 0 or rootBlockNum >= DISK_BLOCKS) {
    return E_OUTOFBOUND;
  }

  int type = StaticBuffer::getStaticBlockType(rootBlockNum);

  if (type == IND_LEAF) {
    // if the block is leaf, then just release the block and return
    IndLeaf leafBlock(rootBlockNum);
    leafBlock.releaseBlock();
    return SUCCESS;
  } else if (type == IND_INTERNAL) {
    // if the block is a internal block, we should recursively release the left and right children blocks
    IndInternal internalBlock(rootBlockNum);

    HeadInfo header;
    internalBlock.getHeader(&header);

    // since there are m entries and m + 1 pointers, we first delete the left most child and
    // iteratively delete all right childs
    InternalEntry internalEntry;
    internalBlock.getEntry(&internalEntry, 0);
    BPlusTree::bPlusDestroy(internalEntry.lChild);

    for (int entryIndex = 0; entryIndex < header.numEntries; entryIndex++) {
      internalBlock.getEntry(&internalEntry, entryIndex);
      BPlusTree::bPlusDestroy(internalEntry.rChild);
    }

    internalBlock.releaseBlock();
    return SUCCESS;
  } else {
    return E_INVALIDBLOCK;
  }
}

// =============================== Stage 11 ===============================
int BPlusTree::bPlusInsert(int relId, char attrName[ATTR_SIZE], Attribute attrVal, RecId recId) {
  AttrCatEntry attrCatEntry;
  int ret = AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);
  if (ret != SUCCESS) {
    return ret;
  }

  // check if the attribute is indexed
  int rootBlockNum = attrCatEntry.rootBlock;
  if (rootBlockNum == -1) {
    return E_NOINDEX;
  }

  // find the leaf block number to insert
  int leafBlkNum = BPlusTree::findLeafToInsert(rootBlockNum, attrVal, attrCatEntry.attrType);

  // create the indexEntry to be inserted and populate its values
  Index indexEntry;
  indexEntry.attrVal = attrVal;
  indexEntry.block = recId.block;
  indexEntry.slot = recId.slot;

  // if the insertion fails, destroy the bplus tree and set the rootblock as -1
  if (BPlusTree::insertIntoLeaf(relId, attrName, leafBlkNum, indexEntry) == E_DISKFULL) {
    BPlusTree::bPlusDestroy(rootBlockNum);

    attrCatEntry.rootBlock = -1;
    AttrCacheTable::setAttrCatEntry(relId, attrName, &attrCatEntry);
    return E_DISKFULL;
  }

  return SUCCESS;
}

// =============================== Stage 11 ===============================
int BPlusTree::findLeafToInsert(int rootBlock, Attribute attrVal, int attrType) {
  int blockNum = rootBlock;

  // traverse the tree until you get a leaf node
  while (StaticBuffer::getStaticBlockType(blockNum) != IND_LEAF) {
    IndInternal internalBlock(blockNum);

    HeadInfo header;
    internalBlock.getHeader(&header);

    int index = 0;
    // iterate through the entries in the internal block and find the path to choose
    while (index < header.numEntries) {
      InternalEntry internalEntry;
      internalBlock.getEntry(&internalEntry, index);

      // find the first entry whose value is greater than attrVal
      if (compareAttrs(attrVal, internalEntry.attrVal, attrType) <= 0) {
        break;
      }

      index++;
    }

    if (index == header.numEntries) {
      // if none of the entry's value is greater than attrVal, choose the right most child of the block
      InternalEntry lastEntry;
      internalBlock.getEntry(&lastEntry, header.numEntries - 1);
      blockNum = lastEntry.rChild;
    } else {
      // if we get an entry whose value is greater than attrVal, choose the left child of that entry
      InternalEntry entry;
      internalBlock.getEntry(&entry, index);
      blockNum = entry.lChild;
    }
  }

  return blockNum;
}

// =============================== Stage 11 ===============================
int BPlusTree::insertIntoLeaf(int relId, char attrName[ATTR_SIZE], int blockNum, Index indexEntry) {
  AttrCatEntry attrCatEntry;
  AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);

  IndLeaf leafBlock(blockNum);

  HeadInfo header;
  leafBlock.getHeader(&header);

  Index indices[header.numEntries + 1];

  bool inserted = false;
  // iterate through the block and insert into indices array
  // keep the sorted order
  for (int entryIndex = 0; entryIndex < header.numEntries; entryIndex++) {
    Index entry;
    leafBlock.getEntry(&entry, entryIndex);

    if (compareAttrs(entry.attrVal, indexEntry.attrVal, attrCatEntry.attrType) <= 0) {
      // if the entry.attrVal is smaller than the indexEntry, simply insert it into the indices
      indices[entryIndex] = entry;
    } else {
      // if we get a value larger than the indexEntry's value, insert the indexEntry and insert the rest of the
      // entries
      // in the block to the array
      indices[entryIndex] = indexEntry;
      inserted = true;

      for (entryIndex++; entryIndex <= header.numEntries; entryIndex++) {
        leafBlock.getEntry(&entry, entryIndex - 1);
        indices[entryIndex] = entry;
      }
      break;
    }
  }

  // if we have not inserted this means all the values in the block was smaller than indexEntry's value
  // so we need to insert it in the last position
  if (not inserted) {
    indices[header.numEntries] = indexEntry;
  }

  // check if there is enough space in the leaf for the new entry to be inserted
  if (header.numEntries < MAX_KEYS_LEAF) {
    // if yes, update the numOfEntries in the header
    header.numEntries++;
    leafBlock.setHeader(&header);

    // insert all the entries in the array to the block in the order
    for (int entryIndex = 0; entryIndex < header.numEntries; entryIndex++) {
      leafBlock.setEntry(&indices[entryIndex], entryIndex);
    }

    return SUCCESS;
  }

  // there is no enough space in the block, so we need to split the leaf
  int newRightBlock = BPlusTree::splitLeaf(blockNum, indices);
  if (newRightBlock == E_DISKFULL) {
    return E_DISKFULL;
  }

  // check if we are spliting a root block or not
  if (header.pblock != -1) {
    // insert the middle value of the existing indices to the parent block (internal block)
    InternalEntry middleEntry;
    middleEntry.attrVal = indices[MIDDLE_INDEX_LEAF].attrVal;
    middleEntry.lChild = blockNum;
    middleEntry.rChild = newRightBlock;

    return BPlusTree::insertIntoInternal(relId, attrName, header.pblock, middleEntry);
  } else {
    // if it is a root block, create a new root block
    return BPlusTree::createNewRoot(relId, attrName, indices[MIDDLE_INDEX_LEAF].attrVal, blockNum, newRightBlock);
  }

  return SUCCESS;
}

// =============================== Stage 11 ===============================
int BPlusTree::splitLeaf(int leafBlockNum, Index indices[]) {
  // get a new block for right rightBlock and get the block of leftBlock
  IndLeaf rightBlock;
  IndLeaf leftBlock(leafBlockNum);

  int rightBlockNum = rightBlock.getBlockNum();
  int leftBlockNum = leftBlock.getBlockNum();

  if (rightBlockNum == E_DISKFULL) {
    return E_DISKFULL;
  }

  HeadInfo leftBlockHeader, rightBlockHeader;
  leftBlock.getHeader(&leftBlockHeader);
  rightBlock.getHeader(&rightBlockHeader);

  int newNumEntries = (MAX_KEYS_LEAF + 1) / 2;

  // set the header for rightBlock
  // after splitting, the blockTypes of both blocks are same
  // number of entries become 32 each
  // p block becomes the p block of the left block
  // l block becomes the leftBlockNum
  // r block becomes the r block of the leftBlock
  rightBlockHeader.blockType = leftBlockHeader.blockType;
  rightBlockHeader.numEntries = newNumEntries;
  rightBlockHeader.pblock = leftBlockHeader.pblock;
  rightBlockHeader.lblock = leftBlockNum;
  rightBlockHeader.rblock = leftBlockHeader.rblock;
  rightBlock.setHeader(&rightBlockHeader);

  // set the header for left block
  // after spliting, the number of entries of left block becomes 32
  // right block becomes the rightBlockNum (newly created block)
  leftBlockHeader.numEntries = newNumEntries;
  leftBlockHeader.rblock = rightBlockNum;
  leftBlock.setHeader(&leftBlockHeader);

  // insert the first 32 entries in the left block and the next 32 in the right block
  for (int i = 0; i <= MIDDLE_INDEX_LEAF; i++) {
    leftBlock.setEntry(&indices[i], i);
    rightBlock.setEntry(&indices[i + MIDDLE_INDEX_LEAF + 1], i);
  }

  return rightBlockNum;
}

// =============================== Stage 11 ===============================
int BPlusTree::insertIntoInternal(int relId, char attrName[ATTR_SIZE], int intBlockNum, InternalEntry intEntry) {
  AttrCatEntry attrCatEntry;
  AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);

  IndInternal internalBlock(intBlockNum);

  HeadInfo header;
  internalBlock.getHeader(&header);

  InternalEntry internalEntries[header.numEntries + 1];

  bool inserted = false;
  int entryIndex = 0;
  // iterate through the block and insert into internalEntries array
  // keep the sorted order
  // when we insert the newInternalNode, update the child pointer
  for (int entry = 0; entry < header.numEntries; entry++) {
    InternalEntry internalEntry;
    internalBlock.getEntry(&internalEntry, entry);

    if (compareAttrs(intEntry.attrVal, internalEntry.attrVal, attrCatEntry.attrType) >= 0) {
      // if the entry.attrVal is smaller than the intEntry, simply insert it into the array
      internalEntries[entryIndex++] = internalEntry;
    } else if (!inserted) {
      // if we get a value larger than the intEntry's value, insert the intEntry, update the child pointer and insert
      // the next entry
      internalEntry.lChild = intEntry.rChild;
      internalEntries[entryIndex++] = intEntry;
      internalEntries[entryIndex++] = internalEntry;
      inserted = true;
    } else {
      // if we have already inserted the new entry in the array, simply add the rest to the array
      internalEntries[entryIndex++] = internalEntry;
    }
  }

  // if we have not inserted this means all the values in the block was smaller than intEntry's value
  // so we need to insert it in the last position
  if (not inserted) {
    internalEntries[header.numEntries] = intEntry;
  }

  // check if there is enough space in the leaf for the new entry to be inserted
  if (header.numEntries < MAX_KEYS_INTERNAL) {
    // if yes, update the numOfEntries in the header
    header.numEntries++;
    internalBlock.setHeader(&header);

    // insert all the entries in the array to the block in the order
    for (int entryIndex = 0; entryIndex < header.numEntries; entryIndex++) {
      internalBlock.setEntry(&internalEntries[entryIndex], entryIndex);
    }

    return SUCCESS;
  }

  // there is no enough space in the block, so we need to split the block
  int newRightBlock = BPlusTree::splitInternal(intBlockNum, internalEntries);
  if (newRightBlock == E_DISKFULL) {
    BPlusTree::bPlusDestroy(intEntry.rChild);
    return E_DISKFULL;
  }

  // check if we are spliting a root block or not
  if (header.pblock != -1) {
    // insert the middle value of the existing entries to the parent block (internal block)
    InternalEntry parentInternalEntry;
    parentInternalEntry.attrVal = internalEntries[MIDDLE_INDEX_INTERNAL].attrVal;
    parentInternalEntry.lChild = intBlockNum;
    parentInternalEntry.rChild = newRightBlock;

    return BPlusTree::insertIntoInternal(relId, attrName, header.pblock, parentInternalEntry);
  } else {
    // if it is a root block, create a new root block
    return BPlusTree::createNewRoot(relId, attrName, internalEntries[MIDDLE_INDEX_INTERNAL].attrVal, intBlockNum,
                                    newRightBlock);
  }

  return SUCCESS;
}

// =============================== Stage 11 ===============================
int BPlusTree::splitInternal(int intBlockNum, InternalEntry internalEntries[]) {
  // get a new block for right rightBlock and get the block of leftBlock
  IndInternal rightBlock;
  IndInternal leftBlock(intBlockNum);

  int rightBlockNum = rightBlock.getBlockNum();
  int leftBlockNum = leftBlock.getBlockNum();

  if (rightBlockNum == E_DISKFULL) {
    return E_DISKFULL;
  }

  HeadInfo leftBlockHeader, rightBlockHeader;
  leftBlock.getHeader(&leftBlockHeader);
  rightBlock.getHeader(&rightBlockHeader);

  int newNumEntries = MAX_KEYS_INTERNAL / 2;

  // set the header for rightBlock
  // after splitting, the number of entries become 50 each
  // p block becomes the p block of the left block
  rightBlockHeader.numEntries = newNumEntries;
  rightBlockHeader.pblock = leftBlockHeader.pblock;
  rightBlock.setHeader(&rightBlockHeader);

  // set the header for left block
  // after spliting, the number of entries of left block becomes 50
  // right block becomes the rightBlockNum (newly created block)
  leftBlockHeader.numEntries = newNumEntries;
  leftBlockHeader.rblock = rightBlockNum;
  leftBlock.setHeader(&leftBlockHeader);

  // insert the 0 to 49th entries in the left block and the 51 to 100th entires in the right block
  // 50th entry will be inserted in the parent block
  for (int i = 0; i < MIDDLE_INDEX_INTERNAL; i++) {
    leftBlock.setEntry(&internalEntries[i], i);
    rightBlock.setEntry(&internalEntries[i + MIDDLE_INDEX_INTERNAL + 1], i);
  }

  int type = StaticBuffer::getStaticBlockType(internalEntries[0].lChild);

  // now we need to update the parent block of the last 50 children of the newly created right block from the left
  // block
  // to the right block for that, first we will set the parent of lChild (first child pointer) of the first entry in
  // right block to right block and for the rest, we take the right childs and update its parent as the right block
  BlockBuffer blockBuffer(internalEntries[MIDDLE_INDEX_INTERNAL + 1].lChild);

  HeadInfo blockHeader;
  blockBuffer.getHeader(&blockHeader);

  blockHeader.pblock = rightBlockNum;
  blockBuffer.setHeader(&blockHeader);

  for (int i = 0; i < MIDDLE_INDEX_INTERNAL; i++) {
    BlockBuffer blockBuffer(internalEntries[i + MIDDLE_INDEX_INTERNAL + 1].rChild);

    blockBuffer.getHeader(&blockHeader);
    blockHeader.pblock = rightBlockNum;
    blockBuffer.setHeader(&blockHeader);
  }

  return rightBlockNum;
}

// =============================== Stage 11 ===============================
int BPlusTree::createNewRoot(int relId, char attrName[ATTR_SIZE], Attribute attrVal, int lChild, int rChild) {
  AttrCatEntry attrCatEntry;
  AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);

  // get a new block for root block
  IndInternal rootBlock;
  int rootBlockNum = rootBlock.getBlockNum();

  // if it fails, destroy the right subtree
  if (rootBlockNum == E_DISKFULL) {
    BPlusTree::bPlusDestroy(rChild);
    return E_DISKFULL;
  }

  // set the number of entries of the root block as 1
  HeadInfo header;
  rootBlock.getHeader(&header);
  header.numEntries = 1;
  rootBlock.setHeader(&header);

  // create an internal entry for insertion into the root block and populate it
  InternalEntry internalEntry;
  internalEntry.attrVal = attrVal;
  internalEntry.lChild = lChild;
  internalEntry.rChild = rChild;

  // insert the internal entry to the first slot
  rootBlock.setEntry(&internalEntry, 0);

  BlockBuffer leftBlock(lChild), rightBlock(rChild);
  HeadInfo leftHeader, rightHeader;

  // update the parent block of left and right child to the new root block
  leftBlock.getHeader(&leftHeader);
  leftHeader.pblock = rootBlockNum;
  leftBlock.setHeader(&leftHeader);

  rightBlock.getHeader(&rightHeader);
  rightHeader.pblock = rootBlockNum;
  rightBlock.setHeader(&rightHeader);

  // update the root block in the attribute cache to the new one
  attrCatEntry.rootBlock = rootBlockNum;
  AttrCacheTable::setAttrCatEntry(relId, attrName, &attrCatEntry);

  return SUCCESS;
}
