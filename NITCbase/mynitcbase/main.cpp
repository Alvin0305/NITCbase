#include "Buffer/StaticBuffer.h"
#include "Cache/OpenRelTable.h"
#include "Disk_Class/Disk.h"
#include "FrontendInterface/FrontendInterface.h"
#include <iostream>
#include <cstring>

void printSchema() {
  RecBuffer relCatBuffer(RELCAT_BLOCK);
  HeadInfo relCatHeader;
  relCatBuffer.getHeader(&relCatHeader);

  for (int i = 0; i < relCatHeader.numEntries; i++) {
    Attribute relCatRecord[RELCAT_NO_ATTRS];
    relCatBuffer.getRecord(relCatRecord, i);

    printf("Relation: %s\n", relCatRecord[RELCAT_REL_NAME_INDEX].sVal);

    int currentAttributeBlockNumber = ATTRCAT_BLOCK;

    do {
      RecBuffer attrCatBuffer(currentAttributeBlockNumber);
      HeadInfo attrCatHeader;
      attrCatBuffer.getHeader(&attrCatHeader);

      for (int j = 0; j < attrCatHeader.numEntries; j++) {
        Attribute attrCatRecord[RELCAT_NO_ATTRS];
        attrCatBuffer.getRecord(attrCatRecord, j);

        if (strcmp(relCatRecord[RELCAT_REL_NAME_INDEX].sVal, attrCatRecord[ATTRCAT_REL_NAME_INDEX].sVal) == 0) {
          const char *attrType = attrCatRecord[ATTRCAT_ATTR_TYPE_INDEX].nVal == NUMBER ? "NUM" : "STR";
          const char *attrName = attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal;
          printf("  %s: %s\n", attrName, attrType);
        }
      }
      currentAttributeBlockNumber = attrCatHeader.rblock;
    } while (currentAttributeBlockNumber != -1);

    printf("\n");
  }
}

void printRelCat() {
  RecBuffer relCatBuffer(RELCAT_BLOCK);
  HeadInfo relCatHeader;
  relCatBuffer.getHeader(&relCatHeader);
  for (int i = 0; i < relCatHeader.numEntries; i++) {
    Attribute relCatRecord[RELCAT_NO_ATTRS];
    relCatBuffer.getRecord(relCatRecord, i);
    printf("%s\n", relCatRecord[RELCAT_REL_NAME_INDEX].sVal);
  }
}

void updateSchema(char *relationName, char *attributeName, char *newAttributeName) {
  RecBuffer attrCatBuffer(ATTRCAT_BLOCK);
  HeadInfo attrCatHeader;
  attrCatBuffer.getHeader(&attrCatHeader);

  for (int i = 0; i < attrCatHeader.numEntries; i++) {
    Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
    attrCatBuffer.getRecord(attrCatRecord, i);

    if (strcmp(attrCatRecord[ATTRCAT_REL_NAME_INDEX].sVal, relationName) == 0
    && strcmp(attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, attributeName) == 0
  ) {
      printf("%s %s\n", attrCatRecord[ATTRCAT_REL_NAME_INDEX].sVal, attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal);
      memset(attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, 0, ATTR_SIZE);
      memcpy(attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, newAttributeName, strlen(newAttributeName));

      attrCatBuffer.setRecord(attrCatRecord, i);
    }
  }
}

void printBMAP(int n) {
  int blockNum = 0;
  do {
    unsigned char buffer[BLOCK_SIZE];
    Disk::readBlock(buffer, blockNum);
    for (int i = 0; i < std::min(BLOCK_SIZE, n); i++) {
      printf("%d: %d\n", blockNum * BLOCK_SIZE + i, buffer[i]);
    }
    n -= BLOCK_SIZE;
    blockNum++;
  } while (n >= 0);
}

void printRelCatAndAttrCatInCache() {
    for (int i = 0; i < 2; i++) {
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

int main(int argc, char *argv[]) {
  /* Initialize the Run Copy of Disk */

  Disk disk_run;
  StaticBuffer buffer;
  OpenRelTable cache;

  // StaticBuffer buffer;
  // OpenRelTable cache;

  //--------------------------------------------
  //      Stage 1 -> First Implementation
  //--------------------------------------------

  /*
  unsigned char buffer1[BLOCK_SIZE];
  unsigned char buffer2[BLOCK_SIZE];

  Disk::readBlock(buffer1, 7000);      // reads the initial contents in the block into the buffer
  char message1[] = "Hello";
  memcpy(buffer1 + 20, message1, 6);   // copies the new content into the buffer
  Disk::writeBlock(buffer1, 7000);     // writes the new content back to the block

  char message2[6];
  Disk::readBlock(buffer2, 7000);      // reads the content back from the same buffer
  memcpy(message2, buffer2 + 20, 6);   // copies it to message2
  std::cout << message2 << std::endl;
  
  return FrontendInterface::handleFrontend(argc, argv);
  */

  //--------------------------------------------
  //      Stage 1 -> Q1 Implementation
  //--------------------------------------------

  // printBMAP(10);

  //--------------------------------------------
  //      Stage 2 -> First Implementation
  //--------------------------------------------

  // RecBuffer relCatBuffer(RELCAT_BLOCK);
  // RecBuffer attrCatBuffer(ATTRCAT_BLOCK);

  // HeadInfo relCatHeader;
  // HeadInfo attrCatHeader;

  // relCatBuffer.getHeader(&relCatHeader);
  // attrCatBuffer.getHeader(&attrCatHeader);

  // for (int i = 0; i < relCatHeader.numEntries; i++) {
  //   Attribute relCatRecord[RELCAT_NO_ATTRS];
  //   relCatBuffer.getRecord(relCatRecord, i);

  //   printf("Relation: %s\n", relCatRecord[RELCAT_REL_NAME_INDEX].sVal);
    
  //   for (int j = 0; j < attrCatHeader.numEntries; j++) {
  //     Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
  //     attrCatBuffer.getRecord(attrCatRecord, j);
    
  //     if (strcmp(attrCatRecord[ATTRCAT_REL_NAME_INDEX].sVal, relCatRecord[RELCAT_REL_NAME_INDEX].sVal) == 0) {
  //       const char *attrType = attrCatRecord[ATTRCAT_ATTR_TYPE_INDEX].nVal == NUMBER ? "NUM" : "STR";
  //       const char *attrName = attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal;
  //       printf("  %s: %s\n", attrName, attrType);
  //     }
  //   }
  //   printf("\n");
  // }

  //--------------------------------------------
  //      Stage 2 -> Q1 Implementation
  //--------------------------------------------

  // RecBuffer relCatBuffer(RELCAT_BLOCK);
  // HeadInfo relCatHeader;
  // relCatBuffer.getHeader(&relCatHeader);

  // for (int i = 0; i < relCatHeader.numEntries; i++) {
  //     Attribute relCatRecord[RELCAT_NO_ATTRS];
  //     relCatBuffer.getRecord(relCatRecord, i);

  //     printf("Relation: %s\n", relCatRecord[RELCAT_REL_NAME_INDEX].sVal);  
  //     int currentBlockNum = ATTRCAT_BLOCK;
  //   do {
  //     RecBuffer attrCatBuffer(currentBlockNum);
  //     HeadInfo attrCatHeader;
  //     attrCatBuffer.getHeader(&attrCatHeader);

      
  //     for (int j = 0; j < attrCatHeader.numEntries; j++) {
  //         Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
  //         attrCatBuffer.getRecord(attrCatRecord, j);
      
  //         if (strcmp(attrCatRecord[ATTRCAT_REL_NAME_INDEX].sVal, relCatRecord[RELCAT_REL_NAME_INDEX].sVal) == 0) {
  //           const char *attrType = attrCatRecord[ATTRCAT_ATTR_TYPE_INDEX].nVal == NUMBER ? "NUM" : "STR";
  //           const char *attrName = attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal;
  //           printf("  %s: %s\n", attrName, attrType);
  //         }
  //       }
  //     currentBlockNum = attrCatHeader.rblock;

  //   } while (currentBlockNum != -1);

  //   printf("\n");
  // }

  //--------------------------------------------
  //      Stage 2 -> Q2 Implementation
  //--------------------------------------------

  // char relName[] = "Students";
  // char attributeName[] = "Batch  ";
  // char newAttributeName[] = "Class";
  // updateSchema(relName, attributeName, newAttributeName);

  // printSchema();

  //--------------------------------------------
  //      Stage 3 -> First Implementation
  //--------------------------------------------

  printRelCatAndAttrCatInCache();
  printSchema();

  return FrontendInterface::handleFrontend(argc, argv);
}