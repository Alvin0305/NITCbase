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

// int Algebra::select(char srcRel[ATTR_SIZE], char targetRel[ATTR_SIZE], char attr[ATTR_SIZE], int op,
//                     char strVal[ATTR_SIZE]) {
//   int srcRelId = OpenRelTable::getRelId(srcRel);
//   if (srcRelId == E_RELNOTOPEN) {
//     return E_RELNOTOPEN;
//   }

//   AttrCatEntry attrCatEntry;
//   int ret = AttrCacheTable::getAttrCatEntry(srcRelId, attr, &attrCatEntry);
//   if (ret == E_ATTRNOTEXIST) {
//     return E_ATTRNOTEXIST;
//   }

//   int type = attrCatEntry.attrType;
//   Attribute attrVal;

//   if (type == NUMBER) {
//     if (isNumber(strVal)) {
//       attrVal.nVal = atof(strVal);
//     } else {
//       return E_ATTRTYPEMISMATCH;
//     }
//   } else if (type == STRING) {
//     strcpy(attrVal.sVal, strVal);
//   }

//   RelCatEntry relCatEntry;
//   RelCacheTable::getRelCatEntry(srcRelId, &relCatEntry);

//   int srcNoAttrs = relCatEntry.numAttrs;
//   int attrTypes[srcNoAttrs];

//   for (int attrIndex = 0; attrIndex < srcNoAttrs; attrIndex++) {
//     AttrCatEntry attrCatEntry;
//     AttrCacheTable::getAttrCatEntry(srcRelId, attrIndex, &attrCatEntry);

//     attrTypes[attrIndex] = attrCatEntry.attrType;
//   }

//   RelCacheTable::resetSearchIndex(srcRelId);

//   printf("|");
//   for (int i = 0; i < relCatEntry.numAttrs; i++) {
//     AttrCatEntry attrCatEntry;
//     AttrCacheTable::getAttrCatEntry(srcRelId, i, &attrCatEntry);
//     printf(" %s |", attrCatEntry.attrName);
//   }

//   printf("\n");

//   while (true) {
//     RecId searchResult = BlockAccess::linearSearch(srcRelId, attr, attrVal, op);

//     if (searchResult.block != -1 && searchResult.slot != -1) {
//       RecBuffer block(searchResult.block);
//       Attribute attributes[srcNoAttrs];
//       block.getRecord(attributes, searchResult.slot);

//       printf("|");
//       for (int attrIndex = 0; attrIndex < srcNoAttrs; attrIndex++) {
//         if (attrTypes[attrIndex] == NUMBER) {
//           printf(" %g |", attributes[attrIndex].nVal);
//         } else {
//           printf(" %s |", attributes[attrIndex].sVal);
//         }
//       }

//       printf("\n");
//     } else {
//       break;
//     }
//   }

//   return SUCCESS;
// }

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