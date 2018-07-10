#include <stdlib.h>
#include "../includes/hfs/hfsplus.h"

#include "../sparsebundlefs-lib/src/sparsebundlefs/sparsebundlefs.h"

static size_t sparsebundleRead(io_func* io, off_t location, size_t size, void *buffer)
{
  return sparsebundlefs_read(io->data, buffer, size, location);
}

static void closeFlatFile(io_func* io)
{
  sparsebundlefs_close(io->data);
}

io_func* openSparsebundleRO(const char* fileName, const char* password) {
  io_func* io;
  
  io = (io_func*) malloc(sizeof(io_func));
  io->data = malloc(sparsebundlefs_getdatasize());
  
  if(io->data == NULL) {
    perror("malloc==null");
    free(io);
    return NULL;
  }

  int rv = sparsebundlefs_open(fileName, password, io->data);
  if(rv != 0) {
    perror("sparsebundlefs_open failed");
    return NULL;
  }

  io->read = &sparsebundleRead;
  io->write = NULL;
  io->close = &closeFlatFile;
  
  return io;
}
