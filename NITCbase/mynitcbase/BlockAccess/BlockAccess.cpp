#include "BlockAccess.h"

#include <cstring>
#include <iostream>

RecId BlockAccess::linearSearch(int relId, char attrName[ATTR_SIZE], union Attribute attrVal, int op) {
  RecId prevSearchIndex;
  RelCacheTable::getSearchIndex(relId, &prevSearchIndex);

  RelCacheEntry relCacheEntry;
  RelCacheTable::getRelCatEntry(relId, &relCacheEntry.relCatEntry);

  int block = prevSearchIndex.block;
  int slot = prevSearchIndex.slot;

  if (block == -1 && slot == -1) {
    block = relCacheEntry.relCatEntry.firstBlk;
    slot = 0;
  } else {
    slot++;
  }

  while (block != -1) {
    RecBuffer recordBlock(block);
    Attribute record[relCacheEntry.relCatEntry.numAttrs];
    recordBlock.getRecord(record, slot);
    int numSlots = relCacheEntry.relCatEntry.numSlotsPerBlk;
    unsigned char slotMap[numSlots];
    recordBlock.getSlotMap(slotMap);

    if (slot >= numSlots) {
      HeadInfo blockHeader;
      recordBlock.getHeader(&blockHeader);
      block = blockHeader.rblock;
      slot = 0;
      continue;
    }

    if (slotMap[slot] == SLOT_UNOCCUPIED) {
      slot++;
      continue;
    }

    char relName[ATTR_SIZE];
    strcpy(relName, relCacheEntry.relCatEntry.relName);

    AttrCatEntry attrCatEntry;
    AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);
    int offset = attrCatEntry.offset;
    Attribute value = record[offset];
    int type = attrCatEntry.attrType;

    int cmpVal = compareAttrs(value, attrVal, type);

    if ((op == NE && cmpVal != 0) || (op == EQ && cmpVal == 0) || (op == LT && cmpVal < 0) ||
        (op == GT && cmpVal > 0) || (op == LE && cmpVal <= 0) || (op == GE && cmpVal >= 0)) {
      RecId newSearchValue = RecId{block, slot};
      RelCacheTable::setSearchIndex(relId, &newSearchValue);

      return RecId{block, slot};
    }

    slot++;
  }

  return RecId{-1, -1};
}

int BlockAccess::renameRelation(char oldRelName[ATTR_SIZE], char newRelName[ATTR_SIZE]) {
  RelCacheTable::resetSearchIndex(RELCAT_RELID);
  Attribute newRelationName;
  strcpy(newRelationName.sVal, newRelName);

  // check if the relation with name newRelName already exists
  char relCatAttrRelName[ATTR_SIZE];
  strcpy(relCatAttrRelName, RELCAT_ATTR_RELNAME);
  RecId alreadyExistingNewRelationRecId =
      BlockAccess::linearSearch(RELCAT_RELID, relCatAttrRelName, newRelationName, EQ);
  if (alreadyExistingNewRelationRecId.block != -1 && alreadyExistingNewRelationRecId.slot != -1) return E_RELEXIST;

  RelCacheTable::resetSearchIndex(RELCAT_RELID);
  Attribute oldRelationName;
  strcpy(oldRelationName.sVal, oldRelName);

  // check if the relation with name oldRelName already exists or not
  RecId oldRelationRecId = BlockAccess::linearSearch(RELCAT_RELID, relCatAttrRelName, oldRelationName, EQ);
  if (oldRelationRecId.block == -1 || oldRelationRecId.slot == -1) return E_RELNOTEXIST;

  // update the relation catalog with new relation name
  RecBuffer relCatBlock(RELCAT_BLOCK);
  Attribute relCatRecord[RELCAT_NO_ATTRS];
  relCatBlock.getRecord(relCatRecord, oldRelationRecId.slot);

  strcpy(relCatRecord[RELCAT_REL_NAME_INDEX].sVal, newRelName);
  relCatBlock.setRecord(relCatRecord, oldRelationRecId.slot);

  RelCacheTable::resetSearchIndex(ATTRCAT_RELID);

  // updates the relation name in attribute catalog entries of that relation
  for (int attrIndex = 0; attrIndex < relCatRecord[RELCAT_NO_ATTRIBUTES_INDEX].nVal; attrIndex++) {
    RecId attrCatRecId = BlockAccess::linearSearch(ATTRCAT_RELID, relCatAttrRelName, oldRelationName, EQ);
    RecBuffer attrCatBlock(attrCatRecId.block);

    Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
    attrCatBlock.getRecord(attrCatRecord, attrCatRecId.slot);

    strcpy(attrCatRecord[ATTRCAT_REL_NAME_INDEX].sVal, newRelName);
    attrCatBlock.setRecord(attrCatRecord, attrCatRecId.slot);
  }

  return SUCCESS;
}

int BlockAccess::renameAttribute(char relName[ATTR_SIZE], char oldAttrName[ATTR_SIZE], char newAttrName[ATTR_SIZE]) {
  RelCacheTable::resetSearchIndex(RELCAT_RELID);

  Attribute relNameAttr;
  strcpy(relNameAttr.sVal, relName);

  // check if a relation with name relName exists
  char relCatAttrRelName[ATTR_SIZE];
  strcpy(relCatAttrRelName, RELCAT_ATTR_RELNAME);

  RecId existingRelId = BlockAccess::linearSearch(RELCAT_RELID, relCatAttrRelName, relNameAttr, EQ);
  if (existingRelId.block == -1 || existingRelId.slot == -1) return E_RELNOTEXIST;

  RelCacheTable::resetSearchIndex(ATTRCAT_RELID);
  RecId attrToRenameRecId{-1, -1};
  Attribute attrCatEntryRecord[ATTRCAT_NO_ATTRS];

  Attribute temp;
  strcpy(temp.sVal, relName);

  // finds the attribute of the relation with given attribute name
  while (true) {
    RecId attrRecId = BlockAccess::linearSearch(ATTRCAT_RELID, relCatAttrRelName, temp, EQ);
    if (attrRecId.block == -1 && attrRecId.slot == -1) break;

    RecBuffer attrCatBlock(attrRecId.block);
    Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
    attrCatBlock.getRecord(attrCatRecord, attrRecId.slot);

    if (strcmp(attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, oldAttrName) == 0) {
      attrToRenameRecId = attrRecId;
      // break;
    }

    if (strcmp(attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, newAttrName) == 0) return E_ATTREXIST;
  }

  if (attrToRenameRecId.block == -1 || attrToRenameRecId.slot == -1) return E_ATTRNOTEXIST;

  RecBuffer attrCatBlock(attrToRenameRecId.block);
  Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
  attrCatBlock.getRecord(attrCatRecord, attrToRenameRecId.slot);
  strcpy(attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, newAttrName);
  attrCatBlock.setRecord(attrCatRecord, attrToRenameRecId.slot);

  return SUCCESS;
}

// =========================== Stage 7 ==================================
// insert a record into a relation
int BlockAccess::insert(int relId, Attribute *record) {
  RelCatEntry relCatEntry;
  RelCacheTable::getRelCatEntry(relId, &relCatEntry);

  int blockNum = relCatEntry.firstBlk;

  RecId recId = {-1, -1};
  int numOfSlots = relCatEntry.numSlotsPerBlk;
  int numOfAttributes = relCatEntry.numAttrs;
  int prevBlockNum = -1;

  // find the block and slot which is free for insertion
  while (blockNum != -1) {
    RecBuffer blockBuffer(blockNum);

    HeadInfo header;
    blockBuffer.getHeader(&header);

    unsigned char slotMap[numOfSlots];
    blockBuffer.getSlotMap(slotMap);

    // loops through the slot map and find a free slot
    for (int slotIndex = 0; slotIndex < numOfSlots; slotIndex++) {
      if (slotMap[slotIndex] == SLOT_UNOCCUPIED) {
        recId.block = blockNum;
        recId.slot = slotIndex;
        break;
      }
    }

    if (recId.block != -1) {
      break;
    }

    // go to next block
    prevBlockNum = blockNum;
    blockNum = header.rblock;
  }

  // there is no free slots in any of the blocks allocated for the relation
  // => we need to allocate a new block for the relation
  if (recId.block == -1 && recId.slot == -1) {
    // RELCAT can span only one block => 20 slots
    if (relId == RELCAT_RELID) return E_MAXRELATIONS;

    // allocate a new free block
    RecBuffer blockBuffer;
    int blockNum = blockBuffer.getBlockNum();
    if (blockNum == E_DISKFULL) return E_DISKFULL;

    recId.block = blockNum;
    recId.slot = 0;

    // create a header for the new block
    HeadInfo head;
    head.blockType = REC;
    head.pblock = -1;
    head.lblock = relCatEntry.numRecs == 0 ? -1 : prevBlockNum;
    head.rblock = -1;
    head.numEntries = 0;
    head.numAttrs = numOfAttributes;
    head.numSlots = numOfSlots;

    blockBuffer.setHeader(&head);

    // mark all the slots as SLOT_UNOCCUPIED
    unsigned char slotMap[numOfSlots];
    for (int i = 0; i < numOfSlots; i++) {
      slotMap[i] = SLOT_UNOCCUPIED;
    }
    blockBuffer.setSlotMap(slotMap);

    if (prevBlockNum != -1) {
      // update the linked list of block (r block of prevBlock points to the current block)
      RecBuffer prevBlock(prevBlockNum);
      HeadInfo prevHead;
      prevBlock.getHeader(&prevHead);

      prevHead.rblock = blockNum;
      prevBlock.setHeader(&prevHead);
    } else {
      // this is the first block of the particular relation
      relCatEntry.firstBlk = recId.block;
      RelCacheTable::setRelCatEntry(relId, &relCatEntry);
    }

    // since there was no space in between => this was the last block of that particular relation
    relCatEntry.lastBlk = recId.block;
    RelCacheTable::setRelCatEntry(relId, &relCatEntry);
  }

  // insert the record into the slot
  RecBuffer blockBuffer(recId.block);
  int ret = blockBuffer.setRecord(record, recId.slot);
  if (ret != SUCCESS) {
    exit(FAILURE);
  }

  unsigned char slotMap[numOfSlots];
  blockBuffer.getSlotMap(slotMap);

  // update the slotmap
  slotMap[recId.slot] = SLOT_OCCUPIED;
  blockBuffer.setSlotMap(slotMap);

  HeadInfo header;
  blockBuffer.getHeader(&header);

  // increment the number of entries in the block and set the header
  header.numEntries++;
  blockBuffer.setHeader(&header);

  // increment the number of records in the relCatEntry in relCache
  relCatEntry.numRecs++;
  RelCacheTable::setRelCatEntry(relId, &relCatEntry);

  return SUCCESS;
}

int BlockAccess::search(int relId, Attribute *record, char attrName[ATTR_SIZE], Attribute attrVal, int op) {
  RecId recId;

  recId = BlockAccess::linearSearch(relId, attrName, attrVal, op);
  if (recId.block == -1 or recId.slot == -1) {
    return E_NOTFOUND;
  }

  RecBuffer block(recId.block);
  return block.getRecord(record, recId.slot);
}

// ============================== Stage 8 =============================
// drop a relation
int BlockAccess::deleteRelation(char relName[ATTR_SIZE]) {
  // user is not allowed to delete RELAIONCAT and ATTRIBUTECAT
  if (strcmp(relName, RELCAT_RELNAME) == 0 || strcmp(relName, ATTRCAT_RELNAME) == 0) {
    return E_NOTPERMITTED;
  }

  RelCacheTable::resetSearchIndex(RELCAT_RELID);
  Attribute relNameAttr;
  strcpy(relNameAttr.sVal, relName);

  // finding the block and slot of the relation catalog entry of the relation in the relation catalog block
  char relcatAttrRelname[ATTR_SIZE] = RELCAT_ATTR_RELNAME;
  RecId relCatRecId = BlockAccess::linearSearch(RELCAT_RELID, relcatAttrRelname, relNameAttr, EQ);
  if (relCatRecId.block == -1 || relCatRecId.slot == -1) {
    return E_RELNOTEXIST;
  }

  Attribute relCatEntryRecord[RELCAT_NO_ATTRS];
  RecBuffer relCatBlock(relCatRecId.block);
  relCatBlock.getRecord(relCatEntryRecord, relCatRecId.slot);

  int firstBlock = relCatEntryRecord[RELCAT_FIRST_BLOCK_INDEX].nVal;
  int numAttrs = relCatEntryRecord[RELCAT_NO_ATTRIBUTES_INDEX].nVal;

  int currentBlock = firstBlock;

  // delete all the record blocks of the relations
  while (currentBlock != -1) {
    RecBuffer currentBlockBuffer(currentBlock);

    HeadInfo currentBlockHeader;
    currentBlockBuffer.getHeader(&currentBlockHeader);

    currentBlock = currentBlockHeader.rblock;
    currentBlockBuffer.releaseBlock();
  }

  RelCacheTable::resetSearchIndex(ATTRCAT_RELID);

  int numOfAttributesDeleted = 0;

  RelCatEntry attrCatEntry;
  RelCacheTable::getRelCatEntry(ATTRCAT_RELID, &attrCatEntry);
  int correctNumSlotsForAttrCat = attrCatEntry.numSlotsPerBlk;

  // delete all the attributes of the relation from the attribute catalog block
  while (true) {
    char relcatRelName[ATTR_SIZE] = RELCAT_ATTR_RELNAME;
    RecId attrCatRecId = BlockAccess::linearSearch(ATTRCAT_RELID, relcatRelName, relNameAttr, EQ);
    if (attrCatRecId.block == -1 || attrCatRecId.slot == -1) {
      break;
    }

    numOfAttributesDeleted++;

    RecBuffer attrCatBuffer(attrCatRecId.block);
    HeadInfo attrCatHeader;
    attrCatBuffer.getHeader(&attrCatHeader);

    int numOfSlots = correctNumSlotsForAttrCat;

    Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
    attrCatBuffer.getRecord(attrCatRecord, attrCatRecId.slot);

    int rootBlock = attrCatRecord[ATTRCAT_ROOT_BLOCK_INDEX].nVal;

    // update the slotmap entry of the attribute
    unsigned char slotMap[numOfSlots];
    attrCatBuffer.getSlotMap(slotMap);
    slotMap[attrCatRecId.slot] = SLOT_UNOCCUPIED;
    attrCatBuffer.setSlotMap(slotMap);

    // update the number of entries in the attribute catalog header
    attrCatHeader.numEntries--;
    attrCatBuffer.setHeader(&attrCatHeader);

    // release the block if there is no entries in the block
    if (attrCatHeader.numEntries == 0) {
      // update the linkedlist of block after deletion of the middle block
      int lBlockNum = attrCatHeader.lblock;
      int rBlockNum = attrCatHeader.rblock;

      // if left block is not -1, to update the linked list, leftHeader.rblock = currentBlock.rblock
      // if left block num is -1, this is the first block of the relation
      if (lBlockNum != -1) {
        RecBuffer prevBlock(lBlockNum);
        HeadInfo prevHeader;
        prevBlock.getHeader(&prevHeader);
        prevHeader.rblock = rBlockNum;
        prevBlock.setHeader(&prevHeader);
      } else {
        attrCatEntry.firstBlk = rBlockNum;
      }

      // if right block is not -1, to update the linked list, rightHeader.lblock = currentBlock.lblock
      // if right block is -1, this is the last block of the relation
      if (rBlockNum != -1) {
        RecBuffer nextBlock(rBlockNum);
        HeadInfo nextHeader;
        nextBlock.getHeader(&nextHeader);
        nextHeader.lblock = lBlockNum;
        nextBlock.setHeader(&nextHeader);
      } else {
        attrCatEntry.lastBlk = lBlockNum;
      }

      RelCacheTable::setRelCatEntry(ATTRCAT_RELID, &attrCatEntry);
      attrCatBuffer.releaseBlock();
    }

    if (rootBlock != -1) {
      // do BPlusDestroy here
    }
  }

  // decrement the number of entries in the relation catalog
  HeadInfo relCatHeader;
  relCatBlock.getHeader(&relCatHeader);

  relCatHeader.numEntries--;
  relCatBlock.setHeader(&relCatHeader);

  // update the slotmap in relation catalog block
  unsigned char slotMap[relCatHeader.numSlots];
  relCatBlock.getSlotMap(slotMap);

  slotMap[relCatRecId.slot] = SLOT_UNOCCUPIED;
  relCatBlock.setSlotMap(slotMap);

  // update the relation catalog entry in relCache
  RelCatEntry relCatEntry;
  RelCacheTable::getRelCatEntry(RELCAT_RELID, &relCatEntry);

  relCatEntry.numRecs--;
  RelCacheTable::setRelCatEntry(RELCAT_RELID, &relCatEntry);

  // update the attribute catalog entry in relCache
  RelCacheTable::getRelCatEntry(ATTRCAT_RELID, &relCatEntry);

  relCatEntry.numRecs -= numOfAttributesDeleted;
  RelCacheTable::setRelCatEntry(ATTRCAT_RELID, &relCatEntry);

  return SUCCESS;
}

// ============================= Stage 9 =============================
// project given set of attributes of a relation
int BlockAccess::project(int relId, Attribute *record) {
  // get the last hit block and slot -> search index
  RecId prevSearchIndex;
  RelCacheTable::getSearchIndex(relId, &prevSearchIndex);

  int block, slot;

  if (prevSearchIndex.block == -1 and prevSearchIndex.slot == -1) {
    // new project operation -> start from beginning
    RelCatEntry relCatEntry;
    RelCacheTable::getRelCatEntry(relId, &relCatEntry);

    block = relCatEntry.firstBlk;
    slot = 0;
  } else {
    // a project operation is done already -> start from next slot
    block = prevSearchIndex.block;
    slot = prevSearchIndex.slot + 1;
  }

  // loop until we get a satisfying slot i.e.,
  //    if this is the last slot, go to next block
  //    if the slot is unoccupied move to next slot
  while (block != -1) {
    RecBuffer currentBlock(block);

    HeadInfo blockHeader;
    currentBlock.getHeader(&blockHeader);

    unsigned char slotMap[blockHeader.numSlots];
    currentBlock.getSlotMap(slotMap);

    if (slot >= blockHeader.numSlots) {
      block = blockHeader.rblock;
      slot = 0;
    } else if (slotMap[slot] == SLOT_UNOCCUPIED) {
      slot++;
    } else {
      break;
    }
  }

  if (block == -1) {
    return E_NOTFOUND;
  }

  // update the search index (last hit)
  RecId nextRecId{block, slot};
  RelCacheTable::setSearchIndex(relId, &nextRecId);

  RecBuffer blockBuffer(block);
  return blockBuffer.getRecord(record, slot);
}