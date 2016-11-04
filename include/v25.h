/*
   THIS STRUCTURE IS DEFINED TO THE SPECIAL FACULTY REGISTER OF V25/V35.
   DATE 08 JULY88

   Copyright (C) NEC Corporation 1988
*/

#ifndef __V25_H__
#define __V25_H__

#include <dos.h>

/*
   SFR STRUCTURE
*/

typedef struct SFR
        {
         char port0;          /* port 0                                       */
         char portm0;         /* port 0 mode                                  */
         char portmc0;        /* port 0 control                               */
         char dummy1[5];
         char port1;          /* port 1                                       */
         char portm1;         /* port 1 mode                                  */
         char portmc1;        /* port 1 control                               */
         char dummy2[5];
         char port2;          /* port 2                                       */
         char portm2;         /* port 2 mode                                  */
         char portmc2;        /* port 2 control                               */
         char dummy3[37];
         char portT;          /* port T                                       */
         char dummy4[2];
         char portmT;         /* port T mode                                  */
         char dummy5[4];
         char intm;           /* external interrupt mode register 'IMR'       */
         char dummy6[3];
         char ems0;           /* int0 macro service control reg.              */
         char ems1;           /* int1 macro service control reg.              */
         char ems2;           /* int2 macro service control reg.              */
         char dummy7[5];
         char exic0;          /* int0 ext. interrupt control register 'IRC'   */
         char exic1;          /* int1 ext. interrupt control register 'IRC'   */
         char exic2;          /* int2 ext. interrupt control register 'IRC'   */
         char dummy8[17];
         char rxb0;           /* receive buffer 0                             */
         char dummy9;
         char txb0;           /* transmit buffer 0                            */
         char dummy10[2];
         char srms0;          /* receive macro service control reg.           */
         char stms0;          /* transmit macro service control reg.          */
         char dummy11;
         char scm0;           /* serial mode register                         */
         char scc0;           /* serial control register                      */
         char brg0;           /* baud rate register                           */
         char scs0;           /* serial status register                       */
         char seic0;          /* serial error interrupt control reg. 'IRC'    */
         char sric0;          /* serial receive interrupt control reg. 'IRC'  */
         char stic0;          /* serial transmit interrupt control reg. 'IRC' */
         char dummy12;
         char rxb1;           /* receive buffer 1                             */
         char dummy13;
         char txb1;           /* transmit buffer 1                            */
         char dummy14[2];
         char srms1;          /* serial receive macro service control reg.    */
         char stms1;          /* serial transmit macro service control reg.   */
         char dummy15;
         char scm1;           /* serial mode register                         */
         char scc1;           /* serial control register                      */
         char brg1;           /* baud rate register                           */
         char scs1;           /* serial status register                       */
         char seic1;          /* serial error interrupt control reg. 'IRC'    */
         char sric1;          /* serial receive interrupt control reg. 'IRC'  */
         char stic1;          /* serial transmit interrupt control reg. 'IRC' */
         char dummy16;
         int  tm0;            /* timer 0                                      */
         int  md0;            /* timer 0 modulus                              */
         int  dummy17[2];
         int  tm1;            /* timer 1                                      */
         int  md1;            /* timer 1 modulus                              */
         int  dummy18[2];
         char tmc0;           /* timer 0 control                              */
         char tmc1;           /* timer 1 control                              */
         char dummy19[2];
         char tmms0;          /* timer 0 macro service control reg.           */
         char tmms1;          /* timer 1 macro service control reg.           */
         char tmms2;          /* timer 2 macro service control reg.           */
         char dummy20[5];
         char tmic0;          /* timer 0 interrupt control reg. 'IRC'         */
         char tmic1;          /* timer 1 interrupt control reg. 'IRC'         */
         char tmic2;          /* timer 2 interrupt control reg. 'IRC'         */
         char dummy21;
         char dmac0;          /* DMA 0 control                                */
         char dmam0;          /* DMA 0 mode                                   */
         char dmac1;          /* DMA 1 control                                */
         char dmam1;          /* DMA 1 mode                                   */
         char dummy22[8];
         char dic0;           /* DMA channel 0 interrupt control reg. 'IRC'   */
         char dic1;           /* DMA channel 1 interrupt control reg. 'IRC'   */
         char dummy23[50];
         char stbc;           /* standby control reg.                         */
         char rfm;            /* refresh mode reg.                            */
         char dummy24[6];
         int  wtc;            /* wait control reg.                            */
         char flag;           /* user flag register                           */
         char prc;            /* processor control register                   */
         char tbic;           /* time base interrupt control reg.             */
         char dummy25[2];
         char irqs;           /* interrupt source register                    */
         char dummy26[12];
         char ispr;           /* interrupt priority register                  */
         char dummy27[2];
         char idb;            /* internal data base reg.                      */
        };

/*
   REGISTER BAMK STRUCTURE
*/

typedef struct REGBNK
        {
         int reserve;
         int vec_pc;
         int save_psw;
         int save_pc;
         int ds0;
         int ss;
         int ps;
         int ds1;
         int iy;
         int ix;
         int bp;
         int sp;
         int bw;
         int dw;
         int cw;
         int aw;
        };

/*
   MACRO CHANNEL STRUCTURE
*/

typedef struct macroChannel_tag
        {
         unsigned char bMSC;
         unsigned char bSFRP;
         unsigned char bSCHR;
         unsigned char bReserved;
         unsigned int  wMSP;
         unsigned int  wMSS;
        } macroChannel;

/*
   macro definitions for I/O port bit set/reset positions
*/

#define  BIT_0   0
#define  BIT_1   1
#define  BIT_2   2
#define  BIT_3   3
#define  BIT_4   4
#define  BIT_5   5
#define  BIT_6   6
#define  BIT_7   7

/* NEC V25:   CLR1  byte ptr [bx + _mem8_], _bitPos_
*/
#define  BIT_CLR(_bitPos_, _mem8_) asm {                            \
                                        push   es;                  \
                                        mov    bx, 0f000h;          \
                                        mov    es, bx;              \
                                        mov    bx, 0ff00h;          \
                                        db     26h;                 \
                                        db     00fh;                \
                                        db     01ah;                \
                                        db     087h;                \
                                        dw     ._mem8_;             \
                                        db     _bitPos_;            \
                                        pop    es;                  \
                                       }


/* NEC V25:   SET1  byte ptr [bx + _mem8_], _bitPos_
*/
#define  BIT_SET(_bitPos_, _mem8_) asm {                            \
                                        push   es;                  \
                                        mov    bx, 0f000h;          \
                                        mov    es, bx;              \
                                        mov    bx, 0ff00h;          \
                                        db     26h;                 \
                                        db     0fh;                 \
                                        db     1ch;                 \
                                        db     87h;                 \
                                        dw     ._mem8_;             \
                                        db     _bitPos_;            \
                                        pop    es;                  \
                                       }

/* NEC V25:   NOT1  byte ptr [bx + _mem8_], _bitPos_
*/
#define  BIT_NOT(_bitPos_, _mem8_) asm {                            \
                                        push   es;                  \
                                        mov    bx, 0f000h;          \
                                        mov    es, bx;              \
                                        mov    bx, 0ff00h;          \
                                        db     26h;                 \
                                        db     00fh;                \
                                        db     01eh;                \
                                        db     087h;                \
                                        dw     ._mem8_;             \
                                        db     _bitPos_;            \
                                        pop    es;                  \
                                       }

#endif /* __V25_H__ */
