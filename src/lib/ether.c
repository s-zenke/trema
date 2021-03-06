/*
 * Author: Naoyoshi Tada
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


#include <assert.h>
#include "log.h"
#include "packet_info.h"
#include "wrapper.h"


#ifdef UNIT_TESTING

#ifdef debug
#undef debug
#endif
#define debug mock_debug
void mock_debug( const char *format, ... );

#endif // UNIT_TESTING


void
fill_ether_padding( buffer *buf ) {
  assert( buf != NULL );

  if ( buf->length + ETH_FCS_LENGTH < ETH_MINIMUM_LENGTH ) {
    size_t padding_length = ETH_MINIMUM_LENGTH - buf->length - ETH_FCS_LENGTH;
    debug( "Adding %u octets padding ( original frame length = %u ).", buf->length, padding_length );
    append_back_buffer( buf, padding_length );
  }
}


bool
parse_ether( buffer *buf ) {
  assert( buf != NULL );
  assert( packet_info( buf )->l2_data.eth != NULL );

  size_t frame_length = buf->length - ( size_t ) ( ( char * ) ( packet_info( buf )->l2_data.l2 ) - ( char * ) buf->data ) - ETH_PREPADLEN;
  ether_header_t *eth = packet_info( buf )->l2_data.eth;
  size_t ether_length = sizeof( *eth ) - ETH_PREPADLEN;
  if ( frame_length < ether_length ) {
    debug( "Frame length is shorter than the length of an Ethernet header ( frame length = %u ).", frame_length );
    return false;
  }

  debug( "Parsing an Ethernet frame "
         "( source mac = %02x:%02x:%02x:%02x:%02x:%02x, "
         "destination mac = %02x:%02x:%02x:%02x:%02x:%02x ).",
         eth->macsa[ 0 ], eth->macsa[ 1 ], eth->macsa[ 2 ],
         eth->macsa[ 3 ], eth->macsa[ 4 ], eth->macsa[ 5 ],
         eth->macda[ 0 ], eth->macda[ 1 ], eth->macda[ 2 ],
         eth->macda[ 3 ], eth->macda[ 4 ], eth->macda[ 5 ] );

  if ( eth->macsa[ 0 ] & 0x01 ) {
    debug( "Source MAC address is multicast or broadcast." );
    return false;
  }

  uint16_t type = ntohs( eth->type );
  vlantag_header_t *vlan_tag = ( void * ) ( eth + 1 );
  uint8_t next_vlan_tags = 0;
  while ( type == ETH_ETHTYPE_TPID ) {
    ether_length += sizeof( *vlan_tag );
    if ( frame_length < ether_length ) {
      debug( "Too short 802.1Q frame ( frame length = %u ).", frame_length );
      return false;
    }
    type = ntohs( vlan_tag->type );
    vlan_tag++;
    next_vlan_tags++;
    debug( "802.1Q header found ( type = %#x, # of headers = %u ).", type, next_vlan_tags );
  }

  if ( type <= ETH_ETHTYPE_8023 ) {
    snap_header_t *snap = ( void * ) vlan_tag;
    ether_length += sizeof( *snap );
    if ( frame_length < ether_length ) {
      debug( "LLC without SNAP or too short SNAP frame ( frame length = %u ).", frame_length );
      return false;
    }
    if ( frame_length < ( size_t ) type ) {
      debug( "Frame length is shorter than the length header field value "
             "( frame length = %u, length field value = %u ).", frame_length, type );
      return false;
    }
    if ( ( snap->llc[ 0 ] == 0xaa ) && ( snap->llc[ 1 ] == 0xaa ) ) {
      if ( snap->llc[ 2 ] == 0x03 || snap->llc[ 2 ] == 0xF3 || snap->llc[ 2 ] == 0xE3
           || snap->llc[ 2 ] == 0xBF || snap->llc[ 2 ] == 0xAF ) {
        type = ntohs( snap->type );
      } else {
        debug( "Unexpected SNAP frame ( length = %u, llc = 0x%02x%02x%02x, oui = 0x%02x%02x%02x, type = %u ).",
               type, snap->llc[ 0 ], snap->llc[ 1 ], snap->llc[ 2 ],
               snap->oui[ 0 ], snap->oui[ 1 ], snap->oui[ 2 ], ntohs( snap->type ) );
        return false;
      }
    } else {
      debug( "Unhandled 802.3 frame ( length = %u, llc = 0x%02x%02x%02x, oui = 0x%02x%02x%02x, type = %u ).",
             type, snap->llc[ 0 ], snap->llc[ 1 ], snap->llc[ 2 ],
             snap->oui[ 0 ], snap->oui[ 1 ], snap->oui[ 2 ], ntohs( snap->type ) );
      type = ETH_ETHTYPE_UKNOWN;
    }
  }

  packet_info( buf )->nvtags = next_vlan_tags;
  if ( next_vlan_tags > 0 ) {
    packet_info( buf )->vtag = ( void * ) ( eth + 1 );
    debug( "802.1Q header found ( tci = %#x, type = %#x ).",
           ntohs( packet_info( buf )->vtag->tci ), ntohs( packet_info( buf )->vtag->type ) );
  }
  else {
    packet_info( buf )->vtag = NULL;
    debug( "No 802.1Q header found." );
  }

  debug( "Ethernet type = %#x.", type );

  packet_info( buf )->ethtype = type;
  packet_info( buf )->l3_data.l3 = ( char * ) packet_info( buf )->l2_data.l2 + ETH_PREPADLEN + ether_length;

  return true;
}


/*
 * Local variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */
