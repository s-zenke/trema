#
# Trema application class.
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


require "trema/network-component"


module Trema
  class App < NetworkComponent
    attr_reader :name
    

    def initialize stanza
      @name = stanza[ :name ]
      @stanza = stanza
      self.class.add self
    end


    def daemonize
      if options
        sh "#{ command } --name #{ name } -d #{ options.join ' ' }"
      else
        sh "#{ command } --name #{ name } -d"
      end
    end


    def run
      if options
        sh "#{ command } --name #{ name } #{ options.join ' ' }"
      else
        sh "#{ command } --name #{ name }"
      end
    end


    def shutdown!
      Trema::Process.read( pid_file, @name ).kill!
    end


    ################################################################################
    private
    ################################################################################


    def pid_file
      File.join Trema.tmp, "#{ @name }.pid"
    end

    
    def command
      @stanza[ :path ]
    end


    def options
      @stanza[ :options ]
    end
  end
end


### Local variables:
### mode: Ruby
### coding: utf-8-unix
### indent-tabs-mode: nil
### End:
