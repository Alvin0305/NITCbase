#include "StaticBuffer.h"
#include <iostream>

unsigned char StaticBuffer::blocks[BUFFER_CAPACITY][BLOCK_SIZE];
struct BufferMetaInfo StaticBuffer::metainfo[BUFFER_CAPACITY];

StaticBuffer::StaticBuffer() {
    for (int bufferIndex = 0; bufferIndex < BUFFER_CAPACITY; bufferIndex++) {
        metainfo[bufferIndex].free = true;
        metainfo[bufferIndex].dirty = false;
        metainfo[bufferIndex].timeStamp = -1;
        metainfo[bufferIndex].blockNum = -1;
    }
}

StaticBuffer::~StaticBuffer() {
    for (int bufferIndex = 0; bufferIndex < BUFFER_CAPACITY; bufferIndex++) {
        if (!metainfo[bufferIndex].free && metainfo[bufferIndex].dirty) {
            Disk::writeBlock(blocks[bufferIndex], metainfo[bufferIndex].blockNum);
        }
    }
}

int StaticBuffer::getFreeBuffer(int blockNum) {
    if (blockNum < 0 || blockNum >= DISK_BLOCKS) {
        return E_OUTOFBOUND;
    }

    for (int bufferIndex = 0; bufferIndex < BUFFER_CAPACITY; bufferIndex++) {
        if (!metainfo[bufferIndex].free) {
            metainfo[bufferIndex].timeStamp++;  
        }
    }

    int allocatedBuffer = -1;
    int bufferIndexWithHighestTS = 0;

    for (int bufferIndex = 0; bufferIndex < BUFFER_CAPACITY; bufferIndex++) {
        if (metainfo[bufferIndex].free) {
            allocatedBuffer = bufferIndex;
            break;
        }

        if (metainfo[bufferIndexWithHighestTS].timeStamp < metainfo[bufferIndex].timeStamp) {
            bufferIndexWithHighestTS = bufferIndex;
        }
    }

    if (allocatedBuffer == -1) {
        if (metainfo[bufferIndexWithHighestTS].dirty) {
            Disk::writeBlock(blocks[bufferIndexWithHighestTS], metainfo[bufferIndexWithHighestTS].blockNum);
        }
        allocatedBuffer = bufferIndexWithHighestTS;
    }

    metainfo[allocatedBuffer].free = false;
    metainfo[allocatedBuffer].dirty = false;
    metainfo[allocatedBuffer].blockNum = blockNum;
    metainfo[allocatedBuffer].timeStamp = -1;

    return allocatedBuffer;
}

int StaticBuffer::getBufferNum(int blockNum) {

    if (blockNum < 0 || blockNum >= DISK_BLOCKS) {
        return E_OUTOFBOUND;
    }

    for (int bufferIndex = 0; bufferIndex < BUFFER_CAPACITY; bufferIndex++) {
        if (metainfo[bufferIndex].blockNum == blockNum) {
            return bufferIndex;
        }
    }

    return E_BLOCKNOTINBUFFER;
}

int StaticBuffer::setDirtyBit(int blockNum) {
    int bufferNum = StaticBuffer::getBufferNum(blockNum);

    if (bufferNum == E_BLOCKNOTINBUFFER) {
        return E_BLOCKNOTINBUFFER;
    }

    if (blockNum < 0 || blockNum >= BUFFER_CAPACITY) {
        return E_OUTOFBOUND;
    }

    metainfo[bufferNum].dirty = true;
    return SUCCESS;
}