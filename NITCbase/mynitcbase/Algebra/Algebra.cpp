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

  for (int attrIndex = 0; attrIndex < srcNoAttrs; attrIndex++) {
    AttrCatEntry attrCatEntry;
    AttrCacheTable::getAttrCatEntry(srcRelId, attrIndex, &attrCatEntry);

    attrTypes[attrIndex] = attrCatEntry.attrType;
  }

  RelCacheTable::resetSearchIndex(srcRelId);

  printf("|");
  for (int i = 0; i < relCatEntry.numAttrs; i++) {
    AttrCatEntry attrCatEntry;
    AttrCacheTable::getAttrCatEntry(srcRelId, i, &attrCatEntry);
    printf(" %s |", attrCatEntry.attrName);
  }

  printf("\n");

  while (true) {
    RecId searchResult = BlockAccess::linearSearch(srcRelId, attr, attrVal, op);

    if (searchResult.block != -1 && searchResult.slot != -1) {
      RecBuffer block(searchResult.block);
      Attribute attributes[srcNoAttrs];
      block.getRecord(attributes, searchResult.slot);

      printf("|");
      for (int attrIndex = 0; attrIndex < srcNoAttrs; attrIndex++) {
        if (attrTypes[attrIndex] == NUMBER) {
          printf(" %g |", attributes[attrIndex].nVal);
        } else {
          printf(" %s |", attributes[attrIndex].sVal);
        }
      }

      printf("\n");
    } else {
      break;
    }
  }

  return SUCCESS;
}

// =========================== Stage 7 ==================================
int Algebra::insert(char srcRel[ATTR_SIZE], int nAttrs, char record[][ATTR_SIZE]) {
  if (strcmp(srcRel, RELCAT_RELNAME) == 0 || strcmp(srcRel, ATTRCAT_RELNAME) == 0) {
    return E_NOTPERMITTED;
  }

  int relId = OpenRelTable::getRelId(srcRel);

  if (relId == E_RELNOTOPEN) {
    return E_RELNOTOPEN;
  }

  RelCatEntry relCatEnty;
  RelCacheTable::getRelCatEntry(relId, &relCatEnty);

  if (relCatEnty.numAttrs != nAttrs) {
    return E_NATTRMISMATCH;
  }

  Attribute recordValues[nAttrs];

  std::cout << "num of attrs are: " << nAttrs << std::endl;
  std::cout << "Attribute values going to insert are: ";

  for (int i = 0; i < nAttrs; i++) {
    AttrCatEntry attrCatEntry;
    int ret = AttrCacheTable::getAttrCatEntry(relId, i, &attrCatEntry);
    if (ret != SUCCESS) {
      std::cout << "Failed to get attr cat entry 1" << std::endl;
    }

    int type = attrCatEntry.attrType;
    if (type == NUMBER) {
      if (isNumber(record[i])) {
        recordValues[i].nVal = atof(record[i]);
        printf("%f -> %f ", atof(record[i]), recordValues[i].nVal);
      } else {
        return E_ATTRTYPEMISMATCH;
      }
    } else if (type == STRING) {
      strcpy(recordValues[i].sVal, record[i]);
      printf("%s -> %s ", record[i], recordValues[i].sVal);
    } else {
      std::cout << "invalid type: " << type << std::endl;
    }
  }

  std::cout << std::endl;

  return BlockAccess::insert(relId, recordValues);
}