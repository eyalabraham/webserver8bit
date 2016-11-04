/* ***************************************************************************

  PLATFORM.H

  Task header file.

  May  14 2012 - corrected wheel clicks count to 68 (from 64) and adjusted related constants
  June  8 2010 - updated to fixed point Q10.5
  Jan  16 2007 - Created

*************************************************************************** */

#ifndef  __PLATFORM__
#define  __PLATFORM__

/* -----------------------------------------
   physical dimention definitions
----------------------------------------- */

// numbers in fixed point Q10.5
#define BASE_WIDTH_CM_FP          0x03d0  // base width 30.5 [cm]
#define BASE_CENTER_CM_FP         0x01e8  // base half width 15.25 [cm]
#define WHEEL_CIRCUMFERENCE_CM_FP 0x0360  // wheel circumference 27 [cm]
#define WHELL_CLICKS_FP           0x0880  // 68 clicks per wheel circumference
#define CLICKS_PER_CM_FP          0x0051  // 2.53125 (2.51852 [click/cm], accuracy +0.51%)
#define CM_PER_CLICK_FP           0x000d  // 0.40625 (0.39706 [cm/click], accuracy +2.26%)
#define TWO_PI                    0x00c9  // 6.28125 (6.28318 [rad], accuracy -0.03%)
#define BASE_WIDTH_CL_FP          0x099a  // base width in clicks 76.81250 (76.81481 [clicks], accuracy -0.00%)
#define BASE_CENTER_CL_FP         0x04cd  // base half width in clicks 38.40625 (38.40741 [clicks], accuracy -0.00%)

// NOT in fixed point
#define WHELL_CLICKS              68      // clicks per wheel encoder
#define WHEEL_CIRCUMFERENCE_CL    68      // wheel circumference in clicks
#define WHEEL_CIRCUMFERENCE_CM    27      // wheel circumference in [cm]
#define BASE_WIDTH_CL             77      // base width in clicks 76.81481
#define BASE_CENTER_CL            38      // base half width in clicks 38.40741

#endif  // __PLATFORM__