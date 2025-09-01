#include "Algebra.h"

#include <stdio.h>
#include <stdlib.h>

#include <cstring>
#include <iostream>

bool isNumber(char *str) {
  int len;
  float ignore;
  int ret = sscanf(str, "%f %n", &ignore, &len);
  return ret == 1 && len == strlen(str);
}

// ======================== Stage 9 ===========================
int Algebra::select(char srcRel[ATTR_SIZE], char targetRel[ATTR_SIZE], char attr[ATTR_SIZE], int op,
                    char strVal[ATTR_SIZE]) {
  int srcRelId = OpenRelTable::getRelId(srcRel);
  if (srcRelId == E_RELNOTOPEN) {
    return E_RELNOTOPEN;
  }

  AttrCatEntry attrCatEntry;
  int ret = AttrCacheTable::getAttrCatEntry(srcRelId, attr, &attrCatEntry);
  if (ret == E_ATTRNOTEXIST) {
    return E_ATTRNOTEXIST;
  }

  int type = attrCatEntry.attrType;
  Attribute attrVal;

  if (type == NUMBER) {
    if (isNumber(strVal)) {
      attrVal.nVal = atof(strVal);
    } else {
      return E_ATTRTYPEMISMATCH;
    }
  } else if (type == STRING) {
    strcpy(attrVal.sVal, strVal);
  }

  RelCatEntry relCatEntry;
  RelCacheTable::getRelCatEntry(srcRelId, &relCatEntry);

  int srcNoAttrs = relCatEntry.numAttrs;
  int attrTypes[srcNoAttrs];
  char attrNames[srcNoAttrs][ATTR_SIZE];

  for (int attrIndex = 0; attrIndex < srcNoAttrs; attrIndex++) {
    AttrCatEntry attrCatEntry;
    AttrCacheTable::getAttrCatEntry(srcRelId, attrIndex, &attrCatEntry);

    attrTypes[attrIndex] = attrCatEntry.attrType;
    strcpy(attrNames[attrIndex], attrCatEntry.attrName);
  }

  ret = Schema::createRel(targetRel, srcNoAttrs, attrNames, attrTypes);
  if (ret != SUCCESS) {
    return ret;
  }

  int targetRelId = OpenRelTable::openRel(targetRel);
  if (targetRelId < 0 or targetRelId >= MAX_OPEN) {
    Schema::deleteRel(targetRel);
    return targetRelId;
  }

  RelCacheTable::resetSearchIndex(srcRelId);
  AttrCacheTable::resetSearchIndex(srcRelId, attr);
  Attribute record[srcNoAttrs];

  // ======================= Stage 10 =======================
  AttrCacheTable::resetSearchIndex(srcRelId, attr);

  while (BlockAccess::search(srcRelId, record, attr, attrVal, op) == SUCCESS) {
    ret = BlockAccess::insert(targetRelId, record);

    if (ret != SUCCESS) {
      Schema::closeRel(targetRel);
      Schema::deleteRel(targetRel);
      return ret;
    }
  }

  printf("Num of comparisons done in bplus tree: %d\n", BPlusTree::numOfComparisons);

  Schema::closeRel(targetRel);
  return SUCCESS;
}

// =========================== Stage 7 ==================================
// insert a record into the srcRel
int Algebra::insert(char srcRel[ATTR_SIZE], int nAttrs, char record[][ATTR_SIZE]) {
  // insertion cannot be done to RELCAT and ATTRCAT
  if (strcmp(srcRel, RELCAT_RELNAME) == 0 || strcmp(srcRel, ATTRCAT_RELNAME) == 0) {
    return E_NOTPERMITTED;
  }

  int relId = OpenRelTable::getRelId(srcRel);

  // insertion can be done to open tables only
  if (relId == E_RELNOTOPEN) {
    return E_RELNOTOPEN;
  }

  RelCatEntry relCatEnty;
  RelCacheTable::getRelCatEntry(relId, &relCatEnty);

  // checks if the number of attributes match
  if (relCatEnty.numAttrs != nAttrs) {
    return E_NATTRMISMATCH;
  }

  Attribute recordValues[nAttrs];

  // creating the record of Attributes
  for (int i = 0; i < nAttrs; i++) {
    AttrCatEntry attrCatEntry;
    AttrCacheTable::getAttrCatEntry(relId, i, &attrCatEntry);

    int type = attrCatEntry.attrType;
    if (type == NUMBER) {
      if (isNumber(record[i])) {
        recordValues[i].nVal = atof(record[i]);
      } else {
        return E_ATTRTYPEMISMATCH;
      }
    } else if (type == STRING) {
      strcpy(recordValues[i].sVal, record[i]);
    }
  }

  return BlockAccess::insert(relId, recordValues);
}

// =========================== Stage 9 ===========================
// project all the attributes of src Rel to target Rel
int Algebra::project(char srcRel[ATTR_SIZE], char targetRel[ATTR_SIZE]) {
  int srcRelId = OpenRelTable::getRelId(srcRel);

  // checks if the table is open or not
  if (srcRelId == E_RELNOTOPEN) {
    return E_RELNOTOPEN;
  }

  RelCatEntry relCatEntry;
  RelCacheTable::getRelCatEntry(srcRelId, &relCatEntry);

  int numAttrs = relCatEntry.numAttrs;
  char attrNames[numAttrs][ATTR_SIZE];
  int attrTypes[numAttrs];

  // populate the attrNames for creating the new relation
  for (int attrIndex = 0; attrIndex < numAttrs; attrIndex++) {
    AttrCatEntry attrCatEntry;
    AttrCacheTable::getAttrCatEntry(srcRelId, attrIndex, &attrCatEntry);
    strcpy(attrNames[attrIndex], attrCatEntry.attrName);
    attrTypes[attrIndex] = attrCatEntry.attrType;
  }

  // create the new relation to project the srcRelation
  int ret = Schema::createRel(targetRel, numAttrs, attrNames, attrTypes);
  if (ret != SUCCESS) {
    return ret;
  }

  // open the new relation
  int targetRelId = OpenRelTable::openRel(targetRel);

  // if failed, delete the newly created relation
  if (targetRelId < 0 or targetRelId >= MAX_OPEN) {
    Schema::deleteRel(targetRel);
    return targetRelId;
  }

  RelCacheTable::resetSearchIndex(srcRelId);
  Attribute record[numAttrs];

  // project each record in the srcRelation to the target relation
  while (BlockAccess::project(srcRelId, record) == SUCCESS) {
    ret = BlockAccess::insert(targetRelId, record);

    // if failed, close and delete the target relation
    if (ret != SUCCESS) {
      Schema::closeRel(targetRel);
      Schema::deleteRel(targetRel);
      return ret;
    }
  }

  // close the created relation at the end
  Schema::closeRel(targetRel);
  return SUCCESS;
}

// =========================== Stage 9 ===========================
// project a list of attributes of src relation to target relation
int Algebra::project(char srcRel[ATTR_SIZE], char targetRel[ATTR_SIZE], int target_nAttrs,
                     char targetAttrs[][ATTR_SIZE]) {
  int srcRelId = OpenRelTable::getRelId(srcRel);

  // checks if the relation is open or not
  if (srcRelId == E_RELNOTOPEN) {
    return E_RELNOTOPEN;
  }

  RelCatEntry relCatEntry;
  RelCacheTable::getRelCatEntry(srcRelId, &relCatEntry);

  int src_nAttrs = relCatEntry.numAttrs;

  // i-th entry in this array represents the offset in a record of source relation for
  // the i-th attribute in target relation
  int attrOffset[target_nAttrs];

  // i-th entry in this array represents the type of i-th attribute in the target relation
  int attrTypes[target_nAttrs];

  // populating attrOffset and attrTypes
  for (int i = 0; i < target_nAttrs; i++) {
    AttrCatEntry attrCatEntry;
    int ret = AttrCacheTable::getAttrCatEntry(srcRelId, targetAttrs[i], &attrCatEntry);
    if (ret == E_ATTRNOTEXIST) {
      return E_ATTRNOTEXIST;
    }

    attrOffset[i] = attrCatEntry.offset;
    attrTypes[i] = attrCatEntry.attrType;
  }

  // create the target relation
  int ret = Schema::createRel(targetRel, target_nAttrs, targetAttrs, attrTypes);
  if (ret != SUCCESS) {
    return ret;
  }

  // open the newly created relation
  int targetRelId = OpenRelTable::openRel(targetRel);
  if (targetRelId < 0 or targetRelId >= MAX_OPEN) {
    Schema::deleteRel(targetRel);
    return targetRelId;
  }

  RelCacheTable::resetSearchIndex(srcRelId);
  Attribute record[src_nAttrs];

  // project each record in the src relation to the target relation
  while (BlockAccess::project(srcRelId, record) == SUCCESS) {
    Attribute projRecord[target_nAttrs];

    for (int i = 0; i < target_nAttrs; i++) {
      projRecord[i] = record[attrOffset[i]];
    }

    // insert the record to the target relation
    ret = BlockAccess::insert(targetRelId, projRecord);

    // if insertion fails, close and delete the target relation
    if (ret != SUCCESS) {
      Schema::closeRel(targetRel);
      Schema::deleteRel(targetRel);
      return ret;
    }
  }

  // finally close the target relation
  Schema::closeRel(targetRel);
  return SUCCESS;
}

// =========================== Stage 12 ===========================
int Algebra::join(char srcRelation1[ATTR_SIZE], char srcRelation2[ATTR_SIZE], char targetRelation[ATTR_SIZE],
                  char attribute1[ATTR_SIZE], char attribute2[ATTR_SIZE]) {
  int ret, ret1, ret2;

  int srcRel1Id = OpenRelTable::getRelId(srcRelation1);
  int srcRel2Id = OpenRelTable::getRelId(srcRelation2);

  if (srcRel1Id == E_RELNOTOPEN or srcRel2Id == E_RELNOTOPEN) {
    return E_RELNOTOPEN;
  }

  // printf("src relations -> %s and %s\n", srcRelation1, srcRelation2);
  // printf("relIds -> %d and %d\n", srcRel1Id, srcRel2Id);
  // printf("%s and %s\n", attribute1, attribute2);

  AttrCatEntry attrCatEntry1, attrCatEntry2;
  ret1 = AttrCacheTable::getAttrCatEntry(srcRel1Id, attribute1, &attrCatEntry1);
  ret2 = AttrCacheTable::getAttrCatEntry(srcRel2Id, attribute2, &attrCatEntry2);

  if (ret1 != SUCCESS or ret2 != SUCCESS) {
    return E_ATTRNOTEXIST;
  }

  // join can be taken only if the types of the attributes are the same
  if (attrCatEntry1.attrType != attrCatEntry2.attrType) {
    return E_ATTRTYPEMISMATCH;
  }

  RelCatEntry relCatEntry1, relCatEntry2;
  RelCacheTable::getRelCatEntry(srcRel1Id, &relCatEntry1);
  RelCacheTable::getRelCatEntry(srcRel2Id, &relCatEntry2);

  int numAttributes1 = relCatEntry1.numAttrs;
  int numAttributes2 = relCatEntry2.numAttrs;

  // check if there is any duplicate attribute names
  // i.e., check if any attribute in relation1 having attribute name as attribute2 other than attribute1
  // and check if any attribute in relation2 having attribute name as attribute1 other than attribute2
  for (int i = 0; i < numAttributes1; i++) {
    AttrCatEntry attrCatEntry1_;
    AttrCacheTable::getAttrCatEntry(srcRel1Id, i, &attrCatEntry1_);

    if (strcmp(attrCatEntry1_.attrName, attribute1) == 0) {
      continue;
    }

    for (int j = 0; j < numAttributes2; j++) {
      AttrCatEntry attrCatEntry2_;
      AttrCacheTable::getAttrCatEntry(srcRel2Id, j, &attrCatEntry2_);
      if (strcmp(attrCatEntry2_.attrName, attribute2) == 0) {
        continue;
      }

      if (strcmp(attrCatEntry1_.attrName, attrCatEntry2_.attrName) == 0) {
        return E_DUPLICATEATTR;
      }
    }
  }

  int rootBlock = attrCatEntry2.rootBlock;

  // if the second relation is not indexed on attribute2, create index on it
  if (rootBlock == -1) {
    ret = BPlusTree::bPlusCreate(srcRel2Id, attribute2);
    if (ret != SUCCESS) {
      return E_DISKFULL;
    }

    rootBlock = attrCatEntry2.rootBlock;
  }

  // the number of attributes in target relation will be one less than the sum of the attribute counts
  // because one attribute is common
  int numOfAttributesInTarget = numAttributes1 + numAttributes2 - 1;

  char targetRelAttrNames[numOfAttributesInTarget][ATTR_SIZE];
  int targetRelAttrTypes[numOfAttributesInTarget];

  // populate the target relation names and target relation attribute types to create the target relation
  for (int i = 0; i < numAttributes1; i++) {
    AttrCatEntry attrCatEntry;
    AttrCacheTable::getAttrCatEntry(srcRel1Id, i, &attrCatEntry);

    strcpy(targetRelAttrNames[i], attrCatEntry.attrName);
    targetRelAttrTypes[i] = attrCatEntry.attrType;
  }

  bool inserted = false;

  for (int j = 0; j < numAttributes2; j++) {
    AttrCatEntry attrCatEntry;
    AttrCacheTable::getAttrCatEntry(srcRel2Id, j, &attrCatEntry);

    if (strcmp(attrCatEntry.attrName, attribute2) == 0) {
      inserted = true;
      continue;
    }

    if (inserted) {
      strcpy(targetRelAttrNames[numAttributes1 + j - 1], attrCatEntry.attrName);
      targetRelAttrTypes[numAttributes1 + j - 1] = attrCatEntry.attrType;
    } else {
      strcpy(targetRelAttrNames[numAttributes1 + j], attrCatEntry.attrName);
      targetRelAttrTypes[numAttributes1 + j] = attrCatEntry.attrType;
    }
  }

  // create the target relation
  ret = Schema::createRel(targetRelation, numOfAttributesInTarget, targetRelAttrNames, targetRelAttrTypes);
  if (ret != SUCCESS) {
    return ret;
  }

  // open the target relation
  // if it fails, delete the target relation
  int targetRelId = OpenRelTable::openRel(targetRelation);
  if (targetRelId < 0 or targetRelId >= MAX_OPEN) {
    Schema::deleteRel(targetRelation);
    return targetRelId;
  }

  Attribute record1[numAttributes1];
  Attribute record2[numAttributes2];
  Attribute targetRecord[numOfAttributesInTarget];

  RelCacheTable::resetSearchIndex(srcRel1Id);
  // now we will iterate the first relation, for each record in first relation, iterate the second relation
  // check if there is any record which have same value for the given attribute in relation 2
  // if yes, create a new record which contains all the attributes of both relations
  // insert the newly created record in the newly created target relation
  while (BlockAccess::project(srcRel1Id, record1) == SUCCESS) {
    RelCacheTable::resetSearchIndex(srcRel2Id);
    AttrCacheTable::resetSearchIndex(srcRel2Id, attribute2);

    // search the second relation to find a record which has the same attribute value
    while (BlockAccess::search(srcRel2Id, record2, attribute2, record1[attrCatEntry1.offset], EQ) == SUCCESS) {
      // if we get such a record, create a new record containing both the relation 1 attributes and
      // relation 2 attributes to insert into the newly created relation
      for (int i = 0; i < numAttributes1; i++) {
        targetRecord[i] = record1[i];
      }

      inserted = false;

      for (int j = 0; j < numAttributes2; j++) {
        if (j == attrCatEntry2.offset) {
          inserted = true;
          continue;
        }

        if (inserted) {
          targetRecord[numAttributes1 + j - 1] = record2[j];
        } else {
          targetRecord[numAttributes1 + j] = record2[j];
        }
      }

      // insert the newly created record into the target relation
      // if it fails, delete the target relation
      ret = BlockAccess::insert(targetRelId, targetRecord);
      if (ret == E_DISKFULL) {
        OpenRelTable::closeRel(targetRelId);
        Schema::deleteRel(targetRelation);

        return E_DISKFULL;
      }
    }
  }

  return SUCCESS;
}