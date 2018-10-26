#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <czmq.h>

#include <Eigen/Core>

template<typename Sample>
struct BlockStreams {
    typedef Sample sample_t;

    typedef Eigen::Array<sample_t, Eigen::Dynamic, Eigen::Dynamic> array_t;
    typedef Eigen::Map<const array_t, Eigen::ColMajor> mapped_array_t;
    
    mapped_array_t array;

    // the raw data array
    void* vdata;
    size_t bytes;
    size_t stride;              // number of columns/channels


    BlockStreams(void* data, size_t bytes, size_t stride)
        : array((sample_t*)data, (bytes/sizeof(sample_t))/stride, stride)
        , vdata(data)
        , bytes(bytes)
        , stride(stride) {
    }

    
};

typedef BlockStreams<short int> BSS;

int main()
{
    zsys_init();
    zsys_set_logident("test_blockarray");

    // no error checking....
    int fd = open("test_dloader.dat", O_RDONLY, 0);
    struct stat st;
    int rc = fstat(fd, &st);
    size_t siz = st.st_size;
    void* vdata = mmap(NULL, siz,
                     PROT_READ, MAP_PRIVATE|MAP_POPULATE, fd, 0);

    zsys_info("making bss");
    auto tbeg = zclock_usecs();
    BSS bss(vdata, siz, 960);

    const auto& arr = bss.array;
    zsys_info("array: nrows=%d ncols=%d arr=0x%x data=0x%x %d",
              arr.rows(), arr.cols(), arr.data(), vdata, zclock_usecs() - tbeg);

    return 0;
}
