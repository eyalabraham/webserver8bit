/*****************************************************
 *                    ABANDONED
 *****************************************************
 * tsrtest.c
 *
 * this is a stand alone program to test TSR function
 * on FlashLite v25 that runs non-standard DOS-like "OS".
 * first pass is to install a TSR that can be triggered
 * by a local Basic program POKE command to a memory location.
 * ultimate goal is for the TSR to replace serial-0 as the
 * console with serial-1.
 * this will be done by replacing the built in INT 10h and 16h
 * with a custome INT 10h and 16h using a TSR.
 *
 * TSR function will hook into periodic clock and check
 * specific memory location for changes. if memory location changed
 * the TSR will print a text message to console.
 *
 *****************************************************/

#include    <stdlib.h>
#include    <stdio.h>
#include    <string.h>
#include    <assert.h>
#include    <dos.h>
#include    <v25.h>

/* -----------------------------------------
   global definitions
----------------------------------------- */


/* -----------------------------------------
   global variables
----------------------------------------- */

short int       bPokeByte = 0;          // this is the memory location that will be tested
unsigned int    wSegment  = 0;          // segment address of POKE byte
unsigned int    wOffset   = 0;          // offset address of POKE byte
unsigned int    wIntPS    = 0;          // original vector PS
unsigned int    wIntPC    = 0;          // original vector PC


/* -----------------------------------------
   functions
----------------------------------------- */


/* -----------------------------------------
 *
 * TSR function
 *
 */
void far TSR(void)
{
    // check 'POKE' location for change
    
    // call original vector
    //((void)(MK_FP(wIntPS, wIntPC)));
    asm{ jmp far wIntPS:wIntPC }
}

/* -----------------------------------------
   startup code
----------------------------------------- */

int main(int argc, char* argv[])
{
    struct SFR _far*    pSfr;           // pointed to NEC v25 IO register set
    struct REGBNK _far* pRegBnk;
    int                 i;

    // parse command line parameters:
    // ser1loop [-v]
    //  '-v' print build and verion information
    for ( i = 1; i < argc; i++ )
    {
        if ( strcmp(argv[i], "-v") == 0 )
        {
            printf("tsrtest.exe %s %s\n", __DATE__, __TIME__);
            return 0;
        }
        else
        {
            printf("main(), bad command line option '%s'\n", argv[i]);
            return 1;
        }
    }

    // setup
    pSfr = MK_FP(0xf000, 0xff00);       // define SFR
    pRegBnk = MK_FP(0xf000, 0xfec0);    // define register bank #6

    // read and print Timer-1 setup
    printf("\n");
    printf("timer-1 control, tmc1: %x\n", pSfr->tmc1);
    printf("timer-1 modulus, md1:  %d\n", pSfr->md1);
    printf("timer-1 interrupt, tmic0: %x, tmic1: %x, tmic2: %x\n", pSfr->tmic0, pSfr->tmic1, pSfr->tmic2);

    // read and print regster bank #6
    wIntPS = pRegBnk->ps;
    wIntPC = pRegBnk->vec_pc;
    printf("timer-1 register bank #6 PS: %x\n", pRegBnk->ps);
    printf("timer-1 register bank #6 PC: %x\n", pRegBnk->vec_pc);

    // hook dummy function into register bank
    pRegBnk->ps = FP_SEG((void *)TSR);
    pRegBnk->vec_pc = FP_OFF((void *)TSR);

    // exit as TSR
    keep(0, (_SS + (_SP/16) - _psp));


    return 0;
}
