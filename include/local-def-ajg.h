/*
   alsa-gateway -- provide a REST/HTTP interface to ALSA-Mixer

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
#define STATUS  void*
#define ERROR   -1
#define FATAL   (void*)-1
#define SUCCESS (void*)0
#define PUBLIC
#define STATIC    static
#define MAX_SNDCARDS 5  // number of active Sound Cards

extern int verbose;

typedef struct {

  int  basicmod;
  int  ctrlid;

} AJG_request;

// main config structure
typedef struct {

  char *logname;           // logfile path for wx2000 info & error log
  char *console;           // console device name (can be a file or a tty)
  char *confname;          // path to config file
  char *pidFile;          // where to store pid when running background

  int  localhostOnly;
  int  httpdPort;
  char *rootdir;

  char cacheTimeout [5];

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
  char *uid;
  char *name;
  char *info;
} AJG_sndcard;

typedef struct {
   AJG_config  *config;
   json_object *sndcards;

   // List of command to execute
  int  killPrevious;

  // level of execution
  int  background;        // run in backround mode
  int  forground;         // run in forground mode
  int  debugLevel;        // 0-9
  int  checkAlsa;         // Display active Alsa Board


  void *httpd;            // anonymous structure for httpd handler
} AJG_session;



#include "proto-def-ajg.h"
