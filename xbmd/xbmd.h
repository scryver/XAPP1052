// (c) Copyright 2009  2009 Xilinx, Inc. All rights reserved.
//
// This file contains confidential and proprietary information
// of Xilinx, Inc. and is protected under U.S. and
// international copyright and other intellectual property
// laws.
//
// DISCLAIMER
// This disclaimer is not a license and does not grant any
// rights to the materials distributed herewith. Except as
// otherwise provided in a valid license issued to you by
// Xilinx, and to the maximum extent permitted by applicable
// law: (1) THESE MATERIALS ARE MADE AVAILABLE "AS IS" AND
// WITH ALL FAULTS, AND XILINX HEREBY DISCLAIMS ALL WARRANTIES
// AND CONDITIONS, EXPRESS, IMPLIED, OR STATUTORY, INCLUDING
// BUT NOT LIMITED TO WARRANTIES OF MERCHANTABILITY, NON-
// INFRINGEMENT, OR FITNESS FOR ANY PARTICULAR PURPOSE; and
// (2) Xilinx shall not be liable (whether in contract or tort,
// including negligence, or under any other theory of
// liability) for any loss or damage of any kind or nature
// related to, arising under or in connection with these
// materials, including for any direct, or any indirect,
// special, incidental, or consequential loss or damage
// (including loss of data, profits, goodwill, or any type of
// loss or damage suffered as a result of any action brought
// by a third party) even if such damage or loss was
// reasonably foreseeable or Xilinx had been advised of the
// possibility of the same.
//
// CRITICAL APPLICATIONS
// Xilinx products are not designed or intended to be fail-
// safe, or for use in any application requiring fail-safe
// performance, such as life-support or safety devices or
// systems, Class III medical devices, nuclear facilities,
// applications related to the deployment of airbags, or any
// other applications that could lead to death, personal
// injury, or severe property or environmental damage
// (individually and collectively, "Critical
// Applications"). Customer assumes the sole risk and
// liability of any use of Xilinx products in Critical
// Applications, subject only to applicable laws and
// regulations governing limitations on product liability.
//
// THIS COPYRIGHT NOTICE AND DISCLAIMER MUST BE RETAINED AS
// PART OF THIS FILE AT ALL TIMES.

//--------------------------------------------------------------------------------
//-- Filename: xbmd.h
//--
//-- Description: Main header file for kernel driver
//--              
//-- XBMD is an example Red Hat device driver which exercises XBMD design
//-- Device driver has been tested on Red Hat Fedora FC9 2.6.15.
//--------------------------------------------------------------------------------
#include <linux/ioctl.h>
#include <linux/types.h>

#define KILOBYTES(x)  ((x) * 1024L)
#define MEGABYTES(x)  (KILOBYTES(x) * 1024L)

#define SUCCESS    0
#define CRIT_ERR  -1

#define VERBOSE    1

#define DMA_BUF_SIZE   MEGABYTES(4)

typedef enum XbmDmaControlReg
{
    Reg_DeviceCS = 0, // NOTE(michiel): Control Status
    Reg_DeviceDMACS,  // NOTE(michiel): DMA Control Status
    Reg_WriteTlpAddress,
    Reg_WriteTlpSize,
    Reg_WriteTlpCount,
    Reg_WriteTlpPattern,
    Reg_ReadTlpPattern,
    Reg_ReadTlpAddress,
    Reg_ReadTlpSize,
    Reg_ReadTlpCount,
    Reg_WriteDMAPerf,
    Reg_ReadDMAPerf,
    Reg_ReadComplStatus,
    Reg_ComplWithData,
    Reg_ComplSize,
    Reg_DeviceLinkWidth,
    Reg_DeviceLinkTlpSize,
    Reg_DeviceMiscControl,
    Reg_DeviceMSIControl,
    Reg_DeviceDirectedLinkChange,
    Reg_DeviceFCControl,
    Reg_DeviceFCPostedInfo,
    Reg_DeviceFCNonPostedInfo,
    Reg_DeviceFCCompletionInfo,
    
} XbmDmaControlReg;

#define XBMD_IOC_MAGIC          '!'

#define XBMD_IOC_INITCARD       _IO(XBMD_IOC_MAGIC, 0)
#define XBMD_IOC_RESET          _IO(XBMD_IOC_MAGIC, 1)
#define XBMD_IOC_DISP_REGS      _IO(XBMD_IOC_MAGIC, 2)
#define XBMD_IOC_READ_CTRL      _IOR(XBMD_IOC_MAGIC, 3, u32 *)
#define XBMD_IOC_READ_DMA_CTRL  _IOR(XBMD_IOC_MAGIC, 4, u32 *)
#define XBMD_IOC_READ_WR_ADDR   _IOR(XBMD_IOC_MAGIC, 5, u32 *)
#define XBMD_IOC_READ_WR_LEN    _IOR(XBMD_IOC_MAGIC, 6, u32 *)
#define XBMD_IOC_READ_WR_COUNT  _IOR(XBMD_IOC_MAGIC, 7, u32 *)
#define XBMD_IOC_READ_WR_PTRN   _IOR(XBMD_IOC_MAGIC, 8, u32 *)
#define XBMD_IOC_READ_RD_PTRN   _IOR(XBMD_IOC_MAGIC, 9, u32 *)
#define XBMD_IOC_READ_RD_ADDR   _IOR(XBMD_IOC_MAGIC, 10, u32 *)
#define XBMD_IOC_READ_RD_LEN    _IOR(XBMD_IOC_MAGIC, 11, u32 *)
#define XBMD_IOC_READ_RD_COUNT  _IOR(XBMD_IOC_MAGIC, 12, u32 *)
#define XBMD_IOC_READ_WR_PERF   _IOR(XBMD_IOC_MAGIC, 13, u32 *)
#define XBMD_IOC_READ_RD_PERF   _IOR(XBMD_IOC_MAGIC, 14, u32 *)
#define XBMD_IOC_READ_CMPL      _IOR(XBMD_IOC_MAGIC, 15, u32 *)
#define XBMD_IOC_READ_CWDATA    _IOR(XBMD_IOC_MAGIC, 16, u32 *)
#define XBMD_IOC_READ_CSIZE     _IOR(XBMD_IOC_MAGIC, 17, u32 *)
#define XBMD_IOC_READ_LINKWDTH  _IOR(XBMD_IOC_MAGIC, 18, u32 *)
#define XBMD_IOC_READ_LINKLEN   _IOR(XBMD_IOC_MAGIC, 19, u32 *)
#define XBMD_IOC_READ_MISC_CTL  _IOR(XBMD_IOC_MAGIC, 20, u32 *)
#define XBMD_IOC_READ_INTRPT    _IOR(XBMD_IOC_MAGIC, 21, u32 *)
#define XBMD_IOC_READ_DIR_LINK  _IOR(XBMD_IOC_MAGIC, 22, u32 *)
#define XBMD_IOC_READ_FC_CTRL   _IOR(XBMD_IOC_MAGIC, 23, u32 *)
#define XBMD_IOC_READ_FC_POST   _IOR(XBMD_IOC_MAGIC, 24, u32 *)
#define XBMD_IOC_READ_FC_NPOST  _IOR(XBMD_IOC_MAGIC, 25, u32 *)
#define XBMD_IOC_READ_FC_CMPL   _IOR(XBMD_IOC_MAGIC, 26, u32 *)
#define XBMD_IOC_WRITE_DMA_CTRL _IOW(XBMD_IOC_MAGIC, 27, u32)
#define XBMD_IOC_WRITE_WR_LEN   _IOW(XBMD_IOC_MAGIC, 28, u32)
#define XBMD_IOC_WRITE_WR_COUNT _IOW(XBMD_IOC_MAGIC, 29, u32)
#define XBMD_IOC_WRITE_WR_PTRN  _IOW(XBMD_IOC_MAGIC, 30, u32)
#define XBMD_IOC_WRITE_RD_LEN   _IOW(XBMD_IOC_MAGIC, 31, u32)
#define XBMD_IOC_WRITE_RD_COUNT _IOW(XBMD_IOC_MAGIC, 32, u32)
#define XBMD_IOC_WRITE_RD_PTRN  _IOW(XBMD_IOC_MAGIC, 33, u32)
#define XBMD_IOC_WRITE_MISC_CTL _IOW(XBMD_IOC_MAGIC, 34, u32)
#define XBMD_IOC_WRITE_DIR_LINK _IOW(XBMD_IOC_MAGIC, 35, u32)
#define XBMD_IOC_RD_BMD_REG     _IOWR(XBMD_IOC_MAGIC, 36, u32 *)
#define XBMD_IOC_RD_CFG_REG     _IOWR(XBMD_IOC_MAGIC, 37, u32 *)
#define XBMD_IOC_WR_BMD_REG     _IOW(XBMD_IOC_MAGIC, 38, u64)
#define XBMD_IOC_WR_CFG_REG     _IOW(XBMD_IOC_MAGIC, 39, u64)

