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
   $Id: $
*/

#include <microhttpd.h>
#include <sys/stat.h>
#include <unistd.h>

#include "local-def-ajg.h"
#define BANNER "<html><head><title>Alsa Json Gateway</title></head><body>Alsa Json Gateway</body></html>"


//  List of Query Commands
static  json_object * Request2Commands = NULL;
#define GATEWAY_PING 1
#define SND_LIST_ALL 2
#define SND_LIST_ONE 3

// use json lib hash table capabilities to handle command parsing
STATIC void initService (AJG_session *session) {

    Request2Commands = json_object_new_object();

    json_object_object_add(Request2Commands, "get-ping"     , json_object_new_int (GATEWAY_PING));
    json_object_object_add(Request2Commands, "get-cards"    , json_object_new_int (SND_LIST_ALL));
    json_object_object_add(Request2Commands, "get-controls" , json_object_new_int (SND_LIST_ONE));

}

STATIC  json_object *gatewayPing (void) {
    static int count;

    json_object * pingJson = json_object_new_object();
	json_object_object_add (pingJson, "ping", json_object_new_int (count++));

    return (pingJson);
}

// process rest API request
STATIC int requestApi (struct MHD_Connection *connection, AJG_session *session, const char *method) {
  static int count;
  const char  *request, *param;
  json_object *cmd;
  int sndcard, done, ret;
  json_object *jsonResponse, *sndcardJ;
  struct MHD_Response  *response;
  AJG_request select;


  // process POST method
  if (0 == strcmp (method, MHD_HTTP_METHOD_POST)) {

      fprintf (stderr, "Post method not implemented [TBD :)]\n");
      return MHD_NO;

  // process GET method
  } else if (0 == strcmp (method, MHD_HTTP_METHOD_GET)) {
      // extract request request attribute from URL through ApiCmd
      request = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "request");


  // ignore any other methods
  } else return MHD_NO;
  
  if (request == NULL) return MHD_NO;

  // extract command value from json object and process it
  done=json_object_object_get_ex (Request2Commands, request, &cmd);
  switch (json_object_get_int(cmd)) {

  	case GATEWAY_PING: // http://localhost:1234/alsa-json?request=gateway-ping
  	    if (verbose) fprintf (stderr, "%d: alsa-json processing GATEWAY_PING\n", count++);
  	     jsonResponse = gatewayPing ();
  	    break;

  	case SND_LIST_ALL: // http://localhost:1234/alsa-json?request=get-cards
  	    if (verbose)  fprintf (stderr, "%d: alsa-json processing SND_LIST_ALL\n", count ++);
  	     jsonResponse = alsaFindCards (session);
  	    break;

  	case SND_LIST_ONE: // http://localhost:1234/alsa-json?request=get-controls&sndcard=2
  	    if (verbose)  fprintf (stderr, "%d: alsa-json processing SND_LIST_ONE\n", count ++);
  	    sndcard = 0; // default card 0
  	    param = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "sndcard");
  	    if (param && ! sscanf (param, "%d", &sndcard)) goto notAnInteger;

  	    param = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "basic");
  	    select.basicmod = 0; // default full mode
  	    if (param && ! sscanf (param, "%d", &select.basicmod)) goto notAnInteger;

  	    param = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "ctrlid");
  	    select.ctrlid = 0;  // default all controls
  	    if (param && ! sscanf (param, "%d", &select.ctrlid)) goto notAnInteger;

  	    sndcardJ = json_object_array_get_idx(alsaFindCards(session), sndcard);
  	    if (sndcardJ == NULL)  goto invalidRequest;
        jsonResponse = alsaFindControls (session, sndcardJ, &select);
 	    break;

  	default: goto invalidRequest;
   }

   // return json object to client [note we need to copy serialize object because libmicrohttpd does not provide adequate free callback
   response = MHD_create_response_from_buffer
              (strlen (json_object_to_json_string(jsonResponse)),
   	 	      (void *) json_object_to_json_string(jsonResponse),
   		      MHD_RESPMEM_MUST_COPY);

  ret = MHD_queue_response (connection, MHD_HTTP_OK, response);
  MHD_destroy_response (response);
  json_object_put (jsonResponse); // decrease reference count to free the json object
  return ret;

invalidRequest: {
          fprintf (stderr, "%d: alsa-json Unknown/Invalid API/Card request=%s\n", count ++, request);
          const char *errorstr = "<html><body>Alsa-Json-Gateway Invalid/Unknown Request</body></html>";
          response = MHD_create_response_from_buffer (strlen (errorstr),
                           (void *) errorstr,	 MHD_RESPMEM_PERSISTENT);
          if (response) {
              ret = MHD_queue_response (connection, MHD_HTTP_INTERNAL_SERVER_ERROR, response);
              MHD_destroy_response (response);
              ret= MHD_YES;
          } else {
              ret= MHD_NO;
          }
          return ret;
}

notAnInteger:
  fprintf (stderr,"\nERR:httpd requestApi request=%s param=%s (should be integer)\n", request, param);
  return MHD_NO;
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
                MHD_add_response_header (response,MHD_HTTP_HEADER_CACHE_CONTROL, session->config->cacheTimeout); // default one hour cache
                MHD_add_response_header (response,MHD_HTTP_HEADER_ETAG, etagValue);
                ret = MHD_queue_response (connection, MHD_HTTP_NOT_MODIFIED, response);

            } else {  // it's a new file, we need to upload it to client
                if (verbose) fprintf (stderr, "Serving: [%s]\n", filepath);
                response =  MHD_create_response_from_fd (sbuf.st_size, fd);
                MHD_add_response_header (response,MHD_HTTP_HEADER_CACHE_CONTROL,session->config->cacheTimeout); // default one hour cache
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

  if (0 == strcmp (url, "/alsa-json")) {
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

PUBLIC void* httpdStart (AJG_session *session) {

  // at 1st call initialise http api hashtable
  if (Request2Commands == NULL) initService (session);

  if (verbose) printf ("starting http port=%d rootdir=%s\n", session->config->httpdPort, session->config->rootdir);

  session->httpd = (void*) MHD_start_daemon (
			MHD_USE_SELECT_INTERNALLY | MHD_USE_DEBUG, // use select and not threads
            session->config->httpdPort,   // port
            &newClient, NULL,       // Tcp Accept call back + extra attribute
            &newRequest, session,  // Http Request Call back + extra attribute
			MHD_OPTION_CONNECTION_TIMEOUT, (unsigned int) 15, MHD_OPTION_END); // 15s + options-end
			// TBD: MHD_OPTION_SOCK_ADDR

  if (session->httpd == NULL) {
     printf ("Error: httpStart invalid httpd port: %d", session->config->httpdPort);
     return FATAL;
  }

  return SUCCESS;
}

// infinit loop
PUBLIC int httpdLoop (AJG_session *session) {
    if (verbose) fprintf (stderr, "Httpd waiting loop\n");
    while (TRUE)  {
        fprintf (stderr, "Use Ctrl-C to quit");
        (void)getc (stdin);
    }
}


PUBLIC int httpdStatus (AJG_session *session) {
    return (MHD_run (session->httpd));
}

PUBLIC void httpdStop (AJG_session *session) {
  MHD_stop_daemon (session->httpd);
}
