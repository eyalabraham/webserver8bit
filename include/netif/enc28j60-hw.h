/* ***************************************************************************

  enc28j60-hw.h

  header file for Microchip EMC28J60 ethernet chip internals
  
  March 2017 - Created

*************************************************************************** */

#ifndef __ENC28J60_HW_H__
#define __ENC28J60_HW_H__

/* -----------------------------------------
   global definitions
----------------------------------------- */

#define     FULL_DUPLEX         1               // set to 0 for half-duplex setup

#define     HIGH_BYTE(x)        ((u8_t) ((u16_t)(x) >> 8))
#define     LOW_BYTE(x)         ((u8_t) ((u16_t)(x)))

#define     PACKET_BUFFER       2048            // size of temporary packet buffer
#define     TOTAL_RAM           8192            // in Bytes
#define     USE_CURR_ADD        0xffff          // use current address pointed by ERDPT or EWRPT
#define     PHY_ID1             0x0083          // PHY IDs for verification
#define     PHY_ID2             0x1400

#define     OP_RCR              0x00            // Read Control Register opcode
#define     OP_RBM              0x3a            // Read Buffer Memory opcode
#define     OP_WCR              0x40            // Write Control Register opcode
#define     OP_WBM              0x7a            // Write Buffer Memory opcode
#define     OP_BFS              0x80            // Bit Field Set opcode
#define     OP_BFC              0xa0            // Bit Field Clear opcode
#define     OP_SC               0xff            // System Command (Soft Reset) opcode


#define     MICMD_MIIRD         0x01
#define     MISTAT_BUSY         0x01
#define     ESTAT_CLKRDY        0x01
#define     ESTAT_TXABRT        0x02
#define     MACON2_MARST        0x80
#define     MACON1_MARXEN       0x01
#define     MACON1_RXPAUS       0x04
#define     MACON1_TXPAUS       0x08
#define     MACON1_PASSALL      0x02            // @@ how does this work in conjuntion with the filter?
#define     MACON3_FRMLEN       0x02
#define     MACON3_TXCRCEN      0x10
#define     MACON3_PADCFG       0xa0
#define     MACON3_FULDPX       0x01
#define     PHCON1_PDPXMD       0x0100          // must match MACON3_FULDPX setting
#define     PHCON1_PLOOPBK      0x4000
#define     PHCON2_HDLDIS       0x0100
#define     PHSTAT2_LSTAT       0x0400
#define     PHSTAT1_LLSTAT      0x0004
#define     ECON1_RXEN          0x04
#define     ECON1_TXRTS         0x08
#define     ECON1_RXRST         0x40
#define     ECON1_TXRST         0x80
#define     EIR_PKTIF           0x40
#define     EIR_RXERIF          0x01
#define     ECON2_AUTOINC       0x80
#define     ECON2_PKTDEC        0x40

/* -----------------------------------------
    ENC28J60 initialization parameters
----------------------------------------- */
#define     INIT_ERXST          0x0000          // receive buffer start 0x0000
#define     INIT_ERXND          0x13ff          // receive buffer end 0x13ff (5K byte)
#define     INIT_ERXRDPT        INIT_ERXST      // receiver read pointer 0x0000
#define     INIT_ERXWRPT        INIT_ERXST      // receiver write pointer 0x0000
#define     INIT_ERDPT          INIT_ERXST      // receiver read pointer

#define     INIT_ETXST          0x1500          // transmit buffer start 0x1500
#define     INIT_EWRPT          INIT_ETXST+1    // transmitter write pointer, one byte after PER_PACK_CTRL location

#define     INIT_ERXFCON        0xa0            // see section 8.0 RECEIVE FILTERS, pg.49

#define     INIT_MAMXFL         1518            // max frame size of 1518 bytes

#if FULL_DUPLEX
#define     INIT_MABBIPG        0x15            // transmistion packet gap definitions
#else
#define     INIT_MABBIPG        0x12            // transmistion packet gap definitions
#endif
#define     INIT_MAIPGL         0x12
#define     INIT_MAIPGH         0x0C

#define     PER_PACK_CTRL       0x0e            // MACON3 will be used to determine how the packet will be transmitted

/* -----------------------------------------
   ENC28J60 register bank identifiers

  +----+----+----+----+----+----+----+----+
  | b7 | b6 | b5 | b4 | b3 | b2 | b1 | b0 |
  +----+----+----+----+----+----+----+----+
    |    |    |    |    |    |    |    |
    |    |    |    |    |    |    |    |
    |    |    |    +----+----+----+----+--- 5bit control register address
    |    |    +---------------------------- BANK0: 0, BANK1: 1, BANK2: 0, BANK3: 1
    |    +---------------------------------        0         0         1         1
    +-------------------------------------- MAC/MII register = 1, ETH register = 0

----------------------------------------- */
typedef enum {
    BANK0   = 0x00,
    BANK1   = 0x20,
    BANK2   = 0x40,
    BANK3   = 0x60,
    BANKANY = BANK0,
    MACMII  = 0x80                              // flag MAC or MII registers for a 16bit read
} regBank_t;

/* -----------------------------------------
   ENC28J60 CONTROL REGISTER MAP
----------------------------------------- */
typedef enum {
	ERDPTL   = 0x00 | BANK0,
	ERDPTH   = 0x01 | BANK0,
	EWRPTL   = 0x02 | BANK0,
	EWRPTH   = 0x03 | BANK0,
	ETXSTL   = 0x04 | BANK0,
	ETXSTH   = 0x05 | BANK0,
	ETXNDL   = 0x06 | BANK0,
	ETXNDH   = 0x07 | BANK0,
	ERXSTL   = 0x08 | BANK0,
	ERXSTH   = 0x09 | BANK0,
	ERXNDL   = 0x0a | BANK0,
	ERXNDH   = 0x0b | BANK0,
	ERXRDPTL = 0x0c | BANK0,
	ERXRDPTH = 0x0d | BANK0,
	ERXWRPTL = 0x0e | BANK0,
	ERXWRPTH = 0x0f | BANK0,
	EDMASTL  = 0x10 | BANK0,
	EDMASTH  = 0x11 | BANK0,
	EDMANDL  = 0x12 | BANK0,
	EDMANDH  = 0x13 | BANK0,
	EDMADSTL = 0x14 | BANK0,
	EDMADSTH = 0x15 | BANK0,
	EDMACSL  = 0x16 | BANK0,
	EDMACSH  = 0x17 | BANK0,

    EHT0     = 0x00 | BANK1,
    EHT1     = 0x01 | BANK1,
    EHT2     = 0x02 | BANK1,
    EHT3     = 0x03 | BANK1,
    EHT4     = 0x04 | BANK1,
    EHT5     = 0x05 | BANK1,
    EHT6     = 0x06 | BANK1,
    EHT7     = 0x07 | BANK1,
    EPMM0    = 0x08 | BANK1,
    EPMM1    = 0x09 | BANK1,
    EPMM2    = 0x0A | BANK1,
    EPMM3    = 0x0B | BANK1,
    EPMM4    = 0x0C | BANK1,
    EPMM5    = 0x0D | BANK1,
    EPMM6    = 0x0E | BANK1,
    EPMM7    = 0x0F | BANK1,
    EPMCSL   = 0x10 | BANK1,
    EPMCSH   = 0x11 | BANK1,
    EPMOL    = 0x14 | BANK1,
    EPMOH    = 0x15 | BANK1,
    EWOLIE   = 0x16 | BANK1,                    // @@ marked "Reserved' in DS39662E
    EWOLIR   = 0x17 | BANK1,                    // @@ marked "Reserved' in DS39662E
    ERXFCON  = 0x18 | BANK1,
	EPKTCNT  = 0x19 | BANK1,

    MACON1   = 0x00 | BANK2 | MACMII,
    MACON2   = 0x01 | BANK2 | MACMII,           // @@ marked "Reserved' in DS39662E
    MACON3   = 0x02 | BANK2 | MACMII,
    MACON4   = 0x03 | BANK2 | MACMII,
    MABBIPG  = 0x04 | BANK2 | MACMII,
    MAIPGL   = 0x06 | BANK2 | MACMII,
    MAIPGH   = 0x07 | BANK2 | MACMII,
    MACLCON1 = 0x08 | BANK2 | MACMII,
    MACLCON2 = 0x09 | BANK2 | MACMII,
    MAMXFLL  = 0x0A | BANK2 | MACMII,
    MAMXFLH  = 0x0B | BANK2 | MACMII,
    MAPHSUP  = 0x0D | BANK2 | MACMII,           // @@ marked "Reserved' in DS39662E
    MICON    = 0x11 | BANK2 | MACMII,           // @@ marked "Reserved' in DS39662E
    MICMD    = 0x12 | BANK2 | MACMII,
    MIREGADR = 0x14 | BANK2 | MACMII,
    MIWRL    = 0x16 | BANK2 | MACMII,
    MIWRH    = 0x17 | BANK2 | MACMII,
    MIRDL    = 0x18 | BANK2 | MACMII,
    MIRDH    = 0x19 | BANK2 | MACMII,

	MAADR5   = 0x00 | BANK3 | MACMII,
	MAADR6   = 0x01 | BANK3 | MACMII,
	MAADR3   = 0x02 | BANK3 | MACMII,
	MAADR4   = 0x03 | BANK3 | MACMII,
	MAADR1   = 0x04 | BANK3 | MACMII,
	MAADR2   = 0x05 | BANK3 | MACMII,
	EBSTSD   = 0x06 | BANK3,
	EBSTCON  = 0x07 | BANK3,
	EBSTCSL  = 0x08 | BANK3,
	EBSTCSH  = 0x09 | BANK3,
	MISTAT   = 0x0a | BANK3 | MACMII,
	EREVID   = 0x12 | BANK3,
	ECOCON   = 0x15 | BANK3,
	EFLOCON  = 0x17 | BANK3,
	EPAUSL   = 0x18 | BANK3,
	EPAUSH   = 0x19 | BANK3,

    EIE      = 0x1b | BANKANY,                  // these registers are visible in any/all banks
    EIR      = 0x1c | BANKANY,
    ESTAT    = 0x1d | BANKANY,
    ECON2    = 0x1e | BANKANY,
    ECON1    = 0x1f | BANKANY
} ctrlReg_t;


/* -----------------------------------------
   ENC28J60 PHY registers
----------------------------------------- */
typedef enum {
	PHCON1  = 0x00,
	PHSTAT1 = 0x01,
	PHID1   = 0x02,
	PHID2   = 0x03,
	PHCON2  = 0x10,
	PHSTAT2 = 0x11,
	PHIE    = 0x12,
	PHIR    = 0x13,
	PHLCON  = 0x14,
} phyReg_t;

#endif  /* __ENC28J60_HW_H__ */

