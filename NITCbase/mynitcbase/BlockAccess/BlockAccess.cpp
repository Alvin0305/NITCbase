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

        if (
            (op == NE && cmpVal != 0) ||
            (op == EQ && cmpVal == 0) ||
            (op == LT && cmpVal < 0) ||
            (op == GT && cmpVal > 0) ||
            (op == LE && cmpVal <= 0) ||
            (op == GE && cmpVal >= 0) 
        ) {
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
    RecId alreadyExistingNewRelationRecId = BlockAccess::linearSearch(RELCAT_RELID, relCatAttrRelName, newRelationName, EQ);
    if (alreadyExistingNewRelationRecId.block != -1 && alreadyExistingNewRelationRecId.slot != -1) {
        return E_RELEXIST;
    }

    RelCacheTable::resetSearchIndex(RELCAT_RELID);
    Attribute oldRelationName;
    strcpy(oldRelationName.sVal, oldRelName);

    // check if the relation with name oldRelName already exists or not
    RecId oldRelationRecId = BlockAccess::linearSearch(RELCAT_RELID, relCatAttrRelName, oldRelationName, EQ);
    if (oldRelationRecId.block == -1 || oldRelationRecId.slot == -1) {
        return E_RELNOTEXIST;
    }

    // update the relation catalog with new relation name
    RecBuffer relCatBlock(RELCAT_BLOCK);
    Attribute relCatRecord[RELCAT_NO_ATTRS];
    relCatBlock.getRecord(relCatRecord, oldRelationRecId.slot);

    strcpy(relCatRecord[RELCAT_REL_NAME_INDEX].sVal, newRelName);
    relCatBlock.setRecord(relCatRecord, oldRelationRecId.slot);

    RelCacheTable::resetSearchIndex(ATTRCAT_RELID);

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

    // std::cout << "comparing " << relCatAttrRelName << " with " << relNameAttr.sVal << std::endl;
    RecId existingRelId = BlockAccess::linearSearch(RELCAT_RELID, relCatAttrRelName, relNameAttr, EQ);
    if (existingRelId.block == -1 || existingRelId.slot == -1) {
        return E_RELNOTEXIST;
    }

    RelCacheTable::resetSearchIndex(ATTRCAT_RELID);
    RecId attrToRenameRecId{-1, -1};
    Attribute attrCatEntryRecord[ATTRCAT_NO_ATTRS];

    Attribute temp;
    strcpy(temp.sVal, relName);

    while (true) {
        RecId attrRecId = BlockAccess::linearSearch(ATTRCAT_RELID, relCatAttrRelName, temp, EQ);
        if (attrRecId.block != -1 && attrRecId.slot != -1) {
            RecBuffer attrCatBlock(attrRecId.block);
            Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
            attrCatBlock.getRecord(attrCatRecord, attrRecId.slot);
            
            if (strcmp(attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, oldAttrName) == 0) {
                attrToRenameRecId = attrRecId;
            }

            if (strcmp(attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, newAttrName) == 0) {
                return E_ATTREXIST;
            }
        } else {
            break;
        }
    }

    if (attrToRenameRecId.block == -1 || attrToRenameRecId.slot == -1) {
        return E_ATTRNOTEXIST;
    }

    RecBuffer attrCatBlock(attrToRenameRecId.block);
    Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
    attrCatBlock.getRecord(attrCatRecord, attrToRenameRecId.slot);
    strcpy(attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, newAttrName);
    attrCatBlock.setRecord(attrCatRecord, attrToRenameRecId.slot);

    return SUCCESS;
}