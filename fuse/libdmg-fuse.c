#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <unistd.h>
//#include "be.h"
#include <errno.h>
#ifdef __APPLE__
#else
  #include <attr/xattr.h>
#endif

#include <unicode/ustring.h>
#include <unicode/ucnv.h>
#include <unicode/utypes.h>

#include "../includes/hfs/hfsplus.h"
#include "../includes/hfs/hfscompress.h"
#include "libdmg-fuse.h"

//#include "sparsefs/SparsefsReader.h"
#include <fcntl.h>


#include "../includes/hfs/hfslib.h"
#include "abstractfile.h"
#include <inttypes.h>

#include <wchar.h>

#define ff (*ffPtr)

char endianness;

void TestByteOrder()
{
	short int word = 0x0001;
	char *byte = (char *) &word;
	endianness = byte[0] ? IS_LITTLE_ENDIAN : IS_BIG_ENDIAN;
}

Volume* volume;
UConverter* g_utf16be;
UConverter* g_utf16le;
UConverter* g_utf8;

//HFSPlusCatalogRecord* getRecordFromPathCached(const char* path, Volume* volume);

#ifdef DEBUG
int myFiller (void *buf, const char *name, const struct stat *stbuf, off_t off)
{
	return 0;
}

char* debugFile = "no file yet";

#endif

int main(int argc, const char** argv)
{
#ifdef DEBUG
//printf("libdmg-fuse started\n");
//printf("sizeof(HFSUniStr255)=%zd\n", sizeof(HFSUniStr255));
//printf("sizeof(HFSPlusExtentDescriptor)=%zd\n", sizeof(HFSPlusExtentDescriptor));
//printf("sizeof(HFSPlusForkData)=%zd\n", sizeof(HFSPlusForkData));
//printf("sizeof(HFSPlusVolumeHeader)=%zd\n", sizeof(HFSPlusVolumeHeader));
//printf("sizeof(BTNodeDescriptor)=%zd\n", sizeof(BTNodeDescriptor));
//printf("sizeof(BTHeaderRec)=%zd\n", sizeof(BTHeaderRec));
//printf("sizeof(HFSPlusExtentKey)=%zd\n", sizeof(HFSPlusExtentKey));
//printf("sizeof(HFSPlusCatalogKey)=%zd\n", sizeof(HFSPlusCatalogKey));
//printf("sizeof(FileInfo)=%zd\n", sizeof(FileInfo));
//printf("sizeof(ExtendedFileInfo)=%zd\n", sizeof(ExtendedFileInfo));
//printf("sizeof(FolderInfo)=%zd\n", sizeof(FolderInfo));
//printf("sizeof(ExtendedFolderInfo)=%zd\n", sizeof(ExtendedFolderInfo));
//printf("sizeof(HFSPlusBSDInfo)=%zd\n", sizeof(HFSPlusBSDInfo));
//printf("sizeof(HFSPlusCatalogFolder)=%zd\n", sizeof(HFSPlusCatalogFolder));
//printf("sizeof(HFSPlusCatalogFile)=%zd\n", sizeof(HFSPlusCatalogFile));
//printf("sizeof(HFSPlusCatalogThread)=%zd\n", sizeof(HFSPlusCatalogThread));
//printf("sizeof(HFSPlusAttrForkData)=%zd\n", sizeof(HFSPlusAttrForkData));
//printf("sizeof(HFSPlusAttrExtents)=%zd\n", sizeof(HFSPlusAttrExtents));
//printf("sizeof(HFSPlusAttrData)=%zd\n", sizeof(HFSPlusAttrData));
//printf("sizeof(HFSPlusAttrRecord)=%zd\n", sizeof(HFSPlusAttrRecord));
//printf("sizeof(HFSPlusAttrKey)=%zd\n", sizeof(HFSPlusAttrKey));
//printf("sizeof(HFSPlusCatalogRecord)=%zd\n", sizeof(HFSPlusCatalogRecord));
//printf("sizeof(CatalogRecordList)=%zd\n", sizeof(CatalogRecordList));
//printf("sizeof(HFSUniStr255)=%zd\n", sizeof(HFSUniStr255));
#endif

		UErrorCode unicode_error = U_ZERO_ERROR;
		g_utf16be = ucnv_open("UTF-16BE", &unicode_error);
		if ( unicode_error != U_ZERO_ERROR ) {
			printf("cannot alloc unicode converter UTF16LE\n");
			return 1;
		}
		g_utf16le = ucnv_open("UTF-16LE", &unicode_error);
		if ( unicode_error != U_ZERO_ERROR ) {
			printf("cannot alloc unicode converter UTF16LE\n");
			return 1;
		}
		g_utf8 = ucnv_open("UTF-8", &unicode_error);
		if ( unicode_error != U_ZERO_ERROR ) {
			printf("cannot alloc unicode converter UTF-8\n");
			return 1;
		}

		struct fuse_operations ops;
		struct fuse_args args = FUSE_ARGS_INIT(0, NULL);
	
		if (argc < 3)
		{
			showHelp(argv[0]);
			return 1;
		}
	
		io_func* io;

		TestByteOrder();

		if(argc < 3) {
			fprintf(stderr, "usage: %s <image-file> <mount point>\n", argv[0]);
			return 1;
		}

		// I didn't test writing and probably will never do for me.
		// So everything is open readonly. If someone needs writing, it still there for dmg. Not implemented in sparsebundle at this time.
		//io = openFlatFileRO(argv[1]);
#ifdef DEBUG
        if ( strstr(argv[1], ".sparsebundle") == argv[1]+strlen(argv[1])-strlen(".sparsebundle") ) {
		    io = openSparsebundleRO(argv[1], "foo");
        }
        else
        if ( strstr(argv[1], ".sparsebundle/") == argv[1]+strlen(argv[1])-strlen(".sparsebundle/") ) {
		    io = openSparsebundleRO(argv[1], "foo");
        }
        else
        if ( strstr(argv[1], ".dmg") == argv[1]+strlen(argv[1])-strlen(".dmg") ) {
            io = openFlatFileRO(argv[1]);
        }else{
            fprintf(stderr, "Unknow format at path '%s'. Must be dmg or sparsebundle\n", argv[1]);
            return 1;
        }
#else
        if ( strstr(argv[1], ".sparsebundle") == argv[1]+strlen(argv[1])-strlen(".sparsebundle") ) {
		    io = openSparsebundleRO(argv[1], NULL);
        }
        else
        if ( strstr(argv[1], ".sparsebundle/") == argv[1]+strlen(argv[1])-strlen(".sparsebundle/") ) {
		    io = openSparsebundleRO(argv[1], NULL);
        }
        else
        if ( strstr(argv[1], ".dmg") == argv[1]+strlen(argv[1])-strlen(".dmg") ) {
            io = openFlatFileRO(argv[1]);
        }else{
            fprintf(stderr, "Unknow format at path '%s'. Must be dmg or sparsebundle\n", argv[1]);
            return 1;
        }
#endif
		if(io == NULL) {
			fprintf(stderr, "error: Cannot open image-file.\n");
			return 1;
		}

		volume = openVolume(io);
		if(volume == NULL) {
			fprintf(stderr, "error: Cannot open volume.\n");
			CLOSE(io);
			return 1;
		}

#ifdef DEBUG

//debugFile = "/ITER/Readme.txt";
//debugFile = "/.apdisk";
//debugFile = "/file+lotxattrs";
debugFile = "/";
//debugFile = "/Backup/Program Files/Prograns/Condor/Condor.exe";
//debugFile = "/SmallFile0";

size_t rv;
size_t rv1;
size_t rv2;
//char buf[2893312+1];

//rv = hfs_readdir("/", buf, myFiller, 0, NULL);

struct fuse_file_info info;
//rv = hfs_open(debugFile, &info);
//rv = hfs_read("", buf, sizeof(buf), 0, &info);
//uint8_t* buf2 = &buf[192513];

//file = g_volume->openFile("/testfile_compressed.txt");
//fh = new std::shared_ptr<Reader>(file);
//rv = file->read(buf, sizeof(buf), 0);

////struct stat stat = g_volume->stat("titi");
//char buf[1025];
//size_t rv;
struct stat st_stat1;
struct stat st_stat2;
//
//rv1 = stat("/Volumes/TestSrc/file+nothing", &st_stat1);
//rv2 = hfs_stat(debugFile, &st_stat2);
//rv = hfs_listxattr(debugFile, buf, sizeof(buf));
//rv = hfs_getxattr(debugFile, "com.apple.FinderInfo", buf, sizeof(buf), 0);
//rv = hfs_getxattr(debugFile, "com.apple.lastuseddate#PS", buf, sizeof(buf), 0);

//HFSPlusCatalogFileOrFolderRecord* record;
//record = (HFSPlusCatalogFileOrFolderRecord*)getRecordFromPathCached("/Desktop/FTPServer.app/Contents/MacOS/FTPServer", volume);
//HFSPlusDecmpfs* compressData;
//getAttribute(volume, record->file.fileID, "com.apple.decmpfs", (uint8_t**)(&compressData));
//flipHFSPlusDecmpfs(compressData);

//HFSPlusCatalogFile* file;
//file = (HFSPlusCatalogFile*)getRecordFromPathCached("/Desktop/FTPServer.app/Contents/MacOS/FTPServer", volume);
//HFSPlusDecmpfs* compressData;
//getAttribute(volume, file->fileID, "com.apple.decmpfs", (uint8_t**)(&compressData));
//FILE* fp = fopen("/JiefLand/Tmp/Sparsefs test/FTPServer.libdmg.uncompressed", "wb");
//AbstractFile* stderrFile = createAbstractFileFromFile(fp);
//writeToFile((HFSPlusCatalogFile*)file, stderrFile, volume);
//free(stderrFile);
//fprintf(stderr, "done\n");

char* cmd;
asprintf(&cmd, "umount %s", argv[2]);
system(cmd);
//exit(0);

#endif

		memset(&ops, 0, sizeof(ops));
	
		ops.getattr = hfs_stat;
        ops.open = hfs_open;
        ops.read = hfs_read;
        ops.release = hfs_release;
        //ops.opendir = hfs_opendir;
		ops.readdir = hfs_readdir;
        ops.readlink = hfs_readlink;
		//ops.releasedir = hfs_releasedir;
        ops.getxattr = hfs_getxattr;
        ops.listxattr = hfs_listxattr;
	
		for (int i = 0; i < argc; i++)
		{
			if (i == 1)
				;
			else
				fuse_opt_add_arg(&args, argv[i]);
		}
		fuse_opt_add_arg(&args, "-oro");
		fuse_opt_add_arg(&args, "-s"); // I don't know if multithread would work.
#ifdef DEBUG
		fuse_opt_add_arg(&args, "-f");
#endif
	
		printf("Everything looks OK, disk mounted\n");

		return fuse_main(args.argc, args.argv, &ops, 0);
}

void showHelp(const char* argv0)
{
	fprintf(stderr, "Usage: %s <file> <mount-point> [fuse args]\n\n", argv0);
	fprintf(stderr, ".DMG files and raw disk images can be mounted.\n");
	fprintf(stderr, "%s automatically selects the first HFS+/HFSX partition.\n", argv0);
}



// Get Record from path. If it's an alias (not a symlink, an osx alias only), resolve it.
// Returns the destination if it's an lias, file otherwise.
// Initialize aliasff with the lias file
HFSPlusCatalogFileOrFolderRecord* hfs_getRecordFromPath(const char* path, Volume* volume, HFSPlusCatalogFileOrFolderRecord** aliasff)
{
    HFSPlusCatalogFileOrFolderRecord* ffPtr;
    ffPtr = (HFSPlusCatalogFileOrFolderRecord*)getRecordFromPath2(path, volume, NULL, NULL, 0);
    if ( ffPtr == NULL ) return NULL;

    HFSPlusCatalogFileOrFolderRecord* hardLinkFilePtr; // destination of the hard link
    if ( ff.file.userInfo.fileType == kHardLinkFileType )
    {
      char pathBuffer[1024];
        sprintf(pathBuffer, "iNode%d", ((HFSPlusCatalogFile*)&ff)->permissions.special.iNodeNum);
      HFSPlusCatalogKey nkey;
        nkey.parentID = volume->metadataDir;
        ASCIIToUnicode(pathBuffer, &nkey.nodeName);
        nkey.keyLength = sizeof(nkey.parentID) + sizeof(nkey.nodeName.length) + (sizeof(uint16_t) * nkey.nodeName.length);

        hardLinkFilePtr = (HFSPlusCatalogFileOrFolderRecord*) search(volume->catalogTree, (BTKey*)(&nkey), NULL, NULL, NULL, 1);
        ASSERT( ffPtr != NULL, "ffPtr != NULL");
        if ( aliasff ) *aliasff = ffPtr;
        return hardLinkFilePtr;
    }else{
        if ( aliasff ) *aliasff = NULL;
        return ffPtr;
    }
}



time_t appleToUnixTime(uint32_t apple)
{
	const time_t offset = 2082844800L; // Nb of seconds between 1st January 1904 12:00:00 and unix epoch
	if (apple == 0)
		return 0; // 0 stays 0, even if it change the date from 1904 to 1970.
	// Force time to wrap around and stay positive number. That's how Mac does it.
	// File from before 70 will have date in the future.
	// Example : Time value 2082844799, that should be Dec 31st 12:59:59 PM will become February 7th 2106 6:28:15 AM.
	return (uint32_t)(apple - offset);
}

void hfs_nativeToStat(const HFSPlusCatalogFileOrFolderRecord* ffPtr, struct stat* stat, int resourceFork)
{
	memset(stat, 0, sizeof(*stat));
    stat->st_blksize = 512;
    stat->st_nlink = 1;

    #define hardLinkFile (*hardLinkFilePtr)
    const HFSPlusCatalogFileOrFolderRecord* hardLinkFilePtr;
    if ( ff.file.userInfo.fileType == kHardLinkFileType )
    {
      char pathBuffer[1024];
        sprintf(pathBuffer, "iNode%d", ((HFSPlusCatalogFile*)&ff)->permissions.special.iNodeNum);
      HFSPlusCatalogKey nkey;
        nkey.parentID = volume->metadataDir;
        ASCIIToUnicode(pathBuffer, &nkey.nodeName);
        nkey.keyLength = sizeof(nkey.parentID) + sizeof(nkey.nodeName.length) + (sizeof(uint16_t) * nkey.nodeName.length);

        hardLinkFilePtr = (HFSPlusCatalogFileOrFolderRecord*) search(volume->catalogTree, (BTKey*)(&nkey), NULL, NULL, NULL, 1);
        stat->st_nlink = hardLinkFile.file.permissions.special.linkCount;
    }else{
        hardLinkFilePtr = &ff;
    }

#ifdef __APPLE__
   stat->st_birthtime = appleToUnixTime(hardLinkFile.file.createDate);
#endif
	stat->st_atime = appleToUnixTime(hardLinkFile.file.accessDate);
	stat->st_mtime = appleToUnixTime(hardLinkFile.file.contentModDate);
	stat->st_ctime = appleToUnixTime(hardLinkFile.file.attributeModDate);
    
	stat->st_mode = hardLinkFile.file.permissions.fileMode;
	stat->st_uid = hardLinkFile.file.permissions.ownerID;
	stat->st_gid = hardLinkFile.file.permissions.groupID;
	stat->st_ino = hardLinkFile.file.fileID;

	if (hardLinkFile.file.recordType == kHFSPlusFileRecord)
	{
		if (!resourceFork)
		{
			stat->st_size = hardLinkFile.file.dataFork.logicalSize;
			stat->st_blocks = hardLinkFile.file.dataFork.totalBlocks;
		}
		else
		{
			stat->st_size = hardLinkFile.file.resourceFork.logicalSize;
			stat->st_blocks = hardLinkFile.file.resourceFork.totalBlocks;
		}

		if (S_ISCHR(stat->st_mode) || S_ISBLK(stat->st_mode))
			stat->st_rdev = hardLinkFile.file.permissions.special.rawDevice;
	}

	if (!stat->st_mode)
	{
		if (hardLinkFile.file.recordType == kHFSPlusFileRecord)
		{
			stat->st_mode = S_IFREG;
			stat->st_mode |= 0444;
		}
		else
		{
			stat->st_mode = S_IFDIR;
			stat->st_mode |= 0555;
		}
	}

    if ((ff.file.permissions.ownerFlags & UF_COMPRESSED) && !stat->st_size) {
        HFSPlusDecmpfs* compressData;
        getAttribute(volume, ff.file.fileID, "com.apple.decmpfs", (uint8_t**)(&compressData), 1);
        flipHFSPlusDecmpfs(compressData);
        if ( compressData ) stat->st_size = compressData->uncompressed_size;
    }

}

int hfs_stat(const char* path, struct stat* stat)
{
#ifdef DEBUG
if ( strstr(path, debugFile) != 0 ) {
	fprintf(stderr, "hfs_stat(%s)\n", path);
}
#endif

	HFSPlusCatalogFileOrFolderRecord* record;
	record = (HFSPlusCatalogFileOrFolderRecord*)getRecordFromPath2(path, volume, NULL, NULL, 0);

	if (!record) return -ENOENT;
	hfs_nativeToStat(record, stat, 0);
	free(record);
	return 0;
}

int hfs_readlink(const char* path, char* buf, size_t size)
{
#ifdef DEBUG
if ( strstr(path, debugFile) != 0 ) {
  fprintf(stderr, "hfs_readlink(%s)\n", path);
}
#endif
    HFSPlusCatalogFileOrFolderRecord* record;
    record = (HFSPlusCatalogFileOrFolderRecord*)getRecordFromPath2(path, volume, NULL, NULL, 0);
    if ( !record ) return -ENOENT;

    if ( record->file.recordType != kHFSPlusFileRecord ) return -EPERM;
    io_func* io;
    io = openRawFile(record->file.fileID, &record->file.dataFork, (HFSPlusCatalogRecord*)record, volume);
    if(io == NULL) {
        hfs_panic("error opening file");
        return -1;
    }
    if ( size > record->file.dataFork.logicalSize ) size = record->file.dataFork.logicalSize;
    else return -ERANGE;
    if(READ(io, 0, size, buf)) {
        buf[size] = 0;
        return 0;
    }
	return -1;
}

typedef struct openfile_st {
	io_func* io;
	off_t size;
	HFSPlusCompressed* hfsPlusCompressed;
	HFSPlusCatalogFile* file;
} openfile_t;

//size_t readFromFile(HFSPlusCatalogFile* file, uint8_t* buf, size_t size, off_t offset, Volume* volume)
//{
//	io_func* io;
//	size_t bytesLeft;
//
//	if(file->permissions.ownerFlags & UF_COMPRESSED) {
//		io = openHFSPlusCompressed(volume, file);
//		if(io == NULL) {
//			hfs_panic("error opening file");
//			return -1;
//		}
//		if ( offset >= ((HFSPlusCompressed*) io->data)->decmpfs->uncompressed_size ) return 0;
//		bytesLeft = ((HFSPlusCompressed*) io->data)->decmpfs->uncompressed_size - offset;
//	} else {
//		io = openRawFile(file->fileID, &file->dataFork, (HFSPlusCatalogRecord*)file, volume);
//		if(io == NULL) {
//			hfs_panic("error opening file");
//			return -1;
//		}
//		if ( offset >= file->dataFork.logicalSize ) return 0;
//		bytesLeft = file->dataFork.logicalSize - offset;
//	}
//	if ( bytesLeft > size ) bytesLeft = size;
//	if(READ(io, offset, bytesLeft, buf)) {
//		return bytesLeft;
//	}
////	size_t read = READ(io, curPosition, bytesLeft, buf);
////	CLOSE(io);
//	return -1;
//}

int hfs_open(const char* path, struct fuse_file_info* info)
{
#ifdef DEBUG
	if ( strstr(path, debugFile) != NULL )
		fprintf(stderr, "hfs_open(%s)\n", path);
#endif
	HFSPlusCatalogFileOrFolderRecord* ffPtr;
	ffPtr = hfs_getRecordFromPath(path, volume, NULL);
	if (!ffPtr) {
		return -ENOENT;
	}
	if ( ff.file.recordType != kHFSPlusFileRecord ) {
		errno = EISDIR;
		free(ffPtr);
		return -1;
	}

	openfile_t* openfile = malloc(sizeof(*openfile));
	openfile->file = (HFSPlusCatalogFile*)&ff;
	openfile->hfsPlusCompressed = NULL;

	if(openfile->file->permissions.ownerFlags & UF_COMPRESSED) {
		openfile->io = openHFSPlusCompressed(volume, openfile->file);
		if(openfile->io == NULL) {
			hfs_panic("error opening file");
			free(openfile);
			return -1;
		}
		openfile->hfsPlusCompressed = ((HFSPlusCompressed*) openfile->io->data);
		openfile->size = ((HFSPlusCompressed*) openfile->io->data)->decmpfs->uncompressed_size;
	} else {
		openfile->io = openRawFile(openfile->file->fileID, &openfile->file->dataFork, (HFSPlusCatalogRecord*)openfile->file, volume);
		if(openfile->io == NULL) {
			hfs_panic("error opening file");
			free(openfile);
			return -1;
		}
#ifdef DEBUG
		((RawFile*)openfile->io->data)->path = strdup(path);
#endif
		openfile->hfsPlusCompressed = NULL;
		openfile->size = openfile->file->dataFork.logicalSize;
	}
	info->fh = (intptr_t)openfile;
	return 0;
}

int hfs_read(const char* path, char* buf, size_t bytes, off_t offset, struct fuse_file_info* info)
{
#ifdef DEBUG
	if ( strstr(path, debugFile) != NULL )
		fprintf(stderr, "hfs_read(%s)\n", path);
#endif
	openfile_t* openfile = (openfile_t*)((intptr_t)info->fh);
	if ( !openfile ) return -EIO;
	if ( offset >= openfile->size ) return 0;

	size_t bytesLeft = openfile->size - offset;
	if ( bytesLeft < bytes ) bytes = bytesLeft;
	off_t position = 0;
	while ( position + 4096 < bytes ) {
		if ( !READ(openfile->io, offset+position, 4096, buf+position) ) return -1;
		position += 4096;
	}
	if ( position < bytes ) {
		if ( !READ(openfile->io, offset+position, bytes-position, buf+position) ) return -1;
	}
	return (int)bytes;
}

int hfs_release(const char* path, struct fuse_file_info* info)
{
	fprintf(stderr, "hfs_release(%s)\n", path);
	openfile_t* openfile = (openfile_t*)((intptr_t)info->fh);
	CLOSE(openfile->io);
	free(openfile->file);
	free(openfile);
//	if ( openfile->hfsPlusCompressed ) {
//		openfile->io->close();
//	}
	return 0;
}


int hfs_readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* info)
{
#ifdef DEBUG
	if ( strstr(path, debugFile) != NULL )
		fprintf(stderr, "hfs_readdir(%s)\n", path);
#endif
	HFSPlusCatalogFolder* record;
	record = (HFSPlusCatalogFolder*)getRecordFromPath2(path, volume, NULL, NULL, 0);
	if (!record) return -ENOENT;
	CatalogRecordList* list;
	CatalogRecordList* theList;
	theList = list = getFolderContents(record->folderID, volume);
	size_t count = 0;

	while(list != NULL)
	{
		HFSPlusCatalogFileOrFolderRecord* folder = (HFSPlusCatalogFileOrFolderRecord*)list->record;
		struct stat stat;
		hfs_nativeToStat((HFSPlusCatalogFileOrFolderRecord*)folder, &stat, 0);

	  UErrorCode unicode_error = U_ZERO_ERROR;
	  char utf8Name[3000];
	  int32_t rv5 = ucnv_fromUChars(g_utf8, utf8Name, sizeof(utf8Name)-1, (UChar*)list->name.unicode, list->name.length, &unicode_error);
	  utf8Name[rv5] = 0;
//printf("nameBuf2 %s\n", utf8Name);

		if (record->folderID != kHFSRootFolderID  || (utf8Name[0] != 0  &&  strcmp(utf8Name, ".HFS+ Private Directory Data\r") != 0  &&  strcmp(utf8Name, ".journal") != 0  &&  strcmp(utf8Name, ".journal_info_block") != 0 ) ) {
			if (filler(buf, utf8Name, NULL, 0) != 0)
				return -ENOMEM;
		}
		list = list->next;
		count ++;
	}
	releaseCatalogRecordList(theList);
	free(record);
	return 0;
}

int hfs_releasedir(const char *path, struct fuse_file_info *fi)
{
    fprintf(stderr, "hfs_releasedir(%s)\n", path);
    return 0;
}

size_t hfs_getxattr2(HFSPlusCatalogFileOrFolderRecord* ffPtr, const char* name, uint8_t** data)
{
    ASSERT(data != NULL, "data != NULL");
    if ( strcmp(name, XATTR_RESOURCE_FORK) == 0 )
    {
        if ( ff.file.recordType != kHFSPlusFileRecord ) return -EPERM;
        io_func* io;
        io = openRawFile(ff.file.fileID, &ff.file.resourceFork, (HFSPlusCatalogRecord*)&ff, volume);
        if(io == NULL) {
            hfs_panic("error opening file");
        }
        if ( ff.file.resourceFork.logicalSize > 0 )
        {
            *data = malloc(ff.file.resourceFork.logicalSize);
            if ( *data == NULL ) hfs_panic("not enough memory ?");
            if(READ(io, 0, ff.file.resourceFork.logicalSize, *data)) {
                CLOSE(io);
                return ff.file.resourceFork.logicalSize;
            }
            free(*data);
            CLOSE(io);
        }
    }
    else
    if ( strcmp(name, XATTR_FINDER_INFO) == 0 )
    {
        *data = malloc(32);
        const char zero[32] = { 0 };
        getVisibleFileInfo(&ff, *data);
        if (  memcmp(*data, zero, 32) != 0 ) return 32;
    }
    else
    {
        return getAttribute(volume, ff.file.fileID, name, data, 0);
    }
    return 0;
}

#ifdef __APPLE__
  int hfs_getxattr(const char* path, const char* name, char* value, size_t size, uint32_t position)
#else
  int hfs_getxattr(const char* path, const char* name, char* value, size_t size)
#endif
{
#if defined(__APPLE__) && defined(DEBUG)
if ( strstr(path, debugFile) != NULL ) {
    printf("hfs_getxattr path=%s, name=%s, value=%llx, size=%zd, position=%d\n", path, name, (uint64_t)value, size, position);
}
#endif

#ifdef __APPLE__
	if ( position != 0 ) {
        printf("hfs_getxattr path=%s, name=%s, value=%llx, size=%zd, position=%d\n", path, name, (uint64_t)value, size, position);
		hfs_panic("WARINNG : position unsupported yet");
	}
#endif
    
    HFSPlusCatalogFileOrFolderRecord* ffPtr;
    ffPtr = (HFSPlusCatalogFileOrFolderRecord*)hfs_getRecordFromPath(path, volume, NULL);
    if ( ffPtr == NULL ) return -ENOENT;

    uint8_t* data = NULL;
    size_t attrLen = hfs_getxattr2(&ff, name, &data);

//if ( strstr(path, debugFile) != NULL ) {
//    printf("hfs_getxattr attrLen=%zd data:", attrLen);
//        for(uint i=0 ;i<attrLen;i++) printf("%x", data[i]);
//        printf("\n");
//}
    if ( value == NULL ) {
        //printf("hfs_getxattr(%s, %s) : value == NULL, returns attrLen=%zd\n", path, name, attrLen);
        if ( data ) free(data);
        free(ffPtr);
        return (int)attrLen;
    }
    if (size < attrLen)
    {
//if ( strstr(path, debugFile) != NULL ) {
//#ifdef __APPLE__
//  printf("hfs_getxattr returns size vlen=%zd, position=%d, attrLen=%zd, size=%zd\n", size, position, attrLen, size);
//#endif
//}
        // looks like high sierra return the maxmum bytes he can, but never ERANGE !
        memcpy(value, data, size);
        free(data);
        free(ffPtr);
        return (int)size;
        //return -ERANGE;
    }


    memcpy(value, data, attrLen);
    free(data);
    free(ffPtr);

//if ( strstr(path, debugFile) != NULL ) {
//#ifdef __APPLE__
//  printf("hfs_getxattr returns vlen=%zd, position=%d, attrLen=%zd\n", size, position, attrLen);
//#endif
//}
    return (int)attrLen;
}

//XAttrList* getAllExtendedAttributesJief2(HFSPlusCatalogFileOrFolderRecord* ffPtr, Volume* volume);

/**************************************************************************************************************************/
void freeAttrList(XAttrList* attrList)
{
    while ( attrList ) {
        XAttrList* attrListNext = attrList->next;
        free(attrList->name);
        free(attrList);
        attrList = attrListNext;
    }
}

int hfs_listxattr(const char* path, char* buffer, size_t size)
{
#ifdef DEBUG
if ( strstr(path, debugFile) != NULL ) {
    fprintf(stderr, "hfs_listxattr(%s, %llx, %zd)\n", path, (uint64_t)(intptr_t)buffer, size);
}
#endif
//    if ( strcmp("/", path) == 0 ) {
//printf("hfs_listxattr(%s) path==/, returns rv=%d\n", path, 0);
//        return 0;
//    }

    HFSPlusCatalogFileOrFolderRecord* ffPtr;
    ffPtr = (HFSPlusCatalogFileOrFolderRecord*)hfs_getRecordFromPath(path, volume, NULL);
    if ( ffPtr == NULL ) {
        //printf("hfs_listxattr(%s) ffPtr == NULL, returns rv=%d\n", path, -ENOENT);
        return -ENOENT;
    }
    
    XAttrList* attrList = getAllExtendedAttributes(&ff, volume);
//if ( strstr(path, debugFile) != NULL )
//{
//    {
//        XAttrList* attrListIdx = attrList;
//        while ( attrListIdx != NULL  ) {
//            ASSERT(attrListIdx->name != NULL, "attrListIdx->name != NULL");
//            printf("%s   ", attrListIdx->name);
//            attrListIdx = attrListIdx->next;
//        }
//    }
//    printf("\n");
//}
    if ( attrList == NULL ) {
        //printf("hfs_listxattr(%s) attrList == NULL, returns rv=%d\n", path, 0);
        free(ffPtr);
        return 0;
    }
    // get the total size
    size_t bufLengthNeeded = 0;
    {
        XAttrList* attrListIdx = attrList;
        while ( attrListIdx != NULL  ) {
            ASSERT(attrListIdx->name != NULL, "attrListIdx->name != NULL");
            bufLengthNeeded += strlen(attrListIdx->name) + 1;
            attrListIdx = attrListIdx->next;
        }
    }
    // return size if buffer == NULL
    if ( buffer == NULL ) {
        //printf("hfs_listxattr(%s) buffer == NULL, returns rv=%zd\n", path, bufLengthNeeded);
        freeAttrList(attrList);
        free(ffPtr);
        return (int)bufLengthNeeded;
    }
    // return ERANGE if not enough space in buffer
    if (size < bufLengthNeeded) {
        //printf("hfs_listxattr returns ERANGE size=%zd, bufLengthNeeded=%zd\n", size, bufLengthNeeded);
        freeAttrList(attrList);
        free(ffPtr);
        return -ERANGE;
    }
    {
      size_t bufferIdx = 0;
        XAttrList* attrListIdx = attrList;
        while ( attrListIdx != NULL  ) {
            ASSERT(attrListIdx->name != NULL, "attrListIdx->name != NULL");
            size_t nameLen = strlen(attrListIdx->name);
            memcpy(buffer+bufferIdx, attrListIdx->name, nameLen+1);
            bufferIdx += nameLen+1;
            attrListIdx = attrListIdx->next;
        }
    }
    freeAttrList(attrList);
    //printf("hfs_listxattr(%s) returns rv=%zd\n", path, bufLengthNeeded);
    free(ffPtr);
    return (int)bufLengthNeeded;
}
