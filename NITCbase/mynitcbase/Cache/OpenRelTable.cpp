#include "OpenRelTable.h"

#include <stdio.h>
#include <stdlib.h>

#include <cstring>
#include <iostream>

OpenRelTableMetaInfo OpenRelTable::tableMetaInfo[MAX_OPEN];

void freeLinkedList(AttrCacheEntry **head) {
  if (!head || !*head) return;

  AttrCacheEntry *current = *head;
  while (current) {
    AttrCacheEntry *next = current->next;
    free(current);
    current = next;
  }

  *head = nullptr;
}

OpenRelTable::OpenRelTable() {
  // initialize relCache and attrCache entries with nullptr;
  for (int i = 0; i < MAX_OPEN; i++) {
    RelCacheTable::relCache[i] = nullptr;
    AttrCacheTable::attrCache[i] = nullptr;
    tableMetaInfo[i].free = true;
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

  RelCacheTable::relCache[RELCAT_RELID] = (struct RelCacheEntry *)malloc(sizeof(RelCacheEntry));
  *(RelCacheTable::relCache[RELCAT_RELID]) = relCacheEntry;

  relCatBlock.getRecord(relCatRecord, RELCAT_SLOTNUM_FOR_ATTRCAT);

  struct RelCacheEntry attrCacheEntry;
  RelCacheTable::recordToRelCatEntry(relCatRecord, &attrCacheEntry.relCatEntry);
  attrCacheEntry.recId.block = RELCAT_BLOCK;
  attrCacheEntry.recId.slot = RELCAT_SLOTNUM_FOR_ATTRCAT;

  char attrCatName[ATTR_SIZE];
  strcpy(attrCatName, relCatRecord[ATTRCAT_REL_NAME_INDEX].sVal);

  RelCacheTable::relCache[ATTRCAT_RELID] = (struct RelCacheEntry *)malloc(sizeof(RelCacheEntry));
  *(RelCacheTable::relCache[ATTRCAT_RELID]) = attrCacheEntry;

  RecBuffer attrCatBlock(ATTRCAT_BLOCK);
  Attribute attrCatRecord[ATTRCAT_NO_ATTRS];

  AttrCacheEntry *headRel = nullptr;
  AttrCacheEntry *prevRel = nullptr;

  for (int i = 0; i < RELCAT_NO_ATTRS; i++) {
    attrCatBlock.getRecord(attrCatRecord, i);
    AttrCacheEntry *curr = (AttrCacheEntry *)malloc(sizeof(AttrCacheEntry));
    AttrCacheTable::recordToAttrCatEntry(attrCatRecord, &curr->attrCatEntry);
    curr->recId.block = ATTRCAT_BLOCK;
    curr->recId.slot = i;
    curr->next = nullptr;

    if (prevRel)
      prevRel->next = curr;
    else
      headRel = curr;

    prevRel = curr;
  }

  AttrCacheTable::attrCache[RELCAT_RELID] = headRel;

  AttrCacheEntry *headAttr = nullptr;
  AttrCacheEntry *prevAttr = nullptr;

  for (int i = 0; i < ATTRCAT_NO_ATTRS; i++) {
    int slotNum = i + RELCAT_NO_ATTRS;
    attrCatBlock.getRecord(attrCatRecord, slotNum);
    AttrCacheEntry *curr = (AttrCacheEntry *)malloc(sizeof(AttrCacheEntry));
    AttrCacheTable::recordToAttrCatEntry(attrCatRecord, &curr->attrCatEntry);
    curr->recId.block = ATTRCAT_BLOCK;
    curr->recId.slot = slotNum;
    curr->next = nullptr;

    if (prevAttr)
      prevAttr->next = curr;
    else
      headAttr = curr;

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
}

OpenRelTable::~OpenRelTable() {
  for (int i = 2; i < MAX_OPEN; i++) {
    if (!OpenRelTable::tableMetaInfo[i].free) {
      OpenRelTable::closeRel(i);
    }
  }

  if (RelCacheTable::relCache[ATTRCAT_RELID]->dirty == true) {
    RelCatEntry relCatEntry;
    RelCacheTable::getRelCatEntry(ATTRCAT_RELID, &relCatEntry);

    Attribute relCatRecord[ATTRCAT_NO_ATTRS];
    RelCacheTable::relCatEntryToRecord(&relCatEntry, relCatRecord);

    RecId recId = RelCacheTable::relCache[ATTRCAT_RELID]->recId;
    RecBuffer relCatBlock(recId.block);
    relCatBlock.setRecord(relCatRecord, recId.slot);
  }

  if (RelCacheTable::relCache[RELCAT_RELID]->dirty == true) {
    RelCatEntry relCatEntry;
    RelCacheTable::getRelCatEntry(RELCAT_RELID, &relCatEntry);

    Attribute relCatRecord[RELCAT_NO_ATTRS];
    RelCacheTable::relCatEntryToRecord(&relCatEntry, relCatRecord);

    RecId recId = RelCacheTable::relCache[RELCAT_RELID]->recId;
    RecBuffer relCatBlock(recId.block);
    relCatBlock.setRecord(relCatRecord, recId.slot);
  }

  free(RelCacheTable::relCache[RELCAT_RELID]);
  free(RelCacheTable::relCache[ATTRCAT_RELID]);

  freeLinkedList(&AttrCacheTable::attrCache[RELCAT_RELID]);
  freeLinkedList(&AttrCacheTable::attrCache[ATTRCAT_RELID]);
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
  for (int i = 0; i < MAX_OPEN; i++) {
    if (!OpenRelTable::tableMetaInfo[i].free && strcmp(relName, OpenRelTable::tableMetaInfo[i].relName) == 0) {
      return i;
    }
  }

  return E_RELNOTOPEN;
}

// ======================== Stage 5 ======================
int OpenRelTable::openRel(char relName[ATTR_SIZE]) {
  // If the relation is already opened, then we will return the relId of the opened relation.
  int alreadyExistingRelId = OpenRelTable::getRelId(relName);
  if (alreadyExistingRelId >= 0) return alreadyExistingRelId;

  // find a free slot in the Cache to put the newly opening table
  int freeSlot = OpenRelTable::getFreeOpenRelTableEntry();
  if (freeSlot == E_CACHEFULL) return E_CACHEFULL;

  int relId = freeSlot;
  RelCacheTable::relCache[relId] = (RelCacheEntry *)malloc(sizeof(RelCacheEntry));

  // reset the search index for RELATIONCAT to search the given relation name in the relation catalog.
  RelCacheTable::resetSearchIndex(RELCAT_RELID);

  union Attribute relNameAttribute;
  strcpy(relNameAttribute.sVal, relName);
  char relCatAttrRelName[ATTR_SIZE];
  strcpy(relCatAttrRelName, RELCAT_ATTR_RELNAME);

  // we find the entry of the relation in the RELATIONCAT using linear search.
  RecId relCatRecId = BlockAccess::linearSearch(RELCAT_RELID, relCatAttrRelName, relNameAttribute, EQ);
  if (relCatRecId.block == -1 || relCatRecId.slot == -1) return E_RELNOTEXIST;

  // add the relation catalog entry for the opening table into the relCache as a relCacheEntry
  struct RelCacheEntry relCacheEntry;
  RecBuffer relCatBlock(relCatRecId.block);
  Attribute relCatRecord[RELCAT_NO_ATTRS];
  relCatBlock.getRecord(relCatRecord, relCatRecId.slot);
  RelCacheTable::recordToRelCatEntry(relCatRecord, &relCacheEntry.relCatEntry);

  RelCacheTable::relCache[relId] = (struct RelCacheEntry *)malloc(sizeof(RelCacheEntry));
  *(RelCacheTable::relCache[relId]) = relCacheEntry;
  RelCacheTable::relCache[relId]->recId.block = relCatRecId.block;
  RelCacheTable::relCache[relId]->recId.slot = relCatRecId.slot;

  // add the attributes of the corresponding relation into the attrCache as attrCacheEntries
  AttrCacheEntry *listHead = nullptr;
  int numOfAttrs = relCatRecord[RELCAT_NO_ATTRIBUTES_INDEX].nVal;

  AttrCacheEntry *prev = nullptr;

  // resets the search index for attribute catalog to search the attributes of the given table in the attribute catalog
  // blocks.
  RelCacheTable::resetSearchIndex(ATTRCAT_RELID);

  for (int i = 0; i < numOfAttrs; i++) {
    // searches and find all the "numAttrs" number of attributes of the given relation and puts it into the AttrCache
    // linked list.
    RecId attrCatRecId = BlockAccess::linearSearch(ATTRCAT_RELID, relCatAttrRelName, relNameAttribute, EQ);
    if (attrCatRecId.block == -1 || attrCatRecId.slot == -1) {
      return E_ATTRNOTEXIST;
    }

    struct AttrCacheEntry *curr = (AttrCacheEntry *)malloc(sizeof(AttrCacheEntry));
    RecBuffer attrCatBlock(attrCatRecId.block);
    Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
    attrCatBlock.getRecord(attrCatRecord, attrCatRecId.slot);
    AttrCacheTable::recordToAttrCatEntry(attrCatRecord, &curr->attrCatEntry);
    curr->next = nullptr;
    curr->recId.block = attrCatRecId.block;
    curr->recId.slot = attrCatRecId.slot;

    if (prev)
      prev->next = curr;
    else
      listHead = curr;

    prev = curr;
  }

  // saves the head of the linked list in the attrCache and initializes the other metadata.
  AttrCacheTable::attrCache[relId] = listHead;
  RelCacheTable::relCache[relId]->dirty = false;
  OpenRelTable::tableMetaInfo[relId].free = false;
  strcpy(OpenRelTable::tableMetaInfo[relId].relName, relName);

  if (AttrCacheTable::attrCache[relId] == nullptr) return E_ATTRNOTEXIST;

  return relId;
}

int OpenRelTable::closeRel(int relId) {
  // relation catalog and attribute catalog won't be allowed to close
  if (relId == RELCAT_RELID || relId == ATTRCAT_RELID) return E_NOTPERMITTED;

  // checks the validity of the relId
  if (relId < 0 || relId >= MAX_OPEN) return E_OUTOFBOUND;

  // checks if the table is opened or not
  if (AttrCacheTable::attrCache[relId] == nullptr) return E_RELNOTOPEN;

  // ======================== Stage 7 ==============================
  if (RelCacheTable::relCache[relId]->dirty == true) {
    Attribute record[RELCAT_NO_ATTRS];
    RelCacheTable::relCatEntryToRecord(&(RelCacheTable::relCache[relId]->relCatEntry), record);

    RecId recId = RelCacheTable::relCache[relId]->recId;

    RecBuffer relCatBlock(recId.block);
    int ret = relCatBlock.setRecord(record, recId.slot);
    if (ret != SUCCESS) {
      printf("Failed to set record");
    }
  }

  // free the caches
  free(RelCacheTable::relCache[relId]);
  freeLinkedList(&AttrCacheTable::attrCache[relId]);

  // mark the pointers as null.
  RelCacheTable::relCache[relId] = nullptr;
  AttrCacheTable::attrCache[relId] = nullptr;

  // update the open rel table meta-info
  OpenRelTable::tableMetaInfo[relId].free = true;
  strcpy(OpenRelTable::tableMetaInfo[relId].relName, "");

  return SUCCESS;
}

int OpenRelTable::getFreeOpenRelTableEntry() {
  for (int i = 0; i < MAX_OPEN; i++) {
    if (OpenRelTable::tableMetaInfo[i].free) {
      return i;
    }
  }

  return E_CACHEFULL;
}