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
// message formats
// -----------------------------
//  1. messages are defined by the originator
//     including message responses
//  2. message name has an abbreviation of the
//     originator task name
//  3. message #define line contains '@@' to
//     mark line to be read be parser
// =========================================

/* =========================================
   any
========================================= */

// -----------------------------------------
//  send 'ping' to a task
#define		ANY_PING				10					// @@  send a ping request to a task
//  < wPayload >  = originator task ID
//  < dwPayload > = DW_DONT_CARE
// -----------------------------------------

// -----------------------------------------
//  'ping' response
#define		ANY_PING_RESP			11					// @@  respond to a ping request
//  < wPayload >  = W_DONT_CARE
//  < dwPayload > = DW_DONT_CARE
// -----------------------------------------

#endif  /* __MESSAGES_H__ */
