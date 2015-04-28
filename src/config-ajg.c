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

   References:
     https://www.gnu.org/software/libmicrohttpd/manual/html_node/index.html#Top
     http://www-01.ibm.com/support/knowledgecenter/SSB23S_1.1.0.9/com.ibm.ztpf-ztpfdf.doc_put.09/gtpc2/cpp_vsprintf.html?cp=SSB23S_1.1.0.9%2F0-3-8-1-0-16-8

*/


#include "local-def-ajg.h"
#include <stdarg.h>
#include <sys/stat.h>
#include <sys/types.h>


#define AJG_CONFIG_JTYPE "AJG_config"

PUBLIC  char *ERROR_LABEL[]=ERROR_LABEL_DEF;

PUBLIC int verbose;
STATIC AJG_ErrorT  AJG_Error [AJG_SUCCESS+1];
STATIC json_object *ajgJsonType;

/* ------------------------------------------------------------------------------
 * Get localtime and return in a string
 * ------------------------------------------------------------------------------ */

PUBLIC char * configTime (void) {
  static char reqTime [26];
  time_t tt;
  struct tm *rt;

  /* Get actual Date and Time */
  time (&tt);
  rt = localtime (&tt);

  strftime (reqTime, sizeof (reqTime), "(%d-%b %H:%M)",rt);

  // return pointer on static data
  return (reqTime);
}

// loaf config from disk and merge with CLI option
PUBLIC AJG_ERROR configLoadFile (AJG_session * session, AJG_config *cliconfig) {
   static char cacheTimeout [10];
   int fd;
   json_object * ajgConfig, *value;

   // default HTTP port
   if (cliconfig->httpdPort == 0) session->config->httpdPort=1234;
   else session->config->httpdPort=cliconfig->httpdPort;

   // cache timeout default one hour
   if (cliconfig->cacheTimeout == 0) session->config->cacheTimeout=3600;
   else session->config->cacheTimeout=cliconfig->cacheTimeout;

   if (cliconfig->rootdir == NULL) {
       session->config->rootdir = getenv("AJGDIR");
       if (session->config->rootdir == NULL) {
           session->config->rootdir = malloc (512);
           strncpy  (session->config->rootdir, getenv("HOME"),512);
           strncat (session->config->rootdir, "/.ajg",512);
       }

       // if directory does not exist createit
       mkdir (session->config->rootdir,  O_RDWR | S_IRWXU | S_IRGRP);
   } else {
       session->config->rootdir =  cliconfig->rootdir;
   }

   // if no session dir create a default path from rootdir
   if  (cliconfig->sessiondir == NULL) {
       session->config->sessiondir = malloc (512);
       strncpy (session->config->sessiondir, session->config->rootdir, 512);
       strncat (session->config->sessiondir, "/sessions",512);
   } else {
       session->config->sessiondir = cliconfig->sessiondir;
   }

   // if no config dir create a default path from sessiondir
   if  (cliconfig->configfile == NULL) {
       session->config->configfile = malloc (512);
       strncpy (session->config->configfile, session->config->sessiondir, 512);
       strncat (session->config->configfile, "/AJG-config.json",512);
   } else {
       session->config->configfile = cliconfig->configfile;
   }

   // if no config dir create a default path from sessiondir
   if  (cliconfig->pidfile == NULL) {
       session->config->pidfile = malloc (512);
       strncpy (session->config->pidfile, session->config->sessiondir, 512);
       strncat (session->config->pidfile, "/AJG-process.pid",512);
   } else {
       session->config->pidfile= cliconfig->pidfile;
   }

   // if no config dir create a default path from sessiondir
   if  (cliconfig->console == NULL) {
       session->config->console = malloc (512);
       strncpy (session->config->console, session->config->sessiondir, 512);
       strncat (session->config->console, "/AJG-console.out",512);
   } else {
       session->config->console= cliconfig->console;
   }

   // just upload json object and return without any further processing
   if((fd = open(session->config->configfile, O_RDONLY)) < 0) {
      if (verbose) fprintf (stderr, "AJG:warning: config at %s: %s\n", session->config->configfile, strerror(errno));
      return AJG_EMPTY;
   }

   // openjson from FD is not public API we need to reopen it !!!
   close(fd);
   ajgConfig = json_object_from_file (session->config->configfile);

   // check it is an AJG_config
   if (json_object_object_get_ex (ajgConfig, "ajgtype", &value)) {
      if (strcmp (AJG_CONFIG_JTYPE, json_object_get_string (value))) {
         fprintf (stderr,"AJG: Error file [%s] is not a valid [%s] type\n ", session->config->configfile, AJG_CONFIG_JTYPE);
         return AJG_FAIL;
      }
   }

   if (!cliconfig->rootdir && json_object_object_get_ex (ajgConfig, "rootdir", &value)) {
      session->config->rootdir =  strdup (json_object_get_string (value));
   }

   if (!cliconfig->sessiondir && json_object_object_get_ex (ajgConfig, "sessiondir", &value)) {
      session->config->sessiondir = strdup (json_object_get_string (value));
   }
   
   if (!cliconfig->pidfile && json_object_object_get_ex (ajgConfig, "pidfile", &value)) {
      session->config->pidfile = strdup (json_object_get_string (value));
   }

   if (!cliconfig->httpdPort && json_object_object_get_ex (ajgConfig, "httpdPort", &value)) {
      session->config->httpdPort = json_object_get_int (value);
   }
   
   if (!cliconfig->setuid && json_object_object_get_ex (ajgConfig, "setuid", &value)) {
      session->config->setuid = json_object_get_int (value);
   }

   if (!cliconfig->localhostOnly && json_object_object_get_ex (ajgConfig, "localhostonly", &value)) {
      session->config->localhostOnly = json_object_get_int (value);
   }
   
   if (!cliconfig->cacheTimeout && json_object_object_get_ex (ajgConfig, "cachetimeout", &value)) {
      session->config->cacheTimeout = json_object_get_int (value);
   }
   // cacheTimeout is an interger but HTTPd wants it as a string
   snprintf (cacheTimeout, sizeof (cacheTimeout),"%d", session->config->cacheTimeout);
   session->cacheTimeout = cacheTimeout; // httpd uses cacheTimeout string version
   json_object_put   (ajgConfig);    // decrease reference count to free the json object

   return AJG_SUCCESS;
}

// Save the config on disk
PUBLIC void configStoreFile (AJG_session * session) {
   json_object * ajgConfig;
   time_t rawtime;
   struct tm * timeinfo;
   int err;

   ajgConfig =  json_object_new_object();

   // add a timestamp and store session on disk
   time ( &rawtime );  timeinfo = localtime ( &rawtime );
   // A copy of the string is made and the memory is managed by the json_object
   json_object_object_add (ajgConfig, "ajgtype"         , json_object_new_string (AJG_CONFIG_JTYPE));
   json_object_object_add (ajgConfig, "timestamp"    , json_object_new_string (asctime (timeinfo)));
   json_object_object_add (ajgConfig, "rootdir"      , json_object_new_string (session->config->rootdir));
   json_object_object_add (ajgConfig, "sessiondir"   , json_object_new_string (session->config->sessiondir));
   json_object_object_add (ajgConfig, "pidfile"      , json_object_new_string (session->config->pidfile));
   json_object_object_add (ajgConfig, "httpdPort"    , json_object_new_int (session->config->httpdPort));
   json_object_object_add (ajgConfig, "setuid"       , json_object_new_int (session->config->setuid));
   json_object_object_add (ajgConfig, "localhostonly", json_object_new_int (session->config->localhostOnly));
   json_object_object_add (ajgConfig, "cachetimeout" , json_object_new_int (session->config->cacheTimeout));

   err = json_object_to_file (session->config->configfile, ajgConfig);
   json_object_put   (ajgConfig);    // decrease reference count to free the json object
   if (err < 0) {
      fprintf(stderr, "AJG: Fail to save config on disk [%s]\n ", session->config->configfile);
   }
}


PUBLIC AJG_session *configInit () {

  AJG_session *session;
  AJG_config  *config;
  int idx, verbosesav;


  session = malloc (sizeof (AJG_session));
  memset (session,0, sizeof (AJG_session));

  // create config handle
  config = malloc (sizeof (AJG_config));
  memset (config,0, sizeof (AJG_config));

  // stack config handle into session
  session->config = config;

  ajgJsonType = json_object_new_string ("AJG_message");

  // initialize JSON constant messages and increase reference count to make them permanent
  verbosesav = verbose;
  verbose = 0;  // run initialisation in silent mode



  for (idx = 0; idx <= AJG_SUCCESS; idx++) {
     AJG_Error[idx].level = idx;
     AJG_Error[idx].label = ERROR_LABEL [idx];
     AJG_Error[idx].json  = jsonNewMessage (idx, NULL);
  }
  verbose = verbosesav;
  return (session);
}


// get JSON object from error level and increase its reference count
PUBLIC json_object *jsonNewStatus (AJG_ERROR level) {

  json_object *target =  AJG_Error[level].json;
  json_object_get (target);

  return (target);
}

// get AJG object type with adequate usage count
PUBLIC json_object *jsonNewAjgType (void) {
  json_object_get (ajgJsonType); // increase reference count
  return (ajgJsonType);
}

// build an ERROR message and return it as a valid json object
PUBLIC  json_object *jsonNewMessage (AJG_ERROR level, char* format, ...) {
   static int count = 0;
   json_object * ajgResponse;
   va_list args;
   char message [512];

   // format message
   if (format != NULL) {
       va_start(args, format);
       vsnprintf (message, sizeof (message), format, args);
       va_end(args);
   }

   ajgResponse = json_object_new_object();
   json_object_object_add (ajgResponse, "ajgtype", jsonNewAjgType ());
   json_object_object_add (ajgResponse, "status" , json_object_new_string (ERROR_LABEL[level]));
   if (format != NULL) {
        json_object_object_add (ajgResponse, "info"   , json_object_new_string (message));
   }
   if (verbose) {
        fprintf (stderr, "AJG:%-6s [%3d]: ", AJG_Error [level].label, count++);
        if (format != NULL) {
            fprintf (stderr, message);
        } else {
            fprintf (stderr, "No Message");
        }
        fprintf (stderr, "\n");
   }

   return (ajgResponse);
}

// Dump a message on stderr
PUBLIC void jsonDumpObject (json_object * jObject) {

   if (verbose) {
        fprintf (stderr, "AJG:dump [%s]\n", json_object_to_json_string(jObject));
   }
}

