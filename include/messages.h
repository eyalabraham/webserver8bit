/* ***************************************************************************

  MESSAGES.H

  Messages definitions.

  Nov 4, 2016 - created

*************************************************************************** */

#ifndef __MESSAGES_H__
#define __MESSAGES_H__

// don't cares
#define    B_DONT_CARE       0
#define    W_DONT_CARE       0
#define    DW_DONT_CARE      0L

// =========================================
// message types:
// -----------------------------
//  'MV_' movement messages
//  'SN_' sensor messages
//  'IR_' IR detection
//  'MS_' miscellaneous messages
// =========================================

/* =========================================
   navigation process --> t_ctrl
========================================= */

// -----------------------------------------
//  move forward unlimited distance forward
#define    MV_FORWARD_CMD     10
//  < wPayload >  = W_DONT_CARE
//  < dwPayload > = DW_DONT_CARE
// -----------------------------------------

// -----------------------------------------
//  move backward unlimited distance backward
#define    MV_BACK_CMD        11
//  < wPayload >  = W_DONT_CARE
//  < dwPayload > = DW_DONT_CARE
// -----------------------------------------

// -----------------------------------------
//  steering offset for linear command
//  applicable only for: MV_FORWARD_CMD, MV_BACK_CMD
#define    MV_LIN_STEER       12
//  < wPayload > = steering offset
//                 motor speed differential 'int' range +8 to -8
//  < dwPayload > = DW_DONT_CARE
// -----------------------------------------

// -----------------------------------------
//  linear move command with distance
#define    MV_LINEAR_CMD      13
//  < wPayload >  = distance in [cm] in FP Qmn
//                  forward: +430 to back: -430
//                  ** no range check is done in t_ctrlex **
//  < dwPayload > = DW_DONT_CARE
// -----------------------------------------

// -----------------------------------------
//  unlimited point turn counter clock-wise command
#define    MV_LEFT_CMD        14
//  < wPayload >  = W_DONT_CARE
//  < dwPayload > = DW_DONT_CARE
// -----------------------------------------

// -----------------------------------------
//  unlimited point turn clock-wise command
#define    MV_RIGHT_CMD       15
//  < wPayload >  = W_DONT_CARE
//  < dwPayload > = DW_DONT_CARE
// -----------------------------------------

// -----------------------------------------
//  point turn command with angle
#define    MV_PTURN_CMD       16
//  < wPayload >  = [rad] fix point Qmn
//                  range CW: +6.28[rad] to CCW: -6.28[rad]
//  < dwPayload > = DW_DONT_CARE
// -----------------------------------------

// -----------------------------------------
//  move speed command
#define    MV_SPEED_CMD       17
//  < wPayload >
//        corresponds to indexes in
//        array 'motorCtrlOrMask' in module t_ctrlex.c
#define    MV_SPEED_8         8
#define    MV_SPEED_7         7
#define    MV_SPEED_6         6
#define    MV_SPEED_5         5
#define    MV_SPEED_4         4 // <-- default in t_ctrl.c t_ctrlex.c with spread of 6
#define    MV_SPEED_3         3
#define    MV_SPEED_2         2
#define    MV_SPEED_1         1
#define    MV_SPEED_MAX       MV_SPEED_8
#define    MV_SPEED_MIN       MV_SPEED_1
//  < dwPayload > = DW_DONT_CARE
// -----------------------------------------

// -----------------------------------------
//  stop any move commands (all stop)
#define    MV_STOP_CMD        18
//  < wPayload >  = W_DONT_CARE
//  < dwPayload > = DW_DONT_CARE
// -----------------------------------------

/* =========================================
   navigation process --> t_dist
========================================= */

// -----------------------------------------
//  distance sensor command
#define    SN_DIST_MODE       19
// < wPayload >
#define    SN_STOW            1
#define    SN_POINT           2
#define    SN_MEASURE         3
#define    SN_GETHC08STAT     4
//  < dwPayload >
//    for N_STOW     = DW_DONT_CARE
//    for N_POINT    = point direction
//                     in +PI/2 left to -PI/2 right [rad]) fixed-point Qm.n
//    for N_MEASURE  = DW_DONT_CARE
//    for N_HC08STAT = DW_DONT_CARE
// -----------------------------------------

/* =========================================
   t_control --> t_nav
========================================= */

// -----------------------------------------
//  linear motion status
//   for MV_FORWARD_CMD, MV_BACK_CMD, MV_LEFT_CMD this messgae will arrive
//   every time wheel average distance crosses ~400[cm]
#define    MV_LINEAR_STAT     30
//  < wPayload >  = platform center travel in [cm] Qmn
//                  forward: +430 to back: -430
//  < dwPayload > = DW_DONT_CARE
// -----------------------------------------

// -----------------------------------------
//  turn status
#define    MV_TURN_STAT       31
//  < wPayload >  = arc stat in [rad] Qmn
//                  range CW: +6.28[rad] to CCW: -6.28[rad]
//  < dwPayload > = DW_DONT_CARE
// -----------------------------------------

// -----------------------------------------
//  motion stop reason code
#define    SN_OBSTACLE        32
//  < wPayload >
#define    SN_RIGHT_SW_ON     1
#define    SN_RIGHT_SW_OFF    2
#define    SN_LEFT_SW_ON      3
#define    SN_LEFT_SW_OFF     4
#define    SN_MID_SW_ON       5
#define    SN_MID_SW_OFF      6
#define    SN_DIST_TRIG_LEFT  7
#define    SN_DIST_TRIG_RIGHT 8
#define    SN_DIST_TRIG_MID   9
//  < dwPayload > = DW_DONT_CARE
// -----------------------------------------

// -----------------------------------------
//  error
#define    SN_ERROR           33
//  < wPayload >
#define    SN_ENCODER_OVR     1
#define    SN_ENCODER_TOV     2
#define    SN_DRIVE_ERR       3
//  < dwPayload > = DW_DONT_CARE
// -----------------------------------------

/* =========================================
   t_sense --> t_ctrl
========================================= */

// -----------------------------------------
//  bumper switch
#define    SN_BUMPER_SW       50
//  < wPayload >
#define    SN_RIGHT_ON        1
#define    SN_RIGHT_OFF       2
#define    SN_LEFT_ON         3
#define    SN_LEFT_OFF        4
#define    SN_MID_ON          5
#define    SN_MID_OFF         6
//  < dwPayload > = DW_DONT_CARE
// -----------------------------------------

// -----------------------------------------
//  sensor error
#define    SN_ENCODER         51
//  < wPayload >
#define    SN_ENCODER_ERR     1
//  < dwPayload > = DW_DONT_CARE
// -----------------------------------------

// -----------------------------------------
//  wheel click data
#define    SN_CLICKS          52
//  < wPayload >  = time interval in [ticks]
//  < dwPayload > = encoder clicks
//                  right wheel clicks int count
//                  left wheel clicks int count
// -----------------------------------------

/* =========================================
   t_dist --> t_nav
========================================= */

// -----------------------------------------
//  distance sensor measured data (GP2D)
#define    SN_DISTANCE       60
//  < wPayload >  = distance in [cm] fixed point
//                   0x00 if distance sensor error
//  < dwPayload > = servo position in [rad] fixed point
//                  (fixed-point Qm.n)
// -----------------------------------------

// -----------------------------------------
//  HC08 microprocessos state
//  response to SN_DIST_MODE with SN_GETHC08STAT
//  < bPayload >
#define    SN_HC08STAT       61
//  < wPayload >  = HC08 status word
//  < dwPayload > = DW_DONT_CARE
// -----------------------------------------

/* =========================================
   <any> --> t_ctrl
========================================= */

// -----------------------------------------
//  override PID constants
#define    MS_PID            70
//  < wPayload >   = proportional constant
//                   (fixed-point Qm.n)
//  < dwPayload  > = integral and differential constant
//                   (fixed-point Qm.n)
// -----------------------------------------

// -----------------------------------------
//  initiate log dump
#define    MS_DATA_LOG       71
//  < wPayload >
#define    MS_RESET          0
#define    MS_PRINT          1  // <- any non-zero value will control printout grouping
//  < dwPayload > = DW_DONT_CARE
// -----------------------------------------

// -----------------------------------------
//  override control loop algorithm
#define    MS_CONTROL_ALGOR   72
//  < wPayload >
#define    MS_OPEN_LOOP       0
#define    MS_PID_CTRL        1
//  < dwPayload > = DW_DONT_CARE
// -----------------------------------------

/* =========================================
   <any> --> t_sonar
========================================= */

// -----------------------------------------
//  activate SONAR
//  (turns SONAR on for operation as obstacle detector)
#define    SONAR_ON           80
//  < wPayload >  = sonar trigger value in [cm]
//  < dwPayload > = [mSec] sample interval
// -----------------------------------------

// -----------------------------------------
//  de-activate SONAR
#define    SONAR_OFF          81
//  < wPayload >  = W_DONT_CARE
//  < dwPayload > = DW_DONT_CARE
// -----------------------------------------

// -----------------------------------------
//  initiate SONAR readout
#define    SONAR_READ         82
//  < wPayload >  = ID of requesting task
//  < dwPayload > = DW_DONT_CARE
// -----------------------------------------

/* =========================================
   t_sonar --> <any>
========================================= */

// -----------------------------------------
//  receive SONAR readout result
//  (message will be returned to requesting task see SONAR_READ)
#define    SONAR_DISTANCE     83
//  < wPayload >  = average readout, in [cm]
//  < dwPayload > = high word: high range point
//                  low  word: low range point
// -----------------------------------------

/* =========================================
   t_sonar --> t_ctrlex
========================================= */

// -----------------------------------------
//  SONAR detection trigger
//  (mimic center bumper switch)
#define    SONAR_OBSTALE      SN_BUMPER_SW
//  < wPayload >
#define    SONAR_DETECTED     SN_MID_ON
//  < dwPayload > = DW_DONT_CARE
// -----------------------------------------

/* =========================================
   <any> --> <any>
   Note: destination task must have code
         to respond to PING
========================================= */

// -----------------------------------------
// ping round trip test.
// respponse with wPayload = W_DONT_CARE
#define    MS_PING           90
//  < wPayload >  = source task ID to return
//  < dwPayload > = DW_DONT_CARE
// -----------------------------------------

/* =========================================
   <any> --> t_dsply
========================================= */

// -----------------------------------------
//  7-segment display code
//  < bPayload >  = display data and attribute
//                  d0 - d3 : display code
//                  d4      : reserved ( =1 )
//                  d5      : reserved ( =1 )
//                  d6      : 1 = blink at high rate (4Hz)
//                            0 = blink at low rate  (1Hz)
//                  d7      : 1 = blink enable
//                            0 = no blink
//  < wPayload >  = W_DONT_CARE
//  < dwPayload > = DW_DONT_CARE
// -----------------------------------------

#endif  // __MESSAGES_H__
