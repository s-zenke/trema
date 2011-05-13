/*
 * Learning switch application that supports multiple switches.
 *
 * Author: Yasuhito Takamiya <yasuhito@gmail.com>
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
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "trema.h"


static const int FORWARDING_DB_ENTRY_TIMEOUT = 300;
static const int FORWARDING_DB_AGING_INTERVAL = 5;

typedef struct {
  uint8_t mac[ OFP_ETH_ALEN ];
  uint16_t port_no;
  time_t updated_at;
} forwarding_db_entry;


static void
delete_forwarding_db_entry( void *key, void *value, void *user_data ) {
  UNUSED( key );
  UNUSED( user_data );

  forwarding_db_entry *entry = value;

  debug( "deleting a forwardning database entry ( mac = "
         "%02x:%02x:%02x:%02x:%02x:%02x, port_no = %u, "
         "updated_at = %lu ).",
         entry->mac[ 0 ], entry->mac[ 1 ], entry->mac[ 2 ],
         entry->mac[ 3 ], entry->mac[ 4 ], entry->mac[ 5 ],
         entry->port_no, entry->updated_at );

  xfree( value );
}


static void
delete_forwarding_db( hash_table *forwarding_db ) {
  debug( "deleting a forwarding database." );

  foreach_hash( forwarding_db, delete_forwarding_db_entry, NULL );
  delete_hash( forwarding_db );
}


static void
age_forwarding_db_entry( void *key, void *value, void *user_data ) {
  hash_table *forwarding_db = user_data;
  forwarding_db_entry *entry = value;

  if ( entry->updated_at + FORWARDING_DB_ENTRY_TIMEOUT < time( NULL ) ) {
    debug( "age out." );
    delete_hash_entry( forwarding_db, key );
    delete_forwarding_db_entry( key, value, NULL );
  }
}


static void
update_forwarding_db( void *user_data ) {
  hash_table *forwarding_db = user_data;

  debug( "start aging." );

  foreach_hash( forwarding_db, age_forwarding_db_entry, forwarding_db );
}


static void
set_forwarding_db_entry( forwarding_db_entry *entry, const uint16_t port_no,
               const uint8_t *mac, const time_t updated_at ) {
  debug( "setting a forwardning database entry ( mac = "
         "%02x:%02x:%02x:%02x:%02x:%02x, port_no = %u, "
         "updated_at = %lu ).",
         mac[ 0 ], mac[ 1 ], mac[ 2 ], mac[ 3 ], mac[ 4 ], mac[ 5 ],
         port_no, updated_at );

  memcpy( entry->mac, mac, sizeof( entry->mac ) );
  entry->port_no = port_no;
  entry->updated_at = updated_at;
}


static void
learn( hash_table *forwarding_db, uint16_t port_no, uint8_t *mac ) {
  time_t now;
  forwarding_db_entry *entry;

  now = time( NULL );

  entry = lookup_hash_entry( forwarding_db, mac );

  if ( entry != NULL ) {
    set_forwarding_db_entry( entry, port_no, mac, now );
  }
  else {
    entry = xmalloc( sizeof( forwarding_db_entry ) );
    set_forwarding_db_entry( entry, port_no, mac, now );
    insert_hash_entry( forwarding_db, entry->mac, entry );
  }
}


static forwarding_db_entry *
lookup_destination( hash_table *forwarding_db, uint8_t *mac ) {
  forwarding_db_entry *entry;

  debug( "looking up forwardning database ( mac = "
         "%02x:%02x:%02x:%02x:%02x:%02x ).",
         mac[ 0 ], mac[ 1 ], mac[ 2 ], mac[ 3 ], mac[ 4 ], mac[ 5 ] );

  entry = lookup_hash_entry( forwarding_db, mac );

  if ( entry != NULL ) {
    debug( "entry found ( port_no = %u, updated_at = %lu ).",
           entry->port_no, entry->updated_at );
  }
  else {
    debug( "entry not found." );
  }

  return entry;
}


static void
do_flooding( packet_in packet_in ) {
  openflow_actions *actions = create_actions();
  append_action_output( actions, OFPP_FLOOD, UINT16_MAX );

  buffer *packet_out;
  if ( packet_in.buffer_id == UINT32_MAX ) {
    packet_out = create_packet_out(
      get_transaction_id(),
      packet_in.buffer_id,
      packet_in.in_port,
      actions,
      packet_in.data
    );
  }
  else {
    packet_out = create_packet_out(
      get_transaction_id(),
      packet_in.buffer_id,
      packet_in.in_port,
      actions,
      NULL
    );
  }
  send_openflow_message( packet_in.datapath_id, packet_out );
  free_buffer( packet_out );
  delete_actions( actions );
}


static void
send_packet( uint16_t destination_port, packet_in packet_in ) {
  openflow_actions *actions = create_actions();
  append_action_output( actions, destination_port, UINT16_MAX );

  struct ofp_match match;
  set_match_from_packet( &match, packet_in.in_port, 0, packet_in.data );

  buffer *flow_mod = create_flow_mod(
    get_transaction_id(),
    match,
    get_cookie(),
    OFPFC_ADD,
    60,
    0,
    UINT16_MAX,
    packet_in.buffer_id,
    OFPP_NONE,
    OFPFF_SEND_FLOW_REM,
    actions
  );
  send_openflow_message( packet_in.datapath_id, flow_mod );
  free_buffer( flow_mod );

  if ( packet_in.buffer_id == UINT32_MAX ) {
    buffer *packet_out = create_packet_out(
      get_transaction_id(),
      packet_in.buffer_id,
      packet_in.in_port,
      actions,
      packet_in.data
    );
    send_openflow_message( packet_in.datapath_id, packet_out );
    free_buffer( packet_out );
  }

  delete_actions( actions );
}


static bool
compare_datapath_id( const void *x, const void *y ) {
  return ( ( memcmp( x, y, sizeof ( uint64_t ) ) == 0 ) ? true : false );
}


static unsigned int
hash_datapath_id( const void *key ) {
  const uint32_t *datapath_id = ( const uint32_t *) key;
  return ( unsigned int ) datapath_id[ 0 ] ^ datapath_id[ 1 ];
}


typedef struct {
  uint64_t datapath_id;
  hash_table *forwarding_db;
} switch_entry;


static switch_entry *
new_switch_entry( uint64_t datapath_id, hash_table *switch_db ) {
  switch_entry *entry = xmalloc( sizeof( switch_entry ) );
  entry->datapath_id = datapath_id;
  entry->forwarding_db = create_hash( compare_mac, hash_mac );
  insert_hash_entry( switch_db, &entry->datapath_id, entry );
  return entry;
}


static void
refresh_switch_entry( switch_entry *entry ) {
  delete_forwarding_db( entry->forwarding_db );
  entry->forwarding_db = create_hash( compare_mac, hash_mac );
}


static void
handle_switch_ready( uint64_t datapath_id, void *switch_db ) {
  switch_entry *entry = lookup_hash_entry( switch_db, &datapath_id );
  if ( entry == NULL ) {
    entry = new_switch_entry( datapath_id, switch_db );
  }
  else {
    refresh_switch_entry( entry );
  }

  add_periodic_event_callback(
    FORWARDING_DB_AGING_INTERVAL,
    update_forwarding_db,
    entry->forwarding_db
  );
}


static void
handle_packet_in( packet_in packet_in ) {
  switch_entry *entry = lookup_hash_entry( packet_in.user_data, &packet_in.datapath_id );
  if ( entry == NULL ) {
    warn( "Packet_in from unknown switch (datapath ID = %#" PRIx64 ")", packet_in.datapath_id );
    return;
  }

  uint8_t *macsa = packet_info( packet_in.data )->l2_data.eth->macsa;
  learn( entry->forwarding_db, packet_in.in_port, macsa );

  uint8_t *macda = packet_info( packet_in.data )->l2_data.eth->macda;
  forwarding_db_entry *destination = lookup_destination( entry->forwarding_db, macda );

  if ( destination == NULL ) {
    do_flooding( packet_in );
  }
  else {
    send_packet( destination->port_no, packet_in );
  }
}


int
main( int argc, char *argv[] ) {
  init_trema( &argc, &argv );

  hash_table *switch_db = create_hash( compare_datapath_id, hash_datapath_id );
  set_switch_ready_handler( handle_switch_ready, switch_db );
  set_packet_in_handler( handle_packet_in, switch_db );

  start_trema();

  return 0;
}


/*
 * Local variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */
