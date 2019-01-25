#include "../libberdip/src/common.h"

#include <sys/ioctl.h>         // Include IOCTL calls to access Kernel Mode Driver
#include <unistd.h>
#include <fcntl.h>

#include "xbmd_user.h"

struct Config
{
    // NOTE(michiel): Config offsets
    u32 pmOffset;
    u32 msiOffset;
    u32 pcieCapOffset;
    u32 deviceCapOffset;
    u32 deviceStatContOffset;
    u32 linkCapOffset;
    u32 linkStatContOffset;
    
    // NOTE(michiel): Register values
    u32 linkWidthCap;
    u32 linkSpeedCap;
    u32 linkWidth;
    u32 linkSpeed;
    u32 linkControl;
    u32 pmStatControl;
    u32 pmCapabilities;
    u32 msiControl;
};

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

internal int
write_data(int file, u32 size, const void *buffer)
{
    return write(file, buffer, size);
}

internal int
read_data(int file, u32 size, void *buffer)
{
    return read(file, buffer, size);
}

internal void
get_capabilities(Config *config, int fd)
{
    s32 nextCapOffset = 0x34;
    s32 currCapOffset = 0;
    s32 capId = 0;
    
    if (ioctl(fd, XBMD_IOC_RD_CFG_REG, &nextCapOffset) < 0) {
        fatal_error("IOCTL failed reading a config reg");
    } else {
        nextCapOffset = nextCapOffset & 0xFF;
    }
    
      do {
        currCapOffset = nextCapOffset;
        if (ioctl(fd, XBMD_IOC_RD_CFG_REG, &nextCapOffset) < 0) {
            fatal_error("IOCTL failed reading a config reg");
        } else {
            capId = nextCapOffset & 0xFF;
            nextCapOffset = (nextCapOffset & 0xFF00) >> 8;
        }
        
        switch (capId) {
            case 1: {
                // NOTE(michiel): Power Management Capability
                config->pmOffset = currCapOffset;
            } break;
            
            case 5: {
                // NOTE(michiel): MSI Capability
                config->msiOffset = currCapOffset;
            } break;
            
            case 16: {
                // NOTE(michiel): PCI Express Capability
                config->pcieCapOffset = currCapOffset;
                config->deviceCapOffset = currCapOffset + 4;
                config->deviceStatContOffset = currCapOffset + 8;
                config->linkCapOffset = currCapOffset + 12;
                config->linkStatContOffset = currCapOffset + 16;
            } break;
            
            default: {
                fatal_error("Read Capability is not valid");
            } break;
        }
    } while (nextCapOffset != 0);
}

internal void
update_config(Config *config, int fd)
{
    u32 regValue = 0;
    
    regValue = config->pmOffset;
    if (ioctl(fd, XBMD_IOC_RD_CFG_REG, &regValue) < 0) {
        fatal_error("IOCTL failed reading power management capabilities");
    } else {
        config->pmCapabilities = regValue >> 16;
    }
    
    regValue = config->pmOffset + 4;
    if (ioctl(fd, XBMD_IOC_RD_CFG_REG, &regValue) < 0) {
        fatal_error("IOCTL failed reading PM status/control");
    } else {
        config->pmStatControl = regValue & 0xFFFF;
    }
    
    regValue = config->msiOffset;
    if (ioctl(fd, XBMD_IOC_RD_CFG_REG, &regValue) < 0) {
        fatal_error("IOCTL failed reading MSI Control");
    } else {
        config->msiControl = regValue >> 16;
    }
    
    regValue = config->linkCapOffset;
    if (ioctl(fd, XBMD_IOC_RD_CFG_REG, &regValue) < 0) {
        fatal_error("IOCTL failed reading Link Cap offset");
    } else {
        config->linkWidthCap = (regValue >> 4) & 0x3F;
        config->linkSpeedCap = regValue & 0xF;
    }
    
    regValue = config->linkStatContOffset;
    if (ioctl(fd, XBMD_IOC_RD_CFG_REG, &regValue) < 0) {
        fatal_error("IOCTL failed reading Link control");
    } else {
        config->linkControl = regValue & 16;
        config->linkSpeed = (regValue >> 16) & 0xF;
        config->linkWidth = (regValue >> 20) & 0x3F;
    }
}

int main(int argc, char **argv)
{
    int fd = open("/dev/xbmd", O_RDWR);
    if (fd <= 0) {
        fatal_error("Could not open file /dev/xbmd");
    }
    
    Config config;
    get_capabilities(&config, fd);
    update_config(&config, fd);
    
    u32 *writeBuffer = allocate_array(u32, 1024);
    u32 *readBuffer  = allocate_array(u32, 1024);
    
    for (u32 i = 0; i < 1024; ++i) {
        writeBuffer[i] = (i + 1) * 7;
    }
    
    for (u32 i = 0; i < 1024; ++i) {
        readBuffer[i] = 0xDEADBEAD;
    }
    
    u32 regValue = config.deviceStatContOffset;
    u32 maxPayloadSize = 32;
    u32 tlpSizeMax = 0; 
    
    if (ioctl(fd, XBMD_IOC_RD_CFG_REG, &regValue) < 0) {
        fatal_error("IOCTL failed reading 0x%03X", XBMD_IOC_RD_CFG_REG);
    } else {
        maxPayloadSize = (regValue & 0x000000E0) >> 5;
        output("Max payload size: %d", maxPayloadSize);
    }
    
    switch (maxPayloadSize) {
        // NOTE(michiel): 128 maxPayloadSize;
        case 0: { tlpSizeMax = 5; maxPayloadSize = 128; } break;
        // NOTE(michiel): 256 maxPayloadSize;
        case 1: { tlpSizeMax = 6; maxPayloadSize = 256; } break;
        // NOTE(michiel): 512 maxPayloadSize;
        case 2: { tlpSizeMax = 7; maxPayloadSize = 512; } break;
        // NOTE(michiel): 1024 maxPayloadSize;
        case 3: { tlpSizeMax = 8; maxPayloadSize = 1024; } break;
        // NOTE(michiel): 2048 maxPayloadSize;
        case 4: { tlpSizeMax = 9; maxPayloadSize = 2048; } break;
        // NOTE(michiel): 4096 maxPayloadSize;
        case 5: { tlpSizeMax = 10; maxPayloadSize = 4096; } break;
        default: { fatal_error("Max payload size is invalid"); } break;
    }
    
    output("Max payload: %d, tlp max size: %d", maxPayloadSize, tlpSizeMax);
    
    u32 writeTLPSize = 32;
    u32 writeTLPCount = 32;
    u32 readTLPSize = 32;
    u32 readTLPCount = 32;
    
    // NOTE(michiel): Copy data to kernel
    write_data(fd, 1024 * sizeof(*writeBuffer), writeBuffer);
    output("Data copied to kernel");
    
    // NOTE(michiel): Setup DMA
    if (ioctl(fd, XBMD_IOC_RESET, 0) < 0) {
        fatal_error("IOCTL failed setting reset");
    } else {
        output("DMA Reset");
    }
    
    u32 dmaControlReg = 0;
    if (ioctl(fd, XBMD_IOC_READ_DMA_CTRL, &dmaControlReg) < 0) {
        fatal_error("IOCTL failed reading DMA Control");
    } else {
        output("DMA Control: 0x%08X", dmaControlReg);
    }
    
    if (ioctl(fd, XBMD_IOC_WRITE_WR_COUNT, writeTLPCount) < 0) {
        fatal_error("IOCTL failed setting the write TLP count");
    }
    
    if (ioctl(fd, XBMD_IOC_WRITE_WR_LEN, writeTLPSize) < 0) {
        fatal_error("IOCTL failed setting the write TLP size");
    }
    
    if (ioctl(fd, XBMD_IOC_WRITE_RD_COUNT, readTLPCount) < 0) {
        fatal_error("IOCTL failed setting the read TLP count");
    }
    
    if (ioctl(fd, XBMD_IOC_WRITE_RD_LEN, readTLPSize) < 0) {
        fatal_error("IOCTL failed setting the read TLP size");
    }
    
    if (ioctl(fd, XBMD_IOC_READ_WR_COUNT, &regValue) < 0) {
        fatal_error("IOCTL failed reading the write TLP count");
    } else {
        output("Write TLP count: %u, expected %u", regValue, writeTLPCount);
    }
    
    if (ioctl(fd, XBMD_IOC_READ_WR_LEN, &regValue) < 0) {
        fatal_error("IOCTL failed reading the write TLP size");
    } else {
        output("Write TLP size: %u, expected %u", regValue, writeTLPSize);
    }
    
    if (ioctl(fd, XBMD_IOC_READ_RD_COUNT, &regValue) < 0) {
        fatal_error("IOCTL failed reading the read TLP count");
    } else {
        output("Read TLP count: %u, expected %u", regValue, readTLPCount);
    }
    
    if (ioctl(fd, XBMD_IOC_READ_RD_LEN, &regValue) < 0) {
        fatal_error("IOCTL failed reading the read TLP size");
    } else {
        output("Read TLP size: %u, expected %u", regValue, readTLPSize);
    }
    
    u32 writeWRRCount = 0; // 1;
    u32 readWRRCount = 0; //1;
    // 0x01010000 or 0x01010020
    u32 miscControl = (writeWRRCount << 24) | (readWRRCount << 16);
    
    if (ioctl(fd, XBMD_IOC_WRITE_MISC_CTL, miscControl) < 0) {
        fatal_error("IOCTL failed setting misc control");
    }
    
    u32 readEnable = 1;
    u32 writeEnable = 0;
    dmaControlReg |= (readEnable << 16) | writeEnable;
    if (ioctl(fd, XBMD_IOC_WRITE_DMA_CTRL, dmaControlReg) < 0) {
        fatal_error("IOCTL failed setting DMA control");
    }
    
    usleep(3000000);
    
    u32 dmaControlWrite = 0;
    u32 dmaControlRead = 0;
    
    if (ioctl(fd, XBMD_IOC_READ_DMA_CTRL, &regValue) < 0) {
        fatal_error("IOCTL failed reading DMA control");
    } else {
        dmaControlReg = regValue;
        dmaControlWrite = dmaControlReg & 0x0000FFFF;
        dmaControlRead = (dmaControlReg & 0xFFFF0000) >> 16;
    }
    
    read_data(fd, writeTLPSize * writeTLPCount * sizeof(u32), readBuffer);
    output("Data copied from kernel");
    
    // NOTE(michiel): Read error check
    if (readEnable) {
        if ((dmaControlRead & 0x1111) != 0x0101) {
            output("DMA Read did not complete succesfully, 0x%04X",
                   dmaControlRead);
        } else {
            output("DMA Read success!");
        }
    }
    
    // NOTE(michiel): Write error check
    if (writeEnable) {
        b32 writeError = false;
        for (u32 i = 0; i < (writeTLPSize * writeTLPCount); ++i) {
            u32 readData = readBuffer[i];
            u32 writeData = writeBuffer[i];
            if (readData != writeData) {
                output("Mismatch: wrote %d, got %d", writeData, readData);
                writeError = true;
            }
        }
        
        if (!writeError) {
            if ((dmaControlWrite & 0x1111) != 0x0101) {
                output("DMA Write did not complete succesfully, 0x%04X",
                       dmaControlWrite);
            } else {
                output("DMA Write success!");
            }
        }
    }
    
    regValue = config.deviceStatContOffset;
    if (ioctl(fd, XBMD_IOC_RD_CFG_REG, &regValue) < 0) {
        fatal_error("Device status read failed");
    } else {
        if ((regValue & 0x00040000) == 0x00040000) {
            fatal_error("Fatal reported by device");
        }
        if ((regValue & 0x00020000) == 0x00020000) {
            output("Non fatal reported by device");
        }
        if ((regValue & 0x00010000) == 0x00010000) {
            output("Correctable reported by device");
        }
    }
    
    s32 linkWidthMultiplier = 0;
    s32 trnClks = 0;
    s32 tempWrMbps = 0;
    s32 tempRdMbps = 0;
    
    char *gen = 0;
    switch (config.linkSpeed)
    {
        case 1: {
            gen = "Generation 1";
            switch (config.linkWidth)
            {
                case 1: { linkWidthMultiplier = 31; } break;
                case 2: { linkWidthMultiplier = 62; } break;
                case 3: { linkWidthMultiplier = 125; } break;
                case 4: { linkWidthMultiplier = 250; } break;
                default: { fatal_error("%s: Link width is not valid", gen); } break;
        }
        } break;
        
        case 2: {
            gen = "Generation 2";
            switch (config.linkWidth)
            {
                case 1: { linkWidthMultiplier = 62; } break;
                case 2: { linkWidthMultiplier = 125; } break;
                case 3: { linkWidthMultiplier = 250; } break;
                case 4: { linkWidthMultiplier = 500; } break;
                default: { fatal_error("%s: Link width is not valid", gen); } break;
            }
        } break;
        
        default: {
            fatal_error("Link speed is not valid");
        } break;
    }
    
    if (writeEnable) {
        if (ioctl(fd, XBMD_IOC_READ_WR_PERF, &trnClks) < 0) {
            fatal_error("IOCTL failed reading write performance");
        }
        
        tempWrMbps = (writeTLPSize * 4 * writeTLPCount * linkWidthMultiplier) / trnClks;
        output("DMA Write TRN Clocks: %d, Perf: %d MB/s", trnClks, tempWrMbps);
    }
    
    if (readEnable) {
        if (ioctl(fd, XBMD_IOC_READ_RD_PERF, &trnClks) < 0) {
            fatal_error("IOCTL failed reading read performance");
        }
        
        tempRdMbps = (readTLPSize * 4 * readTLPCount * linkWidthMultiplier) / trnClks;
        output("DMA Read TRN Clocks: %d, Perf: %d MB/s", trnClks, tempRdMbps);
    }
    
    close(fd);
    
    return 0;
}
