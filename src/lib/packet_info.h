/*
 * Functions for accessing commnly-used header fields values.
 *
 * Usage:
 *
 *   // Parse an Ethernet frame stored in eth_frame. Now the frame is
 *   // parsed and the results are stored in a newly allocated memory area.
 *   parse_packet( eth_frame );
 *
 *   // Now you can refer to the header field values like follows.
 *   switch ( packet_info( eth_frame )->ethtype ) {
 *     case ETH_ETHTYPE_IPV4:
 *       ...
 *     case ETH_ETHTYPE_ARP:
 *      ...
 *
 *   // Finally free the buffer. Note that you have to call free_packet()
 *   // instead of free_buffer() because parse_packet() allocates a memory
 *   // area to save parse results and it is not freed by free_buffer().
 *   free_packet( eth_frame );
 *
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


#ifndef PACKET_INFO_H
#define PACKET_INFO_H


#include "bool.h"
#include "arp.h"
#include "ether.h"
#include "icmp.h"
#include "ipv4.h"
#include "tcp.h"
#include "udp.h"


typedef struct packet_header_info {
  uint16_t ethtype;
  uint8_t nvtags;
  uint8_t ipproto;
  uint32_t nexthop;
  union {
    void *l2;
    ether_header_t *eth;
  } l2_data;
  vlantag_header_t *vtag;
  union {
    void *l3;
    arp_header_t *arp;
    ipv4_header_t *ipv4;
  } l3_data;
  union {
    void *l4;
    icmp_header_t *icmp;
    tcp_header_t *tcp;
    udp_header_t *udp;
  } l4_data;
} packet_header_info;


void free_packet( buffer *buf );
void alloc_packet( buffer *buf );


#define packet_info( buf ) ( ( packet_header_info * ) ( ( buf )->user_data ) )


#endif // PACKET_INFO_H


/*
 * Local variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */
