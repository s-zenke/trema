/*
 * Author: Kazusi Sugyo
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


#include <fcntl.h>
#include <getopt.h>
#include <inttypes.h>
#include <limits.h>
#include <openflow.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "trema.h"
#include "cookie_table.h"
#include "management_interface.h"
#include "message_queue.h"
#include "messenger.h"
#include "ofpmsg_send.h"
#include "openflow_service_interface.h"
#include "secure_channel_receiver.h"
#include "secure_channel_sender.h"
#include "service_interface.h"
#include "switch.h"
#include "xid_table.h"


static struct option long_options[] = {
  { "socket", 1, NULL, 's' },
  { NULL, 0, NULL, 0  },
};

static char short_options[] = "s:";

struct switch_info switch_info;

static const time_t COOKIE_TABLE_AGING_INTERVAL = 3600;

static bool age_cookie_table_enabled = false;


void
usage() {
  printf(
         "OpenFlow Switch Manager.\n"
         "Usage: %s [OPTION]... [DESTINATION-RULE]...\n"
         "\n"
         "  -s, --socket=fd             secure channnel socket\n"
         "  -n, --name=SERVICE_NAME     service name\n"
         "  -l, --logging_level=LEVEL   set logging level\n"
         "  -h, --help                  display this help and exit\n"
         "\n"
         "DESTINATION-RULE:\n"
         "  openflow-message-type::destination-service-name\n"
         "\n"
         "openflow-message-type:\n"
         "  packet_in                   packet-in openflow message type\n"
         "  port_status                 port-status openflow message type\n"
         "  vendor                      vendor openflow message type\n"
         "  state_notify                connection status\n"
         "\n"
         "destination-service-name      destination service name\n"
         , get_executable_name()
         );
}


static int
strtofd( const char *str ) {
  char *ep;
  long l;

  l = strtol( str, &ep, 0 );
  if ( l <  0 || l > INT_MAX || *ep != '\0' ) {
    die( "Invalid socket (%s).", str );
    return 0;
  }
  return ( int ) l;
}


static void
option_parser( int argc, char *argv[] ) {
  int c;

  switch_info.secure_channel_fd = 0; // stdin
  while ( ( c = getopt_long( argc, argv, short_options, long_options, NULL ) ) != -1 ) {
    switch ( c ) {
      case 's':
        switch_info.secure_channel_fd = strtofd( optarg );
        break;

      default:
        usage();
        exit( EXIT_SUCCESS );
        return;
    }
  }
}


static void
service_recv( uint16_t message_type, void *data, size_t data_len ) {
  buffer *buf;
  void *msg;

  buf = alloc_buffer_with_length( data_len );

  msg = append_back_buffer( buf, data_len );
  memcpy( msg, data, data_len );

  service_recv_from_application( message_type, buf );
}


static void
service_send_state( struct switch_info *sw_info, uint64_t *dpid, uint16_t tag ) {
  service_send_to_application( sw_info->state_service_name_list, tag, dpid, NULL );
}


static void
secure_channel_fd_set( fd_set *read_set, fd_set *write_set ) {
  if ( switch_info.secure_channel_fd < 0 ) {
    return;
  }
  FD_SET( switch_info.secure_channel_fd, read_set );
  if ( switch_info.send_queue != NULL && switch_info.send_queue->length > 0 ) {
    FD_SET( switch_info.secure_channel_fd, write_set );
  }
}


static void
secure_channel_fd_isset( fd_set *read_set, fd_set *write_set ) {
  if ( switch_info.secure_channel_fd < 0 ) {
    return;
  }
  if ( FD_ISSET( switch_info.secure_channel_fd, read_set ) ) {
    if ( recv_from_secure_channel( &switch_info ) < 0 ) {
      switch_event_disconnected( &switch_info );
      return;
    }
  }
  if ( FD_ISSET( switch_info.secure_channel_fd, write_set ) ) {
    if ( flush_secure_channel( &switch_info ) < 0 ) {
      switch_event_disconnected( &switch_info );
      return;
    }
  }

  if ( switch_info.recv_queue->length > 0 ) {
    handle_messages_from_secure_channel( &switch_info );
  }
}


static void
switch_set_timeout( long sec, void ( *callback )( void *user_data ), void *user_data ) {
  struct itimerspec interval;

  UNUSED( user_data );

  interval.it_value.tv_sec = sec;
  interval.it_value.tv_nsec = 0;
  interval.it_interval.tv_sec = 0;
  interval.it_interval.tv_nsec = 0;
  add_timer_event_callback( &interval, callback, NULL );
}


static void
switch_unset_timeout( void ( *callback )( void *user_data ) ) {
  delete_timer_event_callback( callback );
}


static void
switch_event_timeout_hello( void *user_data ) {
  UNUSED( user_data );

  if ( switch_info.state != SWITCH_STATE_WAIT_HELLO ) {
    return;
  }
  // delete to hello_wait-timeout timer
  switch_unset_timeout( switch_event_timeout_hello );

  error( "Hello timeout. state:%d, dpid:%#" PRIx64 ", fd:%d.",
         switch_info.state, switch_info.datapath_id, switch_info.secure_channel_fd );
  switch_event_disconnected( &switch_info );
}


static void
switch_event_timeout_features_reply( void *user_data ) {
  UNUSED( user_data );

  if ( switch_info.state != SWITCH_STATE_WAIT_FEATURES_REPLY ) {
    return;
  }
  // delete to features_reply_wait-timeout timer
  switch_unset_timeout( switch_event_timeout_features_reply );

  error( "Features Reply timeout. state:%d, dpid:%#" PRIx64 ", fd:%d.",
         switch_info.state, switch_info.datapath_id, switch_info.secure_channel_fd );
  switch_event_disconnected( &switch_info );
}


int
switch_event_connected( struct switch_info *sw_info ) {
  int ret;

  // send secure channel disconnect state to application
  service_send_state( sw_info, &sw_info->datapath_id, MESSENGER_OPENFLOW_CONNECTED );
  debug( "Send connected state" );
  ret = ofpmsg_send_hello( sw_info );
  if ( ret < 0 ) {
    return ret;
  }
  sw_info->state = SWITCH_STATE_WAIT_HELLO;

  switch_set_timeout( SWITCH_STATE_TIMEOUT_HELLO, switch_event_timeout_hello, NULL );

  return 0;
}


int
switch_event_recv_hello( struct switch_info *sw_info ) {
  int ret;

  if ( sw_info->state == SWITCH_STATE_WAIT_HELLO ) {
    // cancel to hello_wait-timeout timer
    switch_unset_timeout( switch_event_timeout_hello );

    ret = ofpmsg_send_featuresrequest( sw_info );
    if ( ret < 0 ) {
      return ret;
    }
    sw_info->state = SWITCH_STATE_WAIT_FEATURES_REPLY;

    switch_set_timeout( SWITCH_STATE_TIMEOUT_FEATURES_REPLY,
                        switch_event_timeout_features_reply, NULL );
  }

  return 0;
}


int
switch_event_recv_featuresreply( struct switch_info *sw_info, uint64_t *dpid ) {
  int ret;
  uint16_t new_service_name_len;
  char *new_service_name;

  switch ( sw_info->state ) {
  case SWITCH_STATE_WAIT_FEATURES_REPLY:

    sw_info->datapath_id = *dpid;
    sw_info->state = SWITCH_STATE_COMPLETED;

    // cancel to features_reply_wait-timeout timer
    switch_unset_timeout( switch_event_timeout_features_reply );

    // TODO: change process name
    // TODO: set keepalive-timeout

    new_service_name_len = SWITCH_MANAGER_PREFIX_STR_LEN + SWITCH_MANAGER_DPID_STR_LEN + 1;
    new_service_name = xmalloc( new_service_name_len );
    snprintf( new_service_name, new_service_name_len, "%s%" PRIx64, SWITCH_MANAGER_PREFIX, sw_info->datapath_id );

    // rename service_name of messenger
    rename_message_received_callback( get_trema_name(), new_service_name );
    debug( "Rename service name to %s from %s.", new_service_name, get_trema_name() );

    if ( messenger_dump_enabled() ) {
      stop_messenger_dump();
      start_messenger_dump( new_service_name, DEFAULT_DUMP_SERVICE_NAME );
    }

    // notify state and datapath_id
    service_send_state( sw_info, &sw_info->datapath_id, MESSENGER_OPENFLOW_READY );
    debug( "send ready state" );

    ret = ofpmsg_send_setconfig( sw_info );
    if ( ret < 0 ) {
      return ret;
    }
    ret = ofpmsg_send_delete_all_flows( sw_info );
    if ( ret < 0 ) {
      return ret;
    }
    break;

  case SWITCH_STATE_COMPLETED:
    // NOP
    break;

  default:
    notice( "Invalid event 'features reply' from a switch." );
    return -1;

    break;
  }

  return 0;
}


int
switch_event_disconnected( struct switch_info *sw_info ) {
  sw_info->state = SWITCH_STATE_DISCONNECTED;

  if ( sw_info->fragment_buf != NULL ) {
    free_buffer( sw_info->fragment_buf );
    sw_info->fragment_buf = NULL;
  }

  if ( sw_info->send_queue != NULL ) {
    delete_message_queue( sw_info->send_queue );
    sw_info->send_queue = NULL;
  }

  if ( sw_info->recv_queue != NULL ) {
    delete_message_queue( sw_info->recv_queue );
    sw_info->recv_queue = NULL;
  }

  if ( sw_info->secure_channel_fd >= 0 ) {
    close( sw_info->secure_channel_fd );
    sw_info->secure_channel_fd = -1;
  }

  // send secure channle disconnect state to application
  service_send_state( sw_info, &sw_info->datapath_id, MESSENGER_OPENFLOW_DISCONNECTED );
  flush_messenger();
  debug( "send disconnected state" );

  stop_messenger();

  return 0;
}


int
switch_event_recv_openflow_message_from_application( uint64_t *datapath_id, char *application_service_name, buffer *buf ) {

  if ( *datapath_id != switch_info.datapath_id ) {
    error( "Invalid datapath id %#" PRIx64 ".", datapath_id );
    free_buffer( buf );

    return -1;
  }

  return ofpmsg_send( &switch_info, buf, application_service_name );
}


int
switch_event_recv_error( struct switch_info *sw_info ) {
  if ( sw_info->state == SWITCH_STATE_COMPLETED ) {
    return 0;
  }

  return -1;
}


static void
management_recv( uint16_t tag, void *data, size_t data_len ) {
  UNUSED( data );
  UNUSED( data_len );

  switch ( tag ) {
  case DUMP_XID_TABLE:
    dump_xid_table();
    break;

  case DUMP_COOKIE_TABLE:
    dump_cookie_table();
    break;

  case TOGGLE_COOKIE_AGING:
    if ( age_cookie_table_enabled ) {
      delete_periodic_event_callback( age_cookie_table );
      age_cookie_table_enabled = false;
    }
    else {
      add_periodic_event_callback( COOKIE_TABLE_AGING_INTERVAL, age_cookie_table, NULL );
      age_cookie_table_enabled = true;
    }
    break;

  default:
    error( "Undefined management message tag ( tag = %#x )", tag );
  }
}


int
main( int argc, char *argv[] ) {
  int ret;
  int i;
  char *service_name;
  char management_service_name[ MESSENGER_SERVICE_NAME_LENGTH ];

  init_trema( &argc, &argv );
  option_parser( argc, argv );

  create_list( &switch_info.vendor_service_name_list );
  create_list( &switch_info.packetin_service_name_list );
  create_list( &switch_info.portstatus_service_name_list );
  create_list( &switch_info.state_service_name_list );

  // FIXME
#define VENDER_PREFIX "vendor::"
#define PACKET_IN_PREFIX "packet_in::"
#define PORTSTATUS_PREFIX "port_status::"
#define STATE_PREFIX "state_notify::"
  for ( i = optind; i < argc; i++ ) {
    if ( strncmp( argv[i], VENDER_PREFIX, strlen( VENDER_PREFIX ) ) == 0 ) {
      service_name = xstrdup( argv[i] + strlen( VENDER_PREFIX ) );
      insert_in_front( &switch_info.vendor_service_name_list, service_name );
    }
    else if ( strncmp( argv[i], PACKET_IN_PREFIX, strlen( PACKET_IN_PREFIX ) ) == 0 ) {
      service_name = xstrdup( argv[i] + strlen( PACKET_IN_PREFIX ) );
      insert_in_front( &switch_info.packetin_service_name_list, service_name );
    }
    else if ( strncmp( argv[i], PORTSTATUS_PREFIX, strlen( PORTSTATUS_PREFIX ) ) == 0 ) {
      service_name = xstrdup( argv[i] + strlen( PORTSTATUS_PREFIX ) );
      insert_in_front( &switch_info.portstatus_service_name_list, service_name );
    }
    else if ( strncmp( argv[i], STATE_PREFIX, strlen( STATE_PREFIX ) ) == 0 ) {
      service_name = xstrdup( argv[i] + strlen( STATE_PREFIX ) );
      insert_in_front( &switch_info.state_service_name_list, service_name );
    }
  }

  fcntl( switch_info.secure_channel_fd, F_SETFL, O_NONBLOCK );
  // default switch configuration
  switch_info.config_flags = OFPC_FRAG_NORMAL;
  switch_info.miss_send_len = UINT16_MAX;

  switch_info.fragment_buf = NULL;
  switch_info.send_queue = create_message_queue();
  switch_info.recv_queue = create_message_queue();

  init_xid_table();
  init_cookie_table();

  set_fd_set_callback( secure_channel_fd_set );
  set_check_fd_isset_callback( secure_channel_fd_isset );
  add_message_received_callback( get_trema_name(), service_recv );

  snprintf( management_service_name , MESSENGER_SERVICE_NAME_LENGTH,
            "%s.m", get_trema_name() );
  management_service_name[ MESSENGER_SERVICE_NAME_LENGTH - 1 ] = '\0';
  add_message_received_callback( management_service_name, management_recv );

  ret = switch_event_connected( &switch_info );
  if ( ret < 0 ) {
    error( "Failed to set connected state." );
    return -1;
  }

  start_trema();

  finalize_xid_table();
  finalize_cookie_table();

  return 0;
}


/*
 * Local variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */
