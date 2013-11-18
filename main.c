#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <sys/time.h>

void dump_memory_map(void);

int main(int argc, char **argv) {

    void *m1 = mmalloc(50);  // should allocate 64 bytes
    void *m2 = mmalloc(100); // should allocate 128 bytes
    mfree(m1);
    void *m3 = mmalloc(56);  // should allocate 64 bytes
    void *m4 = mmalloc(11);  // should allocate 32 bytes
    mfree(m3);
    void *m5 = mmalloc(30);  // should allocate 64 bytes
    void *m6 = mmalloc(120); // should allocate 128 bytes
    mfree(m2);
    return 0;
}
