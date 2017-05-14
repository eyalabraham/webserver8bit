/*
 *  ppispi.h
 *
 *     header for driver for 8255 PPI to AVR/SPI interface
 *
 */

#ifndef     __ppispi_h__
#define     __ppispi_h__

/* -----------------------------------------
   definitions and types
----------------------------------------- */

// SPI driver function return status codes
// functions will return 'int'
// positive number indicates success status
typedef enum
{
    SPI_OK = 0,                                 // no error
    SPI_BUSY = -1,                              // SPI interface already in use, try again
    SPI_ERR_TX_TIMEOUT = -2,                    // transmit time out, ready not received from interface
    SPI_ERR_RX_TIMEOUT = -3,                    // receive time out, ready not received from interface
    SPI_IO_DIR_ERR = -4,                        // wrong IO direction, read in a write function or write in a read function
    SPI_RD_ERR = -5,			                // read initiated before input buffer cleared
    SPI_WR_ERR = -6                             // error with SPI write
} spiDevErr_t;

typedef enum                                    // valid devices supported
{
    LCD_CMD,                                    // ST7735R LCD controller commands
    LCD_DATA,                                   // ST7735R LCD controller data
    ETHERNET_RD,                                // ENC28J60 Ethernet read
    ETHERNET_WR,                                // ENC28J60 Ethernet write
    SD_CARD_RD,                                 // SD card read
    SD_CARD_WR,                                 // SD card write
    KEEP_CS,                                    // leave last CS asserted
    NONE                                        // no valid device selected
} spiDevice_t;

/* -----------------------------------------
   function prototypes
----------------------------------------- */
void spiIoInit(void);                           // call once to initialize IO
spiDevErr_t spiReadByte(spiDevice_t, unsigned char*);  // read a byte from a device
spiDevErr_t spiWriteByte(spiDevice_t, unsigned char);  // write a byte to a device
spiDevErr_t spiReadByteKeepCS(spiDevice_t, unsigned char*);  // read a byte from a device and keep CS asserted (active)
spiDevErr_t spiWriteByteKeepCS(spiDevice_t, unsigned char);  // write a byte to a device and keep CS asserted (active)

/* -----------------------------------------
   DMA function prototypes
----------------------------------------- */
// read/write a block of data into/out-of buffer location of certain size, at completion call a callback function
spiDevErr_t spiReadBlock(spiDevice_t, unsigned char*, unsigned int, void(*)(void));
spiDevErr_t spiWriteBlock(spiDevice_t, unsigned char*, unsigned int, void(*)(void));

#endif      /*  __ppispi_h__  */

