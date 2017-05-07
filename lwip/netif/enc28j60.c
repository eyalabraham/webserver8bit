/**
 * @file
 * Ethernet Interface driver for Microchip ENC28J60
 *
 */

/*
 * Copyright (c) 2001-2004 Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 *
 * Author: Adam Dunkels <adam@sics.se>
 *
 */

/*
 * This is a Ethernet Interface driver for Microchip ENC28J60.
 * If follows the basic skeleton driver staructure and implements
 * driver support for my setup
 *
 * Written by Eyal Abraham, March 2017
 *
 */

//#define     DRV_DEBUG_FUNC_EXIT
//#define     DRV_DEBUG_FUNC_NAME
#define     DRV_DEBUG_FUNC_PARAM

#if defined DRV_DEBUG_FUNC_EXIT || defined DRV_DEBUG_FUNC_NAME || defined DRV_DEBUG_FUNC_PARAM
#include    <stdio.h>
#endif

#include    <assert.h>
#include    <string.h>

#include    "lwip/opt.h"
#include    "lwip/def.h"
#include    "lwip/mem.h"
#include    "lwip/pbuf.h"
#include    "lwip/stats.h"
#include    "lwip/snmp.h"
#include    "lwip/etharp.h"

#include    "netif/enc28j60.h"
#include    "netif/enc28j60-hw.h"

#include    "ppispi.h"

/* -----------------------------------------
   types
----------------------------------------- */
#define     IFNAME0     'e'                     // interface's two-character name
#define     IFNAME1     '0'

/* -----------------------------------------
   types
----------------------------------------- */
/*
 * struct to hold private data used to operate the ethernet interface.
 * keeping the ethernet address of the MAC in this struct is not necessary
 * as it is already kept in the struct netif, but left it here from the example.
 *
 */
struct rxStat_t
{
    u8_t     nextPacketL;
    u8_t     nextPacketH;
    u16_t    rxByteCount;
    u8_t     rxStatus1;
    u8_t     rxStatus2;
};

struct txStat_t
{
    u16_t   txByteCount;
    u8_t    txStatus1;
    u8_t    txStatus2;
    u16_t   txTotalXmtCount;
    u8_t    txStatus3;
};

struct enc28j60_t
{
    union
    {
        u8_t   rxStatusVector[sizeof(struct rxStat_t)];
        struct rxStat_t rxStatVector;
    };
    union
    {
        u8_t   txStatusVector[sizeof(struct txStat_t)];
        struct txStat_t txStatVector;
    };
    struct eth_addr ethaddr;                            // MAC address, hard coded to what I need...
    u16_t           phyID1;                             // device ID
    u16_t           phyID2;
    u8_t            revID;                              // silicone revision ID
};

/* -----------------------------------------
   static function prototype
----------------------------------------- */

// ENC28J60 device register access functions
static int    setRegisterBank(regBank_t);
static int    readControlRegister(ctrlReg_t, u8_t*);
static int    writeControlRegister(ctrlReg_t, u8_t);
static int    readPhyRegister(phyReg_t, u16_t*);
static int    writePhyRegister(phyReg_t, u16_t);
static int    controlBit(ctrlReg_t, u8_t);
static int    setControlBit(ctrlReg_t, u8_t);
static int    clearControlBit(ctrlReg_t, u8_t);

// ENC28J60 device and memory access with block DMA
static err_t  enc28j60Init(struct enc28j60_t*);
static void   spiCallBack(void);
static int    readMemBuffer(u8_t*, u16_t, u16_t);
static int    writeMemBuffer(u8_t*, u16_t, u16_t);

// ENC28J60 protocol functionality
static void   ethReset(void);
static int    packetWaiting(void);
static void   extractPacketInfo(struct enc28j60_t*);

// LwIP stack functions
static err_t  low_level_init(struct netif*);
static err_t  low_level_output(struct netif*, struct pbuf*);
static struct pbuf* low_level_input(struct netif*);

/* -----------------------------------------
   driver globals
----------------------------------------- */
volatile int dmaComplete;                       // DMA block IO completion flag
u8_t         tempPacketBuffer[PACKET_BUFFER];   // temporary packet buffer @@ static allocation, should I opt for dynamic?

/* -----------------------------------------
 * setRegisterBank()
 *
 *  set/select an enc28j60 register bank through ECON1 register
 *
 *  return 0 if no error, otherwise return SPI error level
 *
 *  decided to ignore spiWrite error, and only report
 *  spiRead errors, mainly time-out
 *
 * ----------------------------------------- */
static int setRegisterBank(regBank_t bank)
{
    u8_t    econ1Reg;
    int     result = SPI_BUSY;

    if ( bank == BANK0 || bank == BANK1 || bank == BANK2 || bank == BANK3 )
    {
        spiWriteByteKeepCS(ETHERNET_WR, (OP_RCR | ECON1));  // read ECON1
        if ( (result = spiReadByte(ETHERNET_RD, &econ1Reg)) != SPI_OK )
            goto ABORT_BANKSEL;
        econ1Reg &= 0xfc;                                   // set bank number bits [BSEL1 BSEL0]
        econ1Reg |= (((u8_t)bank) >> 5);
        spiWriteByteKeepCS(ETHERNET_WR, (OP_WCR | ECON1));  // write ECON1
        spiWriteByte(ETHERNET_WR, econ1Reg);
    }

ABORT_BANKSEL:
#ifdef DRV_DEBUG_FUNC_EXIT
    printf("setRegisterBank(%d) %d\n", bank, result);
#endif
    return result;                                          // bad bank number parameter passed in
}

/* -----------------------------------------
 * readControlRegister()
 *
 *  read an enc28j60 control register
 *  read any of the ETH, MAC and MII registers in any order
 *  the function will take care of switching register banks and
 *  of ETH vs. MAC/MII register reads that require an extra dummy byte read
 *
 *  return 0 if no error, otherwise return SPI error level
 *
 *  decided to ignore spiWrite error, and only report
 *  spiRead errors, mainly time-out
 *
 * ----------------------------------------- */
static int readControlRegister(ctrlReg_t reg, u8_t* byte)
{
    u8_t    bank;
    u8_t    regId;
    u8_t    regValue;
    int     result = SPI_RD_ERR;

    bank = (reg & 0x60);                                // isolate bank number
    regId = (reg & 0x1f);                               // isolate register number

    if ( (result = setRegisterBank(bank)) != SPI_OK)    // select bank
        goto ABORT_RCR;

    spiWriteByteKeepCS(ETHERNET_WR, (OP_RCR | regId));  // issue a control register read command

    if ( reg & MACMII )                                 // is this a MAC or MII register?
    {                                                   // yes, then issue an extra dummy read
        if ( (result = spiReadByteKeepCS(ETHERNET_RD, &regValue)) != SPI_OK)
            goto ABORT_RCR;
    }

    if ( (result = spiReadByte(ETHERNET_RD, &regValue)) != SPI_OK) //issue actual read for the byte
        goto ABORT_RCR;

    *byte = regValue;

ABORT_RCR:
#ifdef DRV_DEBUG_FUNC_EXIT
    printf("readControlRegister(0x%02x,0x%02x) %d\n", reg, *byte, result);
#endif
    return result;
}

/* -----------------------------------------
 * writeControlRegister()
 *
 *  write to an enc28j60 control register
 *
 *  return 0 if no error, otherwise return SPI error level
 *
 *  decided to ignore spiWrite error, and only report
 *  spiRead errors, mainly time-out
 *
 * ----------------------------------------- */
static int writeControlRegister(ctrlReg_t reg, u8_t byte)
{
    u8_t    bank;
    u8_t    regId;
    int     result = SPI_WR_ERR;

    bank = (reg & 0x60);                                // isolate bank number
    regId = (reg & 0x1f);                               // isolate register number

    if ( (result = setRegisterBank(bank)) != SPI_OK)    // select bank
        goto ABORT_WCR;

    spiWriteByteKeepCS(ETHERNET_WR, (OP_WCR | regId));  // issue a control register write command
    spiWriteByte(ETHERNET_WR, byte);                    // and send the data byte

ABORT_WCR:
#ifdef DRV_DEBUG_FUNC_EXIT
    printf("writeControlRegister(0x%02x,0x%02x) %d\n", reg, byte, result);
#endif
    return result;
}

/* -----------------------------------------
 * readPhyRegister()
 *
 *  read from an enc28j60 PHY register:
 *  1. Write the address of the PHY register to read
 *     from into the MIREGADR register.
 *  2. Set the MICMD.MIIRD bit. The read operation
 *     begins and the MISTAT.BUSY bit is set.
 *  3. Wait 10.24 μs. Poll the MISTAT.BUSY bit to be
 *     certain that the operation is complete. While
 *     busy, the host controller should not start any
 *     MIISCAN operations or write to the MIWRH
 *     register.
 *     When the MAC has obtained the register
 *     contents, the BUSY bit will clear itself.
 *  4. Clear the MICMD.MIIRD bit.
 *  5. Read the desired data from the MIRDL and
 *     MIRDH registers. The order that these bytes are
 *     accessed is unimportant.
 *
 *  return 0 if no error, otherwise return SPI error level
 *
 *  decided to ignore spiWrite error, and only report
 *  spiRead errors, mainly time-out
 *
 * ----------------------------------------- */
static int readPhyRegister(phyReg_t reg, u16_t* word)
{
    u8_t    byte;
    int     result = SPI_RD_ERR;

    if ( (result = writeControlRegister(MIREGADR, reg)) != SPI_OK)  // set PHY register address
        goto ABORT_PHYRD;

    setControlBit(MICMD, MICMD_MIIRD);              // start the read operation
    while ( controlBit(MISTAT, MISTAT_BUSY) ) {};   // wait for read operation to complete @@ timeouts?
    clearControlBit(MICMD, MICMD_MIIRD);            // clear read request
    readControlRegister(MIRDL, &byte);              // read low byte
    *word = byte;
    readControlRegister(MIRDH, &byte);              // read high byte
    *word += ((u16_t) byte) << 8;

ABORT_PHYRD:
#ifdef DRV_DEBUG_FUNC_EXIT
    printf("readPhyRegister(0x%02x,0x%04x) %d\n", reg, *word, result);
#endif
    return result;
}

/* -----------------------------------------
 * writePhyRegister()
 *
 *  write to an enc28j60 PHY register
 *  1. Write the address of the PHY register to write to
 *     into the MIREGADR register.
 *  2. Write the lower 8 bits of data to write into the
 *     MIWRL register.
 *  3. Write the upper 8 bits of data to write into the
 *     MIWRH register. Writing to this register automatically
 *     begins the MII transaction, so it must
 *     be written to after MIWRL. The MISTAT.BUSY
 *     bit becomes set.
 *
 *  return 0 if no error, otherwise return SPI error level
 *
 *  decided to ignore spiWrite error, and only report
 *  spiRead errors, mainly time-out
 *
 * ----------------------------------------- */
static int writePhyRegister(phyReg_t reg, u16_t word)
{
    int     result = SPI_WR_ERR;

    if ( (result = writeControlRegister(MIREGADR, reg)) != SPI_OK)  // set PHY register address
        goto ABORT_PHYWR;

    writeControlRegister(MIRDL, (u8_t) word);
    writeControlRegister(MIRDH, (u8_t) (word >> 8));
    while ( controlBit(MISTAT, MISTAT_BUSY) ) {};   // wait for write operation to complete @@ timeouts?

ABORT_PHYWR:
#ifdef DRV_DEBUG_FUNC_EXIT
    printf("writePhyRegister(0x%02x,0x%04x) %d\n", reg, word, result);
#endif
    return result;
}

/* -----------------------------------------
 * controlBit()
 *
 *  read and test control register bit(s)
 *  the control register value is tested using a
 *  bitwise AND with 'position'.
 *
 * ----------------------------------------- */
static int controlBit(ctrlReg_t reg, u8_t position)
{
    u8_t    value;
    int     result = 0;                                 // which is also 'SPI_BUSY'...

    if ( readControlRegister(reg, &value) == SPI_OK)    // get register value
        result = ((value & position) ? 1 : 0);

#ifdef DRV_DEBUG_FUNC_EXIT
    printf("controlBit(0x%02x,0x%02x) %d\n", reg, position, result);
#endif
    return result;
}

/* -----------------------------------------
 * setControlBit()
 *
 *  set a control register bit position
 *  -- register OR 'position'
 *
 * ----------------------------------------- */
static int setControlBit(ctrlReg_t reg, u8_t position)
{
    u8_t    bank;
    u8_t    regId;
    u8_t    regValue;
    int     result = -1;                                    // which is also 'SPI_BUSY'...

    if ( reg & MACMII )                                     // is this a MAC or MII register?
    {                                                       // yes, can't use BFS command on MAC or MII, but...
        result = readControlRegister(reg, &regValue);       // can read register,
        regValue |= position;                               // modify
        result = writeControlRegister(reg, regValue);       // and write back
    }
    else
    {
        bank = (reg & 0x60);                                // isolate bank number
        regId = (reg & 0x1f);                               // isolate register number

        if ( (result = setRegisterBank(bank)) != SPI_OK)    // select bank
            goto ABORT_BFS;

        spiWriteByteKeepCS(ETHERNET_WR, (OP_BFS | regId));  // issue a control register write command
        spiWriteByte(ETHERNET_WR, position);                // and send the data byte
    }

ABORT_BFS:
#ifdef DRV_DEBUG_FUNC_EXIT
    printf("setControlBit(0x%02x,0x%02x) %d\n", reg, position, result);
#endif
    return result;
}

/* -----------------------------------------
 * clearControlBit()
 *
 *  clear a control register bit position
 *  -- register AND (NOT 'position')
 *
 * ----------------------------------------- */
static int clearControlBit(ctrlReg_t reg, u8_t position)
{
    u8_t    bank;
    u8_t    regId;
    u8_t    regValue;
    int     result = -1;                                    // which is also 'SPI_BUSY'...

    if ( reg & MACMII )                                     // is this a MAC or MII register?
    {                                                       // yes, can't use BFC command on MAC or MII, but...
        result = readControlRegister(reg, &regValue);       // can read register,
        regValue &= ~position;                              // modify
        result = writeControlRegister(reg, regValue);       // and write back
    }
    else
    {
        bank = (reg & 0x60);                                // isolate bank number
        regId = (reg & 0x1f);                               // isolate register number

        if ( (result = setRegisterBank(bank)) != SPI_OK)    // select bank
            goto ABORT_BFC;

        spiWriteByteKeepCS(ETHERNET_WR, (OP_BFC | regId));  // issue a control register write command
        spiWriteByte(ETHERNET_WR, position);                // and send the data byte
    }

ABORT_BFC:
#ifdef DRV_DEBUG_FUNC_EXIT
    printf("clearControlBit(0x%02x,0x%02x) %d\n", reg, position, result);
#endif
    return result;
}

/*------------------------------------------------
 * spiCallBack()
 *
 *  SPI block transfer DMA completion callback
 *
 */
static void spiCallBack(void)
{
    dmaComplete = 1;

#ifdef DRV_DEBUG_FUNC_PARAM
    printf("%s()\n dmaComplete=%d\n", __func__, dmaComplete);
#endif
#ifdef DRV_DEBUG_FUNC_EXIT
    printf("spiCallBack()\n");
#endif
}

/* -----------------------------------------
 * readMemBuffer()
 *
 *  read 'length' bytes from ENC28J60 memory
 *  starting at 'address' into destination location 'dest'
 *  use DMA or single byte access.
 *  always assume internal read pointer ERDPT is auto increment
 *  function does not range check 'address' parameter
 *  return 0 if no error, otherwise return SPI error level
 *
 * ----------------------------------------- */
static int readMemBuffer(u8_t *dest, u16_t address, u16_t length)
{
    int     result = SPI_RD_ERR;
    int     i;

#ifdef DRV_DEBUG_FUNC_PARAM
    printf("%s()\n len=%u address=0x%04x\n", __func__, length, address);
#endif

    if ( length == 0 )
        return SPI_RD_ERR;

    if ( address != USE_CURR_ADD )                  // change address pointer or use default
    {
        writeControlRegister(ERDPTL, LOW_BYTE(address));
        writeControlRegister(ERDPTH, HIGH_BYTE(address));
    }

    if ( length == 1 )
    {                                               // read a single byte
        spiWriteByteKeepCS(ETHERNET_WR, OP_RBM);    // issue read memory command
        result = spiReadByte(ETHERNET_RD, dest);    // read a byte and release CS
    }
    else
    {                                               // use DMA to read multiple bytes
        dmaComplete = 0;
        spiWriteByteKeepCS(ETHERNET_WR, OP_RBM);    // issue read memory command
/*
        result = spiReadBlock(ETHERNET_RD, dest, length, spiCallBack); // start DMA transfer
        printf(" result = %d\n", result);
        if ( result != SPI_OK )
            goto ABORT_RBM;
        while ( !dmaComplete ) {};                  // wait for DMA transfer to complete
*/
        for ( i = 0; i < (length-1); i++)
            spiReadByteKeepCS(ETHERNET_RD, (dest+i));
        result = spiReadByte(ETHERNET_RD, (dest+i));
    }

    return result;
}

/* -----------------------------------------
 * writeMemBuffer()
 *
 *  write 'length' bytes into ENC28J60 memory
 *  starting at 'address' from source location 'src'
 *  use DMA or single byte access.
 *  always assume internal read pointer EWRPT is auto increment
 *  function does not range check 'address' parameter
 *  return 0 if no error, otherwise return SPI error level
 *
 * ----------------------------------------- */
static int writeMemBuffer(u8_t *src, u16_t address, u16_t length)
{
    int     result = SPI_WR_ERR;
    int     i;

#ifdef DRV_DEBUG_FUNC_PARAM
    printf("%s()\n len=%u address=0x%04x\n", __func__, length, address);
#endif

    if ( length == 0 )
        return SPI_WR_ERR;

    if ( address != USE_CURR_ADD )                  // change address pointer or use default
    {
        writeControlRegister(EWRPTL, LOW_BYTE(address));
        writeControlRegister(EWRPTH, HIGH_BYTE(address));
    }

    if ( length == 1 )
    {                                               // write a single byte
        spiWriteByteKeepCS(ETHERNET_WR, OP_WBM);    // issue write memory command
        result = spiWriteByte(ETHERNET_WR, *src);   // write a byte and release CS
    }
    else
    {                                               // use DMA to write multiple bytes
        dmaComplete = 0;
        spiWriteByteKeepCS(ETHERNET_WR, OP_WBM);    // issue write memory command
/*
        result = spiWriteBlock(ETHERNET_WR, src, length, spiCallBack); // start DMA transfer
        printf(" result = %d\n", result);
        if ( result != SPI_OK )
            goto ABORT_WBM;
        while ( !dmaComplete ) {};                  // wait for DMA transfer to complete
*/
        for ( i = 0; i < (length-1); i++)
            spiWriteByteKeepCS(ETHERNET_WR, *(src+i));
        result = spiWriteByte(ETHERNET_WR, *(src+i));
    }

    return result;

}

/* -----------------------------------------
 * ethLinkUp()
 *
 *  returns the link status
 *  by reading LLSTAT in PHSTAT1
 *  ( @@ what is the effect of using LSTAT in PHSTAT2? )
 *
 * ----------------------------------------- */
int ethLinkUp(void)
{
    u16_t   phyStatus;
    int     linkState;

    readPhyRegister(PHSTAT2, &phyStatus);
    linkState = ((phyStatus & PHSTAT2_LSTAT) ? 1 : 0);

/*
    readPhyRegister(PHSTAT1, &phyStatus);
    linkState = ((phyStatus & PHSTAT1_LLSTAT) ? 1 : 0);
*/

#ifdef DRV_DEBUG_FUNC_EXIT
    printf("ethLinkUp(%d)\n",linkState);
#endif

    return linkState;
}

/* -----------------------------------------
 * ethReset()
 *
 *  software reset of the ENC28J60
 *  after issuing this reset, 50uSec need to elapse before
 *  proceeding to altering or reading PHY registers
 *
 * ----------------------------------------- */
static void ethReset(void)
{
    int     i;

#ifdef DRV_DEBUG_FUNC_NAME
    printf("%s()\n",__func__);
#endif

    spiWriteByte(ETHERNET_WR, OP_SC);           // issue a reset command
    for (i = 0; i < 550; i++);                  // wait ~2mSec because ESTAT.CLKRDY is not reliable (errata #2, DS80349C)
}

/* -----------------------------------------
 * packetWaiting()
 *
 *  return '1' if unread packet(s) waiting in Rx buffer
 *  return '0' if not
 *
 * ----------------------------------------- */
static int packetWaiting(void)
{
    u8_t    packetCount;

    readControlRegister(EPKTCNT, &packetCount); // check packet count waiting in input buffer
    return ( packetCount > 0 ? 1 : 0);
}

/* -----------------------------------------
 * extractPacketInfo()
 *
 *  we've determined that a packet is waiting in the
 *  receiver buffer, this function extracts the packet
 *  status vector into the device/interface private structure,
 *  advances the read pointer, but does not read the packet
 *  data.
 *  the function assumes that the ERDPTH:L pointer is set
 *  to the start of the packet vector, after reading the vector
 *  the ERDPTH:L pointer point to the start of the packet to read
 *
 * ----------------------------------------- */
static void extractPacketInfo(struct enc28j60_t *ethif)
{
    u8_t    i;
    u8_t    temp;

    spiWriteByteKeepCS(ETHERNET_WR, OP_RBM);            // initiate memory read

    for ( i = 0; i < (sizeof(struct rxStat_t)-1); i++ ) // loop to read first bytes of status vector
    {
        spiReadByteKeepCS(ETHERNET_RD, &temp);
        ethif->rxStatusVector[i] = temp;
    }

    spiReadByte(ETHERNET_RD, &temp);                    // read last byte of vector and release CS
    ethif->rxStatusVector[i] = temp;
}

/* -----------------------------------------
 * enc28j60Init()
 *
 * hardware initialization function for the ENC28J60 chip
 * called from low_level_init().
 * function is broken out here for compatibility with the way I tested other HW
 * functions of the project. do not use from within lwIP!
 *
 * param:  pointer to interface private data/state structure
 * return: 0 initialization done and link up, non-0 initialization failed
 *
 * ----------------------------------------- */
static err_t enc28j60Init(struct enc28j60_t *ethState)
{
    u16_t   tmpPhyReg;
    u8_t    perPacketCtrl = PER_PACK_CTRL;
    err_t   result = ERR_IF;                                // signal an interface error

#ifdef DRV_DEBUG_FUNC_NAME
    printf("%s()\n",__func__);
#endif

    ethReset();                                             // issue a reset command

    readPhyRegister(PHID1, &(ethState->phyID1));            // read and store PHY ID and device revision
    readPhyRegister(PHID2, &(ethState->phyID2));
    readControlRegister(EREVID, &(ethState->revID));

#ifdef DRV_DEBUG_FUNC_PARAM
    printf("enc28j60Init()\n PHID1=0x%04x PHID2=0x%04x Rev=0x%02x\n", ethState->phyID1, ethState->phyID2, ethState->revID);
#endif

    assert(ethState->phyID1 == PHY_ID1);                    // assert on ID values!
    assert(ethState->phyID2 == PHY_ID2);

    /* initialize private device structure
     */
    ethState->rxStatVector.nextPacketH = 0;
    ethState->rxStatVector.nextPacketL = 0;
    ethState->rxStatVector.rxByteCount = 0;
    ethState->rxStatVector.rxStatus1 = 0;
    ethState->rxStatVector.rxStatus2 = 0;

    ethState->ethaddr.addr[0] = MAC0;                       // MAC address
    ethState->ethaddr.addr[1] = MAC1;
    ethState->ethaddr.addr[2] = MAC2;
    ethState->ethaddr.addr[3] = MAC3;
    ethState->ethaddr.addr[4] = MAC4;
    ethState->ethaddr.addr[5] = MAC5;

    /* initialize device buffer pointers and receiver filter
     */
    writeControlRegister(ERXSTL, LOW_BYTE(INIT_ERXST));     // initialize receive buffer start
    writeControlRegister(ERXSTH, HIGH_BYTE(INIT_ERXST));
    writeControlRegister(ERXNDL, LOW_BYTE(INIT_ERXND));     // initialize receive buffer end
    writeControlRegister(ERXNDH, HIGH_BYTE(INIT_ERXND));
    writeControlRegister(ERDPTL, LOW_BYTE(INIT_ERDPT));     // initialize read pointer
    writeControlRegister(ERDPTH, HIGH_BYTE(INIT_ERDPT));
    writeControlRegister(ERXWRPTL, LOW_BYTE(INIT_ERXWRPT)); // force write pointer to ERXST, Errata #5 workaround
    writeControlRegister(ERXWRPTH, HIGH_BYTE(INIT_ERXWRPT));
    writeControlRegister(ERXRDPTL, LOW_BYTE(INIT_ERXRDPT)); // force read pointer to ERXST
    writeControlRegister(ERXRDPTH, HIGH_BYTE(INIT_ERXRDPT));

    writeControlRegister(ERXFCON, INIT_ERXFCON);            // setup device packet filter

    /* MAC initialization settings
     */
    setControlBit(MACON1, MACON1_MARXEN);                                       // enable the MAC to receive frames
#if FULL_DUPLEX
    setControlBit(MACON1, (MACON1_TXPAUS | MACON1_RXPAUS));                     // for full duplex, set TXPAUS and RXPAUS to allow flow control
    setControlBit(MACON3, MACON3_FULDPX);                                       // set for full duplex
#else
    clearControlBit(MACON1, (MACON1_TXPAUS | MACON1_RXPAUS));                   // for half duplex, clear TXPAUS and RXPAUS
    clearControlBit(MACON3, MACON3_FULDPX);                                     // set for half duplex
#endif
    setControlBit(MACON3, (MACON3_PADCFG | MACON3_TXCRCEN | MACON3_FRMLEN));    // full frame padding + CRC

    writeControlRegister(MAMXFLL, LOW_BYTE(INIT_MAMXFL));   // max frame size
    writeControlRegister(MAMXFLH, HIGH_BYTE(INIT_MAMXFL));

    writeControlRegister(MABBIPG, INIT_MABBIPG);            // no detail why, just how to program them
	writeControlRegister(MAIPGL, INIT_MAIPGL);
	writeControlRegister(MAIPGH, INIT_MAIPGH);

    writeControlRegister(MAADR1, MAC0);                     // MAC address
    writeControlRegister(MAADR2, MAC1);
    writeControlRegister(MAADR3, MAC2);
    writeControlRegister(MAADR4, MAC3);
    writeControlRegister(MAADR5, MAC4);
    writeControlRegister(MAADR6, MAC5);

#if FULL_DUPLEX
    readPhyRegister(PHCON1, &tmpPhyReg);                    // set PHY to match half duplex setup MACON3_FULDPX
    tmpPhyReg |= PHCON1_PDPXMD;
    tmpPhyReg &= ~PHCON1_PLOOPBK;                           // make sure loop-back is off
    writePhyRegister(PHCON1, tmpPhyReg);
#else
    readPhyRegister(PHCON1, &tmpPhyReg);                    // set PHY to match half duplex setup MACON3_FULDPX
    tmpPhyReg &= ~PHCON1_PDPXMD;
    tmpPhyReg &= ~PHCON1_PLOOPBK;                           // make sure loop-back is off
    writePhyRegister(PHCON1, tmpPhyReg);

    readPhyRegister(PHCON2, &tmpPhyReg);                    // disable half duplex loopback
    tmpPhyReg |= PHCON2_HDLDIS;
    writePhyRegister(PHCON2, tmpPhyReg);
#endif  /* FULL_DUPLEX */

    writeMemBuffer(&perPacketCtrl, (u16_t) INIT_ETXST, 1);  // prep Tx buffer with per-packet control byte

    setControlBit(ECON2, ECON2_AUTOINC);                    // set auto increment memory pointer operation

    tmpPhyReg = 30000;
    while ( tmpPhyReg && ethLinkUp() == 0 )                 // wait for link up
    {
        tmpPhyReg--;
    }

    if ( ethLinkUp() == 1 )
    {
        setControlBit(ECON1, ECON1_RXEN);                   // enable frame reception @@ do I do these here?
        result = ERR_OK;
    }

#ifdef DRV_DEBUG_FUNC_EXIT
    printf("PHY ID 0x%04x:0x%04x\n", ethState->phyID1, ethState->phyID2);
    printf("enc28j60Init() %d\n", result);
#endif

    return result;
}

/* -----------------------------------------
 * low_level_init()
 *
 * hardware initialization function for the ENC28J60 chip
 * called from enc28j60_init().
 *
 * param: netif the already initialized lwip network interface structure
 *        for this ethernet interface
 * ----------------------------------------- */
static err_t low_level_init(struct netif *netif)
{
  err_t   result = ERR_IF;                      // signal an interface error

#ifdef DRV_DEBUG_FUNC_NAME
  printf("%s()\n",__func__);
#endif

  /* set MAC hardware address length */
  netif->hwaddr_len = ETHARP_HWADDR_LEN;

  /* set MAC hardware address */
  netif->hwaddr[0] = MAC0;
  netif->hwaddr[1] = MAC1;
  netif->hwaddr[2] = MAC2;
  netif->hwaddr[3] = MAC3;
  netif->hwaddr[4] = MAC4;
  netif->hwaddr[5] = MAC5;

  /* maximum transfer unit */
  netif->mtu = MTU;                             // @@ is this ok?

  /* device capabilities */
  /* don't set NETIF_FLAG_ETHARP if this device is not an ethernet one */
  netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP;
  netif->num   = 0;

  /* chip initialization
   *
   */
  if ( (result = enc28j60Init(netif->state)) == ERR_OK )
    netif->flags |= (NETIF_FLAG_LINK_UP | NETIF_FLAG_UP);   // if ENC28J60 initializes properly then set state to link up

#ifdef DRV_DEBUG_FUNC_EXIT
  printf("low_level_init() done\n");
#endif

  return result;
}

/* -----------------------------------------
 * This function should do the actual transmission of the packet. The packet is
 * contained in the pbuf that is passed to the function. This pbuf
 * might be chained.
 *
 * @param netif the lwip network interface structure for this ethernet interface
 * @param p the MAC packet to send (e.g. IP packet including MAC addresses and type)
 * @return ERR_OK if the packet could be sent
 *         an err_t value if the packet couldn't be sent
 *
 * @note Returning ERR_MEM here if a DMA queue of your MAC is full can lead to
 *       strange results. You might consider waiting for space in the DMA queue
 *       to become available since the stack doesn't retry to send a packet
 *       dropped because of memory failure (except for the TCP timers).
 * ----------------------------------------- */
static err_t low_level_output(struct netif *netif, struct pbuf *p)
{
  struct enc28j60_t *ethif = netif->state;
  struct pbuf       *q;
  u8_t               tempU8;
  u16_t              tempU16;

#ifdef DRV_DEBUG_FUNC_NAME
  printf("%s()\n",__func__);
#endif

  writeControlRegister(ETXSTL, LOW_BYTE(INIT_ETXST));       // explicitly set the transmit buffer start
  writeControlRegister(ETXSTH, HIGH_BYTE(INIT_ETXST));

  writeControlRegister(EWRPTL, LOW_BYTE(INIT_EWRPT));       // setup buffer write pointer
  writeControlRegister(EWRPTH, HIGH_BYTE(INIT_EWRPT));

  // transfer packet data into ENC28J60 output buffer
  for (q = p; q != NULL; q = q->next)
  {
    // send the data from the pbuf to the interface, one pbuf at a time.
    // the size of the data in each pbuf is kept in the ->len variable.
    assert(writeMemBuffer((u8_t*) q->payload, USE_CURR_ADD, q->len) == SPI_OK); // @@ convert to proper error handling
  }

  readControlRegister(EWRPTH, &tempU8);                     // get value of write pointer
  tempU16 = (u16_t) tempU8 << 8;
  readControlRegister(EWRPTL, &tempU8);
  tempU16 += tempU8;
  tempU16--;

  writeControlRegister(ETXNDL, LOW_BYTE(tempU16));          // set buffer end address
  writeControlRegister(ETXNDH, HIGH_BYTE(tempU16));

#ifdef DRV_DEBUG_FUNC_PARAM
    printf("low_level_output()\n tot_len=%u txBufferEnd=0x%04x\n", p->tot_len, tempU16);
#endif

  setControlBit(ECON1, ECON1_TXRTS);                        // enable/start frame transmission

  MIB2_STATS_NETIF_ADD(netif, ifoutoctets, p->tot_len);
  if (((u8_t*)p->payload)[0] & 1)
  {
    // broadcast or multicast packet
    MIB2_STATS_NETIF_INC(netif, ifoutnucastpkts);
  }
  else
  {
    // unicast packet
    MIB2_STATS_NETIF_INC(netif, ifoutucastpkts);
  }

  if ( controlBit(ECON1, ECON1_TXRTS) ) {}                  // wait for transmission to complete

  tempU16++;                                                // read the packet transmit status vector
  readMemBuffer(ethif->txStatusVector, tempU16, sizeof(struct txStat_t));

#ifdef DRV_DEBUG_FUNC_PARAM
    printf(" bytes Tx %u\n Stat1    0x%02x\n Stat2    0x%02x\n Stat3    0x%02x\n total Tx %u\n",
            ethif->txStatVector.txByteCount,
            ethif->txStatVector.txStatus1,
            ethif->txStatVector.txStatus2,
            ethif->txStatVector.txStatus3,
            ethif->txStatVector.txTotalXmtCount);
#endif

  if ( controlBit(ESTAT, ESTAT_TXABRT) == 1 )               // check for errors
  {
      //LINK_STATS_INC(link.memerr);
      //LINK_STATS_INC(link.drop);
      MIB2_STATS_NETIF_INC(netif, ifoutdiscards);           // handle error counters @@ handle error recovery?
      MIB2_STATS_NETIF_INC(netif, ifouterrors);
#ifdef DRV_DEBUG_FUNC_PARAM
      printf(" *** packet transmission failure ***\n");
#endif
  }
  else
      LINK_STATS_INC(link.xmit);

  return ERR_OK;
}

/* -----------------------------------------
 * low_level_input()
 *
 * allocate a pbuf and transfer the bytes of the incoming
 * packet from the interface into the pbuf.
 *
 * param:  netif the lwip network interface structure
 * return: a pbuf filled with the received packet (including MAC header)
 *         NULL on memory error
 * ----------------------------------------- */
static struct pbuf* low_level_input(struct netif *netif)
{
    struct enc28j60_t   *ethif = netif->state;
    struct pbuf         *p, *q;
    u16_t                len, xferCount, offset;

#ifdef DRV_DEBUG_FUNC_NAME
    printf("%s()\n",__func__);
#endif

    if ( !packetWaiting() )
        return NULL;

    // update ERDPT to point to next packet read address start
    writeControlRegister(ERDPTL, ethif->rxStatVector.nextPacketL);
    writeControlRegister(ERDPTH, ethif->rxStatVector.nextPacketH);

    // then read received packet information
    // extract next packet address, current packet length and errors bit[20,21,22,23]
    // into 'ethif'
    extractPacketInfo(ethif);

#ifdef DRV_DEBUG_FUNC_PARAM
    printf("low_level_input()\n len   %u\n next  0x%02x%02x\n stat1 0x%02x\n stat2 0x%02x\n",
               ethif->rxStatVector.rxByteCount,
               ethif->rxStatVector.nextPacketH,
               ethif->rxStatVector.nextPacketL,
               ethif->rxStatVector.rxStatus1,
               ethif->rxStatVector.rxStatus2);
#endif

    // Obtain the size of the packet and put it into the "len" variable.
    len = ethif->rxStatVector.rxByteCount;

    // allocate a pbuf chain of pbufs from the pool
    p = pbuf_alloc(PBUF_RAW, len, PBUF_POOL);

    if (p != NULL)
    {
      // read the waiting ethernet packet into the temporary buffer
      assert(len <= PACKET_BUFFER);
      readMemBuffer(tempPacketBuffer, USE_CURR_ADD, len);

#ifdef DRV_DEBUG_FUNC_PARAM
      {
        int i;
        printf(" dst: ");
        for (i = 0; i < 6; i++)
            printf("%02x ", tempPacketBuffer[i]);
        printf("\n src: ");
        for (i = 6; i < 12; i++)
            printf("%02x ", tempPacketBuffer[i]);
        printf("\n typ: ");
        for (i = 12; i < 14; i++)
            printf("%02x ", tempPacketBuffer[i]);
        printf("\n");
      }
#endif

      // iterate over the pbuf chain until we have read the entire packet into the pbuf
      offset = 0;
      for (q = p; q != NULL; q = q->next)
      //for (q = p; q->tot_len == p->len; q = q->next) // see here: http://www.nongnu.org/lwip/2_0_x/group__pbuf.html
      {
          xferCount = ((len - offset) > q->len) ? q->len : (len - offset);
          memcpy(q->payload, (tempPacketBuffer + offset), xferCount);
          offset += xferCount;
#ifdef DRV_DEBUG_FUNC_PARAM
          printf(" xferd %d bytes to pbuf\n", offset);
#endif
      }

      MIB2_STATS_NETIF_ADD(netif, ifinoctets, p->tot_len);
      if (((u8_t*)p->payload)[0] & 1)
      {
        // broadcast or multicast packet
        MIB2_STATS_NETIF_INC(netif, ifinnucastpkts);
      }
      else
      {
        // unicast packet
        MIB2_STATS_NETIF_INC(netif, ifinucastpkts);
      }
      LINK_STATS_INC(link.recv);
  }
  else
  {
    LINK_STATS_INC(link.memerr);
    LINK_STATS_INC(link.drop);
    MIB2_STATS_NETIF_INC(netif, ifindiscards);

#ifdef DRV_DEBUG_FUNC_PARAM
    printf(" *** 'pbuf' alloc err, packet dropped ***\n");
#endif
  }

  // acknowledge that a packet has been read from ENC28J60
  writeControlRegister(ERXRDPTL, ethif->rxStatVector.nextPacketL);  // update ERXRDPT to free read-packet buffer space
  writeControlRegister(ERXRDPTH, ethif->rxStatVector.nextPacketH);
  setControlBit(ECON2, ECON2_PKTDEC);                               // decrement packet waiting count

  return p;
}

/* -----------------------------------------
 * This function should be called when a packet is ready to be read
 * from the interface. It uses the function low_level_input() that
 * should handle the actual reception of bytes from the network
 * interface. Then the type of the received packet is determined and
 * the appropriate input function is called.
 *
 * @param netif the lwip network interface structure for this ethernet interface
 * ----------------------------------------- */
void enc28j60_input(struct netif *netif)
{
  struct eth_hdr *ethhdr;
  struct pbuf *p;

#ifdef DRV_DEBUG_FUNC_NAME
  printf("%s()\n",__func__);
#endif

  // move received packet into a new pbuf
  p = low_level_input(netif);

  // if no packet could be read, silently ignore this
  if (p != NULL)
  {
    // pass all packets to ethernet_input, which decides what packets it supports
    if ( netif->input(p, netif) != ERR_OK )
    {
      LWIP_DEBUGF(NETIF_DEBUG, ("enc28j60_input: IP input error\n"));
      pbuf_free(p);
      p = NULL;
    }
  }
}

/* -----------------------------------------
 * Should be called at the beginning of the program to set up the
 * network interface. It calls the function low_level_init() to do the
 * actual setup of the hardware.
 *
 * This function should be passed as a parameter to netif_add().
 *
 * @param netif the lwip network interface structure for this ethernet interface
 * @return ERR_OK if the loopif is initialized
 *         ERR_MEM if private data couldn't be allocated
 *         any other err_t on error
 * ----------------------------------------- */
err_t enc28j60_init(struct netif *netif)
{
  struct enc28j60_t *ethif;
  err_t              result = ERR_IF;           // signal an interface error

#ifdef DRV_DEBUG_FUNC_NAME
  printf("%s()\n",__func__);
#endif

  LWIP_ASSERT("netif != NULL", (netif != NULL));

  ethif = mem_malloc(sizeof(struct enc28j60_t));
  if (ethif == NULL) {
    LWIP_DEBUGF(NETIF_DEBUG, ("enc28j60_init: out of memory\n"));
    return ERR_MEM;
  }

#if LWIP_NETIF_HOSTNAME
  /* Initialize interface hostname */
  netif->hostname = HOSTNAME;
#endif /* LWIP_NETIF_HOSTNAME */

  /*
   * Initialize the snmp variables and counters inside the struct netif.
   * The last argument should be replaced with your link speed, in units
   * of bits per second.
   */
  MIB2_INIT_NETIF(netif, snmp_ifType_ethernet_csmacd, LINK_SPEED_OF_YOUR_NETIF_IN_BPS);

  netif->state = ethif;
  netif->name[0] = IFNAME0;
  netif->name[1] = IFNAME1;

  /* We directly use etharp_output() here to save a function call.
   * You can instead declare your own function an call etharp_output()
   * from it if you have to do some checks before sending (e.g. if link
   * is available...) */
  netif->output = etharp_output;
  netif->linkoutput = low_level_output;

  // initialize the hardware
  result = low_level_init(netif);

#if defined DRV_DEBUG_FUNC_PARAM && defined DRV_DEBUG_FUNC_NAME
  printf(" [exit] %s() exit %d\n", __func__, result);
#endif

  return result;
}
