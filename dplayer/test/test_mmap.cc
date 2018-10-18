#include <iostream>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
int main()
{
    const int N = 6;
    int fd = open("test_mmap.dat", O_RDONLY, 0);
    short int* numbers = (short int*) mmap(NULL, sizeof(short int) * N,
                                           PROT_READ, MAP_SHARED, fd, 0);
    for (size_t ind=0; ind<N; ++ind) {
        std::cout << ind << " " << numbers[ind] << std::endl;
    }
    return 0;
}
