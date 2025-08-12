#include "OpenRelTable.h"

#include <cstring>
#include <stdlib.h>
#include <iostream>
#include <stdio.h>

OpenRelTableMetaInfo OpenRelTable::tableMetaInfo[MAX_OPEN];

OpenRelTable::OpenRelTable() {
    
    // initialize relCache and attrCache entries with nullptr;
    for (int i = 0; i < MAX_OPEN; i++) {
        RelCacheTable::relCache[i] = nullptr;
        AttrCacheTable::attrCache[i] = nullptr;
    }

    RecBuffer relCatBlock(RELCAT_BLOCK);
    Attribute relCatRecord[RELCAT_NO_ATTRS];
    relCatBlock.getRecord(relCatRecord, RELCAT_SLOTNUM_FOR_RELCAT);

    char relCatName[ATTR_SIZE];
    strcpy(relCatName, relCatRecord[RELCAT_REL_NAME_INDEX].sVal);

    struct RelCacheEntry relCacheEntry;
    RelCacheTable::recordToRelCatEntry(relCatRecord, &relCacheEntry.relCatEntry);
    relCacheEntry.recId.block = RELCAT_BLOCK;
    relCacheEntry.recId.slot = RELCAT_SLOTNUM_FOR_RELCAT;

    RelCacheTable::relCache[RELCAT_RELID] = (struct RelCacheEntry*) malloc(sizeof(RelCacheEntry));
    *(RelCacheTable::relCache[RELCAT_RELID]) = relCacheEntry;

    relCatBlock.getRecord(relCatRecord, RELCAT_SLOTNUM_FOR_ATTRCAT);

    struct RelCacheEntry attrCacheEntry;
    RelCacheTable::recordToRelCatEntry(relCatRecord, &attrCacheEntry.relCatEntry);
    attrCacheEntry.recId.block = RELCAT_BLOCK;
    attrCacheEntry.recId.slot = RELCAT_SLOTNUM_FOR_ATTRCAT; 

    char attrCatName[ATTR_SIZE];
    strcpy(attrCatName, relCatRecord[ATTRCAT_REL_NAME_INDEX].sVal);

    RelCacheTable::relCache[ATTRCAT_RELID] = (struct RelCacheEntry*) malloc(sizeof(RelCacheEntry));
    *(RelCacheTable::relCache[ATTRCAT_RELID]) = attrCacheEntry;

    RecBuffer attrCatBlock(ATTRCAT_BLOCK);
    Attribute attrCatRecord[ATTRCAT_NO_ATTRS];

    AttrCacheEntry *headRel = nullptr;
    AttrCacheEntry *prevRel = nullptr;

    for (int i = 0; i < RELCAT_NO_ATTRS; i++) {
        attrCatBlock.getRecord(attrCatRecord, i);
        AttrCacheEntry *curr = (AttrCacheEntry*) malloc(sizeof(AttrCacheEntry));
        AttrCacheTable::recordToAttrCatEntry(attrCatRecord, &curr->attrCatEntry);
        curr->recId.block = ATTRCAT_BLOCK;
        curr->recId.slot = i;
        curr->next = nullptr;

        if (prevRel) prevRel->next = curr;
        else headRel = curr;

        prevRel = curr;
    }

    AttrCacheTable::attrCache[RELCAT_RELID] = headRel;

    AttrCacheEntry *headAttr = nullptr;
    AttrCacheEntry *prevAttr = nullptr;

    for (int i = 0; i < ATTRCAT_NO_ATTRS; i++) {
        int slotNum = i + RELCAT_NO_ATTRS;
        attrCatBlock.getRecord(attrCatRecord, slotNum);
        AttrCacheEntry* curr = (AttrCacheEntry*) malloc(sizeof(AttrCacheEntry));
        AttrCacheTable::recordToAttrCatEntry(attrCatRecord, &curr->attrCatEntry);
        curr->recId.block = ATTRCAT_BLOCK;
        curr->recId.slot = slotNum;
        curr->next = nullptr;

        if (prevAttr) prevAttr->next = curr;
        else headAttr = curr;

        prevAttr = curr;
    }

    AttrCacheTable::attrCache[ATTRCAT_RELID] = headAttr;

    for (int i = 0; i < MAX_OPEN; i++) {
        OpenRelTable::tableMetaInfo[i].free = true;    
    }

    // setting up OpenRelTable::tableMetaInfo for opening and closing relations
    OpenRelTable::tableMetaInfo[RELCAT_RELID].free = false;
    OpenRelTable::tableMetaInfo[ATTRCAT_RELID].free = false;
    strcpy(OpenRelTable::tableMetaInfo[RELCAT_RELID].relName, relCatName);
    strcpy(OpenRelTable::tableMetaInfo[ATTRCAT_RELID].relName, attrCatName);

    // ==========================================================================
    //           Stage 3 -> Q1 -> adding the students table to the cache
    // ==========================================================================

    // comment this for Stage 5 -> This will be already implemented using open relation method

    struct RelCacheEntry studRelCacheEntry;
    Attribute studRelRecord[RELCAT_NO_ATTRS];
    int studentRelId = 2;
    RecBuffer relCatBlock_(RELCAT_BLOCK);
    relCatBlock_.getRecord(studRelRecord, studentRelId);

    RelCacheTable::recordToRelCatEntry(studRelRecord, &studRelCacheEntry.relCatEntry);
    RelCacheTable::relCache[studentRelId] = (struct RelCacheEntry *) malloc(sizeof(RelCacheEntry));
    studRelCacheEntry.recId.block = RELCAT_BLOCK;
    studRelCacheEntry.recId.slot = studentRelId;
    *(RelCacheTable::relCache[studentRelId]) = studRelCacheEntry;

    RecBuffer studAttrCatBlock(ATTRCAT_BLOCK);
    Attribute studAttrCatRecord[ATTRCAT_NO_ATTRS];

    AttrCacheEntry *studHeadAttr = nullptr;
    AttrCacheEntry *studPrevAttr = nullptr;

    for (int i = 0; i < studRelCacheEntry.relCatEntry.numAttrs; i++) {
        int slotNum = i + RELCAT_NO_ATTRS + ATTRCAT_NO_ATTRS;
        studAttrCatBlock.getRecord(studAttrCatRecord, slotNum);
        AttrCacheEntry *curr = (AttrCacheEntry *) malloc(sizeof(AttrCacheEntry));
        AttrCacheTable::recordToAttrCatEntry(studAttrCatRecord, &curr->attrCatEntry);
        curr->recId.block = ATTRCAT_BLOCK;
        curr->recId.slot = slotNum;
        curr->next = nullptr;

        if (studPrevAttr) studPrevAttr->next = curr;
        else studHeadAttr = curr;

        studPrevAttr = curr;
    }

    AttrCacheTable::attrCache[studentRelId] = studHeadAttr;

    OpenRelTable::tableMetaInfo[studentRelId].free = false;
    strcpy(OpenRelTable::tableMetaInfo[studentRelId].relName, "Students");

}

OpenRelTable::~OpenRelTable() {
    for (int i = 0; i < MAX_OPEN; i++) {
        if (RelCacheTable::relCache[i]) {
            free(RelCacheTable::relCache[i]);
            RelCacheTable::relCache[i] = nullptr;
        }
    }

    for (int i = 0; i < MAX_OPEN; i++) {
        AttrCacheEntry *entry = AttrCacheTable::attrCache[i];
        while (entry) {
            AttrCacheEntry *next = entry->next;
            free(entry);
            entry = next;
        }
        AttrCacheTable::attrCache[i] = nullptr;
    }

    // ================= Not stage 3 ===================
    // for (int i = 2; i < MAX_OPEN; i++) {
    //     if (!OpenRelTable::tableMetaInfo[i].free) {
    //         OpenRelTable::closeRel(i);
    //     }
    // }
}

void printRelCatAndAttrCatInCache_(int relId) {
    for (int i = 0; i <= relId; i++) {
      RelCatEntry relCatBuf;
      int result = RelCacheTable::getRelCatEntry(i, &relCatBuf);
      printf("Relation: %s\n", relCatBuf.relName);

      for (int j = 0; j < relCatBuf.numAttrs; j++) {
        AttrCatEntry attrCatBuf[ATTRCAT_NO_ATTRS];
        AttrCacheTable::getAttrCatEntry(i, j, attrCatBuf);
  
        const char *attrType = (attrCatBuf->attrType == NUMBER) ? "NUM" : "STR";
        const char *attrName = attrCatBuf->attrName;

        printf("  %s: %s\n", attrName, attrType);
      }
      printf("\n");
    }
}

int OpenRelTable::getRelId(char relName[ATTR_SIZE]) {
    // hard coded for Stage 4 -> commented for Stage 5
    // if (strcmp(relName, RELCAT_RELNAME) == 0) return RELCAT_RELID;
    // if (strcmp(relName, ATTRCAT_RELNAME) == 0) return ATTRCAT_RELID;

    for (int i = 0; i < MAX_OPEN; i++) {
        if (!OpenRelTable::tableMetaInfo[i].free && 
            strcmp(relName, OpenRelTable::tableMetaInfo[i].relName) == 0) {
            return i;
        }
    }

    return E_RELNOTOPEN;
}

// ======================== Not Stage 4 ======================
// int OpenRelTable::openRel(char relName[ATTR_SIZE]) {
//     int alreadyExistingRelId = OpenRelTable::getRelId(relName);
//     if (alreadyExistingRelId >= 0) {
//         return alreadyExistingRelId;
//     }

//     int freeSlot = OpenRelTable::getFreeOpenRelTableEntry();
//     if (freeSlot == E_CACHEFULL) {
//         return E_CACHEFULL;
//     }

//     int relId = freeSlot;
//     RelCacheTable::relCache[relId] = (RelCacheEntry *) malloc(sizeof(RelCacheEntry));
//     RelCacheTable::resetSearchIndex(RELCAT_RELID);

//     union Attribute relNameAttribute;
//     strcpy(relNameAttribute.sVal, relName);
//     char relCatAttrRelName[ATTR_SIZE];
//     strcpy(relCatAttrRelName, RELCAT_ATTR_RELNAME);

//     RecId relCatRecId = BlockAccess::linearSearch(RELCAT_RELID, relCatAttrRelName, relNameAttribute, EQ);
//     if (relCatRecId.block == -1 || relCatRecId.slot == -1) {
//         return E_RELNOTEXIST;
//     }

//     // add the relation catalog entry for the opening table into the relCache as a relCacheEntry
//     struct RelCacheEntry relCacheEntry;
//     RecBuffer relCatBlock(RELCAT_BLOCK);
//     Attribute relCatRecord[RELCAT_NO_ATTRS];
//     relCatBlock.getRecord(relCatRecord, relCatRecId.slot);
//     RelCacheTable::recordToRelCatEntry(relCatRecord, &relCacheEntry.relCatEntry);

//     RelCacheTable::relCache[relId] = (struct RelCacheEntry*) malloc(sizeof(RelCacheEntry));
//     *(RelCacheTable::relCache[relId]) = relCacheEntry;

//     // add the attributes of the corresponding relation into the attrCache as attrCacheEntries
//     AttrCacheEntry *listHead = nullptr;
//     int numOfAttrs = relCatRecord[RELCAT_NO_ATTRIBUTES_INDEX].nVal;
    
//     AttrCacheEntry *prev = nullptr;

//     RelCacheTable::resetSearchIndex(ATTRCAT_RELID);

//     for (int i = 0; i < numOfAttrs; i++) {
//         RecId attrCatRecId = BlockAccess::linearSearch(ATTRCAT_RELID, relCatAttrRelName, relNameAttribute, EQ);
//         if (attrCatRecId.block == -1 || attrCatRecId.slot == -1) {
//             return E_ATTRNOTEXIST;
//         }

//         struct AttrCacheEntry *curr = (AttrCacheEntry *) malloc(sizeof(AttrCacheEntry));
//         RecBuffer attrCatBlock(attrCatRecId.block);
//         Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
//         attrCatBlock.getRecord(attrCatRecord, attrCatRecId.slot);
//         AttrCacheTable::recordToAttrCatEntry(attrCatRecord, &curr->attrCatEntry);
//         curr->next = nullptr;
//         curr->recId.block = attrCatRecId.block;
//         curr->recId.slot = attrCatRecId.slot;
        
//         if (prev) prev->next = curr;
//         else listHead = curr;

//         prev = curr;
//     }

//     AttrCacheTable::attrCache[relId] = listHead;
//     OpenRelTable::tableMetaInfo[relId].free = false;
//     strcpy(OpenRelTable::tableMetaInfo[relId].relName, relName);

//     // printRelCatAndAttrCatInCache_(relId);

//     if (AttrCacheTable::attrCache[relId] == nullptr) {
//         return E_ATTRNOTEXIST;
//     }

//     return relId;
// }

// int OpenRelTable::closeRel(int relId) {
//     if (relId == RELCAT_RELID || relId == ATTRCAT_RELID) {
//         return E_NOTPERMITTED;
//     }

//     if (relId < 0 || relId >= MAX_OPEN) {
//         return E_OUTOFBOUND;
//     }

//     if (AttrCacheTable::attrCache[relId] == nullptr) {
//         return E_RELNOTOPEN;
//     }

//     free(RelCacheTable::relCache[relId]);
//     RelCacheTable::relCache[relId] = nullptr;

//     AttrCacheEntry *entry = AttrCacheTable::attrCache[relId];
//     while (entry) {
//         AttrCacheEntry *next = entry->next;
//         free(entry);
//         entry = next;
//     }
//     AttrCacheTable::attrCache[relId] = nullptr;

//     OpenRelTable::tableMetaInfo[relId].free = true;
//     return SUCCESS;
// }

// int OpenRelTable::getFreeOpenRelTableEntry() {
//     for (int i = 0; i < MAX_OPEN; i++) {
//         if (OpenRelTable::tableMetaInfo[i].free) {
//             return i;
//         }
//     }

//     return E_CACHEFULL;
// }