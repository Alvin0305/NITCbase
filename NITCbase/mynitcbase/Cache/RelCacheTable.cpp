#include "RelCacheTable.h"

#include <cstring>
#include <iostream>

RelCacheEntry *RelCacheTable::relCache[MAX_OPEN];

int RelCacheTable::getRelCatEntry(int relId, RelCatEntry* relCatBuf) {
    if (relId < 0 || relId >= MAX_OPEN) {
        return E_OUTOFBOUND;
    }

    if (relCache[relId] == nullptr) {
        return E_RELNOTOPEN;
    }

    *relCatBuf = relCache[relId]->relCatEntry;

    return SUCCESS;
}

void RelCacheTable::recordToRelCatEntry(union Attribute record[RELCAT_NO_ATTRS], RelCatEntry *relCatEntry) {
    strcpy(relCatEntry->relName, record[RELCAT_REL_NAME_INDEX].sVal);
    relCatEntry->firstBlk = (int) record[RELCAT_FIRST_BLOCK_INDEX].nVal;
    relCatEntry->lastBlk = (int) record[RELCAT_LAST_BLOCK_INDEX].nVal;
    relCatEntry->numAttrs = (int) record[RELCAT_NO_ATTRIBUTES_INDEX].nVal;
    relCatEntry->numRecs = (int) record[RELCAT_NO_RECORDS_INDEX].nVal;
    relCatEntry->numSlotsPerBlk = (int) record[RELCAT_NO_SLOTS_PER_BLOCK_INDEX].nVal;

    // modification
    relCatEntry->numOfBlks = (int) relCatEntry->lastBlk - (int) relCatEntry->firstBlk + 1;
}

int RelCacheTable::getSearchIndex(int relId, RecId *searchIndex) {
    if (relId < 0 || relId >= MAX_OPEN) {
        return E_OUTOFBOUND;
    }

    if (RelCacheTable::relCache[relId] == nullptr) {
        return E_RELNOTOPEN;
    }

    *searchIndex = RelCacheTable::relCache[relId]->searchIndex;
    return SUCCESS;
}

int RelCacheTable::setSearchIndex(int relId, RecId *searchIndex) {
    if (relId < 0 || relId >= MAX_OPEN) {
        return E_OUTOFBOUND;
    }

    if (RelCacheTable::relCache[relId] == nullptr) {
        return E_RELNOTOPEN;
    }

    RelCacheTable::relCache[relId]->searchIndex = *searchIndex;
    return SUCCESS;
}

int RelCacheTable::resetSearchIndex(int relId) {
    struct RecId searchIndex = RecId{-1, -1};
    return RelCacheTable::setSearchIndex(relId, &searchIndex);
}