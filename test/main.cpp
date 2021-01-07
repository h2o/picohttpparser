//
// Created by Mpho Mbotho on 2021-01-06.
//
#define CATCH_CONFIG_RUNNER

#include "catch2/catch.hpp"
#include <sys/mman.h>

char *inputbuf = nullptr;
long pagesize;

bool initialize()
{
    pagesize = sysconf(_SC_PAGESIZE);
    assert(pagesize >= 1);

    inputbuf = (char *) mmap(NULL, pagesize * 3, PROT_NONE, MAP_ANON | MAP_PRIVATE, -1, 0);
    if (inputbuf == MAP_FAILED) {
        WARN("mmap(...) failed - " << strerror(errno));
        return false;
    }

    inputbuf += pagesize * 2;
    if (mprotect(inputbuf - pagesize, pagesize, PROT_READ | PROT_WRITE) != 0) {
        WARN("mprotect(...) failed - " << strerror(errno));
        munmap(inputbuf - pagesize * 2, pagesize * 3);
        return false;
    }

    return true;
}
int main(int argc, const char *argv[])
{
    int result{0xff};
    if (initialize()) {
        result = Catch::Session().run(argc, argv);
    }
    munmap(inputbuf - pagesize * 2, pagesize * 3);
    return (result < 0xff ? result: 0xff);
}