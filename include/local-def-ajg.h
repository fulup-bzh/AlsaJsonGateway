/*
   alsajson-gw -- provide a REST/HTTP interface to ALSA-Mixer

   Copyright (C) 2015, Fulup Ar Foll

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   $Id: $
*/

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/signal.h>
#include <sys/types.h>
#include <time.h>
#include <json.h>


/* other definitions --------------------------------------------------- */

typedef int BOOL;
#ifndef FALSE
  #define FALSE 0
#endif
#ifndef TRUE
  #define TRUE 1
#endif

#define PUBLIC
#define STATIC    static
#define MAX_SNDCARDS 5  // number of active Sound Cards

// prebuild json error are constructed in config-ajg
typedef enum  { AJG_FALSE, AJG_TRUE, AJG_FATAL, AJG_FAIL, AJG_WARNING, AJG_EMPTY, AJG_SUCCESS} AJG_ERROR;
extern char *ERROR_LABEL[];
#define ERROR_LABEL_DEF {"false", "true","fatal", "fail", "warning", "empty", "success"}

// Error code are requested through function to manage json usage count
typedef struct {
  int   level;
  char* label;
  json_object *json;
} AJG_ErrorT;

// Post handler
typedef struct {
  char* data;
  int   len;
  int   uid;
} AJG_HttpPost;


// some usefull static object initialized when entering listen loop.
extern int verbose;

typedef struct {
  const char  *cardid; // sound card cardid
  int   quiet;
  int   numid;
  const char *args;
  const char *data;

  void *cardhandle; // use to keep track of last card probed
  char *cardname;   // cardname from alsaCardProbe

} AJG_request;

// main config structure
typedef struct {

  char *logname;           // logfile path for wx2000 info & error log
  char *console;           // console device name (can be a file or a tty)

  int  localhostOnly;
  int   httpdPort;
  char *rootdir;           // base dir for httpd file download
  char *pidfile;           // where to store pid when running background
  char *sessiondir;        // where to store mixer session files
  char *configfile;        // where to store configuration on gateway exit
  uid_t setuid;

  int  cacheTimeout;

} AJG_config;

// Command line stucture hold cli --command + help text
typedef struct {
  int  val;        // command number within application
  int  has_arg;    // command number within application
  char *name;      // command as used in --xxxx cli
  char *help;      // help text
} AJG_options;

typedef struct {
  int  index;
  char *cardid;
  char *name;
  char *info;
} AJG_sndcard;

typedef struct {
  AJG_config  *config;   // pointer to current config

  // List of commands to execute
  int  killPrevious;
  int  background;        // run in backround mode
  int  foreground;        // run in forground mode
  int  checkAlsa;         // Display active Alsa Board
  int  configsave;        // Save config on disk on start

  char *cacheTimeout;     // http require timeout to be a string
  void *httpd;            // anonymous structure for httpd handler
  int  fakemod;           // respond to GET/POST request without interacting with sndboard
  int  forceexit;         // when autoconfig from script force exit before starting server
} AJG_session;

//  List of HTTP Query Commands
typedef enum  {
      CARD_GET_NAME, GATEWAY_PING, CARD_GET_ALL, CARD_GET_ONE, CTRL_GET_ALL,
      CTRL_GET_ONE, CTRL_SET_ONE, CTRL_SET_MANY, SESSION_LIST, SESSION_STORE, SESSION_LOAD
} AJG_REST_CMD;

#include "proto-def-ajg.h"
