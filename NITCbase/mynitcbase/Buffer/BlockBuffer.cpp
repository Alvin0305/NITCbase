#include "BlockBuffer.h"

#include <cstdlib>
#include <cstring>
#include <iostream>

int compareAttrs(Attribute attr1, Attribute attr2, int attrType){
    int diff;
    if (attrType == STRING) {
        diff = strcmp(attr1.sVal, attr2.sVal);
    } else {
        diff = attr1.nVal - attr2.nVal;
    }

    if (diff < 0) return -1;
    if (diff > 0) return 1;
    return 0;
}

BlockBuffer::BlockBuffer(int blockNum) {
    this->blockNum = blockNum;
}

RecBuffer::RecBuffer(int blockNum) : BlockBuffer::BlockBuffer(blockNum) {}

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

    // =============================
    //          Stage 2
    // =============================

    
    // unsigned char buffer[BLOCK_SIZE];
    // Disk::readBlock(buffer, this->blockNum);

    // memcpy(&head->blockType, buffer + 0, 4);
    // memcpy(&head->pblock, buffer + 4, 4);
    // memcpy(&head->lblock, buffer + 8, 4);
    // memcpy(&head->rblock, buffer + 12, 4);
    // memcpy(&head->numEntries, buffer + 16, 4);
    // memcpy(&head->numAttrs, buffer + 20, 4);
    // memcpy(&head->numSlots, buffer + 24, 4);

    // memcpy(&head->numSlots, buffer + 24, 4);
    // memcpy(&head->numEntries, buffer + 16, 4);
    // memcpy(&head->numAttrs, buffer + 20, 4);
    // memcpy(&head->rblock, buffer + 12, 4);
    // memcpy(&head->lblock, buffer + 8, 4);

    return SUCCESS;
    
}

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

    //-----------------------------
    //          Stage 2
    // ----------------------------

    // struct HeadInfo head;
    // BlockBuffer::getHeader(&head);

    // int attrCount = head.numAttrs;
    // int slotCount = head.numSlots;

    // if (slotNum < 0 || slotNum >= slotCount) {
    //     return E_OUTOFBOUND;
    // }

    // unsigned char buffer[BLOCK_SIZE];
    // Disk::readBlock(buffer, this->blockNum);

    // int recordSize = attrCount * ATTR_SIZE;
    // unsigned char *slotPointer = buffer + HEADER_SIZE + slotCount + slotNum * recordSize;

    // memcpy(rec, slotPointer, recordSize);
    // return SUCCESS;
    
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
    // Disk::writeBlock(buffer, this->blockNum);

    int ret_ = StaticBuffer::setDirtyBit(this->blockNum);

    if (ret_ != SUCCESS) {
        std::cout << "Failed to set dirty bit" << std::endl;
    }

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
    } else {
        for (int bufferIndex = 0; bufferIndex < BUFFER_CAPACITY; bufferIndex++) {
            if (bufferIndex == bufferNum) {
                StaticBuffer::metainfo[bufferIndex].timeStamp = 0;
            } else {
                StaticBuffer::metainfo[bufferIndex].timeStamp++;
            }
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