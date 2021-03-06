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


require "fileutils"
require "trema/executables"
require "trema/flow"
require "trema/ofctl"
require "trema/openflow-switch"
require "trema/path"
require "trema/process"
require "trema/switch"


module Trema
  #
  # Open vSwitch http://openvswitch.org/
  #
  class OpenVswitch < OpenflowSwitch
    #
    # Creates a new Open vSwitch from {DSL::Vswitch}
    #
    # @example
    #   switch = Trema::OpenVswitch.new( stanza, 6633 )
    #
    # @return [OpenVswitch]
    #
    # @api public
    #
    def initialize stanza, port
      super stanza
      @port = port
      @interfaces = []
      @ofctl = Trema::Ofctl.new
    end


    #
    # Returns if switch is initialized by switch daemon
    #
    # @example
    #   switch.ready?
    #
    # @return [boolean]
    #
    # @api public
    #
    def ready?
      begin
        @log ||= File.open( log_file, "r" )
        while @log.gets
          return false if $_.nil?
          return true if /flow_mod \(xid=.+\): DEL: cookie:0x0 idle:0 hard:0 pri:0 buf:0 flags:0 actions=drop/=~ $_
        end
      rescue
        false
      end
    end
    

    #
    # Add a network interface used for a virtual port
    #
    # @example
    #   switch.add_interface "trema3-0"
    #
    # @return [undefined]
    #
    # @api public
    #
    def add_interface name
      @interfaces << name
    end


    #
    # Returns the unique name that identifies an Open vSwitch instance
    #
    # @example
    #   switch.datapath_id
    #
    # @return [String]
    #
    # @api public
    #
    def datapath
      "vsw_#{ @name }"
    end


    #
    # Runs an Open vSwitch process
    #
    # @example
    #   switch.run!
    #
    # @return [undefined]
    #
    # @api public
    #
    def run!
      FileUtils.rm_f log_file
      sh "sudo #{ Executables.ovs_openflowd } #{ options }"
    end


    #
    # Kills running Open vSwitch process
    #
    # @example
    #   switch.shutdown!
    #
    # @return [undefined]
    #
    # @api public
    #
    def shutdown!
      Trema::Process.read( pid_file, @name ).kill!
    end
    

    #
    # Returns flow entries
    #
    # @example
    #   switch.flows
    #
    # @return [Array]
    #
    # @api public
    #
    def flows
      @ofctl.flows( self ).select do | each |
        not each.rspec_flow?
      end
    end
    

    #
    # A stub handler when flow_mod_add is called
    #
    # @example
    #   def switch.flow_mod_add line
    #     p "New flow_mod!: #{ line }"
    #   end
    #
    # @return [undefined]
    #
    # @api public
    #
    def flow_mod_add line
      # Do nothing
    end
    

    ################################################################################
    private
    ################################################################################


    #
    # The IP address
    #
    # @return [String]
    #
    # @api private
    #
    def ip
      @stanza[ :ip ]
    end


    #
    # Command-line options
    #
    # @return [String]
    #
    # @api private
    #
    def options
      default_options.join( " " ) + " netdev@#{ datapath } tcp:#{ ip }:#{ @port }"
    end


    #
    # The list of --xyz= command-line options
    #
    # @return [Array]
    #
    # @api private
    #
    def default_options
      [
       "--detach",
       "--out-of-band",
       "--no-resolv-conf",
       "--fail=closed",
       "--inactivity-probe=180",
       "--rate-limit=40000",
       "--burst-limit=20000",
       "--pidfile=#{ pid_file }",
       "--verbose=ANY:file:dbg",
       "--verbose=ANY:console:err",
       "--log-file=#{ log_file }",
       "--datapath-id=#{ dpid_long }",
      ] + ports_option
    end


    #
    # --ports= option
    #
    # @return [Array]
    #
    # @api private
    #
    def ports_option
      @interfaces.empty? ? [] : [ "--ports=#{ @interfaces.join( "," ) }" ]
    end


    #
    # The path of pid file
    #
    # @return [String]
    #
    # @api private
    #
    def pid_file
      File.join Trema.tmp, "openflowd.#{ @name }.pid"
    end
    

    #
    # The path of log file
    #
    # @return [String]
    #
    # @api private
    #
    def log_file
      "#{ Trema.tmp }/log/openflowd.#{ @name }.log"
    end
  end
end


### Local variables:
### mode: Ruby
### coding: utf-8-unix
### indent-tabs-mode: nil
### End:
