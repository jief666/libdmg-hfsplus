#include <stdlib.h>
#include <string.h>
#include "../includes/hfs/hfsplus.h"

static inline void flipAttrData(HFSPlusAttrData* data) {
  FLIPENDIAN(data->recordType);
  FLIPENDIAN(data->size);
}

static inline void flipAttrForkData(HFSPlusAttrForkData* data) {
  FLIPENDIAN(data->recordType);
  flipForkData(&data->theFork);
}

static inline void flipAttrExtents(HFSPlusAttrExtents* data) {
  FLIPENDIAN(data->recordType);
  flipExtentRecord(&data->extents);
}

static int attrCompare(BTKey* vLeft, BTKey* vRight) {
	HFSPlusAttrKey* left;
	HFSPlusAttrKey* right;
	uint16_t i;

	uint16_t cLeft;
	uint16_t cRight;

	left = (HFSPlusAttrKey*) vLeft;
	right =(HFSPlusAttrKey*) vRight;

	if(left->fileID < right->fileID) {
		return -1;
	} else if(left->fileID > right->fileID) {
		return 1;
	} else {
		for(i = 0; i < left->name.length; i++) {
			if(i >= right->name.length) {
				return 1;
			} else {
				cLeft = left->name.unicode[i];
				cRight = right->name.unicode[i];

				if(cLeft < cRight)
					return -1;
				else if(cLeft > cRight)
					return 1;
			}
		}

		if(i < right->name.length) {
			return -1;
		} else {
			/* do a safety check on key length. Otherwise, bad things may happen later on when we try to add or remove with this key */
			/*if(left->keyLength == right->keyLength) {
			  return 0;
			  } else if(left->keyLength < right->keyLength) {
			  return -1;
			  } else {
			  return 1;
			  }*/
			return 0;
		}
	}
}

#define UNICODE_START (sizeof(uint16_t) + sizeof(uint16_t) + sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint16_t))

static BTKey* attrKeyRead(off_t offset, io_func* io) {
	int i;
	HFSPlusAttrKey* key;

	key = (HFSPlusAttrKey*) malloc(sizeof(HFSPlusAttrKey));

	if(!READ(io, offset, UNICODE_START, key)) {
        free(key);
		return NULL;
    }
    
	FLIPENDIAN(key->keyLength);
	FLIPENDIAN(key->fileID);
	FLIPENDIAN(key->startBlock);
	FLIPENDIAN(key->name.length);
  
    ASSERT(key->name.length <= 255, "key->nodeName.length <= 255");
    ASSERT(key->keyLength >= (UNICODE_START - sizeof(uint16_t) + (key->name.length * sizeof(uint16_t))), "key->keyLength >= (UNICODE_START - sizeof(uint16_t) + (key->name.length * sizeof(uint16_t)))");

	if(!READ(io, offset + UNICODE_START, key->name.length * sizeof(uint16_t), ((unsigned char *)key) + UNICODE_START)) {
        free(key);
		return NULL;
    }
	for(i = 0; i < key->name.length; i++) {
		FLIPENDIAN(key->name.unicode[i]);
	}

	return (BTKey*)key;
}

static int attrKeyWrite(off_t offset, BTKey* toWrite, io_func* io) {
	HFSPlusAttrKey* key;
	uint16_t keyLength;
	uint16_t nodeNameLength;
	int i;

	keyLength = toWrite->keyLength;
	key = (HFSPlusAttrKey*) malloc(keyLength + sizeof(uint16_t));
	memcpy(key, toWrite, keyLength);

	nodeNameLength = key->name.length;

	FLIPENDIAN(key->keyLength);
	FLIPENDIAN(key->fileID);
	FLIPENDIAN(key->startBlock);
	FLIPENDIAN(key->name.length);

	for(i = 0; i < nodeNameLength; i++) {
		FLIPENDIAN(key->name.unicode[i]);
	}

	if(!WRITE(io, offset, keyLength, key))
		return FALSE;

	free(key);

	return TRUE;
}

static void attrKeyPrint(BTKey* toPrint) {
	HFSPlusAttrKey* key;

	key = (HFSPlusAttrKey*)toPrint;

	printf("attribute%d:%d:", key->fileID, key->startBlock);
	printUnicode(&key->name);
}

static void* attrDataRead(off_t offset, io_func* io) {
	HFSPlusAttrRecord* record;

	record = (HFSPlusAttrRecord*) malloc(sizeof(HFSPlusAttrRecord));

	if(!READ(io, offset, sizeof(uint32_t), record)) {
        free(record);
		return NULL;
    }

	FLIPENDIAN(record->recordType);
	switch(record->recordType)
	{
		case kHFSPlusAttrInlineData:
			if(!READ(io, offset, sizeof(HFSPlusAttrData), record)) {
                free(record);
				return NULL;
            }

			flipAttrData((HFSPlusAttrData*) record);

			record = realloc(record, sizeof(HFSPlusAttrData) + ((HFSPlusAttrData*) record)->size);
			if(!READ(io, offset + sizeof(HFSPlusAttrData), ((HFSPlusAttrData*) record)->size, ((HFSPlusAttrData*) record)->data)) {
                free(record);
				return NULL;
            }
			break;

		case kHFSPlusAttrForkData:
			if(!READ(io, offset, sizeof(HFSPlusAttrForkData), record)) {
                free(record);
				return NULL;
            }
			flipAttrForkData((HFSPlusAttrForkData*) record);
			break;

		case kHFSPlusAttrExtents:
			if(!READ(io, offset, sizeof(HFSPlusAttrExtents), record)) {
                free(record);
				return NULL;
            }
			flipAttrExtents((HFSPlusAttrExtents*) record);
			break;
	}
	return (BTKey*)record;
}

static int updateAttributes(Volume* volume, HFSPlusAttrKey* skey, HFSPlusAttrRecord* srecord) {
	HFSPlusAttrKey key;
//    HFSPlusAttrRecord* record;
	int ret = FALSE;
    int len;

	memcpy(&key, skey, skey->keyLength);

	switch(srecord->recordType) {
		case kHFSPlusAttrInlineData: {
			len = srecord->attrData.size + sizeof(HFSPlusAttrData);
			HFSPlusAttrRecord* record = (HFSPlusAttrRecord*) malloc(len);
      		memcpy(record, srecord, len);
			flipAttrData((HFSPlusAttrData*) record);
			removeFromBTree(volume->attrTree, (BTKey*)(&key));
      		ret = addToBTree(volume->attrTree, (BTKey*)(&key), len, (unsigned char *)record);
			free(record);
			break;
        }
		case kHFSPlusAttrForkData: {
			HFSPlusAttrForkData* record = (HFSPlusAttrForkData*) malloc(sizeof(HFSPlusAttrForkData));
            memcpy(record, srecord, sizeof(HFSPlusAttrForkData));
			flipAttrForkData((HFSPlusAttrForkData*) record);
			removeFromBTree(volume->attrTree, (BTKey*)(&key));
      		ret = addToBTree(volume->attrTree, (BTKey*)(&key), sizeof(HFSPlusAttrForkData), (unsigned char *)record);
			free(record);
			break;
        }
		case kHFSPlusAttrExtents: {
			HFSPlusAttrExtents* record = (HFSPlusAttrExtents*) malloc(sizeof(HFSPlusAttrExtents));
      		memcpy(record, srecord, sizeof(HFSPlusAttrExtents));
			flipAttrExtents((HFSPlusAttrExtents*) record);
			removeFromBTree(volume->attrTree, (BTKey*)(&key));
      		ret = addToBTree(volume->attrTree, (BTKey*)(&key), sizeof(HFSPlusAttrExtents), (unsigned char *)record);
			free(record);
			break;
        }
	}

	return ret;
}

size_t getAttribute(Volume* volume, uint32_t fileID, const char* name, uint8_t** data, int panicIfNotFound) {
	HFSPlusAttrKey key;
	HFSPlusAttrRecord* record;
	size_t size;
	int exact;

	if(!volume->attrTree)
		return FALSE;

	memset(&key, 0 , sizeof(HFSPlusAttrKey));
	key.fileID = fileID;
	key.startBlock = 0;
	ASCIIToUnicode(name, &key.name);
	key.keyLength = sizeof(HFSPlusAttrKey) - sizeof(HFSUniStr255) + sizeof(key.name.length) + (sizeof(uint16_t) * key.name.length);

	*data = NULL;

	record = (HFSPlusAttrRecord*) search(volume->attrTree, (BTKey*)(&key), &exact, NULL, NULL, panicIfNotFound);
    if ( !record ) return 0;

	if(exact == FALSE) {
		if(record)
			free(record);

		return 0;
	}

	switch(record->recordType)
	{
		case kHFSPlusAttrInlineData:
			size = record->attrData.size;
			*data = (uint8_t*) malloc(size);
			memcpy(*data, record->attrData.data, size);
			free(record);
			return size;
		default:
			fprintf(stderr, "unsupported attribute node format\n");
			return 0;
	}
}

int setAttribute(Volume* volume, uint32_t fileID, const char* name, uint8_t* data, size_t size) {
	HFSPlusAttrKey key;
	HFSPlusAttrData* record;
	int ret;

	if(!volume->attrTree)
		return FALSE;

	memset(&key, 0 , sizeof(HFSPlusAttrKey));
	key.fileID = fileID;
	key.startBlock = 0;
	ASCIIToUnicode(name, &key.name);
	key.keyLength = sizeof(HFSPlusAttrKey) - sizeof(HFSUniStr255) + sizeof(key.name.length) + (sizeof(uint16_t) * key.name.length);

	record = (HFSPlusAttrData*) malloc(sizeof(HFSPlusAttrData) + size);
	memset(record, 0, sizeof(HFSPlusAttrData));

	record->recordType = kHFSPlusAttrInlineData;
	record->size = size;
	memcpy(record->data, data, size);

	ret = updateAttributes(volume, &key, (HFSPlusAttrRecord*) record);

	free(record);
	return ret;
}

int unsetAttribute(Volume* volume, uint32_t fileID, const char* name) {
	HFSPlusAttrKey key;

	if(!volume->attrTree)
		return FALSE;

	memset(&key, 0 , sizeof(HFSPlusAttrKey));
	key.fileID = fileID;
	key.startBlock = 0;
	ASCIIToUnicode(name, &key.name);
	key.keyLength = sizeof(HFSPlusAttrKey) - sizeof(HFSUniStr255) + sizeof(key.name.length) + (sizeof(uint16_t) * key.name.length);
	return removeFromBTree(volume->attrTree, (BTKey*)(&key));
}
    
#include <unicode/utypes.h>
#include <unicode/ucnv.h>
extern UConverter* g_utf8;

void addUtf8ItemInXAttrNameList(XAttrList** list, const char* name, size_t len)
{
    XAttrList* item;
    if ( !*list ) {
//printf("malloc *list = malloc(sizeof(XAttrList))\n");
        *list = malloc(sizeof(XAttrList));
        item = *list;
    }else{
        item = *list;
        while ( item->next ) item = item->next;
//printf("malloc item->next = malloc(sizeof(XAttrList));\n");
        item->next = malloc(sizeof(XAttrList));
        item = item->next;
    }
//printf("malloc item->name = (char*) malloc(len + 1);\n");
    item->name = (char*) malloc(len + 1);
    memcpy(item->name, name, len);
    item->name[len] = 0;
    item->next = NULL;
}

size_t UTF16_to_UTF8(HFSUniStr255* utf16, char* utf8, size_t utf8len)
{
    UErrorCode unicode_error = U_ZERO_ERROR;
    int32_t rv5 = ucnv_fromUChars(g_utf8, utf8, (int32_t)utf8len, (UChar*)utf16->unicode, utf16->length, &unicode_error); // TODO error iconv ????
    return rv5;
}

#define ff (*ffPtr)

void getVisibleFileInfo(HFSPlusCatalogFileOrFolderRecord* ffPtr, uint8_t buf[32])
{
    #define newUserInfo (*((FileInfo*)buf))
    #define newFolderInfo (*((FolderInfo*)buf))
    #define newFinderInfo (*((ExtendedFileInfo*)(buf+16)))
    #define newExtendedFolderInfo (*((ExtendedFolderInfo*)(buf+16)))
    if (ff.file.recordType == kHFSPlusFileRecord)
    {
        // Push finder only if there is non zero data in it, excepted non-exposed field.
        newUserInfo = ff.file.userInfo;
        if ( newUserInfo.fileType == kSymLinkFileType ) memset(&newUserInfo.fileType, 0, sizeof(newUserInfo.fileType));
        if ( newUserInfo.fileCreator == kSymLinkCreator ) memset(&newUserInfo.fileCreator, 0, sizeof(newUserInfo.fileCreator));
        newFinderInfo = ff.file.finderInfo;
        newFinderInfo.document_id = 0;
        newFinderInfo.date_added = 0;
        newFinderInfo.write_gen_counter = 0;
        flipFileInfo(&newUserInfo);
        flipExtendedFileInfo(&newFinderInfo);
    }else{
        // Folder don't hace ressource fork
        // Push finder only if there is non zero data in it, excepted non-exposed field.
        newFolderInfo = ff.folder.userInfo;
        newExtendedFolderInfo = ff.folder.finderInfo;
        newExtendedFolderInfo.document_id = 0;
        newExtendedFolderInfo.date_added = 0;
        newExtendedFolderInfo.write_gen_counter = 0;
        flipFolderInfo(&newFolderInfo);
        flipExtendedFolderInfo(&newExtendedFolderInfo);
    }
    #undef newUserInfo
    #undef newFinderInfo
}

void* searchNodeAttr(BTree* tree, uint32_t root, BTKey* searchKey, int *exact, uint32_t *nodeNumber, int *recordNumber, int panicIfNotFound);

size_t getExtendedAttribute(Volume* volume, uint32_t fileID, const char* name, uint8_t** data) {
    HFSPlusAttrKey key;
    HFSPlusAttrRecord* record;
    size_t size;
    int exact;

    if(!volume->attrTree)
        return FALSE;

    memset(&key, 0 , sizeof(HFSPlusAttrKey));
    key.fileID = fileID;
    key.startBlock = 0;
    ASCIIToUnicode(name, &key.name);
    key.keyLength = sizeof(HFSPlusAttrKey) - sizeof(HFSUniStr255) + sizeof(key.name.length) + (sizeof(uint16_t) * key.name.length);

    *data = NULL;

    record = (HFSPlusAttrRecord*) search(volume->attrTree, (BTKey*)(&key), &exact, NULL, NULL, 0);
//    record = (HFSPlusAttrRecord*) searchNodeAttr(volume->attrTree, volume->attrTree->headerRec->rootNode, (BTKey*)(&key), &exact, NULL, NULL, 1);

    if(exact == FALSE) {
        if(record)
            free(record);

        return 0;
    }

    switch(record->recordType)
    {
        case kHFSPlusAttrInlineData:
            size = record->attrData.size;
            *data = (uint8_t*) malloc(size);
            memcpy(*data, record->attrData.data, size);
            free(record);
            return size;
        default:
            fprintf(stderr, "unsupported attribute node format\n");
            return 0;
    }
}

//
//void* searchNodeAttr(BTree* tree, uint32_t root, BTKey* searchKey, int *exact, uint32_t *nodeNumber, int *recordNumber, int panicIfNotFound) {
//  BTNodeDescriptor* descriptor;
//  HFSPlusAttrKey* key;
//  off_t recordOffset;
//  off_t recordDataOffset = 0;
//  off_t lastRecordDataOffset = 0;
//
//  int res;
//  int i;
//
//  descriptor = readBTNodeDescriptor(root, tree); // read only the header;
////  uint16_t* firstRecordOffset = ((uint8_t*)descriptor) +tree->headerRec->nodeSize - sizeof(uint16_t);
//
//
//  if(descriptor == NULL)
//    return NULL;
//
//
//
//  if (descriptor->kind == kBTIndexNode )
//  {
////for(i = 0; i < descriptor->numRecords; i++)
////{
////    off_t o = getRecordOffset(i, root, tree);
////    HFSPlusAttrKey* k = READ_KEY(tree, o, tree->io);
////    off_t rdo = o + k->keyLength + sizeof(k->keyLength);
//////    FLIPENDIAN(k->fileID);
////    printf("node %d[%d] : offset %llx, fileID 0x%x\n", root, i, o - (off_t)root * tree->headerRec->nodeSize, k->fileID);
////}
//      for(i = 0; i < descriptor->numRecords; i++)
//      {
//        recordOffset = getRecordOffset(i, root, tree);
//        key = READ_KEY(tree, recordOffset, tree->io);
//        recordDataOffset = recordOffset + key->keyLength + sizeof(key->keyLength);
//
//        res = COMPARE(tree, key, searchKey);
//        free(key);
//        if ( i == 0 ) {
//            lastRecordDataOffset = recordDataOffset;
//        }
//        if (res < 0) {
//            lastRecordDataOffset = recordDataOffset;
//        }else{
//            break;
//        }
//      }
//      uint32_t* childIndex = READ_DATA(tree, lastRecordDataOffset, tree->io);
//      void* node = searchNodeAttr(tree, *childIndex, searchKey, exact, nodeNumber, recordNumber, panicIfNotFound);
//      return node;
//  }
//  else
//  if (descriptor->kind == kBTLeafNode ) {
////        recordOffset = getRecordOffset(i, root, tree);
////        key = READ_KEY(tree, recordOffset, tree->io);
////        recordDataOffset = recordOffset + key->keyLength + sizeof(key->keyLength);
////        uint8_t* rv = READ_DATA(tree, lastRecordDataOffset, tree->io);
//    *nodeNumber = root;
//    return descriptor;
//  }
//
//  return NULL;
//}
//
//XAttrList* getAllExtendedAttributesJief2x(HFSPlusCatalogFileOrFolderRecord* ffPtr, Volume* volume) {
//    BTree* tree;
//    HFSPlusAttrKey key;
//    HFSPlusAttrRecord* record;
//    uint32_t nodeNumber;
//    int recordNumber;
//    BTNodeDescriptor* descriptor;
//    HFSPlusAttrKey* currentKey;
//    off_t recordOffset;
//    XAttrList* list = NULL;
//    XAttrList* lastItem = NULL;
//    XAttrList* item = NULL;
//
//
//
//    uint8_t buf[32];
//    const char zero[32] = { 0 };
//    getVisibleFileInfo(&ff, buf);
//    if (  memcmp(buf, zero, 32) != 0 ) addUtf8ItemInXAttrNameList(&list, XATTR_FINDER_INFO, strlen(XATTR_FINDER_INFO));
//
//    // Push ressource fork only if there is one
//    if (ff.file.recordType == kHFSPlusFileRecord  &&  ff.file.resourceFork.logicalSize != 0  &&  !(ff.file.permissions.ownerFlags & UF_COMPRESSED) ) addUtf8ItemInXAttrNameList(&list, XATTR_RESOURCE_FORK, strlen(XATTR_RESOURCE_FORK));
//
//
//    if(!volume->attrTree)
//        return NULL;
//
//    memset(&key, 0 , sizeof(HFSPlusAttrKey));
//    key.fileID = ffPtr->file.fileID;
//    key.startBlock = 0;
//    key.name.length = 0;
//    key.keyLength = sizeof(HFSPlusAttrKey) - sizeof(HFSUniStr255) + sizeof(key.name.length) + (sizeof(uint16_t) * key.name.length);
//
//    tree = volume->attrTree;
//    descriptor = (BTNodeDescriptor*) searchNodeAttr(tree, tree->headerRec->rootNode, (BTKey*)(&key), NULL, &nodeNumber, &recordNumber, 0);
//    if(descriptor == NULL)
//        return NULL;
//
//
//    do {
//
////for(uint32_t i = 0; i < descriptor->numRecords; i++)
////{
////    off_t o = getRecordOffset(i, nodeNumber, tree);
////    HFSPlusAttrKey* k = READ_KEY(tree, o, tree->io);
////    off_t rdo = o + k->keyLength + sizeof(k->keyLength);
//////    FLIPENDIAN(k->fileID);
////    printf("node %d[%d] : offset %llx, fileID 0x%x\n", nodeNumber, i, o - (off_t)nodeNumber * tree->headerRec->nodeSize, k->fileID);
////}
//      for(uint16_t i = 0; i < descriptor->numRecords; i++)
//      {
//        recordOffset = getRecordOffset(i, nodeNumber, tree);
//        HFSPlusAttrKey* currentKey = (HFSPlusAttrKey*)READ_KEY(tree, recordOffset, tree->io);
//    //    off_t recordDataOffset = recordOffset + currentKey->keyLength + sizeof(currentKey->keyLength);
//
//        if ( currentKey->fileID != ff.file.fileID ) continue;
//
//    //    HFSPlusAttrRecord* attrRecord = READ_DATA(tree, recordDataOffset, tree->io);
//        char utf8buf[sizeof(currentKey->name.unicode)*sizeof(currentKey->name.unicode[0])+10];
//        size_t len = UTF16_to_UTF8(&currentKey->name, utf8buf, sizeof(utf8buf));
//
//        if ( !(ff.file.permissions.ownerFlags & UF_COMPRESSED)  ||  strncmp(utf8buf, "com.apple.decmpfs", sizeof("com.apple.decmpfs")) != 0 ) {
//            addUtf8ItemInXAttrNameList(&list, utf8buf, len);
//        }
//
//        free(currentKey);
//
//      }
//      nodeNumber = descriptor->fLink;
//      if ( nodeNumber ) {
//            descriptor = readBTNodeDescriptor(nodeNumber, tree); // read only the header;
//      }
//    }
//    while (nodeNumber);
//
//    return list;
//}

XAttrList* getAllExtendedAttributes(HFSPlusCatalogFileOrFolderRecord* ffPtr, Volume* volume) {
    BTree* tree;
    HFSPlusAttrKey key;
    HFSPlusAttrRecord* record;
    uint32_t nodeNumber;
    int recordNumber;
    BTNodeDescriptor* descriptor;
    HFSPlusAttrKey* currentKey;
    off_t recordOffset;
    XAttrList* list = NULL;



    uint8_t buf[32];
    const char zero[32] = { 0 };
    getVisibleFileInfo(&ff, buf);
    if (  memcmp(buf, zero, 32) != 0 ) addUtf8ItemInXAttrNameList(&list, XATTR_FINDER_INFO, strlen(XATTR_FINDER_INFO));
    
    // Push ressource fork only if there is one
    if (ff.file.recordType == kHFSPlusFileRecord  &&  ff.file.resourceFork.logicalSize != 0  &&  !(ff.file.permissions.ownerFlags & UF_COMPRESSED) ) addUtf8ItemInXAttrNameList(&list, XATTR_RESOURCE_FORK, strlen(XATTR_RESOURCE_FORK));


    if(!volume->attrTree)
        return NULL;

    memset(&key, 0 , sizeof(HFSPlusAttrKey));
    key.fileID = ffPtr->file.fileID;
    key.startBlock = 0;
    key.name.length = 0;
    key.keyLength = sizeof(HFSPlusAttrKey) - sizeof(HFSUniStr255) + sizeof(key.name.length) + (sizeof(uint16_t) * key.name.length);

    tree = volume->attrTree;
    record = (HFSPlusAttrRecord*) search(tree, (BTKey*)(&key), NULL, &nodeNumber, &recordNumber, 0);

    if(record == NULL)
        return NULL;
    
    while(nodeNumber != 0) {
        descriptor = readBTNodeDescriptor(nodeNumber, tree);

        while(recordNumber < descriptor->numRecords) {
            recordOffset = getRecordOffset(recordNumber, nodeNumber, tree);
            currentKey = (HFSPlusAttrKey*) READ_KEY(tree, recordOffset, tree->io);

            if(currentKey->fileID == ffPtr->file.fileID) {

                char utf8buf[sizeof(currentKey->name.unicode)*sizeof(currentKey->name.unicode[0])+10];
                size_t len = UTF16_to_UTF8(&currentKey->name, utf8buf, sizeof(utf8buf));
        
                if ( !(ff.file.permissions.ownerFlags & UF_COMPRESSED)  ||  strncmp(utf8buf, "com.apple.decmpfs", sizeof("com.apple.decmpfs")) != 0 ) {
                    addUtf8ItemInXAttrNameList(&list, utf8buf, len);
                }
                free(currentKey);
            } else {
                free(currentKey);
                free(descriptor);
                free(record);
                return list;
            }

            recordNumber++;
        }

        nodeNumber = descriptor->fLink;
        recordNumber = 0;

        free(descriptor);
    }
    free(record);
    return list;
}

BTree* openAttributesTree(io_func* file) {
	return openBTree(file, &attrCompare, &attrKeyRead, &attrKeyWrite, &attrKeyPrint, &attrDataRead);
}

