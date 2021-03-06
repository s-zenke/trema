/*
 * OpenFlow Packet_in message filter
 *
 * Author: Kazushi SUGYO
 *
 * Copyright (C) 2008-2011 NEC Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2, as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */


#include <getopt.h>
#include <openflow.h>
#include <stdio.h>
#include <unistd.h>
#include "match_table.h"
#include "trema.h"


#ifdef UNIT_TESTING

#define static
#define main packetin_filter_main

#ifdef printf
#undef printf
#endif
#define printf( fmt, args... )  mock_printf2( fmt, ##args )
int mock_printf2(const char *format, ...);

#ifdef error
#undef error
#endif
#define error( fmt, args... ) mock_error( fmt, ##args )
void mock_error( const char *format, ... );

#ifdef set_match_from_packet
#undef set_match_from_packet
#endif
#define set_match_from_packet mock_set_match_from_packet
void mock_set_match_from_packet( struct ofp_match *match, const uint16_t in_port,
                                 const uint32_t wildcards, const buffer *packet );

#ifdef create_packet_in
#undef create_packet_in
#endif
#define create_packet_in mock_create_packet_in
buffer *mock_create_packet_in( const uint32_t transaction_id, const uint32_t buffer_id,
                               const uint16_t total_len, uint16_t in_port,
                               const uint8_t reason, const buffer *data );

#ifdef insert_match_entry
#undef insert_match_entry
#endif
#define insert_match_entry mock_insert_match_entry
void mock_insert_match_entry( struct ofp_match *ofp_match, uint16_t priority,
                              const char *service_name, const char *entry_name );

#ifdef lookup_match_entry
#undef lookup_match_entry
#endif
#define lookup_match_entry mock_lookup_match_entry
match_entry *mock_lookup_match_entry( struct ofp_match *match );

#ifdef send_message
#undef send_message
#endif
#define send_message mock_send_message
bool mock_send_message( const char *service_name, const uint16_t tag, const void *data,
                        size_t len );

#ifdef init_trema
#undef init_trema
#endif
#define init_trema mock_init_trema
void mock_init_trema( int *argc, char ***argv );

#ifdef set_packet_in_handler
#undef set_packet_in_handler
#endif
#define set_packet_in_handler mock_set_packet_in_handler
bool mock_set_packet_in_handler( packet_in_handler callback, void *user_data );

#ifdef start_trema
#undef start_trema
#endif
#define start_trema mock_start_trema
void mock_start_trema( void );

#ifdef get_executable_name
#undef get_executable_name
#endif
#define get_executable_name mock_get_executable_name
const char *mock_get_executable_name( void );

#endif // UNIT_TESTING


void
usage() {
  printf(
	 "OpenFlow Packet in Filter.\n"
	 "Usage: %s [OPTION]... [PACKETIN-FILTER-RULE]...\n"
	 "\n"
	 "  -n, --name=SERVICE_NAME     service name\n"
	 "  -d, --daemonize             run in the background\n"
	 "  -l, --logging_level=LEVEL   set logging level\n"
	 "  -h, --help                  display this help and exit\n"
	 "\n"
	 "PACKETIN-FILTER-RULE:\n"
	 "  match-type::destination-service-name\n"
	 "\n"
	 "match-type:\n"
	 "  lldp                        LLDP ethernet frame type and priority is 0x8000\n"
	 "  packet_in                   any packet and priority is zero\n"
	 "\n"
	 "destination-service-name      destination service name\n"
	 , get_executable_name()
	 );
}


static void
handle_packet_in( uint64_t datapath_id, uint32_t transaction_id,
                  uint32_t buffer_id, uint16_t total_len,
                  uint16_t in_port, uint8_t reason, const buffer *data,
                  void *user_data ) {
  UNUSED( user_data );

  char match_str[ 1024 ];
  struct ofp_match ofp_match;   // host order

  set_match_from_packet( &ofp_match, in_port, 0, data );
  match_to_string( &ofp_match, match_str, sizeof( match_str ) );

  match_entry *match_entry = lookup_match_entry( &ofp_match );
  if ( match_entry == NULL ) {
    debug( "No match entry found." );
    return;
  }

  buffer *buf = create_packet_in( transaction_id, buffer_id, total_len, in_port,
                                  reason, data );

  openflow_service_header_t *message;
  message = append_front_buffer( buf, sizeof( openflow_service_header_t ) );
  message->datapath_id = htonll( datapath_id );
  message->service_name_length = htons( 0 );
  if ( !send_message( match_entry->service_name, MESSENGER_OPENFLOW_MESSAGE,
                      buf->data, buf->length ) ) {
    error( "Failed to send a message to %s ( entry_name = %s, match = %s ).",
           match_entry->service_name, match_entry->entry_name, match_str );
    free_buffer( buf );
    return;
  }

  debug( "Sending a message to %s ( entry_name = %s, match = %s ).",
         match_entry->service_name, match_entry->entry_name, match_str );

  free_buffer( buf );
}


// TODO: use pubsub lib.
static void
register_dl_type_filter( uint16_t dl_type, uint16_t priority,
                         const char *service_name, const char *entry_name ) {
  struct ofp_match ofp_match;
  memset( &ofp_match, 0, sizeof( struct ofp_match ) );
  ofp_match.wildcards = OFPFW_ALL & ~OFPFW_DL_TYPE;
  ofp_match.dl_type = dl_type;

  insert_match_entry( &ofp_match, priority, service_name, entry_name );
}


// TODO: use pubsub lib.
static void
register_any_filter( uint16_t priority, const char *service_name,
                     const char *entry_name ) {
  struct ofp_match ofp_match;
  memset( &ofp_match, 0, sizeof( struct ofp_match ) );
  ofp_match.wildcards = OFPFW_ALL;

  insert_match_entry( &ofp_match, priority, service_name, entry_name );
}


static const char *
match_type( const char *type, char *name ) {
  size_t len = strlen( type );
  if ( strncmp( name, type, len ) != 0 ) {
    return NULL;
  }

  return name + len;
}


// built-in packetin-filter-rule
static const char LLDP_PACKET_IN[] = "lldp::";
static const char ANY_PACKET_IN[] = "packet_in::";

static bool
set_match_type( int argc, char *argv[] ) {
  int i;
  const char *service_name;
  for ( i = 1; i < argc; i++ ) {
    if ( ( service_name = match_type( LLDP_PACKET_IN, argv[ i ] ) ) != NULL ) {
      register_dl_type_filter( ETH_ETHTYPE_LLDP, OFP_DEFAULT_PRIORITY,
                               service_name, "filter-lldp" );
    }
    else if ( ( service_name = match_type( ANY_PACKET_IN, argv[ i ] ) ) != NULL ) {
      register_any_filter( 0, service_name, "filter-any" );
    }
    else {
      return false;
    }
  }

  return true;
}


int
main( int argc, char *argv[] ) {
  init_trema( &argc, &argv );

  init_match_table();

  // built-in packetin-filter-rule
  if ( !set_match_type( argc, argv ) ) {
    usage();
    finalize_match_table();
    exit( EXIT_FAILURE );
  }

  set_packet_in_handler( handle_packet_in, NULL );

  start_trema();

  finalize_match_table();

  return 0;
}


/*
 * Local variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */
