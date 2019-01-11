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
// Define Result values
#define SUCCESS                    0
#define CRIT_ERR                  -1

// Debug - define will output more info
#define Verbose 1

// Max DMA Buffer Size
#define BUF_SIZE                  (4096 * 1024)

enum {

  INITCARD,     // NOTE(michiel): Init dma control registers
  INITRST,      // NOTE(michiel): Reset dma control
  DISPREGS,     // NOTE(michiel): Unused
  RDDCSR,       // NOTE(michiel): Read the Device Control Status Register
    RDDDMACR,     // NOTE(michiel): Read the DMA Control Status Register
    RDWDMATLPA,   // NOTE(michiel): Read the write DMA TLP Address
  RDWDMATLPS,   // NOTE(michiel): Read the write DMA TLP Size
  RDWDMATLPC,   // NOTE(michiel): Read the write DMA TLP Count
  RDWDMATLPP,   // NOTE(michiel): Read the write DMA TLP Pattern
  RDRDMATLPP,   // NOTE(michiel): Read the read DMA TLP Pattern
  RDRDMATLPA,   // NOTE(michiel): Read the read DMA TLP Address
  RDRDMATLPS,   // NOTE(michiel): Read the read DMA TLP Size
  RDRDMATLPC,   // NOTE(michiel): Read the read DMA TLP Count
  RDWDMAPERF,   // NOTE(michiel): Read the write DMA Performance
  RDRDMAPERF,   // NOTE(michiel): Read the read DMA Performance
  RDRDMASTAT,   // NOTE(michiel): Read the read DMA Status
  RDNRDCOMP,    // NOTE(michiel): Read number of read completion with Data
  RDRCOMPDSIZE, // NOTE(michiel): Read the read completion size
  RDDLWSTAT,    // NOTE(michiel): Read device link width status
  RDDLTRSSTAT,  // NOTE(michiel): Read device link transaction size status
  RDDMISCCONT,  // NOTE(michiel): Read device miscellaneous control
  RDDMISCONT,   // NOTE(michiel): Read device MSI control
  RDDLNKC,      // NOTE(michiel): Read device directed link change
  DFCCTL,       // NOTE(michiel): Read device flow control control
  DFCPINFO,     // NOTE(michiel): Read device flow control posted info
  DFCNPINFO,    // NOTE(michiel): Read device flow control non-posted info
  DFCINFO,      // NOTE(michiel): Read device flow control completion info

  RDCFGREG,     // NOTE(michiel): Read config register
  WRCFGREG,     // NOTE(michiel): Write config register
  RDBMDREG,     // NOTE(michiel): Read BMD register
  WRBMDREG,     // NOTE(michiel): Write BMD register

  WRDDMACR,     // NOTE(michiel): Write DMA control status register
  WRWDMATLPS,   // NOTE(michiel): Write the write DMA TLP size
  WRWDMATLPC,   // NOTE(michiel): Write the write DMA TLP count
  WRWDMATLPP,   // NOTE(michiel): Write the write DMA TLP pattern
  WRRDMATLPS,   // NOTE(michiel): Write the read DMA TLP size
  WRRDMATLPC,   // NOTE(michiel): Write the read DMA TLP count
  WRRDMATLPP,   // NOTE(michiel): Write the read DMA TLP pattern
  WRDMISCCONT,  // NOTE(michiel): Write device miscellaneous control
  WRDDLNKC,     // NOTE(michiel): Write device directed link change

  NUMCOMMANDS

};

