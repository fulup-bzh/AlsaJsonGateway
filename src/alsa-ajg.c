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
   http://alsa-lib.sourcearchive.com/documentation/1.0.20/modules.html
   http://alsa-lib.sourcearchive.com/documentation/1.0.8/group__Control_gd48d44da8e3bfe150e928267008b8ff5.html
   http://alsa.opensrc.org/HowTo_access_a_mixer_control
   https://github.com/json-c/json-c
   https://linuxprograms.wordpress.com/2010/05/20/json-c-libjson-tutorial/

   $Id: $
*/

#include "local-def-ajg.h"
#include <alsa/asoundlib.h>

// Loop on everypotential Sound card and register active one
PUBLIC json_object * alsaFindCards (AJG_session *session) {
      char cardName[32], *info, *name, *uid;
      int  card, status, err, index;
      json_object *sndcards =json_object_new_array();
      snd_ctl_t   *handle;
      snd_ctl_card_info_t *cardinfo;
      snd_ctl_card_info_alloca(&cardinfo);

      // if exist clean previous sndcards object
      if (session->sndcards != NULL) json_object_put (session->sndcards);

      // loop on potential card number
      for (card =0; card < 32; card++) {
          json_object *sndcard;
          // not really clean, but this is how aplay search for ALSA boards
          sprintf(cardName, "hw:%d", card);

          // no more card
          if ((err = snd_ctl_open(&handle, cardName, 0)) < 0) {
             break;
          }

          if ((err = snd_ctl_card_info(handle, cardinfo)) < 0) {
              error("Control device %s hw info error: %s", cardName, snd_strerror(err));
              snd_ctl_close(handle);
              continue;
          }

          sndcard = json_object_new_object();

          index = card;
          uid   = strdup(cardName);
          name  = strdup(snd_ctl_card_info_get_name (cardinfo));
          info  = strdup(snd_ctl_card_info_get_longname (cardinfo));
          if (verbose) fprintf (stderr, "Found n°%-2d uid=%-15s name=%-30s info=%s\n", card, uid, info, info);

          // create json object
 		  json_object_object_add (sndcard, "index", json_object_new_int (index));
 		  json_object_object_add (sndcard, "uid"  , json_object_new_string (uid));
 		  json_object_object_add (sndcard, "name" , json_object_new_string (name));
 		  json_object_object_add (sndcard, "info" , json_object_new_string (info));

          // add current sndcard to sndcards object
          json_object_array_add (sndcards, sndcard);
          snd_ctl_close(handle);
      }

   // keep track of sndcard object in case we need it ???
   session->sndcards =  json_object_get(sndcards);
   return (session->sndcards);
}

STATIC json_object *DB2StringJsonOject (long dB) {
    char label [20];
	if (dB < 0) {
		snprintf(label, sizeof(label), "-%li.%02lidB", -dB / 100, -dB % 100);
	} else {
		snprintf(label, sizeof(label), "%li.%02lidB", dB / 100, dB % 100);
	}

    // json function takes care of string copy
	return (json_object_new_string (label));
}


// Direct port from amixer TLV decode routine. This code is too complex for me.
// I hopefully did not break it when porting it.
STATIC json_object *decodeTlv(unsigned int *tlv, unsigned int tlv_size) {
    char label[20];
	unsigned int type = tlv[0];
	unsigned int size;
	unsigned int idx = 0;
	const char *chmap_type = NULL;
	json_object * decodeTlvJson =json_object_new_object();

	if (tlv_size < 2 * sizeof(unsigned int)) {
		printf("TLV size error!\n");
		return NULL;
	}
	type = tlv[idx++];
	size = tlv[idx++];
	tlv_size -= 2 * sizeof(unsigned int);
	if (size > tlv_size) {
		fprintf(stderr, "TLV size error (%i, %i, %i)!\n", type, size, tlv_size);
		return NULL;
	}
	switch (type) {

	case SND_CTL_TLVT_CONTAINER: {
        json_object * containerJson = json_object_new_array();

		size += sizeof(unsigned int) -1;
		size /= sizeof(unsigned int);
		while (idx < size) {
    		json_object *embedJson;

			if (tlv[idx+1] > (size - idx) * sizeof(unsigned int)) {
				fprintf(stderr, "TLV size error in compound!\n");
				return NULL;
			}
			embedJson = decodeTlv(tlv + idx, tlv[idx+1] + 8);
			json_object_array_add  (containerJson, embedJson);
			idx += 2 + (tlv[idx+1] + sizeof(unsigned int) - 1) / sizeof(unsigned int);
		}
		json_object_object_add (decodeTlvJson, "container",  containerJson);
		break;
		}

	case SND_CTL_TLVT_DB_SCALE: {
	    json_object * dbscaleJson = json_object_new_object();

		if (size != 2 * sizeof(unsigned int)) {
		    json_object * arrayJson =  json_object_new_array();
			while (size > 0) {
				snprintf(label, sizeof(label), "0x%08x,", tlv[idx++]);
    			json_object_array_add (arrayJson, json_object_new_string (label));
				size -= sizeof(unsigned int);
			}
			json_object_object_add (dbscaleJson, "array", arrayJson);
		} else {
			json_object_object_add (dbscaleJson, "min",  DB2StringJsonOject ((int)tlv[2]));
			json_object_object_add (dbscaleJson, "step", DB2StringJsonOject (tlv[3] & 0xffff));
			json_object_object_add (dbscaleJson, "mute", DB2StringJsonOject ((tlv[3] >> 16) & 1));
		}
		json_object_object_add (decodeTlvJson, "dbscale",  dbscaleJson);
		break;
		}

#ifdef SND_CTL_TLVT_DB_LINEAR
	case SND_CTL_TLVT_DB_LINEAR: {
	    json_object * dbLinearJson = json_object_new_object();

		if (size != 2 * sizeof(unsigned int)) {
		    json_object * arrayJson =  json_object_new_array();
			while (size > 0) {
				snprintf(label, sizeof(label),"0x%08x,", tlv[idx++]);
    			json_object_array_add (arrayJson, json_object_new_string (label));
				size -= sizeof(unsigned int);
			}
			json_object_object_add (dbLinearJson, "offset", arrayJson);
		} else {
			json_object_object_add (dbLinearJson, "min", DB2StringJsonOject((int)tlv[2]));
			json_object_object_add (dbLinearJson, "max", DB2StringJsonOject((int)tlv[3]));
		}
		json_object_object_add (decodeTlvJson, "dblinear",  dbLinearJson);
		break;
		}
#endif

#ifdef SND_CTL_TLVT_DB_RANGE
	case SND_CTL_TLVT_DB_RANGE: {
        json_object *dbRangeJson = json_object_new_object();

		if ((size % (6 * sizeof(unsigned int))) != 0) {
			json_object *arrayJson   = json_object_new_array();
			while (size > 0) {
				snprintf(label, sizeof(label),"0x%08x,", tlv[idx++]);
    			json_object_array_add (arrayJson, json_object_new_string (label));
				size -= sizeof(unsigned int);
			}
    		json_object_object_add (dbRangeJson, "dbrange", arrayJson);
			break;
		}
		while (size > 0) {
   		    json_object * embedJson = json_object_new_object();
			snprintf(label, sizeof(label),"%i,", tlv[idx++]);
			json_object_object_add (embedJson, "rangemin", json_object_new_string (label));
			snprintf(label, sizeof(label),"%i", tlv[idx++]);
			json_object_object_add (embedJson, "rangemax", json_object_new_string (label));
			embedJson = decodeTlv(tlv + idx, 4 * sizeof(unsigned int));
			json_object_object_add (embedJson, "tlv", embedJson);
			idx += 4;
			size -= 6 * sizeof(unsigned int);
			json_object_array_add (dbRangeJson, embedJson);
		}
		json_object_object_add (decodeTlvJson, "dbrange",  dbRangeJson);
		break;
		}
#endif
#ifdef SND_CTL_TLVT_DB_MINMAX
	case SND_CTL_TLVT_DB_MINMAX:
	case SND_CTL_TLVT_DB_MINMAX_MUTE: {
	    json_object * dbMinMaxJson = json_object_new_object();

        if (size != 2 * sizeof(unsigned int)) {
            json_object * arrayJson = json_object_new_array();
            while (size > 0) {
                snprintf(label, sizeof(label),"0x%08x,", tlv[idx++]);
                json_object_array_add (arrayJson, json_object_new_string (label));
                size -= sizeof(unsigned int);
            }
            json_object_object_add (dbMinMaxJson, "array", arrayJson);

        } else {
            json_object_object_add (dbMinMaxJson, "min", DB2StringJsonOject((int)tlv[2]));
            json_object_object_add (dbMinMaxJson, "max", DB2StringJsonOject((int)tlv[3]));
        }

        if (type == SND_CTL_TLVT_DB_MINMAX_MUTE) {
          json_object_object_add (decodeTlvJson ,"dbminmaxmute", dbMinMaxJson);
        } else {
          json_object_object_add (decodeTlvJson ,"dbminmax", dbMinMaxJson);
        }
		break;
		}
#endif
#ifdef SND_CTL_TLVT_CHMAP_FIXED
	case SND_CTL_TLVT_CHMAP_FIXED:
		chmap_type = "fixed";
		/* Fall through */
	case SND_CTL_TLVT_CHMAP_VAR:
		if (!chmap_type)
			chmap_type = "variable";
		/* Fall through */
	case SND_CTL_TLVT_CHMAP_PAIRED:
		if (!chmap_type)
			chmap_type = "paired";

	    json_object * chmapJson = json_object_new_object();
        json_object * arrayJson = json_object_new_array();

		while (size > 0) {
			snprintf(label, sizeof(label),"%s", snd_pcm_chmap_name(tlv[idx++]));
			size -= sizeof(unsigned int);
			json_object_array_add (arrayJson, json_object_new_string (label));
		}
		json_object_object_add (chmapJson,chmap_type, arrayJson);
		json_object_object_add (decodeTlvJson ,"chmap", chmapJson);
		break;
#endif
	default: {
		printf("unk-%i-", type);
        json_object * arrayJson = json_object_new_array();

		while (size > 0) {
			snprintf(label, sizeof(label),"0x%08x,", tlv[idx++]);
			size -= sizeof(unsigned int);
			json_object_array_add (arrayJson, json_object_new_string (label));
		}
		break;
		json_object_object_add (decodeTlvJson ,"unknown", arrayJson);
		}
	}

	return (decodeTlvJson);
}

// pack Alsa element's ACL into a JSON object
STATIC json_object *  getControlAcl (snd_ctl_elem_info_t *info) {

    json_object * jsonAclCtrl = json_object_new_object();

    json_object_object_add (jsonAclCtrl, "read"   , json_object_new_boolean(snd_ctl_elem_info_is_readable(info)));
    json_object_object_add (jsonAclCtrl, "write"  , json_object_new_boolean(snd_ctl_elem_info_is_writable(info)));
    json_object_object_add (jsonAclCtrl, "inact"  , json_object_new_boolean(snd_ctl_elem_info_is_inactive(info)));
    json_object_object_add (jsonAclCtrl, "volat"  , json_object_new_boolean(snd_ctl_elem_info_is_volatile(info)));
    json_object_object_add (jsonAclCtrl, "lock"   , json_object_new_boolean(snd_ctl_elem_info_is_locked(info)));

    // if TLV is readable we insert its ACL
    if (!snd_ctl_elem_info_is_tlv_readable(info)) {
        json_object * jsonTlv = json_object_new_object();

        json_object_object_add (jsonTlv, "read"   , json_object_new_boolean(snd_ctl_elem_info_is_tlv_readable(info)));
        json_object_object_add (jsonTlv, "write"  , json_object_new_boolean(snd_ctl_elem_info_is_tlv_writable(info)));
        json_object_object_add (jsonTlv, "command", json_object_new_boolean(snd_ctl_elem_info_is_tlv_commandable(info)));

        json_object_object_add (jsonAclCtrl, "tlv", jsonTlv);
    }
    return (jsonAclCtrl);
}


// pack element from ALSA control into a JSON object
STATIC json_object * getAlsaControl (snd_hctl_elem_t *elem, snd_ctl_elem_info_t *info,  AJG_request *request) {

	int err;
	unsigned int *tlv;
    json_object *jsonAlsaCtrl,*jsonClassCtrl;
	snd_ctl_elem_id_t *elemid;
	snd_ctl_elem_type_t elemtype;
	snd_ctl_elem_value_t *control;
	snd_ctl_elem_value_alloca(&control);

	int count, idx;

	// allocate ram for ALSA elements
	snd_ctl_elem_id_alloca   (&elemid);

    // get elemId out of elem
    snd_hctl_elem_get_id(elem, elemid);

    // when ctrlid is set, return only this ctrl
    if (request->numid != 0 && request->numid != snd_ctl_elem_id_get_numid(elemid)) return NULL;

    // build a json object out of element
    jsonAlsaCtrl = json_object_new_object(); // http://alsa-lib.sourcearchive.com/documentation/1.0.24.1-3/group__Control_ga4e4f251147f558bc2ad044e836e449d9.html
    json_object_object_add (jsonAlsaCtrl,"numid", json_object_new_int(snd_ctl_elem_id_get_numid (elemid)));
    json_object_object_add (jsonAlsaCtrl,"iface", json_object_new_string(snd_ctl_elem_iface_name(snd_ctl_elem_id_get_interface(elemid))));
    json_object_object_add (jsonAlsaCtrl,"name" , json_object_new_string(snd_ctl_elem_id_get_name (elemid)));
    json_object_object_add (jsonAlsaCtrl,"actif", json_object_new_boolean(!snd_ctl_elem_info_is_inactive(info)));

    elemtype = snd_ctl_elem_info_get_type(info);

	// number item and value(s) within control.
	count = snd_ctl_elem_info_get_count (info);

	if (snd_ctl_elem_info_is_readable(info)) {
	    json_object *jsonValuesCtrl = json_object_new_array();

		if ((err = snd_hctl_elem_read(elem, control)) < 0) {
		       json_object *jsonValuesCtrl = json_object_new_object();
       		   json_object_object_add (jsonValuesCtrl,"error", json_object_new_string(snd_strerror(err)));
		} else {
			for (idx = 0; idx < count; idx++) { // start from one in amixer.c !!!
				switch (elemtype) {
				case SND_CTL_ELEM_TYPE_BOOLEAN: {
					char *onoff;
					json_object_array_add (jsonValuesCtrl, json_object_new_boolean (snd_ctl_elem_value_get_boolean(control, idx)));
					break;
					}
				case SND_CTL_ELEM_TYPE_INTEGER:
					json_object_array_add (jsonValuesCtrl, json_object_new_int (snd_ctl_elem_value_get_integer(control, idx)));
					break;
				case SND_CTL_ELEM_TYPE_INTEGER64:
					json_object_array_add (jsonValuesCtrl, json_object_new_int64 (snd_ctl_elem_value_get_integer64(control, idx)));
					break;
				case SND_CTL_ELEM_TYPE_ENUMERATED:
					json_object_array_add (jsonValuesCtrl, json_object_new_int (snd_ctl_elem_value_get_enumerated(control, idx)));
					break;
				case SND_CTL_ELEM_TYPE_BYTES:
					json_object_array_add (jsonValuesCtrl, json_object_new_int ((int)snd_ctl_elem_value_get_byte(control, idx)));
					break;
				case SND_CTL_ELEM_TYPE_IEC958: {
					json_object *jsonIec958Ctrl = json_object_new_object();
					snd_aes_iec958_t iec958;
					snd_ctl_elem_value_get_iec958(control, &iec958);

					json_object_object_add (jsonIec958Ctrl,"AES0",json_object_new_int(iec958.status[0]));
					json_object_object_add (jsonIec958Ctrl,"AES1",json_object_new_int(iec958.status[1]));
					json_object_object_add (jsonIec958Ctrl,"AES2",json_object_new_int(iec958.status[2]));
					json_object_object_add (jsonIec958Ctrl,"AES3",json_object_new_int(iec958.status[3]));
					json_object_array_add  (jsonValuesCtrl, jsonIec958Ctrl);
					break;
					}
				default:
					json_object_array_add (jsonValuesCtrl, json_object_new_string ("?unknown?"));
					break;
				}
			}
		}
		json_object_object_add (jsonAlsaCtrl,"value",jsonValuesCtrl);
    }


    if (!request->quiet) {  // in simple mode do not print usable values
        jsonClassCtrl = json_object_new_object();
        json_object_object_add (jsonClassCtrl,"type" , json_object_new_string(snd_ctl_elem_type_name(elemtype)));
		json_object_object_add (jsonClassCtrl,"count", json_object_new_int(count));

		switch (elemtype) {
			case SND_CTL_ELEM_TYPE_INTEGER:
				json_object_object_add (jsonClassCtrl,"min",  json_object_new_int(snd_ctl_elem_info_get_min(info)));
				json_object_object_add (jsonClassCtrl,"max",  json_object_new_int(snd_ctl_elem_info_get_max(info)));
				json_object_object_add (jsonClassCtrl,"step", json_object_new_int(snd_ctl_elem_info_get_step(info)));
				break;
			case SND_CTL_ELEM_TYPE_INTEGER64:
				json_object_object_add (jsonClassCtrl,"min",  json_object_new_int64(snd_ctl_elem_info_get_min64(info)));
				json_object_object_add (jsonClassCtrl,"max",  json_object_new_int64(snd_ctl_elem_info_get_max64(info)));
				json_object_object_add (jsonClassCtrl,"step", json_object_new_int64(snd_ctl_elem_info_get_step64(info)));
				break;
			case SND_CTL_ELEM_TYPE_ENUMERATED: {
				unsigned int item, items = snd_ctl_elem_info_get_items(info);
				json_object *jsonEnum = json_object_new_array();

				for (item = 0; item < items; item++) {
					snd_ctl_elem_info_set_item(info, item);
					if ((err = snd_hctl_elem_info(elem, info)) >= 0) {
						json_object_array_add (jsonEnum, json_object_new_string(snd_ctl_elem_info_get_item_name(info)));
					}
				}
				json_object_object_add (jsonClassCtrl, "enums",jsonEnum);
				break;
			}
			default: break; // ignore any unknown type
			}

		// add collected class info with associated ACLs
		json_object_object_add (jsonAlsaCtrl,"ctrl", jsonClassCtrl);
		json_object_object_add (jsonAlsaCtrl,"acl"  , getControlAcl (info));

		// check for tlv [direct port from amixer.c]
		if (snd_ctl_elem_info_is_tlv_readable(info)) {
				unsigned int *tlv;
				tlv = malloc(4096);
				if ((err = snd_hctl_elem_tlv_read(elem, tlv, 4096)) < 0) {
					error("Control %s element TLV read error\n", snd_strerror(err));
					free(tlv);
				} else {
					json_object_object_add (jsonAlsaCtrl,"tlv", decodeTlv (tlv, 4096));
		   }
		}
   }
   return (jsonAlsaCtrl);
}


PUBLIC json_object *alsaFindControls(AJG_session *session, json_object *sndcard, AJG_request *request) {
	int err;
    char const *uid,*tmp;
    int  count=0;

    snd_ctl_elem_iface_t iface;

	snd_hctl_t *handle;
	snd_hctl_elem_t *elem;
	snd_ctl_elem_info_t *info;
	json_object *jsonsndcard, *jsonAlsaCtrls, *jsonAlsaCtrl, *jsonAclCtrl, *slot;

	// allocate ram for ALSA elements
	snd_ctl_elem_info_alloca (&info);

    // retrieve sound card uid from json object
    json_object_object_get_ex (sndcard, "uid", &slot);
    uid = json_object_get_string (slot);

	if ((err = snd_hctl_open(&handle, uid, 0)) < 0) {
		error("Control %s open error: %s", uid, snd_strerror(err));
		return NULL;
	}
	if ((err = snd_hctl_load(handle)) < 0) {
		error("Control %s local error: %s\n", uid, snd_strerror(err));
		return NULL;
	}

	// main json structure for our sound card
	jsonsndcard = json_object_new_object();
	json_object_object_add (jsonsndcard,"sndcard",  sndcard);

	// create an json array to hold all sndcard controls
    jsonAlsaCtrls = json_object_new_array();

	for (elem = snd_hctl_first_elem(handle); elem != NULL; elem = snd_hctl_elem_next(elem)) {

        if ((err = snd_hctl_elem_info(elem, info)) < 0) {
			error("Control %s snd_hctl_elem_info error: %s\n", uid, snd_strerror(err));
			return FATAL;
		}

        // each control is added into a JSON array
		jsonAlsaCtrl = getAlsaControl (elem, info, request);
       	if (jsonAlsaCtrl) json_object_array_add (jsonAlsaCtrls, jsonAlsaCtrl);

	}

    // add controls json array to sndcard
	json_object_object_add (jsonsndcard,"controls", jsonAlsaCtrls);
	snd_hctl_close(handle);

   	// fprintf (stdout, "Json object: %s\n",json_object_to_json_string_ext(jsonsndcard, JSON_C_TO_STRING_PRETTY));
	return (json_object_get(jsonsndcard));
}

PUBLIC json_object *alsaSetControls(AJG_session *session, int idxcard, AJG_request *request) {
    static json_object * okresponse =NULL;
	int err;
	snd_ctl_t *handle;
	snd_ctl_elem_info_t *info;
    snd_ctl_elem_id_t *id;
	snd_ctl_elem_value_t *control;
    json_object *response, *slot;
    char uidcard [6];

    // make standard response only once
    if (okresponse == NULL) okresponse=json_tokener_parse("{status: done}");

    // open sound card with low level API as the other seems not stable enough
    /* FIXME: Remove it when hctl find works ok !!! */
    snprintf (uidcard, sizeof(uidcard),"hw:%i",idxcard);
    if (err = snd_ctl_open(&handle, uidcard, 0) < 0) {
   		error("Control %s open error: %s\n", uidcard, snd_strerror(err));
   		return NULL;
    }

    /* FIXME: Remove it when hctl find works ok !!! */
   	snd_ctl_elem_id_alloca(&id);
    snd_ctl_elem_id_set_numid (id, request->numid);

    snd_ctl_elem_info_alloca(&info);
    snd_ctl_elem_info_set_id(info, id);
    if ((err = snd_ctl_elem_info(handle, info)) < 0) {
   		error("Cannot find the given element from control %s\n", uidcard);
   		goto OnErrorExit;
    }

  	snd_ctl_elem_info_get_id(info, id);
	snd_ctl_elem_value_alloca(&control);
    snd_ctl_elem_value_set_id(control, id);
	if ((err = snd_ctl_elem_read(handle, control)) < 0) {
   	    error("Cannot read the given element from control %s\n", uidcard);
	    goto OnErrorExit;
	}

    err = snd_ctl_ascii_value_parse(handle, control, info, request->args);
  	if (err < 0) {
  	    error("Control %s fail to parse args=%s: %s\n", uidcard, request->args, snd_strerror(err));
  	    goto OnErrorExit;
  	}

	if ((err = snd_ctl_elem_write(handle, control)) < 0) {
	   error("Control %s element write error: %s\n", uidcard, snd_strerror(err));
  	   goto OnErrorExit;
    }

    // in quiet mode we only return OK otherwise we request full value of modified control
	if (request->quiet) {
        response = okresponse;
	} else {
	    // in verbose mode we return a modified controls
	    json_object *sndcard = json_object_array_get_idx(alsaFindCards(session), idxcard);
        response = alsaFindControls (session, sndcard, request);
	}

	return (response);

OnErrorExit:
    snd_ctl_close(handle);
	return NULL;
}
