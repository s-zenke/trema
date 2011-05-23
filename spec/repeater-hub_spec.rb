#
# Author: Yasuhito Takamiya <yasuhito@gmail.com>
#
# Copyright (C) 2008-2011 NEC Corporation
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2, as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
#


require File.join( File.dirname( __FILE__ ), "spec_helper" )
require "trema"
require "pp"


include Trema


class RepeaterHub < Controller
  def packet_in message
    send_flow_mod_add(
      message.datapath_id,
      :match => Match.from( message ),
      :buffer_id => message.buffer_id,
      :action => ActionOutput.new( OFPP_FLOOD )
    )
    unless message.buffered?
      send_packet_out(
        message.datapath_id,
        message.buffer_id,
        message.in_port,
        ActionOutput.new( OFPP_FLOOD ),
        message.data
      )
    end
  end
end


describe RepeaterHub do
  before :each do
    trema_conf do
      vswitch("repeater_hub") {
        datapath_id "0xabc"
      }

      vhost("host1") {
        promisc "On"
        ip "192.168.0.1"
        netmask "255.255.0.0"
        mac "00:00:00:01:00:01"
      }

      vhost("host2") {
        promisc "On"
        ip "192.168.0.2"
        netmask "255.255.0.0"
        mac "00:00:00:01:00:02"
      }

      vhost("host3") {
        promisc "On"
        ip "192.168.0.3"
        netmask "255.255.0.0"
        mac "00:00:00:01:00:03"
      }

      link "repeater_hub", "host1"
      link "repeater_hub", "host2"
      link "repeater_hub", "host3"
    end
  end


  it "should send flow_mod messages" do
    trema_session( RepeaterHub ) do
      Switch[ "repeater_hub" ].should_receive( :flow_mod_add ).at_least( 1 )
    end    
  end
  
  
  it "should respond to packet_in" do
    trema_session( RepeaterHub ) do
      Host[ "host1" ].send_packet Host[ "host2" ], :pps => 500

      Switch[ "repeater_hub" ].flows.each do | each |
        # pp each
      end

      Host[ "host1" ].show_tx_stats
      Host[ "host2" ].show_rx_stats
    end
  end
end


### Local variables:
### mode: Ruby
### coding: utf-8-unix
### indent-tabs-mode: nil
### End: