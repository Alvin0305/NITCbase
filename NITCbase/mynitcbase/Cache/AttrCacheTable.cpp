#include "AttrCacheTable.h"

#include <cstring>
#include <iostream>

AttrCacheEntry *AttrCacheTable::attrCache[MAX_OPEN];

int AttrCacheTable::getAttrCatEntry(int relId, int attrOffset, AttrCatEntry *attrCatBuf) {
    if (relId < 0 || relId >= MAX_OPEN) {
        return E_OUTOFBOUND;
    }

    if (attrCache[relId] == nullptr) {
        return E_RELNOTOPEN;
    }

    for (AttrCacheEntry *entry = attrCache[relId]; entry != nullptr; entry = entry->next) {
        if (entry->attrCatEntry.offset == attrOffset) {
            *attrCatBuf = entry->attrCatEntry;

            return SUCCESS;
        }
    }

    return E_ATTRNOTEXIST;
}

void AttrCacheTable::recordToAttrCatEntry(union Attribute record[ATTRCAT_NO_ATTRS], AttrCatEntry *attrCatEntry) {
    strcpy(attrCatEntry->attrName, record[ATTRCAT_ATTR_NAME_INDEX].sVal);
    strcpy(attrCatEntry->relName, record[ATTRCAT_REL_NAME_INDEX].sVal);
    attrCatEntry->attrType = (int) record[ATTRCAT_ATTR_TYPE_INDEX].nVal;
    attrCatEntry->offset = (int) record[ATTRCAT_OFFSET_INDEX].nVal;
    attrCatEntry->primaryFlag = (int) record[ATTRCAT_PRIMARY_FLAG_INDEX].nVal;
    attrCatEntry->rootBlock = (int) record[ATTRCAT_ROOT_BLOCK_INDEX].nVal;
}

int AttrCacheTable::getAttrCatEntry(int relId, char attrName[ATTR_SIZE], AttrCatEntry *attrCatBuffer) {
    if (relId < 0 || relId >= MAX_OPEN) {
        return E_OUTOFBOUND;
    }

    if (AttrCacheTable::attrCache[relId] == nullptr) {
        return E_RELNOTOPEN;
    }

    for (AttrCacheEntry *entry = AttrCacheTable::attrCache[relId]; entry != nullptr; entry = entry->next) {
        if (strcmp(entry->attrCatEntry.attrName, attrName) == 0) {
            *attrCatBuffer = entry->attrCatEntry;
            return SUCCESS;
        }
    }

    return E_ATTRNOTEXIST;
}

int AttrCacheTable::setSearchIndex(int relId, char attrName[ATTR_SIZE], IndexId *searchIndex) {
    if (relId < 0 || relId >= MAX_OPEN) {
        return E_OUTOFBOUND;
    }

    if (AttrCacheTable::attrCache[relId] == nullptr) {
        return E_RELNOTOPEN;
    }

    for (AttrCacheEntry *entry = AttrCacheTable::attrCache[relId]; entry != nullptr; entry = entry->next) {
        if (strcmp(entry->attrCatEntry.attrName, attrName) == 0) {
            entry->searchIndex = *searchIndex;
            return SUCCESS;
        }
    }

    return E_ATTRNOTEXIST;
}

int AttrCacheTable::setSearchIndex(int relId, int attrOffset, IndexId *searchIndex) {
    if (relId < 0 || relId >= MAX_OPEN) {
        return E_OUTOFBOUND;
    }

    if (AttrCacheTable::attrCache[relId] == nullptr) {
        return E_RELNOTOPEN;
    }

    for (AttrCacheEntry *entry = AttrCacheTable::attrCache[relId]; entry != nullptr; entry = entry->next) {
        if (entry->attrCatEntry.offset == attrOffset) {
            entry->searchIndex = *searchIndex;
            return SUCCESS;
        }
    }

    return E_ATTRNOTEXIST;
}

int AttrCacheTable::resetSearchIndex(int relId, int attrOffset) {
    IndexId newSearchIndex = IndexId{-1, -1};
    return AttrCacheTable::setSearchIndex(relId, attrOffset, &newSearchIndex);
}

int AttrCacheTable::resetSearchIndex(int relId, char attrName[ATTR_SIZE]) {
    IndexId newSearchIndex = IndexId{-1, -1};
    return AttrCacheTable::setSearchIndex(relId, attrName, &newSearchIndex);
}

int AttrCacheTable::getSearchIndex(int relId, int attrOffset, IndexId *searchIndex) {
    if (relId < 0 || relId >= MAX_OPEN) {
        return E_OUTOFBOUND;
    }

    if (AttrCacheTable::attrCache[relId] == nullptr) {
        return E_RELNOTOPEN;
    }

    for (AttrCacheEntry *entry = AttrCacheTable::attrCache[relId]; entry != nullptr; entry = entry->next) {
        if (entry->attrCatEntry.offset == attrOffset) {
            *searchIndex = entry->searchIndex;
            return SUCCESS;
        }
    }

    return E_ATTRNOTEXIST;
}

int AttrCacheTable::getSearchIndex(int relId, char attrName[ATTR_SIZE], IndexId *searchIndex) {
    if (relId < 0 || relId >= MAX_OPEN) {
        return E_OUTOFBOUND;
    }

    if (AttrCacheTable::attrCache[relId] == nullptr) {
        return E_RELNOTOPEN;
    }

    for (AttrCacheEntry *entry = AttrCacheTable::attrCache[relId]; entry != nullptr; entry = entry->next) {
        if (strcmp(entry->attrCatEntry.attrName, attrName) == 0) {
            *searchIndex = entry->searchIndex;
            return SUCCESS;
        }
    }

    return E_ATTRNOTEXIST;
}