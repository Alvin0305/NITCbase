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
int BlockAccess::insert(int relId, Attribute *record) {
  RelCatEntry relCatEntry;
  int ret = RelCacheTable::getRelCatEntry(relId, &relCatEntry);
  if (ret != SUCCESS) {
    std::cout << "Failed to get rel cat entry 1" << std::endl;
  }

  int blockNum = relCatEntry.firstBlk;

  RecId recId = {-1, -1};
  int numOfSlots = relCatEntry.numSlotsPerBlk;
  int numOfAttributes = relCatEntry.numAttrs;
  int prevBlockNum = -1;

  while (blockNum != -1) {
    RecBuffer blockBuffer(blockNum);

    HeadInfo header;
    ret = blockBuffer.getHeader(&header);
    if (ret != SUCCESS) {
      std::cout << "Failed to get header 2" << std::endl;
    }

    unsigned char slotMap[numOfSlots];
    ret = blockBuffer.getSlotMap(slotMap);
    if (ret != SUCCESS) {
      std::cout << "Failed to get slot map 3" << std::endl;
    }

    for (int slotIndex = 0; slotIndex < numOfSlots; slotIndex++) {
      if (slotMap[slotIndex] == SLOT_UNOCCUPIED) {
        std::cout << "GOT AN UNOCCUPIED SLOT" << std::endl;
        recId.block = blockNum;
        recId.slot = slotIndex;
        break;
      }
    }

    if (recId.block == -1 && recId.slot == -1) {
      std::cout << "block and slot are -1" << std::endl;
      break;
    }
    prevBlockNum = blockNum;
    blockNum = header.rblock;
  }

  std::cout << "rec id got is: " << recId.block << " " << recId.slot << std::endl;

  if (recId.block == -1 && recId.slot == -1) {
    if (relId == RELCAT_RELID) return E_MAXRELATIONS;

    RecBuffer blockBuffer;
    int blockNum = blockBuffer.getBlockNum();
    if (blockNum == E_DISKFULL) return E_DISKFULL;

    recId.block = blockNum;
    recId.slot = 0;

    HeadInfo head;
    head.blockType = REC;
    head.pblock = -1;
    head.lblock = relCatEntry.numRecs == 0 ? -1 : prevBlockNum;
    head.rblock = -1;
    head.numEntries = 0;
    head.numAttrs = numOfAttributes;
    head.numSlots = numOfSlots;

    ret = blockBuffer.setHeader(&head);
    if (ret != SUCCESS) {
      std::cout << "Failed to set header 5" << std::endl;
    }

    unsigned char slotMap[numOfSlots];
    for (int i = 0; i < numOfSlots; i++) {
      slotMap[i] = SLOT_UNOCCUPIED;
    }
    ret = blockBuffer.setSlotMap(slotMap);
    if (ret != SUCCESS) {
      std::cout << "Failed to set slot map 6" << std::endl;
    }

    if (prevBlockNum != -1) {
      RecBuffer prevBlock(prevBlockNum);
      HeadInfo prevHead;
      ret = prevBlock.getHeader(&prevHead);
      if (ret != SUCCESS) {
        std::cout << "Failed to get header 7" << std::endl;
      }
      prevHead.rblock = blockNum;
      ret = prevBlock.setHeader(&prevHead);
      if (ret != SUCCESS) {
        std::cout << "Failed to set header 8" << std::endl;
      }
    } else {
      relCatEntry.firstBlk = recId.block;

      ret = RelCacheTable::setRelCatEntry(relId, &relCatEntry);
      if (ret != SUCCESS) {
        std::cout << "Failed to set rel cat entry 9" << std::endl;
      }
    }

    relCatEntry.lastBlk = recId.block;
    ret = RelCacheTable::setRelCatEntry(relId, &relCatEntry);
    if (ret != SUCCESS) {
      std::cout << "Failed to set rel cat entry 10" << std::endl;
    }
  }

  std::cout << "after changing correction of rec id: " << recId.block << " " << recId.slot << std::endl;

  RecBuffer blockBuffer(recId.block);
  ret = blockBuffer.setRecord(record, recId.slot);
  if (ret != SUCCESS) {
    std::cout << "Failed to set record 11" << std::endl;
    exit(FAILURE);
  }

  unsigned char slotMap[numOfSlots];
  ret = blockBuffer.getSlotMap(slotMap);
  if (ret != SUCCESS) {
    std::cout << "Failed to get slot map 12" << std::endl;
  }
  slotMap[recId.slot] = SLOT_OCCUPIED;
  ret = blockBuffer.setSlotMap(slotMap);
  if (ret != SUCCESS) {
    std::cout << "Failed to set slot map 13" << std::endl;
  }

  HeadInfo header;
  ret = blockBuffer.getHeader(&header);
  if (ret != SUCCESS) {
    std::cout << "Failed to get header 14" << std::endl;
  }
  header.numEntries++;
  ret = blockBuffer.setHeader(&header);
  if (ret != SUCCESS) {
    std::cout << "Failed to set header 15" << std::endl;
  }

  relCatEntry.numRecs++;
  ret = RelCacheTable::setRelCatEntry(relId, &relCatEntry);
  if (ret != SUCCESS) {
    std::cout << "Failed to set rel cat entry 16" << std::endl;
  }

  return SUCCESS;
}