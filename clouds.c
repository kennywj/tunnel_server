/***************************************************************************
 *                                  _   _ ____  _
 *  Project                     ___| | | |  _ \| |
 *                             / __| | | | |_) | |
 *                            | (__| |_| |  _ <| |___
 *                             \___|\___/|_| \_\_____|
 *
 * Copyright (C) 1998 - 2015, Daniel Stenberg, <daniel@haxx.se>, et al.
 *
 * This software is licensed as described in the file COPYING, which
 * you should have received as part of this distribution. The terms
 * are also available at https://curl.haxx.se/docs/copyright.html.
 *
 * You may opt to use, copy, modify, merge, publish, distribute and/or sell
 * copies of the Software, and permit persons to whom the Software is
 * furnished to do so, under the terms of the COPYING file.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ***************************************************************************/
/* <DESC>
 * simple HTTP POST using the easy interface
 * </DESC>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include "triclouds_api.h"

struct url_data {
    size_t size;
    char* data;
};
       
//
// function: response handler         
// to handle the HTTP POST response message
//  return real get data size
static size_t
response_handler(void *contents, size_t size, size_t nmemb, void *userp)        
{
	struct url_data *data = (struct url_data *)userp;
    size_t n = (size * nmemb);
        
//#ifdef DEBUG
//    fprintf(stderr, "prev data at %p, size=%ld\n", data->data, data->size);
//#endif
    data->data = realloc(data->data, data->size + n + 1); /* +1 for '\0' */
    if(data->data==NULL) {
        fprintf(stderr, "Failed to allocate memory.\n");
        return 0;
    }
//#ifdef DEBUG
//    fprintf(stderr, "after data at %p, size=%ld, new %ld\n", data->data, data->size, n);
//#endif
    memcpy(&data->data[data->size], contents, n);
    data->size += n;
    data->data[data->size] = '\0';

    return n;
}

//
// 	function:	clouds_service
//	Description
//	interface function to Triclouds service,
//  parameter:
//		url:	RESTFull API URL
//		cmd:	command (JSON format)
//		resp: 	buffer for response.
//		bufsize:buffer size of response
//	return:
//		>=0 size of response message
//		<0:	error code
//        
int clouds_service(char *url, char *cmd, char *resp, int bufsize)
{
  	CURL *curl;
  	CURLcode res=CURLE_OK ;
	struct url_data data;
	int n=-1;
	
    data.size = 0;
    data.data = malloc(1); /* reasonable size initial buffer */
    
  	/* In windows, this will init the winsock stuff */
  	curl_global_init(CURL_GLOBAL_ALL);

  	/* get a curl handle */
  	curl = curl_easy_init();
  	if(curl) {
    /* First set the URL that is about to receive our POST. This URL can
       just as well be a https:// URL if that is what should receive the
       data. */
//#ifdef DEBUG
//        fprintf(stderr, "%s\n%s\n", url, cmd);
//#endif       
    	curl_easy_setopt(curl, CURLOPT_URL, url);
    	struct curl_slist *headers = NULL;
    	headers = curl_slist_append(headers, "Accept: application/json");
    	headers = curl_slist_append(headers, "Content-Type: application/json");
    	headers = curl_slist_append(headers, "charsets: utf-8");
    	// set up receive function
    	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, response_handler);
    	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);
    	/* Now specify the POST data */    	
    	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, cmd);
    	curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strlen(cmd));
    	curl_easy_setopt(curl, CURLOPT_POST, 1);
    	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    
    	/* Perform the request, res will get the return code */
    	res = curl_easy_perform(curl);
    	/* Check for errors */
    	if (res != CURLE_OK)
      		fprintf(stderr, "curl_easy_perform() failed: %s\n",
        curl_easy_strerror(res));

    	/* always cleanup */
    	curl_easy_cleanup(curl);
    	curl_slist_free_all(headers);
  	}
  	curl_global_cleanup();
  	if (res == CURLE_OK && data.data)
 	{
  
		n = (data.size>(bufsize-1)?(bufsize-1):data.size);
//#ifdef DEBUG
//    	fprintf(stderr, "datasize %d, bufsize %d, %s\n", data.size, bufsize, data.data);
//#endif		    
		memcpy(resp, data.data,n);
		resp[n]='\0';
  		free(data.data);
	}
	else
		return -res;
   	return n;
}
