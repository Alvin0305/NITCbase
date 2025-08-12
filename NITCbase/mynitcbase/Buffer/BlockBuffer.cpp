#include "BlockBuffer.h"

#include <cstdlib>
#include <cstring>
#include <iostream>

int compareAttrs(Attribute attr1, Attribute attr2, int attrType){
    double diff;
    if (attrType == STRING) {
        return strcmp(attr1.sVal, attr2.sVal);
    } else {
        if (attr1.nVal < attr2.nVal) {
            return -1;
        } else if (attr1.nVal > attr2.nVal) {
            return 1;
        } else {
            return 0;
        }
    }
}

BlockBuffer::BlockBuffer(int blockNum) {
    this->blockNum = blockNum;
}

// ======================== Stage 7 ======================
// BlockBuffer::BlockBuffer(char blockTypeChar) {
//     unsigned char *bufferPtr;
//     int blockType = blockTypeChar == 'R' ? REC :
//                     blockTypeChar == 'I' ? IND_INTERNAL :
//                     blockTypeChar == 'L' ? IND_LEAF: UNUSED_BLK;

//     int freeBlockNum = getFreeBlock(blockType);
    
//     if (freeBlockNum < 0 || blockNum >= DISK_BLOCKS) {
//         std::cout << "Failed to get a free block" << std::endl;
//         this->blockNum = freeBlockNum;
//         return;
//     } else {
//         this->blockNum = freeBlockNum;
//     }
// }

RecBuffer::RecBuffer(int blockNum) : BlockBuffer::BlockBuffer(blockNum) {}

// ======================== Stage 7 ======================
// RecBuffer::RecBuffer() : BlockBuffer::BlockBuffer('R') {}

int BlockBuffer::getHeader(struct HeadInfo *head) {

    // ============================= 
    //          Stage 3
    // =============================

    unsigned char *bufferPtr;
    int ret = BlockBuffer::loadBlockAndGetBufferPtr(&bufferPtr);
    if (ret != SUCCESS) {
        return ret;
    }

    memcpy(&head->numSlots, bufferPtr + 24, 4);
    memcpy(&head->numEntries, bufferPtr + 16, 4);
    memcpy(&head->numAttrs, bufferPtr + 20, 4);
    memcpy(&head->rblock, bufferPtr + 12, 4);
    memcpy(&head->lblock, bufferPtr + 8, 4);
    
    return SUCCESS;
}

// int BlockBuffer::setHeader(struct HeadInfo *head) {
//     unsigned char *bufferPtr;
//     int ret = loadBlockAndGetBufferPtr(&bufferPtr);

//     if (ret != SUCCESS) {
//         return ret;
//     }

//     struct HeadInfo *bufferHeader = (struct HeadInfo *) bufferPtr;

//     bufferHeader->numSlots = head->numSlots;
//     bufferHeader->blockType = head->blockType;
//     bufferHeader->lblock = head->lblock;
//     bufferHeader->numAttrs = head->numAttrs;
//     bufferHeader->numEntries = head->numEntries;
//     bufferHeader->pblock = head->pblock;
//     bufferHeader->rblock = head->rblock;

//     int ret_ = StaticBuffer::setDirtyBit(this->blockNum);
//     if (ret_ != SUCCESS) {
//         return ret_;
//     }

//     return SUCCESS;
// }

int RecBuffer::getRecord(union Attribute *rec, int slotNum) {

    // =============================
    //          Stage 3
    // =============================

    struct HeadInfo head;
    BlockBuffer::getHeader(&head);

    int attrCount = head.numAttrs;
    int slotCount = head.numSlots;

    unsigned char *bufferPtr;
    int ret = BlockBuffer::loadBlockAndGetBufferPtr(&bufferPtr);
    if (ret != SUCCESS) {
        return ret;
    }

    int recordSize = attrCount * ATTR_SIZE;
    unsigned char *slotPointer = bufferPtr + HEADER_SIZE + slotCount + slotNum * recordSize;

    memcpy(rec, slotPointer, recordSize);
    return SUCCESS;
}

int RecBuffer::setRecord(union Attribute *rec, int slotNum) {
    unsigned char *buffer;
    int ret = RecBuffer::loadBlockAndGetBufferPtr(&buffer);

    if (ret != SUCCESS) {
        return ret;
    }

    struct HeadInfo head;
    BlockBuffer::getHeader(&head);

    int attrCount = head.numAttrs;
    int slotCount = head.numSlots;

    if (slotNum < 0 || slotNum >= slotCount) {
        return E_OUTOFBOUND;
    }

    int recordSize = attrCount * ATTR_SIZE;
    unsigned char *slotPointer = buffer + HEADER_SIZE + slotCount + slotNum * recordSize;

    memcpy(slotPointer, rec, recordSize);
    Disk::writeBlock(buffer, this->blockNum);       // uncomment this for further stages

    // ==================== Not stage 3 ============= 
    // int ret_ = StaticBuffer::setDirtyBit(this->blockNum);

    // if (ret_ != SUCCESS) {
    //     std::cout << "Failed to set dirty bit" << std::endl;
    // }

    return SUCCESS;
}

//-----------------------------
//          Stage 3
// ----------------------------

int BlockBuffer::loadBlockAndGetBufferPtr(unsigned char **bufferPtr) {
    int bufferNum = StaticBuffer::getBufferNum(this->blockNum);

    if (bufferNum == E_BLOCKNOTINBUFFER) {
        bufferNum = StaticBuffer::getFreeBuffer(this->blockNum);

        if (blockNum == E_OUTOFBOUND) {
            return E_OUTOFBOUND;
        }

        Disk::readBlock(StaticBuffer::blocks[bufferNum], this->blockNum);
    } 
    
    // ======================== Stage 6 ======================
    // else {
    //     for (int bufferIndex = 0; bufferIndex < BUFFER_CAPACITY; bufferIndex++) {
    //         if (bufferIndex == bufferNum) {
    //             StaticBuffer::metainfo[bufferIndex].timeStamp = 0;
    //         } else {
    //             StaticBuffer::metainfo[bufferIndex].timeStamp++;
    //         }
    //     }
    // }

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

// ======================== Not Stage 4 ======================

// ======================== Stage 7 ======================
// int BlockBuffer::setBlockType(int blockType) {
//     unsigned char *bufferPtr;
//     int ret = loadBlockAndGetBufferPtr(&bufferPtr);
//     if (ret != SUCCESS) {
//         return ret;
//     }

//     (*(int32_t *) bufferPtr) = blockType;
    
//     StaticBuffer::blockAllocMap[this->blockNum] = blockType;

//     int ret_ = StaticBuffer::setDirtyBit(this->blockNum);
//     if (ret_ != SUCCESS) {
//         return ret_;
//     }

//     return SUCCESS;
// }

// ======================== Stage 7 ======================
// int BlockBuffer::getFreeBlock(int blockType) {
    // for (char blockAlloc = StaticBuffer::blockAllocMap[0]; blockAlloc)
// }