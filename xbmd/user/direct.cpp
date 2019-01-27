#include "../libberdip/src/common.h"

#include <unistd.h>
#include <fcntl.h>

#include "xbmd_user.h"

internal void
fatal_error(char *message, ...)
{
    va_list args;
    fprintf(stderr, "FATAL: ");
    va_start(args, message);
    vfprintf(stderr, message, args);
    va_end(args);
    fprintf(stderr, ".\n");
    
    exit(1);
}

internal void
output(char *message, ...)
{
    va_list args;
    fprintf(stdout, "OUTPUT: ");
    va_start(args, message);
    vfprintf(stdout, message, args);
    va_end(args);
    fprintf(stdout, ".\n");
}

int main(int argc, char **argv)
{
    int fd = open("/dev/xbmd", O_RDWR);
    if (fd <= 0) {
        fatal_error("Could not open file /dev/xbmd");
    }
    
    u32 *writeBuffer = allocate_array(u32, 1024);
    u32 *readBuffer  = allocate_array(u32, 1024);
    
    for (u32 i = 0; i < 1024; ++i) {
        writeBuffer[i] = (i + 1) * 3;
    }
    
    for (u32 i = 0; i < 1024; ++i) {
        readBuffer[i] = 0xDEADBEAD;
    }
    
    read(fd, readBuffer, 1024 * sizeof(u32));
    output("Data copied from kernel");
    
    // NOTE(michiel): Copy data to kernel
    write(fd, writeBuffer, 1024 * sizeof(u32));
    output("Data copied to kernel");
    
    read(fd, readBuffer, 1024 * sizeof(u32));
    output("Data copied from kernel");
    
    // NOTE(michiel): Write error check
        b32 writeError = false;
        for (u32 i = 0; i < 1024; ++i) {
            u32 readData = readBuffer[i];
            u32 writeData = writeBuffer[i];
            if (readData != writeData) {
                output("Mismatch: wrote %d, got %d", writeData, readData);
                writeError = true;
            }
    }
    
    close(fd);
    
    return 0;
}
