#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <czmq.h>

int fopen(const char* filename)
{
    int fd = open(filename, O_RDONLY, 0);
    if (fd < 0) {
        zsys_error("failed to open file %s", filename);
        assert (fd == 0);
    }
    return fd;
}

size_t fsize(int fd)
{
    struct stat st;
    int rc = fstat(fd, &st);
    if (rc < 0) {
        zsys_error("failed to stat %d", fd);
        assert (rc == 0);
    }
    return st.st_size;
}

void* mmap_read(int fd, size_t siz)
{
    void* vdata = mmap(NULL, siz,
                     PROT_READ, MAP_PRIVATE|MAP_POPULATE, fd, 0);
    close(fd);                  // mmap stays available
    if (vdata == MAP_FAILED) {
        zsys_error("failed to mmap fd %d for size %jd", fd, siz);
        assert (vdata != MAP_FAILED);
    }
    return vdata;
}

void mmap_close(void* &data, size_t siz)
{
    munmap(data, siz);
    data = NULL;
}

void* file_read(int fd, size_t siz)
{
    void* vdata = malloc(siz);
    if (!vdata) {
        zsys_error("failed to alloc size %jd", siz);
        assert(vdata);
    }
    int rc = read(fd, vdata, siz);
    if (rc < 0) {
        free (vdata);
        zsys_error("failed to read %d for size %jd", fd, siz);
        assert (rc == 0);
    }
    close(fd);
    return vdata;
}

void file_close(void* &data, size_t /*unused*/) {
    free(data);
    data = NULL;
}

template<typename sample_t>
sample_t sum(void* data, size_t nbytes)
{
    const size_t sample_size = sizeof(sample_t);
    const size_t end = nbytes/sample_size;
    sample_t tot = 0;           // will likely overflow.
    sample_t* arr = (sample_t*)data;
    for (size_t ind=0; ind != end; ++ind) {
        tot += arr[ind];
    }
    return tot;
}

double dt(int64_t t1, int64_t t2)
{
    return 1e-6*(t2-t1);
}
double mb(size_t siz)
{
    return 1e-6*siz;
}

int main(int argc, char* argv[])
{
    zsys_init();
    zsys_set_logident("test_mmap");

    if (argc < 3) {
        zsys_error("usage: test_mmap [file|mmap] filename.dat");
        return -1;
    }

    const char* meth = argv[1];
    const char* filename = argv[2];

    if (not (streq(meth, "file") or streq(meth, "mmap")) ) {
        zsys_error("unknown method: %s", meth);
        return -1;
    }

    zsys_info("%s %s", meth, filename);

    auto tbeg = zclock_usecs();
    int fd = fopen(filename);
    size_t siz = fsize(fd);
    auto topen = zclock_usecs();
    void* data=NULL;
    if (streq(meth, "file")) {
        data = file_read(fd, siz);
    }
    else {
        data = mmap_read(fd, siz);
    }
    auto tread = zclock_usecs();
    sum<short int>(data, siz);
    auto tuse = zclock_usecs();
    if (streq(meth, "file")) {
        file_close(data, siz);
    }
    else {
        mmap_close(data, siz);
    }
    auto tend = zclock_usecs();

    const double s = mb(siz);
    zsys_info("%s: %.3fM ", meth, s);

    double t = 0;

    t = dt(tbeg, topen);
    zsys_info("open:\t%.2e s %f MB/s", t, s/t);

    t = dt(topen, tread);
    zsys_info("read:\t%.2e s %f MB/s", t, s/t);
    
    t = dt(tread, tuse);
    zsys_info("use:\t%.2e s %f MB/s", t, s/t);

    t = dt(tuse, tend);
    zsys_info("close:\t%.2e s %f MB/s", t, s/t);
    
    return 0;
}
