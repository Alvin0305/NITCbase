## Disk
![[Pasted image 20250727184152.png]]

Total 8192 blocks
Each of size 2048 B
0 -> 3: Block Allocation Map (BMAP)
4: Block for Relation Catalog (REC)
5: First Block of Attribute Catalog (REC)

```C++
class Disk {
public:
	Disk();
	~Disk();
	static int readBlock(unsigned char *buffer, int blockNum);
	static int writeBlock(unsigned char *buffer, int blockNum);
}
```
return value of readBlock and writeBlock is either SUCCESS or E_OUTOFBOUND

### **TYPES OF BLOCKS**

- Record Block
- Internal Index Block
- Leaf Index Block

***RECORD BLOCK***

![[Pasted image 20250727185020.png]]

**There are only two datatypes: String and Number (STR, NUM) each taking size of 16 B.**
Consider there are K attributes and L number of records can be stored in a block (Number of slots).
So each attribute takes 16K bytes size

- First 4 B -> to identify the type of the block -> REC / IND_INTERNAL / IND_LEAF.
- Next 4 B -> pointer to parent
- Next 4 B -> pointer to left child
- Next 4 B -> pointer to right child
- Next 4 B -> stores the number of records currently in the block
- Next 4 B -> stores the number of attributes in the block
- Next 4 B -> stores the total number of records that can be stored in the block (Number of slots)
- Next 4 B -> Reserved for future purpose
- Next L B -> SlotMap for marking whether a slot is empty or not.
- Rest -> Used to store the records. Remaining is left unused

32 + L + 16 * K * L <= 2048 bytes
Number of slots = L = floor(2016 / (16 x K + 1))

***INTERNAL INDEX BLOCK***

![[Pasted image 20250727191116.png]]

Contains maximum of 100 keys
First 4 B -> to identify the type of Block -> IND_INTERNAL
Next 4 B -> pointer to parent block (Relevant) -> needed for balancing, insertion, splitting B+ tree
Next 4 B -> pointer to left block (Not relevant) 
Next 4 B -> pointer to right block (Not relevant)
Next 4 B -> number of entries (Relevant) -> needed for understanding how many keys are valid in the block and for iteration
Next 4 B -> number of attributes (Not Relevant)
Next 4 B -> number of slots (Not Relevant)
Next 4 B -> Reserved for future purpose
Rest -> for storing keys (pointer to child and the value on which the indexing is done)
Last 12 B -> Unused

***LEAF INDEX NODE***

![[Pasted image 20250727192514.png]]

Contains at max 63 entries
First 4 B -> to identify that the block is IND_LEAF
Next 4 B -> parent block number
Next 4 B -> left block number
Next 4 B -> Number of entries
Next 4 B -> Number of attributes
Next 4 B -> Number of slots
Next 4 B -> Reserved for future purpose
Rest -> each row of size 32 B containing the attribute value (value on which indexing is done), block number, slot number. Remaining is left unused.

## Relational Catalog

![[Pasted image 20250727200647.png]]

Each record is of the form
- Relation Name
- Number of Attributes
- Number of Records
- First Block
- Last Block
- Number of Slots
Total we can store 20 tables. But 1 out of that is used for Relation Catalog Table and 1 for Attribute Catalog Table. Therefore, we can store at max 18 tables.

## Attribute Catalog

![[Pasted image 20250727201354.png]]
Each record in a Attribute Catalog record contains
- Relation Name
- Attribute Name
- Attribute Datatype
- Primary Flag
- Root Block Number
- Offset

# ***Stage 3***
## Disk Buffer
- Implemented in StaticBuffer class
- unsigned char blocks\[BUFFER_CAPACITY]\[BLOCK_SIZE]
- BUFFER CAPACITY -> 32
- Every time you need a block, check if the block is in the static buffer. If yes take it. If no, read the block from the disk into a free block in static buffer and use it.

```C++
// BlockBuffer.cpp

int BlockBuffer::getHeader(struct HeadInfo *head) {
	unsigned char *bufferPtr;
	int ret = loadBlockAndGetBufferPtr(&bufferPtr);
	if (ret != SUCCESS) {
		return ret;
	}

	memcpy(&head->blockType, bufferPtr + 0, 4);
	memcpy(&head->pblock, bufferPtr + 4, 4);
	memcpy(&head->lblock, bufferPtr + 8, 4);
	memcpy(&head->rblock, bufferPtr + 12, 4);
	memcpy(&head->numEntries, bufferPtr + 16, 4);
	memcpy(&head->numAttrs, bufferPtr + 20, 4);
	memcpy(&head->numSlots, bufferPtr + 24, 4);

	return SUCCESS;
}

int BlockBuffer::getRecord(union Attribute *rec, int slotNum) {
	unsigned char *bufferPtr;
	int ret = loadBlockAndGetBufferPtr(&bufferPtr);
	if (ret != SUCCESS) {
		return ret;
	}

	struct HeadInfo head;
	BlockBuffer::getHeader(&head);
	int slotMapSize = head->numSlots;
	int attrCount = head->numAttrs;

	int recordSize = ATTR_SIZE * attrCount;
	int recordPointer = HEAD_SIZE + slotMapSize + slotNum * recordSize;

	memcpy(rec, bufferPtr + recordPointer, recordSize);
	return SUCCESS:
}

int BlockBuffer::loadBlockAndGetBufferPtr(unsigned char **bufferPtr) {
	int bufferNum = StaticBuffer::getBlockNum(this->blockNum);
	if (bufferNum == E_BLOCKNOTINBUFFER) {
		bufferNum = StaticBuffer::getFreeBuffer(this->blockNum);

		if (bufferNum == E_OUTOFBOUND) {
			return E_OUTOFBOUND;	
		}

		Disk::readBlock(StaticBuffer::blocks[blockNum], this->blockNum);
	}

	*bufferPtr = StaticBuffer::blocks[blockNum];

	return SUCCESS;
}
```

```C++
// StaticBuffer.cpp

unsigned char StaticBuffer::blocks[BUFFER_CAPACITY][BLOCK_SIZE];
struct BufferMetaInfo StaticBuffer::metainfo[BUFFER_CAPACITY];

StaticBuffer::StaticBuffer() {
	for (int i = 0; i < BUFFER_CAPACITY; i++) {
		StaticBuffer::metainfo[i].free = true;
	}
}

StaticBuffer::~StaticBuffer() {
	// to be implemented later
}

int StaticBuffer::getFreeBuffer(int blockNum) {
	if (blockNum < 0 || blockNum >= DISK_BLOCKS) {
		return E_OUTOFBOUND;
	}

	int allocatedBuffer;

	for (int i = 0; i < BUFFER_CAPACITY; i++) {
		if (StaticBuffer::blocks[i].free) {
			allocatedBuffer = i;
			break;
		}
	}

	metainfo[allocatedBuffer].free = false;
	metainfo[allocatedBuffer].blockNum = blockNum;

	return allocatedBuffer;
}

int StaticBuffer::getBufferNum(int blockNum) {
	if (blockNum < 0 || blockNum >= DISK_BLOCKS) {
		return E_OUTOFBOUND;
	}

	for (int i = 0; i < BUFFER_CAPACITY; i++) {
		if (StaticBuffer::blocks[i].blockNum == blockNum) {
			return i;
		}
	}

	return E_BLOCKNOTINBUFFER;
}
```

## Cache Layer

- Relation Catalog and Attribute Catalogs are frequently used, so keeping them in Cache would make the system faster.
- At max we can open MAX_OPEN = 12 relations at a time
relId -> a common index which can be used to identify any relation.

- There are 3 data structures used in this
- When a table is opened
	- its relational catalog data will be stored in RelCacheTable
	- its attribute catalog data will be stored in AttrCacheTable
	- metadata like free status and relName will be stored in OpenRelTable
- For every relation opened, it will be stored in all the three tables and all of them have the same index for the same table. This index is called RelId.
- RelCacheTable
	- Contains an array of RelCacheEntry of size MAX_OPEN
		- Each RelCacheEntry contains a RelCatEntry which is the record from corresponding row in Relation Catalog and some metadata.
- AttrCacheTable
	- Contains an array of AttrCacheEntry of size MAX_OPEN
		- Each AttrCacheEntry contains an AttrCatEntry which is the record from corresponding row in Attribute Catalog and some metadata and the next pointer to the next attribute of the same relation.
- OpenRelTable
	- Contains an array of OpenRelTableMetaInfo
		- Each OpenRelTableMetaInfo contains free status and relation name

```C++
// structures used for Cache layer 

typedef struct RecId {
	int blockNum;
	int slotNum;
} RecId;

// Same as the entry in Relation Catalog
typedef struct RelCatEntry {
	unsigned char relName[ATTR_SIZE];
	int numAttrs;
	int numRecords;
	int firstBlk;
	int lastBlk;
	int numSlotsPerBlk;
} RelCatEntry;

typedef struct RelCacheEntry {
	RelCatEntry relCatEntry;
	bool dirty;
	RecId recId;
	RecId searchIndex;
} RelCacheEntry;

// Same as the entry in Attribute Catalog
typedef struct AttrCatEntry {
	unsigned char relName[ATTR_SIZE];
	unsigned char attrName[ATTR_SIZE];
	int attrType;
	bool primaryFlag;
	int rootBlock;
	int offset;
} AttrCatEntry;

typedef struct AttrCacheEntry {
	AttrCatEntry attrCatEntry;
	bool dirty;
	RecId recId;
	RecId searchIndex;
	struct AttrCacheEntry *next;
} AttrCacheEntry;

typedef struct OpenRelTableMetaInfo {
	bool free;
	unsigned char relName[ATTR_SIZE];
} OpenRelTableMetaInfo;

```

```C++
// RelCacheTable.cpp

RelCacheEntry *relCacheTable::relCache[MAX_OPEN];

int RelCacheTable::getRelCatEntry(int relId, RelCatEntry *relCatBuffer) {
	if (relId < 0 || relId >= MAX_OPEN) {
		return E_OUTOFBOUND:
	}

	if (relCache[relId] == nullptr) {
		return E_RELNOTOPEN;
	}

	*relCatBuffer = relCache[relId]->relCatEntry;
	return SUCCESS;
}

void RelCacheTable::recordToRelCatEntry(union Attribute record[RELCAT_NO_ATTRS], RelCatEntry *relCatEntry) {
	strcpy(relCatEntry->relName, record[RELCAT_REL_NAME_INDEX].sVal);
	relCatEntry->numRec = (int) record[RELCAT_NO_RECORDS_INDEX].nVal;
	relCatEntry->numAttrs = (int) record[RELCAT_NO_ATTRIBUTES_INDEX].nVal;
	relCatEntry->firstBlk = (int) record[RELCAT_FIRST_BLOCK_INDEX].nVal;
	relCatEntry->lastBlk = (int) record[RELCAT_LAST_BLOCK_INDEX].nVal;
	relCatEntry->numSlotsPerBlk = (int) record[RELCAT_NO_SLOTS_PER_BLOCK].nVal;
}

```

```C++
// AttrCacheTable.cpp

AttrCacheEntry *AttrCacheTable::attrCache[MAX_OPEN];

int AttrCacheTable::getAttrCatEntry(int relId, int attrOffset, AttrCatEntry *attrCatBuffer) {
	if (relId < 0 || relId >= MAX_OPEN) {
		return E_OUTOFBOUND;	
	}

	if (attrCache[relId] == nullptr) {
		return E_RELNOTOPEN;
	}

	for (AttrCacheEntry *entry = attrCache[relId]; entry != nullptr; entry = entry->next) {
		if (entry->attrCatEntry.offset == attrOffset) {
			*attrCatBuffer = entry->attrCatEntry;
			return SUCCESS;
		}
	}

	return E_ATTRNOTEXIST;
}

void AttrCacheTable::recordToAttrCatEntry(union Attribute record[ATTRCAT_NO_ATTRS], AttrCatEntry *attrCatEntry) {
	strcpy(attrCatEntry->relName, record[ATTRCAT_REL_NAME_INDEX].sVal);
	strcpy(attrCatEntry->attrName, record[ATTRCAT_ATTR_NAME_INDEX].sVal);
	attrCatEntry->attrType = (int) record[ATTRCAT_ATTR_TYPE_INDEX].nVal;
	attrCatEntry->primaryFlag = (int) record[ATTRCAT_PRIMARY_FLAG_INDEX].nVal;
	attrCatEntry->rootBlock = (int) record[ATTRCAT_ROOT_BLOCK_INDEX].nVal;
	attrCatEntry->offset = (int) record[ATTRCAT_OFFSET_INDEX].nVal;
}
```

# ***Stage 4***
## Linear Search on relations

In this stage we use the select method in Algebra.cpp to call the linearSearch method in BlockAccess.cpp to select all the records satisfying the given condition. linearSearch method is responsible to return the block number and slot number as RecId of the record which satisfies the condition one at a time. select method calls linear search until there is no more record satisfying the condition. To implement this without infinite loop, we keep a searchIndex which records the last search hit in a relation. Before starting search, we set the searchIndex for that relation as {-1, -1}. On every successful search, we update the searchIndex accordingly. So for every search call, it will check if the searchIndex is {-1, -1}: if yes, it means, it is the first iteration of the search so it takes the first block of the relation and search its first slot and set the searchIndex accordingly. On further searches, if searchIndex {block, slot}, then the search will be done on {block, slot + 1} till slot <= slotNum. When slot > slotNum, block is updated to block.right and slot is updated to 0. If the linearSearch returns {-1, -1}, this means, there is no further matched records and we stop the select method. 

## ***Stage 5***

## Opening Relations

To use any DML command, we need the relation to be opened. Since at max 12 relations can be opened, out of which 2 are Relation Catalog and Attribute Catalog, we can only have 10 opened relations at a time. 

## ***Stage 7***

### Inserting into a Relation

```C++
BlockBuffer(char blockType);
BlockBuffer(int blockNum);

RecBuffer();
RecBuffer(int blockNum);
```

Till now, we have been using the 2nd constructor for both BlockBuffer and RecBuffer. Now we need to implement the 1st constructor
First constructor is needed when the below 2 conditions are met:
- A record is added into a relation or when a relation is created resulting in a insertion in the relation catalog and attribute catalog
- None of the already allocated blocks have a free slot to store the inserted record -> a new block should be allocated for insertion

## ***Stage 10***

### B Plus Tree
- Order of a tree is the maximum number of children that a node in the tree can have
- For a tree of order m, each node can have at most m - 1 entries.
- Entries in each node are sorted
	- All entries in the left sub tree will have a key less than the parent's key value
	- All entries in the right sub tree will have a key greater than the parent's key value
- Each node in the tree is a list of pairs containing an attribute value and a pointer to the record that contains the particular record

#### B Trees
- Two kinds of nodes
	- Leaf node
	- Internal node
- Every internal node has at most m children
- A internal node with k children have k - 1 entries
- Each entry in an internal node has
	- left child pointer
	- key value
	- record pointer
	- right child pointer
- the key value will be greater that those of all the elements in it's left child and is lesser than those of all entries in its right child.
- All leaf nodes are at the same level
- When a node is full -> It splits them into two such that each is at least have full
- The root has at least 2 children if it is not a leaf node

Disadvantage
- Since the record pointer is stored within the index blocks, this reduce the number of entries that can be stored in a index block which results in increasing the height of the entire tree

#### B+ Tree
- Two kinds of nodes
	- Leaf Node
		- key value
		- record pointer
	- Internal node
		- left child pointer
		- key value
		- right child pointer
- record pointers are stored only in the leaf nodes not in the internal index nodes
- When a leaf node becomes full and is split -> the middle entry is stored in both the leaf and parent node (Different from B Tree)
- When a internal node becomes full and is split -> the middle entry is stored only in the parent node (Same as B Tree)
- So, all the leaves appear in the same level and all the keys appear in the leaves
- The leaf nodes maintain a sorted linked list order. This helps in faster range queries. Only the traversal of the leaves are needed, but in B trees a traversal through all the layers is needed

##### **Searching in B+ Tree**
- Start from root
- Compare "k" with each keys in the root node until we get an entry with key > k
	- If we get such a entry, we move to the left child
	- else we go to the right most child of the node
- Keep doing this until we reach a leaf node
	- Do a linear search on the leaf node to find our key and the corresponding value

##### **Insertion in B+ Tree**
- Traverse down the tree using the previous search algorithm to find the leaf node for insertion
- If the leaf is not full, add the entry
- else, split the leaf
	- allocate a new leaf and add half of the elements of the current leaf node to the new leaf node
	- insert a copy of the current leaf node's largest key  (i.e., middle before split) into the parent
	- If parent is full, split it too. Add the middle key to its parent node
	- Repeat until a parent is found that need not be split
		- If the root has to be split, then a new node is created and the middle key is added to it. This new node becomes the new root node

### Indexing

- Internal Index block contains 100 entries -> 101 child pointers
- Each entry in leaf node is a 3 tuple -> 16B attribute value, 4B block number, 4B slot number and 8B unused space => 32B (There is total 63 entries in a leaf block)
- 

### Internal Index Block

![[Pasted image 20250825212359.png]]
### Leaf Index Block Structure

![[Pasted image 20250825212410.png]]


