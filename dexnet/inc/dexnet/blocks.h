// Give structure to an array
#ifndef dexnet_blocks_h_seen
#define dexnet_blocks_h_seen


namespace dexnet {
    namespace blocks {

        template<typename Sample>
        struct BlockStreams {
            typedef Sample sample_t;

            const sample_t* array;

            // the raw data array
            const void* vdata;
            size_t nbytes;
            size_t ncols;              // number of columns/channels
            size_t nrows;

            BlockStreams(const void* data, size_t nbytes, size_t stride)
                : array((sample_t*)data)
                , vdata(data)
                , nbytes(nbytes)
                , ncols(stride)
                , nrows(nbytes/(sizeof(sample_t)*stride)) {
            }

            typedef std::vector<const sample_t*> excerpt_t;
            // return a block in the form of addresses of the starting
            // columns.  If returned vector is empty it means the requested
            // excerpt is out of bounds.
            excerpt_t excerpt(size_t col, size_t row, size_t width, size_t height) {
                if (col >= ncols or col + width > ncols) { return excerpt_t(); }
                if (row >= nrows or row + height > nrows) { return excerpt_t(); }
                excerpt_t ret(height, nullptr);
                for (size_t ind=0; ind != height; ++ind) {
                    const size_t irow = row + ind;
                    ret[ind] = array + col + irow*ncols;
                }
                zsys_debug("excerpt c/r=(%d/%d) + w/h=(%d/%d) = %jd",
                           col, row, width, height, ret.size());
                return ret;
            }

    
        };

    }
}
#endif
