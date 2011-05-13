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


require "trema/cli"
require "trema/phost"
require "trema/network-component"


module Trema
  #
  # The controller class of host
  #
  class Host < NetworkComponent
    #
    # Set a network interface
    #
    # @example
    #   host.interface #=> "trema0-1"
    #
    # @return [String]
    #
    # @api public
    #
    attr_accessor :interface


    #
    # Creates a new Trema host from {DSL::Vhost}
    #
    # @example
    #   host = Trema::Host.new( stanza )
    #
    # @return [Host]
    #
    # @api public
    #
    def initialize stanza
      @stanza = stanza
      @phost = Phost.new( self )
      @cli = Cli.new( self )
      self.class.add self
    end


    #
    # Define host attribute accessors
    #
    # @example
    #   host.name  # delegated to @stanza[ :name ]
    #
    # @return an attribute value
    #
    # @api public
    #
    def method_missing message, *args
      @stanza.__send__ :[], message
    end


    #
    # Runs a host process
    #
    # @example
    #   host.run! => self
    #
    # @return [Host]
    #
    # @api public
    #
    def run!
      @phost.run
      @cli.set_ip_and_mac_address
      @cli.enable_promisc if @stanza[ :promisc ]
      self
    end


    #
    # Kills running host
    #
    # @example
    #   host.shutdown!
    #
    # @return [undefined]
    #
    # @api public
    #
    def shutdown!
      @phost.shutdown!
    end


    #
    # Add arp entries of <code>hosts</code>
    #
    # @example
    #   host.add_arp_entry [ host1, host2, host3 ]
    #
    # @return [undefined]
    #
    # @api public
    #
    def add_arp_entry hosts
      hosts.each do | each |
        @cli.add_arp_entry each
      end
    end


    #
    # Send packets to <code>dest</code>
    #
    # @example
    #   host.send_packet host1, :pps => 100
    #
    # @return [undefined]
    #
    # @api public
    #
    def send_packet dest, options = {}
      @cli.send_packets dest, options
    end


    #
    # Returns tx stats
    #
    # @example
    #   host.tx_stats
    #
    # @return [Stat]
    #
    # @api public
    #
    def tx_stats
      @cli.tx_stats
    end


    #
    # Returns rx stats
    #
    # @example
    #   host.rx_stats
    #
    # @return [Stat]
    #
    # @api public
    #
    def rx_stats
      @cli.rx_stats
    end
  end
end


### Local variables:
### mode: Ruby
### coding: utf-8-unix
### indent-tabs-mode: nil
### End:
