#include "Schema.h"

#include <cmath>
#include <cstring>
#include <iostream>

int Schema::openRel(char relname[ATTR_SIZE]) {
  int ret = OpenRelTable::openRel(relname);
  if (ret >= 0) {
    return SUCCESS;
  }

  return ret;
}

int Schema::closeRel(char relname[ATTR_SIZE]) {
  if (strcmp(relname, RELCAT_RELNAME) == 0 || strcmp(relname, ATTRCAT_RELNAME) == 0) {
    return E_NOTPERMITTED;
  }

  int relId = OpenRelTable::getRelId(relname);

  if (relId == E_RELNOTOPEN) {
    return E_RELNOTOPEN;
  }

  return OpenRelTable::closeRel(relId);
}

int Schema::renameRel(char oldRelName[ATTR_SIZE], char newRelName[ATTR_SIZE]) {
  if (strcmp(oldRelName, RELCAT_RELNAME) == 0 || strcmp(oldRelName, ATTRCAT_RELNAME) == 0 ||
      strcmp(newRelName, RELCAT_RELNAME) == 0 || strcmp(newRelName, ATTRCAT_RELNAME) == 0) {
    return E_NOTPERMITTED;
  }

  if (OpenRelTable::getRelId(oldRelName) != E_RELNOTOPEN) {
    return E_RELOPEN;
  }

  return BlockAccess::renameRelation(oldRelName, newRelName);
}

int Schema::renameAttr(char relName[ATTR_SIZE], char oldAttrName[ATTR_SIZE], char newAttrName[ATTR_SIZE]) {
  if (strcmp(relName, RELCAT_RELNAME) == 0 || strcmp(relName, ATTRCAT_RELNAME) == 0) {
    return E_NOTPERMITTED;
  }

  if (OpenRelTable::getRelId(relName) != E_RELNOTOPEN) {
    return E_RELOPEN;
  }

  return BlockAccess::renameAttribute(relName, oldAttrName, newAttrName);
}

// ============================== Stage 8 =============================
// create a new relation
int Schema::createRel(char relName[], int nAttrs, char attrs[][ATTR_SIZE], int attrType[]) {
  Attribute relNameAsAttribute;
  strcpy(relNameAsAttribute.sVal, relName);

  RecId targetRelId = {-1, -1};

  RelCacheTable::resetSearchIndex(RELCAT_RELID);
  char relCatAttrRelName[ATTR_SIZE] = RELCAT_ATTR_RELNAME;
  targetRelId = BlockAccess::linearSearch(RELCAT_RELID, relCatAttrRelName, relNameAsAttribute, EQ);

  // checking whether a table with that name already exists
  if (targetRelId.block != -1 && targetRelId.slot != -1) {
    return E_RELEXIST;
  }

  // checking whether the given list of attributes itself contains any duplicates
  for (int i = 0; i < nAttrs; i++) {
    for (int j = i + 1; j < nAttrs; j++) {
      if (strcmp(attrs[i], attrs[j]) == 0) {
        return E_DUPLICATEATTR;
      }
    }
  }

  // created the rel cat entry to be inserted in the relation catalog
  Attribute relCatRecord[RELCAT_NO_ATTRS];
  strcpy(relCatRecord[RELCAT_REL_NAME_INDEX].sVal, relName);
  relCatRecord[RELCAT_NO_ATTRIBUTES_INDEX].nVal = nAttrs;
  relCatRecord[RELCAT_NO_RECORDS_INDEX].nVal = 0;
  relCatRecord[RELCAT_FIRST_BLOCK_INDEX].nVal = -1;
  relCatRecord[RELCAT_LAST_BLOCK_INDEX].nVal = -1;
  relCatRecord[RELCAT_NO_SLOTS_PER_BLOCK_INDEX].nVal = floor((2016 * 1.00) / (16 * nAttrs + 1));

  // puts the created relation catalog entry in relation catalog
  int retVal = BlockAccess::insert(RELCAT_RELID, relCatRecord);
  if (retVal != SUCCESS) {
    return retVal;
  }

  // inserting each attribute into the attribute catalog
  for (int i = 0; i < nAttrs; i++) {
    Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
    strcpy(attrCatRecord[ATTRCAT_REL_NAME_INDEX].sVal, relName);
    strcpy(attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, attrs[i]);
    attrCatRecord[ATTRCAT_ATTR_TYPE_INDEX].nVal = attrType[i];
    attrCatRecord[ATTRCAT_PRIMARY_FLAG_INDEX].nVal = -1;
    attrCatRecord[ATTRCAT_ROOT_BLOCK_INDEX].nVal = -1;
    attrCatRecord[ATTRCAT_OFFSET_INDEX].nVal = i;

    retVal = BlockAccess::insert(ATTRCAT_RELID, attrCatRecord);
    // if insertion fails, the relation catalog entry in relation catalog should be deleted
    if (retVal != SUCCESS) {
      Schema::deleteRel(relName);
      return retVal;
    }
  }

  return SUCCESS;
}

// ============================== Stage 8 =============================
// drop a relation
int Schema::deleteRel(char relName[]) {
  // RELATIONCAT and ATTRIBUTECAT cannot be deleted
  if (strcmp(relName, RELCAT_RELNAME) == 0 || strcmp(relName, ATTRCAT_RELNAME) == 0) {
    return E_NOTPERMITTED;
  }

  // if the relation is already open, it is not allowed to delete it.
  int relId = OpenRelTable::getRelId(relName);
  if (relId != E_RELNOTOPEN) {
    return E_RELOPEN;
  }

  return BlockAccess::deleteRelation(relName);
}

// =============================== Stage 11 ===============================
int Schema::createIndex(char relName[ATTR_SIZE], char attrName[ATTR_SIZE]) {
  // creating index is not allowed on RELCAT or ATTRCAT
  if (strcmp(relName, RELCAT_RELNAME) == 0 or strcmp(relName, ATTRCAT_RELNAME) == 0) {
    return E_NOTPERMITTED;
  }

  // index can be created only on opened relations
  int relId = OpenRelTable::getRelId(relName);
  if (relId == E_RELNOTOPEN) {
    return E_RELNOTOPEN;
  }

  return BPlusTree::bPlusCreate(relId, attrName);
}

// =============================== Stage 11 ===============================
int Schema::dropIndex(char relName[ATTR_SIZE], char attrName[ATTR_SIZE]) {
  // creating and dropping index is not allowed on RELCAT or ATTRCAT
  if (strcmp(relName, RELCAT_RELNAME) == 0 or strcmp(relName, ATTRCAT_RELNAME) == 0) {
    return E_NOTPERMITTED;
  }

  // if the relation is not open, return error
  int relId = OpenRelTable::getRelId(relName);
  if (relId == E_RELNOTOPEN) {
    return E_RELNOTOPEN;
  }

  AttrCatEntry attrCatEntry;
  int ret = AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);
  if (ret != SUCCESS) {
    return E_ATTRNOTEXIST;
  }

  // if root block is -1 => it is not indexed, return error
  int rootBlock = attrCatEntry.rootBlock;
  if (rootBlock == -1) {
    return E_NOINDEX;
  }

  BPlusTree::bPlusDestroy(rootBlock);

  // update the rootblock as -1 and set the attrCatEntry
  attrCatEntry.rootBlock = -1;
  AttrCacheTable::setAttrCatEntry(relId, attrName, &attrCatEntry);

  return SUCCESS;
}