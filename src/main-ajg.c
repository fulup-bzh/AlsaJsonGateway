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

*/



#include "local-def-ajg.h"
#include <syslog.h>
#include <setjmp.h>
#include <signal.h>
#include <getopt.h>

// Define command line option
 #define SET_VERBOSE        101
 #define SET_BACKGROUND     105
 #define SET_FORGROUND      106
 #define KILL_PREV_EXIT     107
 #define KILL_PREV_REST     108

 #define SET_TCP_PORT       111
 #define SET_ROOT_DIR       112
 #define SET_CACHE_TO       113
 #define SET_UID            114
 #define SET_PID_FILE       115
 #define SET_SESSION_DIR    116
 #define SET_CONFIG_FILE    117
 #define SET_CONFIG_SAVE    118

 #define SET_LOCAL_ONLY     120
 #define CHECK_ALSA_CARDS   121

 #define DISPLAY_VERSION    131
 #define DISPLAY_HELP       132

static sigjmp_buf exitpoint; // context save for set/longjmp
static sigjmp_buf restartpoint; // context save for set/longjmp

// Supported option
static  AJG_options cliOptions [] = {
  {SET_VERBOSE      ,0,"verbose"         , "Verbose Mode"},

  {SET_FORGROUND    ,0,"foreground"      , "Get all in foreground mode"},
  {SET_BACKGROUND   ,0,"daemon"          , "Get all in background mode"},
  {KILL_PREV_EXIT   ,0,"kill"            , "Kill active process if any and exit"},
  {KILL_PREV_REST   ,0,"restart"         , "Kill active process if any and restart"},

  {SET_TCP_PORT     ,1,"port"            , "HTTP listening TCP port  [default 1234]"},
  {SET_ROOT_DIR     ,1,"rootdir"         , "HTTP Root Directory [default $PWD/public"},
  {SET_CACHE_TO     ,1,"cache-eol"       , "Client cache end of live [default 3600s]"},
  {SET_UID          ,1,"setuid"          , "Change user id [default don't change]"},
  {SET_PID_FILE     ,1,"pidfile"         , "PID file path [default none]"},
  {SET_SESSION_DIR  ,1,"sessiondir"      , "Sessions file path [default rootdir/sessions]"},
  {SET_CONFIG_FILE  ,1,"config"          , "Config Filename [default rootdir/sessions/configs/default.ajg]"},
  {SET_CONFIG_SAVE  ,0,"save"            , "Save config on disk [default no]"},

  //  {SET_LOCAL_ONLY   ,0,"localhost"       , "Restric client to localhost"},
  {CHECK_ALSA_CARDS ,0,"checkalsa"       , "List Alsa Sound Card"},

  {DISPLAY_VERSION  ,0,"version"         , "Display version and copyright"},
  {DISPLAY_HELP     ,0,"help"            , "Display this help"},
  {0, 0, 0}
 };

// GNU CLI getopts nterface.
struct option ggcOption;
struct option *gnuOptions;

/*----------------------------------------------------------
 | signalQuit
 |  return to intitial exitpoint on order to close backend
 |  before exiting.
 +--------------------------------------------------------- */
void signalQuit (int signum)
{
  if (verbose) printf ("INF:signalQuit received signal to quit\n");
  longjmp (exitpoint, signum);
}

/*----------------------------------------------------------
 | timeout signalQuit
 |
 +--------------------------------------------------------- */
void signalFail (int signum) {

  sigset_t sigset;

  // unlock timeout signal to allow a new signal to come
  sigemptyset (&sigset);
  sigaddset   (&sigset, SIGABRT);
  sigprocmask (SIG_UNBLOCK, &sigset, 0);

  fprintf (stderr, "%s ERR:getAllBlock acquisition timeout\n",configTime());
  syslog (LOG_ERR, "Daemon fail and restart [please report bug]");
  longjmp (restartpoint, signum);
}


/*----------------------------------------------------------
 | printversion
 |   print version and copyright
 +--------------------------------------------------------- */
 static void printVersion (void) {

   fprintf (stderr,"\n----------------------------------------- \n");
   fprintf (stderr,"|  AJG [Alsa Json Gateway] version=%3.2f |\n", AJQ_VERSION);
   fprintf (stderr,"----------------------------------------- \n");
   fprintf (stderr,"|  Copyright(C) 2015 Fulup Ar Foll [fulup -at- breizhme.net]\n");
   fprintf (stderr,"|  AJG comes with ABSOLUTELY NO WARRANTY.\n");
   fprintf (stderr,"|  Licence [what ever makes you happy] until you fix bugs by yourself :)\n\n");
   exit (0);

 } // end printVersion

/*----------------------------------------------------------
 | printHelp
 |   print information from long option array
 +--------------------------------------------------------- */

 static void printHelp(char *name) {
    int ind;
    char command[20];

    fprintf (stderr,"%s:\nallowed options\n", name);
    for (ind=0; cliOptions [ind].name != NULL;ind++)
    {
      // display options
      if (cliOptions [ind].has_arg == 0 )
      {
	     fprintf (stderr,"  --%-15s %s\n", cliOptions [ind].name, cliOptions[ind].help);
      } else {
         sprintf(command,"%s=xxxx", cliOptions [ind].name);
         fprintf (stderr,"  --%-15s %s\n", command, cliOptions[ind].help);
      }
    }
    fprintf (stderr,"Example:\n  %s\\\n  --verbose --checkalsa\n", name);
} // end printHelp

/*----------------------------------------------------------
 | writePidFile
 |   write a file in /var/run/alsajson-gw with pid
 +--------------------------------------------------------- */
static int writePidFile (AJG_config *config, int pid) {
  FILE *file;

  // if no pid file configure just return
  if (config->pidfile == NULL) return 0;

  // open pid file in write mode
  file = fopen(config->pidfile,"w");
  if (file == NULL) {
    fprintf (stderr,"%s ERR:writePidFile fail to open [%s]\n",configTime(), config->pidfile);
    return -1;
  }

  // write pid in file and close
  fprintf (file, "%d\n", pid);
  fclose  (file);
}

/*----------------------------------------------------------
 | readPidFile
 |   read file in /var/run/alsajson-gw with pid
 +--------------------------------------------------------- */
static int readPidFile (AJG_config *config) {
  int  pid;
  FILE *file;
  int  status;

  if (config->pidfile == NULL) return -1;

  // open pid file in write mode
  file = fopen(config->pidfile,"r");
  if (file == NULL) {
    fprintf (stderr,"%s ERR:readPidFile fail to open [%s]\n",configTime(), config->pidfile);
    return -1;
  }

  // write pid in file and close
  status = fscanf  (file, "%d\n", &pid);
  fclose  (file);

  // never kill pid 0
  if (status != 1) return -1;

  return (pid);
}

/*----------------------------------------------------------
 | closeSession
 |   try to close everything before leaving
 +--------------------------------------------------------- */
static void closeSession (AJG_session *session) {


}


/*----------------------------------------------------------
 | listenLoop
 |   Main listening HTTP loop
 +--------------------------------------------------------- */
static void listenLoop (AJG_session *session) {
  AJG_ERROR  err;

  if (signal (SIGABRT, signalFail) == SIG_ERR) {
        fprintf (stderr, "%s ERR: main fail to install Signal handler\n", configTime());
        return;
  }

  // ------ Start httpd server
  if (session->config->httpdPort > 0) {

       // if no rootdir let's use current dir
       if (session->config->rootdir == NULL) session->config->rootdir=getcwd(NULL,0);

        err = httpdStart (session);
        if (err != AJG_SUCCESS) return;

        // infinite loop
        httpdLoop(session);

        fprintf (stderr, "hoops returned from infinite loop [report bug]\n");
  }
}

/*----------------------------------------------------------
 | probeAlsa
 |   Probe ALSA and list sound cards. This is a simple
 |   test function for json object structure
 +--------------------------------------------------------- */
static AJG_ERROR probeAlsa (AJG_session *session) {
      AJG_request request;
      json_object *sndcards, *sndlist, *sndcard, *slot, *element;
      int index, idx, length;
      char const *uid, *name, *info;

      // create a dummy HTTP request and probe sound cards
      memset (&request,0,sizeof (request));
      sndlist =  alsaFindCard(session, &request);

      // search for sound card descriptor & get sndcard list
      if (! json_object_object_get_ex (sndlist, "ajgtype", &element))   return AJG_FAIL;
      if (!strcmp (json_object_get_string (element), "AJG_sndcards"))   return AJG_FAIL;
      if (! json_object_object_get_ex (sndlist, "sndcards", &sndcards)) return AJG_FAIL;

      length = json_object_array_length (sndcards);
      fprintf (stderr,"\n---- Check Alsa [%d] cards ------\n", length);

      // scan through json object [note json object are not really design for this, but nice to check json structure before exporting them ]
      for (idx=0; idx < length; idx++) {
         sndcard = json_object_array_get_idx(sndcards, idx);

         json_object_object_get_ex (sndcard, "index", &slot);
         index = json_object_get_int (slot);

         json_object_object_get_ex (sndcard, "uid", &slot);
         uid = json_object_get_string (slot);

         json_object_object_get_ex (sndcard, "name", &slot);
         name = json_object_get_string (slot);

         json_object_object_get_ex (sndcard, "info", &slot);
         info = json_object_get_string (slot);

         fprintf (stderr, " + %-2d uid=%-5s name:%-25s [%s]\n", index, uid, name, info);
      }
      fprintf (stderr,"---- Check Alsa Done ------");

  return AJG_SUCCESS;
}

/*---------------------------------------------------------
 | main
 |   Parse option and launch action
 +--------------------------------------------------------- */

int main(int argc, char *argv[])  {
  AJG_session    *session;
  char*          programName = argv [0];
  int            optionIndex = 0;
  int            optc, ind, consoleFD;
  int            pid, nbcmd, status;
  AJG_config     cliconfig; // temp structure to store CLI option before file config upload

  // ------------- Build session handler & init config -------
  session =  configInit ();
  memset (&cliconfig,0,sizeof(cliconfig));


  // ------------------ Process Command Line -----------------------

  // if no argument print help and return
  if (argc < 2) {
       printHelp(programName);
       return (-1);
  }

  // build GNU getopt info from cliOptions
  nbcmd = sizeof (cliOptions) / sizeof (AJG_options);
  gnuOptions = malloc (sizeof (ggcOption) * nbcmd);
  for (ind=0; ind < nbcmd;ind++) {
    gnuOptions [ind].name    = cliOptions[ind].name;
    gnuOptions [ind].has_arg = cliOptions[ind].has_arg;
    gnuOptions [ind].flag    = 0;
    gnuOptions [ind].val     = cliOptions[ind].val;
  }

  // get all options from command line
  while ((optc = getopt_long (argc, argv, "vsp?", gnuOptions, &optionIndex))
        != EOF)
  {
    switch (optc)
    {
     case SET_VERBOSE:
       verbose = 1;
       break;

    case SET_TCP_PORT:
       if (optarg == 0) goto needValueForOption;
       if (!sscanf (optarg, "%d", &cliconfig.httpdPort)) goto notAnInteger;
       break;

    case SET_ROOT_DIR:
       if (optarg == 0) goto needValueForOption;
       cliconfig.rootdir   = optarg;
       break;

    case SET_PID_FILE:
       if (optarg == 0) goto needValueForOption;
       cliconfig.pidfile   = optarg;
       break;

    case SET_SESSION_DIR:
       if (optarg == 0) goto needValueForOption;
       cliconfig.sessiondir   = optarg;
       break;

    case  SET_CONFIG_FILE:
       if (optarg == 0) goto needValueForOption;
       cliconfig.configfile   = optarg;
       break;

    case  SET_CACHE_TO:
       if (optarg == 0) goto needValueForOption;
       if (!sscanf (optarg, "%d", &cliconfig.cacheTimeout)) goto notAnInteger;
       break;


    case SET_CONFIG_SAVE:
       if (optarg != 0) goto noValueForOption;
       session->configsave  = 1;
       break;

    case SET_UID:
       if (optarg != 0) goto noValueForOption;
       if (!sscanf (optarg, "%d", &cliconfig.setuid)) goto notAnInteger;
       break;

    case SET_FORGROUND:
       if (optarg != 0) goto noValueForOption;
       session->foreground  = 1;
       break;

    case SET_BACKGROUND:
       if (optarg != 0) goto noValueForOption;
       session->background  = 1;
       break;

     case KILL_PREV_REST:
       if (optarg != 0) goto noValueForOption;
       session->killPrevious  = 1;
       break;

     case KILL_PREV_EXIT:
       if (optarg != 0) goto noValueForOption;
       session->killPrevious  = 2;
       break;

    case DISPLAY_VERSION:
       if (optarg != 0) goto noValueForOption;
       printVersion();
       goto normalExit;

    case DISPLAY_HELP:
     default:
       printHelp(programName);
       goto normalExit;

    case CHECK_ALSA_CARDS:
       if (probeAlsa (session) != AJG_SUCCESS) goto errSoundCard;
       goto normalExit;

    }
  }

  // Create session config
  configInit (/* session & config are initialized globally */);

  // ------------------ sanity check ----------------------------------------
  if  ((session->background) && (session->foreground)) {
    fprintf (stderr, "%s ERR: cannot select foreground & background at the same time\n",configTime());
     exit (-1);
  }

  // ------------------ Some useful default values -------------------------
  if  ((session->background == 0) && (session->foreground == 0)) session->foreground=1;

  // open syslog if ever needed
  openlog("alsajson-gw", 0, LOG_DAEMON);

  // -------------- Try to kill any previsou process if asked ---------------------
  if (session->killPrevious) {
    pid = readPidFile (&cliconfig);  // enforce commandline option
    switch (pid) {
    case -1:
      fprintf (stderr, "%s ERR:main --kill ignored no PID file [%s]\n",configTime(), cliconfig.pidfile);
      break;
    case 0:
      fprintf (stderr, "%s ERR:main --kill ignored no active alsajson-gw process\n",configTime());
      break;
    default:
      status = kill (pid,SIGINT );
      if (status == 0) {
	     if (verbose) printf ("%s INF:main signal INTR sent to pid:%d \n", configTime(), pid);
      } else {
         // try kill -9
         status = kill (pid,9);
         if (status != 0)  fprintf (stderr, "%s ERR:main failled to killed pid=%d \n",configTime(), pid);
      }
    } // end switch pid

    if (session->killPrevious >= 2) goto normalExit;
  } // end killPrevious



  // ------------------ clean exit on CTR-C signal ------------------------
  if (signal (SIGINT, signalQuit) == SIG_ERR) {
    fprintf (stderr, "%s Quit Signal received.",configTime());
    return (-1);
  }

  // save exitpoint context when returning from longjmp closeSession and exit
  status = setjmp (exitpoint); // return !+ when coming from longjmp
  if (status != 0) {
    if (verbose) printf ("INF:main returning from longjump after signal [%d]\n", status);
    closeSession (session);
    goto exitOnSignal;
  }

  // let's run this program with a low priority
  nice (20);


  // ------------------ Finaly Process Commands -----------------------------

   // if exist merge config file with CLI arguments
   configLoadFile  (session, &cliconfig);

   // if --save then store config on disk upfront
   if (session->configsave) configStoreFile (session);

    if (session->config->setuid) {
        int err;

        err = setuid(session->config->setuid);
        if (err) error ("Fail to change program uid error=%d", snd_strerror(err));
    }

    // let's not take the risk to run as ROOT
    if (getuid == 0)  setuid(65534);  // run as nobody

    // check session dir and create if it does not exist
    if (sessionCheckdir (session) != AJG_SUCCESS) goto errSessiondir;
    if (verbose) fprintf (stderr, "AJG: Init config done\n");



    // ---- run in foreground mode --------------------
    if (session->foreground) {

        if (verbose) fprintf (stderr,"AJG: Foreground mode\n");

        // write a pid file for --kill-previous and --raise-debug option
        status = writePidFile (session->config, getpid());
        if (status == -1) goto errorPidFile;

        // enter listening loop in foreground
        listenLoop(session);
        goto exitInitLoop;
  } // end foreground


  // --------- run in background mode -----------
  if (session->background) {

      // check first we can talk with ALSA board
      // if (status != 0) goto errorCommand;
      if (verbose) printf ("AJG: Entering background mode\n");

      // open /dev/console to redirect output messAJGes
      consoleFD = open(session->config->console, O_WRONLY | O_APPEND | O_CREAT , 0640);
      if (consoleFD < 0) goto errConsole;

      // fork process when running background mode
      pid = fork ();

      // son process get all data in standalone mode
      if (pid == 0) {

 	     printf ("\nAJG: background mode [pid:%d console:%s]\n", getpid(),session->config->console);
 	     if (verbose) printf ("AJG:info use '%s --kill --pidfile=%s' # to exit daemon\n", programName,session->config->pidfile);

         // redirect default I/O on console
         close (2); dup(consoleFD);  // redirect stderr
         close (1); dup(consoleFD);  // redirect stdout
         close (0);           // no need for stdin
         close (consoleFD);

    	 setsid();   // allow father process to fully exit
	     sleep (2);  // allow main to leave and release port

         fprintf (stderr, "----------------------------\n", getpid());
         fprintf (stderr, "%s INF:main background pid=%d\n", configTime(), getpid());
         fflush  (stderr);

         // if everything look OK then look forever
         syslog (LOG_ERR, "AJG: Entering infinite loop in background mode");

         // should normally never return from this loop
         listenLoop(session);
         syslog (LOG_ERR, "AJG:FAIL background infinite loop exited check [%s]\n", session->config->console);

         goto exitInitLoop;
      }

      // if fail nothing much to do
      if (pid == -1) goto errorFork;

      // fork worked and we are in father process
      status = writePidFile (session->config, pid);
      if (status == -1) goto errorPidFile;

      // we are in father process, we don't need this one
      exit (0);

  } // end background-foreground

normalExit:
  closeSession (session);   // try to close everything before leaving
  if (verbose) printf ("\n---- Alsa Json Gateway Normal End ------\n");
  exit (0);

// ------------- Fatal ERROR display error and quit  -------------
errorPidFile:
  fprintf (stderr,"\nERR:main Failled to write pid file [%s]\n\n", session->config->pidfile);
  exit (-1);

errorSon:
  fprintf (stderr,"\nERR:main Son Process Failled to get data from station\n\n");
  exit (-1);

errorFork:
  fprintf (stderr,"\nERR:main Failled to fork son process\n\n");
  exit (-1);

errorCommand:
  fprintf (stderr,"\nERR:main Failled to send/get command \n\n");
  exit (-1);

invalidDisplay:
  fprintf (stderr,"\nERR:main cannot open configured display\n\n");
  exit (-1);

invalidPort:
  fprintf (stderr,"\nERR:main cannot open configured communication port\n\n");
  exit (-1);

needValueForOption:
  fprintf (stderr,"\nERR:main option [--%s] need a value i.e. --%s=xxx\n\n"
          ,gnuOptions[optionIndex].name, gnuOptions[optionIndex].name);
  exit (-1);

noValueForOption:
  fprintf (stderr,"\nERR:main option [--%s] don't take value\n\n"
          ,gnuOptions[optionIndex].name);
  exit (-1);

notAnInteger:
  fprintf (stderr,"\nERR:main option [--%s] requirer an interger i.e. --%s=9\n\n"
          ,gnuOptions[optionIndex].name, gnuOptions[optionIndex].name);
  exit (-1);

errorSyntax:
  fprintf (stderr,"\nERR:main no or invalid parameters parameters\n\n");
  printHelp (programName);
  exit (-1);

exitOnSignal:
  fprintf (stderr,"\n%s INF:main pid=%d received exit signal (Hopefully crtl-C or --kill-previous !!!)\n\n"
                 ,configTime(), getpid());
  exit (-1);

errConsole:
  fprintf (stderr,"\nERR:cannot open /dev/console (use --foreground)\n\n");
  exit (-1);

errSessiondir:
  fprintf (stderr,"\nERR:cannot read/write session dir\n\n");
  exit (-1);

errSoundCard:
  fprintf (stderr,"\nERR:fail to probe sound cards\n\n");
  exit (-1);

exitInitLoop:
  // try to unlink pid file if any
  if (session->background && session->config->pidfile != NULL)  unlink (session->config->pidfile);
  exit (-1);

} /* END main() */
