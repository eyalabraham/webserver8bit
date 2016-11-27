/* ***************************************************************************

  NAMES.H

  Tasks names for web server

  Nov 20 2016 - Created

*************************************************************************** */

#ifndef     __NAMES_H__
#define     __NAMES_H__

// *** NOTE: task name must be 8 characters or less ***
//                                  "........"
#define     TASK_NAME_TERM          "t_term"        // terminal task for command line and monitoring
#define     TASK_NAME_NTP           "t_ntp"         // system time/clock, and 'ntp' client
#define     TASK_NAME_IP            "t_ip"          // IP stack and ethernet driver
#define     TASK_NAME_WS            "t_ws"          // web server task
#define     TASK_NAME_FILEIO        "t_fileio"      // file IO task and SD card handler
#define     TASK_NAME_LCD           "t_lcd"         // LCD display handler
#define     TASK_NAME_SIO           "t_sio"         // SPI serial interface handler for 't_lcd' and 't_fileio'
#define     TASK_NAME_DUMMY         "t_dummy"       // dummy task for testing, responds only to 'ping' messages

#endif /* __NAMES_H__ */
