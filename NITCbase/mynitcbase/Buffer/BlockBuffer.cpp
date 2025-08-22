#include "BlockBuffer.h"

#include <cstdlib>
#include <cstring>
#include <iostream>

int compareAttrs(Attribute attr1, Attribute attr2, int attrType) {
  double diff;
  if (attrType == STRING)
    return strcmp(attr1.sVal, attr2.sVal);
  else {
    if (attr1.nVal < attr2.nVal)
      return -1;
    else if (attr1.nVal > attr2.nVal)
      return 1;
    else
      return 0;
  }
}

BlockBuffer::BlockBuffer(int blockNum) { this->blockNum = blockNum; }

// ======================== Stage 7 ======================
// used in the constructor of RecBuffer without arguments (for creating a record buffer of a free block)
BlockBuffer::BlockBuffer(char blockTypeChar) {
  unsigned char *bufferPtr;
  int blockType = blockTypeChar == 'R'   ? REC
                  : blockTypeChar == 'I' ? IND_INTERNAL
                  : blockTypeChar == 'L' ? IND_LEAF
                                         : UNUSED_BLK;

  int freeBlockNum = getFreeBlock(blockType);

  if (freeBlockNum < 0 || blockNum >= DISK_BLOCKS) {
    std::cout << "Failed to get a free block" << std::endl;
    this->blockNum = freeBlockNum;
  } else {
    this->blockNum = freeBlockNum;
  }
}

RecBuffer::RecBuffer(int blockNum) : BlockBuffer::BlockBuffer(blockNum) {}

// ======================== Stage 7 ======================
// used to create a Record Buffer of a free block using the BlockBuffer constructor
RecBuffer::RecBuffer() : BlockBuffer::BlockBuffer('R') {}

// ======================== Stage 7 ======================
// return the blockNum
int BlockBuffer::getBlockNum() { return this->blockNum; }

int BlockBuffer::getHeader(struct HeadInfo *head) {
  unsigned char *bufferPtr;
  int ret = BlockBuffer::loadBlockAndGetBufferPtr(&bufferPtr);
  if (ret != SUCCESS) return ret;

  memcpy(&head->blockType, bufferPtr + 0, 4);
  memcpy(&head->pblock, bufferPtr + 4, 4);
  memcpy(&head->lblock, bufferPtr + 8, 4);
  memcpy(&head->rblock, bufferPtr + 12, 4);
  memcpy(&head->numEntries, bufferPtr + 16, 4);
  memcpy(&head->numAttrs, bufferPtr + 20, 4);
  memcpy(&head->numSlots, bufferPtr + 24, 4);

  return SUCCESS;
}

// ======================== Stage 7 ===========================
// updated the header and sets the dirty bit
int BlockBuffer::setHeader(struct HeadInfo *head) {
  unsigned char *bufferPtr;
  int ret = loadBlockAndGetBufferPtr(&bufferPtr);

  if (ret != SUCCESS) return ret;

  struct HeadInfo *bufferHeader = (struct HeadInfo *)bufferPtr;

  bufferHeader->blockType = head->blockType;
  bufferHeader->pblock = head->pblock;
  bufferHeader->lblock = head->lblock;
  bufferHeader->rblock = head->rblock;
  bufferHeader->numEntries = head->numEntries;
  bufferHeader->numAttrs = head->numAttrs;
  bufferHeader->numSlots = head->numSlots;

  int ret_ = StaticBuffer::setDirtyBit(this->blockNum);
  if (ret_ != SUCCESS) return ret_;

  return SUCCESS;
}

int RecBuffer::getRecord(union Attribute *rec, int slotNum) {
  struct HeadInfo head;
  BlockBuffer::getHeader(&head);

  int attrCount = head.numAttrs;
  int slotCount = head.numSlots;

  unsigned char *bufferPtr;
  int ret = BlockBuffer::loadBlockAndGetBufferPtr(&bufferPtr);
  if (ret != SUCCESS) return ret;

  int recordSize = attrCount * ATTR_SIZE;
  unsigned char *slotPointer = bufferPtr + HEADER_SIZE + slotCount + slotNum * recordSize;

  memcpy(rec, slotPointer, recordSize);
  return SUCCESS;
}

int RecBuffer::setRecord(union Attribute *rec, int slotNum) {
  unsigned char *buffer;
  int ret = RecBuffer::loadBlockAndGetBufferPtr(&buffer);

  if (ret != SUCCESS) return ret;

  struct HeadInfo head;
  BlockBuffer::getHeader(&head);

  int attrCount = head.numAttrs;
  int slotCount = head.numSlots;

  if (slotNum < 0 || slotNum >= slotCount) return E_OUTOFBOUND;

  int recordSize = attrCount * ATTR_SIZE;
  unsigned char *slotPointer = buffer + HEADER_SIZE + slotCount + slotNum * recordSize;

  memcpy(slotPointer, rec, recordSize);

  int ret_ = StaticBuffer::setDirtyBit(this->blockNum);

  if (ret_ != SUCCESS) std::cout << "Failed to set dirty bit" << std::endl;

  return SUCCESS;
}

int BlockBuffer::loadBlockAndGetBufferPtr(unsigned char **bufferPtr) {
  int bufferNum = StaticBuffer::getBufferNum(this->blockNum);

  if (bufferNum == E_BLOCKNOTINBUFFER) {
    bufferNum = StaticBuffer::getFreeBuffer(this->blockNum);

    if (blockNum == E_OUTOFBOUND) return E_OUTOFBOUND;

    Disk::readBlock(StaticBuffer::blocks[bufferNum], this->blockNum);
  }

  else {
    for (int bufferIndex = 0; bufferIndex < BUFFER_CAPACITY; bufferIndex++) {
      if (bufferIndex == bufferNum)
        StaticBuffer::metainfo[bufferIndex].timeStamp = 0;
      else
        StaticBuffer::metainfo[bufferIndex].timeStamp++;
    }
  }

  *bufferPtr = StaticBuffer::blocks[bufferNum];

  return SUCCESS;
}

int RecBuffer::getSlotMap(unsigned char *slotMap) {
  unsigned char *bufferPtr;
  int result = BlockBuffer::loadBlockAndGetBufferPtr(&bufferPtr);
  if (result != SUCCESS) {
    return result;
  }

  struct HeadInfo head;
  BlockBuffer::getHeader(&head);

  int numOfSlots = head.numSlots;
  memcpy(slotMap, bufferPtr + HEADER_SIZE, numOfSlots);

  return SUCCESS;
}

// ============================ Stage 7 ==========================
// update the slot map and set the dirty bit
int RecBuffer::setSlotMap(unsigned char *slotMap) {
  unsigned char *bufferPtr;
  int result = BlockBuffer::loadBlockAndGetBufferPtr(&bufferPtr);
  if (result != SUCCESS) {
    return result;
  }

  struct HeadInfo head;
  BlockBuffer::getHeader(&head);

  int numSlots = head.numSlots;
  memcpy(bufferPtr + HEADER_SIZE, slotMap, numSlots);

  int ret = StaticBuffer::setDirtyBit(this->blockNum);
  if (ret != SUCCESS) {
    return ret;
  }

  return SUCCESS;
}

// ======================== Stage 7 ======================
// updates the block type and sets the dirty bit
int BlockBuffer::setBlockType(int blockType) {
  unsigned char *bufferPtr;
  int ret = loadBlockAndGetBufferPtr(&bufferPtr);
  if (ret != SUCCESS) {
    return ret;
  }

  (*(int32_t *)bufferPtr) = blockType;

  StaticBuffer::blockAllocMap[this->blockNum] = blockType;

  int ret_ = StaticBuffer::setDirtyBit(this->blockNum);
  if (ret_ != SUCCESS) {
    return ret_;
  }

  return SUCCESS;
}

// ======================== Stage 7 ======================
// get a free block based on the block allocation map in StaticBuffer
int BlockBuffer::getFreeBlock(int blockType) {
  int blockNum;
  for (blockNum = 0; blockNum < DISK_BLOCKS; blockNum++) {
    if (StaticBuffer::blockAllocMap[blockNum] == UNUSED_BLK) {
      break;
    }
  }

  if (blockNum == DISK_BLOCKS) {
    return E_DISKFULL;
  }

  this->blockNum = blockNum;

  int bufferIndex = StaticBuffer::getFreeBuffer(blockNum);
  if (bufferIndex < 0 || bufferIndex >= BUFFER_CAPACITY) {
    std::cout << "ERROR: Buffer Full" << std::endl;
    return bufferIndex;
  }

  HeadInfo newHeader;
  newHeader.lblock = -1;
  newHeader.rblock = -1;
  newHeader.pblock = -1;
  newHeader.numAttrs = 0;
  newHeader.numEntries = 0;
  newHeader.numSlots = 0;

  BlockBuffer::setHeader(&newHeader);
  BlockBuffer::setBlockType(blockType);

  return blockNum;
}

// ============================== Stage 8 =============================
// set StaticBuffer::metainfo[blockNum].free = true and StaticBuffer::blockAllocMap[blockNum] = UNUSED_BLK
void BlockBuffer::releaseBlock() {
  if (blockNum == INVALID_BLOCKNUM || StaticBuffer::blockAllocMap[this->blockNum] == UNUSED_BLK) {
    return;
  }

  int bufferNum = StaticBuffer::getBufferNum(this->blockNum);
  if (bufferNum >= 0 and bufferNum < BUFFER_CAPACITY) {
    StaticBuffer::metainfo[bufferNum].free = true;
  }

  StaticBuffer::blockAllocMap[this->blockNum] = UNUSED_BLK;
  this->blockNum = INVALID_BLOCKNUM;
}