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

   Object:
    1) REST Gateway with Alsa Mixer C-API.
    2) Minimalist web server to serve HTML partials and JS files to HTML5 UI.

   Syntax:  ./daemon-ajg --verbose --httpdport=1234 --rootdir=$HOME/public

   Features/Restriction:
    - handle ETAG to limit upload to modified/new files [cache default 3600s]
    - handles redirect to index.htlm when path is a directory [code 301]
    - only support GET method
    - does not follow link.

   References: https://www.gnu.org/software/libmicrohttpd/manual/html_node/index.html#Top
   http://libmicrohttpd.sourcearchive.com/documentation/0.4.2/microhttpd_8h.html
   https://gnunet.org/svn/libmicrohttpd/src/examples/fileserver_example_external_select.c
   https://github.com/json-c/json-c
   $Id: $
*/

#include <microhttpd.h>
#include <sys/stat.h>
#include <unistd.h>

#include "local-def-ajg.h"
#define BANNER "<html><head><title>Alsa Json Gateway</title></head><body>Alsa Json Gateway</body></html>"


//  List of Query Commands
static  json_object * Request2Commands = NULL;
#define GATEWAY_PING   1
#define CARD_GET_ALL   2
#define CARD_GET_ONE   3
#define CTRL_GET_ALL   4
#define CTRL_GET_ONE   5
#define CTRL_SET_ONE   6
#define SESSION_LIST   7
#define SESSION_STORE  8
#define SESSION_LOAD   9

static int rqtcount;  // dummy request rqtcount to make each message be different

// use json lib hash table capabilities to handle command parsing
STATIC void initService (AJG_session *session) {
    rqtcount = 0;
    Request2Commands = json_object_new_object();

    json_object_object_add(Request2Commands, "ping-get"     , json_object_new_int (GATEWAY_PING));
    json_object_object_add(Request2Commands, "card-get-all" , json_object_new_int (CARD_GET_ALL));
    json_object_object_add(Request2Commands, "card-get-one" , json_object_new_int (CARD_GET_ONE));
    json_object_object_add(Request2Commands, "ctrl-get-all" , json_object_new_int (CTRL_GET_ALL));
    json_object_object_add(Request2Commands, "ctrl-get-one" , json_object_new_int (CTRL_GET_ONE));
    json_object_object_add(Request2Commands, "ctrl-set-one" , json_object_new_int (CTRL_SET_ONE));
    json_object_object_add(Request2Commands, "session-list" , json_object_new_int (SESSION_LIST));
    json_object_object_add(Request2Commands, "session-store", json_object_new_int (SESSION_STORE));
    json_object_object_add(Request2Commands, "session-load" , json_object_new_int (SESSION_LOAD));
}

STATIC  json_object *gatewayPing (void) {
    json_object * pingJson = jsonNewMessage (AJG_SUCCESS,"count=%d", rqtcount);
    return (pingJson);
}

// process rest API query
STATIC int requestApi (struct MHD_Connection *connection, AJG_session *session, const char *method) {
  const char  *query, *param;
  json_object *cmd;
  int sndcard, done, ret;
  json_object *jsonResponse, *sndcardJ;
  struct MHD_Response  *response;
  AJG_request request;
  const char *serialized;

  // clean up session
  rqtcount++;
  memset (&request, 0, sizeof (request));
  jsonResponse=NULL;

  // process POST method
  if (0 == strcmp (method, MHD_HTTP_METHOD_POST)) {

      fprintf (stderr, "Post method not implemented [TBD :)]\n");
      return MHD_NO;

  // process GET method
  } else if (0 == strcmp (method, MHD_HTTP_METHOD_GET)) {
      // extract request query attribute from URL through ApiCmd
      query = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "request");
  // ignore any other methods
  } else return MHD_NO;

  // extract command value from json object and process it
  if (query == NULL) goto invalidRequest;
  done=json_object_object_get_ex (Request2Commands, query, &cmd);

  request.sndcard = -1; // no default card
  param = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "sndcard");
  if (param && ! sscanf (param, "%d", &request.sndcard)) goto invalidRequest;

  param = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "quiet");
  if (param && ! sscanf (param, "%d", &request.quiet)) goto invalidRequest;

  request.numid = -1;  // no default
  param = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "numid");
  if (param && ! sscanf (param, "%d", &request.numid)) goto invalidRequest;

  // no default name
  request.args = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "args");

  switch (json_object_get_int(cmd)) {


  	case GATEWAY_PING: // http://localhost:1234/jsonapi?request=ping-get [&sndcard=0]
  	    if (verbose) fprintf (stderr, "%d: alsajson GATEWAY_PING\n", rqtcount);

        if (request.sndcard < 0)  jsonResponse = gatewayPing ();
        else jsonResponse = alsaFindCard (session, &request);
  	    break;

  	case CARD_GET_ALL: // http://localhost:1234/jsonapi?request=card-get-all
  	    if (verbose)  fprintf (stderr, "%d: alsajson CARD_GET_ALL\n", rqtcount ++);
  	    jsonResponse = alsaFindCard (session, &request); // sndcard = -1
  	    break;

  	case CARD_GET_ONE: // http://localhost:1234/jsonapi?request=card-get-one&sndcard=0
  	    if (verbose)  fprintf (stderr, "%d: alsajson CARD_GET_ONE card=%d\n", rqtcount ++, request.sndcard );
   	    if (request.sndcard < 0) goto invalidRequest;
  	    jsonResponse = alsaFindCard (session, &request);
  	    break;

  	case CTRL_GET_ALL: // http://localhost:1234/jsonapi?request=ctrl-get-all&sndcard=0
  	    if (verbose)  fprintf (stderr, "%d: alsajson CTRL_GET_ALL\n", rqtcount ++);
  	    request.numid = -1; // force list-all
  	    jsonResponse = alsaGetControl (session, &request);  // numid == -1
  	    break;

  	case CTRL_GET_ONE: // http://localhost:1234/jsonapi?request=ctrl-get-one&sndcard=2&numid=5&quiet=0
  	    if (verbose)  fprintf (stderr, "%d: alsajson CTRL_GET_ONE card=%d numid=%d\n", rqtcount ++, request.sndcard ,request.numid);
        jsonResponse = alsaGetControl (session, &request);
 	    break;

  	case CTRL_SET_ONE: {// http://localhost:1234/jsonapi?request=ctrl-set-one&sndcard=2&quiet=1&numid=128&args=10,5
  	    if (verbose)  fprintf (stderr, "%d: alsajson processing CTRL_SET_ONE card=%d numid=%d\n", rqtcount ++, request.sndcard ,request.numid);
        jsonResponse = alsaSetControl (session, &request);
 	    break;
 	    }

    case SESSION_LIST:  {// http://localhost:1234/jsonapi?request=session-list&sndcard=0
       jsonResponse = alsaListSession (session, &request); // list session for requested sndcard
       break;
    }

  	case SESSION_LOAD: { // http://localhost:1234/jsonapi?request=session-load&sndcard=2&args=sessionname
  	   json_object *jsonSession;
       if (verbose)  fprintf (stderr, "%d: alsajson SESSION_LOAD card=%d\n", rqtcount ++, request.sndcard );

       jsonResponse = alsaLoadSession (session, &request);  // push session to alsa board
       if (jsonResponse != NULL) break; // we got an error
       jsonResponse = alsaGetControl (session, &request);    // we return effective data from sound board
       break;
   	}

  	case SESSION_STORE: {// http://localhost:1234/jsonapi?request=session-store&sndcard=2&args=sessionname

      if (verbose)  fprintf (stderr, "%d: alsajson SESSION_STORE card=%d session=%d\n", rqtcount ++, request.sndcard, request.args );
      jsonResponse = alsaStoreSession (session, &request);  // push session to alsa board
      break;
   	}



  	default:
  	   goto invalidRequest;
   }


   // send response to client with a http AJG_SUCCESS status code
   // [note we need to copy serialize object because libmicrohttpd does not provide adequate free callback
   if (jsonResponse == NULL) {
      printf ("AJG:DEVBUG Request:%d Query=%s SndCard=%d NumId=%d Response=>NULL [please report bug]\n", rqtcount ++, query, request.sndcard ,request.numid);
      goto invalidRequest;
   }
   serialized = json_object_to_json_string(jsonResponse);
   response = MHD_create_response_from_buffer (strlen (serialized), (void*)serialized, MHD_RESPMEM_MUST_COPY);

    ret = MHD_queue_response (connection, MHD_HTTP_OK, response);
    MHD_destroy_response (response);
    json_object_put (jsonResponse); // decrease reference rqtcount to free the json object
    return ret;

invalidRequest: { // send response to client with a http ERROR status code

    json_object *ajgMessage = jsonNewMessage (AJG_FATAL
        , "Invalid/Unknown Request:%d Query=%s SndCard=%d NumId=%d", rqtcount ++, query, request.sndcard ,request.numid);

    serialized = json_object_to_json_string(ajgMessage);
    response = MHD_create_response_from_buffer (strlen (serialized), (void*)serialized, MHD_RESPMEM_MUST_COPY);

    ret = MHD_queue_response (connection, MHD_HTTP_BAD_REQUEST, response);
    MHD_destroy_response (response);
    json_object_put (jsonResponse); // decrease reference rqtcount to free the json object
    return ret;
    }
}

// Create check etag value
STATIC void computeEtag (char *etag, int maxlen, struct stat *sbuf) {
   int time;

   time = sbuf->st_mtim.tv_sec;
   snprintf (etag, maxlen, "%d", time);
}

// minimal httpd file server for static HTML,JS,CSS,etc...
STATIC requestFile (struct MHD_Connection *connection, AJG_session *session, const char* url) {
    int fd;
    int ret;
    struct stat sbuf;
    struct MHD_Response  *response;
    char filepath [521];

    // build full path from rootdir + url
    strncpy (filepath, session->config->rootdir, sizeof (filepath));
    strncat (filepath, url, sizeof (filepath));

    // try to open file and get its size
    if ( (-1 == (fd = open (filepath, O_RDONLY))) || (0 != fstat (fd, &sbuf)) ) {

          fprintf (stderr, "Fail to open file: [%s] error:%s\n", filepath, strerror(errno));

          // file is not accessible
          if (fd != -1) close (fd);
          const char *errorstr = "<html><body>Alsa-Json-Gateway Unknown or Not readable file</body></html>";
          response = MHD_create_response_from_buffer (strlen (errorstr),
                         (void *) errorstr,	 MHD_RESPMEM_PERSISTENT);
          ret = MHD_queue_response (connection, MHD_HTTP_INTERNAL_SERVER_ERROR, response);
          ret= MHD_YES;


    } else { // open file is OK

        // if url is a directory let's add index.html and redirect client
        if (S_ISDIR (sbuf.st_mode)) {
            strncpy (filepath, url, sizeof (filepath));



            if (url [strlen (url) -1] != '/') strncat (filepath, "/", sizeof (filepath));
            strncat (filepath, "index.html", sizeof (filepath));
            close (fd);
            response = MHD_create_response_from_buffer (0,"", MHD_RESPMEM_PERSISTENT);
            MHD_add_response_header (response,MHD_HTTP_HEADER_LOCATION, filepath);
            ret = MHD_queue_response (connection, MHD_HTTP_MOVED_PERMANENTLY, response);

        } else  if (! S_ISREG (sbuf.st_mode)) { // only standard file any other one including symbolic links are refused.

            fprintf (stderr, "Fail file: [%s] is not a regular file\n", filepath);
            const char *errorstr = "<html><body>Alsa-Json-Gateway Invalid file type</body></html>";
            response = MHD_create_response_from_buffer (strlen (errorstr),
                         (void *) errorstr,	 MHD_RESPMEM_PERSISTENT);
            ret = MHD_queue_response (connection, MHD_HTTP_INTERNAL_SERVER_ERROR, response);

        } else {  // we're facing a regular file, check ETAG and if needed send it
            const char *etagCache;
            char etagValue[15];

            // https://developers.google.com/web/fundamentals/performance/optimizing-content-efficiency/http-caching?hl=fr
            // ftp://ftp.heanet.ie/disk1/www.gnu.org/software/libmicrohttpd/doxygen/dc/d0c/microhttpd_8h.html

            // Check etag value and load file only when modification date changes
            etagCache = MHD_lookup_connection_value(connection, MHD_HEADER_KIND, MHD_HTTP_HEADER_IF_NONE_MATCH);
            computeEtag (etagValue, sizeof (etagValue), &sbuf);

            if (etagCache != NULL && strcmp (etagValue, etagCache) == 0) {

                close (fd); // file did not change since last upload
                if (verbose) fprintf (stderr, "Not Modify: [%s]\n", filepath);
                response = MHD_create_response_from_buffer (0,"", MHD_RESPMEM_PERSISTENT);
                MHD_add_response_header (response,MHD_HTTP_HEADER_CACHE_CONTROL, session->cacheTimeout); // default one hour cache
                MHD_add_response_header (response,MHD_HTTP_HEADER_ETAG, etagValue);
                ret = MHD_queue_response (connection, MHD_HTTP_NOT_MODIFIED, response);

            } else {  // it's a new file, we need to upload it to client
                if (verbose) fprintf (stderr, "Serving: [%s]\n", filepath);
                response =  MHD_create_response_from_fd (sbuf.st_size, fd);
                MHD_add_response_header (response,MHD_HTTP_HEADER_CACHE_CONTROL,session->cacheTimeout); // default one hour cache
                MHD_add_response_header (response,MHD_HTTP_HEADER_ETAG, etagValue);
                ret = MHD_queue_response (connection, MHD_HTTP_OK, response);
            }
        }
    }
    MHD_destroy_response (response);
    return ret;
}

STATIC int newRequest (void *cls,
  struct MHD_Connection *connection,
  const char *url,
  const char *method,
  const char *version,
  const char *upload_data, size_t *upload_data_size, void **ptr) {

  static int aptr;
  AJG_session *session = cls;
  int ret;

  if (0 == strcmp (url, "/jsonapi")) {
       ret = requestApi (connection, session, method);
  } else {
      if (0 != strcmp (method, MHD_HTTP_METHOD_GET)) return MHD_NO;   /* unexpected method */
      ret = requestFile (connection, session, url);
  }

  return ret;
}

STATIC int newClient (void *cls, const struct sockaddr * addr, socklen_t addrlen) {
  // check if client is comming from an acceptable IP
  return (MHD_YES); // MHD_NO
}

PUBLIC AJG_ERROR httpdStart (AJG_session *session) {

  // at 1st call initialise http api hashtable
  if (Request2Commands == NULL) initService (session);

  if (verbose) printf ("starting http port=%d rootdir=%s\n", session->config->httpdPort, session->config->rootdir);

  session->httpd = (void*) MHD_start_daemon (
			MHD_USE_SELECT_INTERNALLY | MHD_USE_DEBUG, // use request and not threads
            session->config->httpdPort,   // port
            &newClient, NULL,       // Tcp Accept call back + extra attribute
            &newRequest, session,  // Http Request Call back + extra attribute
			MHD_OPTION_CONNECTION_TIMEOUT, (unsigned int) 15, MHD_OPTION_END); // 15s + options-end
			// TBD: MHD_OPTION_SOCK_ADDR

  if (session->httpd == NULL) {
     printf ("Error: httpStart invalid httpd port: %d", session->config->httpdPort);
     return AJG_FATAL;
  }
  return AJG_SUCCESS;
}

// infinit loop
PUBLIC AJG_ERROR httpdLoop (AJG_session *session) {
    static int count =0;

    initService(session); // initialise http static data

    if (verbose) fprintf (stderr, "AJG: entering httpd waiting loop\n");
    if (session->foreground) {

        while (TRUE)  {
            fprintf (stderr, "AJG: Use Ctrl-C to quit");
            (void)getc (stdin);
        }
    } else {
        while (TRUE) {
           sleep (3600);
           if (verbose) fprintf (stderr, "AJG:info httpd alive [%d]\n", count++);
        }
    }

    // should never return from here
    return AJG_FATAL;
}


PUBLIC int httpdStatus (AJG_session *session) {
    return (MHD_run (session->httpd));
}

PUBLIC void httpdStop (AJG_session *session) {
  MHD_stop_daemon (session->httpd);
}
