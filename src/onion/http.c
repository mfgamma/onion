/*
	Onion HTTP server library
	Copyright (C) 2010-2013 David Moreno Montero

	This library is free software; you can redistribute it and/or
	modify it under the terms of, at your choice:
	
	a. the GNU Lesser General Public License as published by the 
	 Free Software Foundation; either version 3.0 of the License, 
	 or (at your option) any later version.
	
	b. the GNU General Public License as published by the 
	 Free Software Foundation; either version 2.0 of the License, 
	 or (at your option) any later version.

	This library is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	Lesser General Public License for more details.

	You should have received a copy of the GNU Lesser General Public
	License and the GNU General Public License along with this 
	library; if not see <http://www.gnu.org/licenses/>.
	*/

#include <malloc.h>
#include <stdlib.h>

#include "types.h"
#include "http.h"
#include "types_internal.h"
#include "listen_point.h"
#include "request.h"
#include "log.h"
#include "ro_block.h"

static ssize_t onion_http_read(onion_request *req, char *data, size_t len);
ssize_t onion_http_write(onion_request *req, const char *data, size_t len);
int onion_http_read_ready(onion_request *req);
onion_connection_status onion_http_parse(onion_request *req, onion_ro_block *block);

/**
 * @struct onion_http_t
 * @memberof onion_http_t
 */
struct onion_http_t{
};

/**
 * @short Creates an HTTP listen point 
 * @memberof onion_http_t
 */
onion_listen_point* onion_http_new()
{
	onion_listen_point *ret=onion_listen_point_new();
	
	ret->read=onion_http_read;
	ret->write=onion_http_write;
	ret->close=onion_listen_point_request_close_socket;
	ret->read_ready=onion_http_read_ready;
	
	return ret;
}

/**
 * @short Reads data from the http connection
 * @memberof onion_http_t
 */
static ssize_t onion_http_read(onion_request *con, char *data, size_t len){
	return read(con->connection.fd, data, len);
}

/**
 * @short HTTP client has data ready to be readen
 * @memberof onion_http_t
 */
int onion_http_read_ready(onion_request *req){
	char buffer[1500];
	ssize_t len=req->connection.listen_point->read(req, buffer, sizeof(buffer));
	
	if (len<=0)
		return OCS_CLOSE_CONNECTION;
	
	onion_ro_block robuffer;
	onion_ro_block_init(&robuffer, buffer, len);
	
	if (!req->parser.parse)
		req->parser.parse=onion_http_parse;
	
	onion_connection_status st=OCS_INTERNAL_ERROR;
	while (req->parser.parse && !onion_ro_block_eof(&robuffer)){
		size_t len=onion_ro_block_remaining(&robuffer);
		st=req->parser.parse(req, &robuffer);
		if (len == onion_ro_block_remaining(&robuffer)){
			ONION_ERROR("Parser did not consume data. Bogus parser, aborting petition.");
			return OCS_INTERNAL_ERROR;
		}
		ONION_DEBUG0("%d bytes left", onion_ro_block_remaining(&robuffer));
		if (st<0)
			return st;
		if (st==OCS_REQUEST_READY){
			st=onion_request_process(req);
			if (!req->parser.parse){
				ONION_DEBUG("Setting again http parser for this request: %d bytes left", onion_ro_block_remaining(&robuffer));
				req->parser.parse=onion_http_parse;
			}
		}
	}
	
	return st;
}

/**
 * @short Write data to the HTTP client
 * @memberof onion_http_t
 */
ssize_t onion_http_write(onion_request *con, const char *data, size_t len){
	return write(con->connection.fd, data, len);
}

