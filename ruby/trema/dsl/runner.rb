#
# Runs DSL objects in right order.
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


module Trema
  module DSL
    class Runner
      def initialize context
        @context = context
      end


      def run
        maybe_run_trema_services
        maybe_run_apps
      end


      def daemonize
        maybe_run_trema_services
        maybe_daemonize_apps
      end


      ################################################################################
      private
      ################################################################################


      def maybe_run_trema_services
        maybe_run_tremashark
        maybe_run_switch_manager
        maybe_run_packetin_filter
        maybe_create_links
        maybe_run_hosts
        maybe_run_switches
      end


      def maybe_run_tremashark
        @context.tremashark.run if @context.tremashark
      end


      def maybe_run_switch_manager
        @context.switch_manager.run if @context.switch_manager
      end


      def maybe_run_packetin_filter
        @context.packetin_filter.run if @context.packetin_filter
      end


      def maybe_create_links
        maybe_delete_links # Fool proof
        @context.links.each do | name, link |
          link.up!
        end
      end


      def maybe_delete_links
        @context.links.each do | name, link |
          link.down!
        end
      end


      def maybe_run_hosts
        @context.hosts.each do | name, host |
          host.run
        end
      end


      def maybe_run_switches
        @context.switches.each do | name, switch |
          switch.run
        end

        @context.hosts.each do | name, host |
          host.add_arp_entry @context.hosts.values - [ host ]
        end
      end


      def maybe_run_apps
        return if @context.apps.empty?

        @context.apps[ 0..-2 ].each do | each |
          each.daemonize
        end
        trap( "SIGINT" ) do
          print( "\nterminated\n" )
          exit(0)
        end
        pid = ::Process.fork do
          @context.apps.last.run
        end
        ::Process.waitpid pid
      end


      def maybe_daemonize_apps
        @context.apps.each do | each |
          each.daemonize
        end
      end
    end
  end
end


### Local variables:
### mode: Ruby
### coding: utf-8-unix
### indent-tabs-mode: nil
### End:
