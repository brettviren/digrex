#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <czmq.h>

#include <vector>

#include "dexnet/blocks.h"

typedef dexnet::blocks::BlockStreams<short int> BSS;

typedef std::pair<size_t, void*> data_t;
data_t load(const char* filename) {
    // no error checking....
    int fd = open(filename, O_RDONLY, 0);
    struct stat st;
    int rc = fstat(fd, &st);
    size_t siz = st.st_size;
    void* vdata = mmap(NULL, siz,
                     PROT_READ, MAP_PRIVATE|MAP_POPULATE, fd, 0);    
    return std::make_pair(siz, vdata);
}


int main()
{
    zsys_init();
    zsys_set_logident("test_blockarray");

    auto data = load("test_dloader.dat");

    zsys_info("making bss");
    auto tbeg = zclock_usecs();
    BSS bss(data.second, data.first, 960);

    zsys_info("ncols=%d X nrows=%d", bss.ncols, bss.nrows);

    auto empty = bss.excerpt(bss.ncols-2, 10,  3, 30);
    assert(empty.empty());

    auto okay = bss.excerpt(10, 10, 20, 20);
    assert(okay.size());

    auto tload = zclock_usecs();
    const size_t chunk = 240;
    const size_t nchunks = 2000;
    for (size_t isend = 0; isend < bss.nrows/nchunks; ++isend) {
        for (size_t ichunk = 0; ichunk < bss.ncols/chunk; ++ichunk) {
            auto vp = bss.excerpt(ichunk*chunk, isend*nchunks, chunk,nchunks);
            assert(vp.size() == nchunks);
        }
    }
    auto tend = zclock_usecs();
    zsys_info("spin through in %fs", 1.0e-6*(tend-tload));

    munmap(data.second, data.first);

    return 0;
}
