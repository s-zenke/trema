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


require File.join( File.dirname( __FILE__ ), "..", "..", "spec_helper" )
require "trema/dsl/context"


module Trema
  module DSL
    describe Context do
      before :each do
        @context = Context.new
      end


      it "should remember hosts" do
        Host.add mock( "host #0", :name => "host #0" )
        Host.add mock( "host #1", :name => "host #1" )
        Host.add mock( "host #2", :name => "host #2" )

        @context.should have( 3 ).hosts

        @context.hosts[ "host #0" ].name.should == "host #0"
        @context.hosts[ "host #1" ].name.should == "host #1"
        @context.hosts[ "host #2" ].name.should == "host #2"
      end


      it "should remember switches" do
        Trema::Switch.add mock( "switch #0", :name => "switch #0" )
        Trema::Switch.add mock( "switch #1", :name => "switch #1" )
        Trema::Switch.add mock( "switch #2", :name => "switch #2" )

        @context.should have( 3 ).switches
      end


      it "should remember apps" do
        Trema::App.add mock( "app #0", :name => "app #0" )
        Trema::App.add mock( "app #1", :name => "app #1" )
        Trema::App.add mock( "app #2", :name => "app #2" )

        @context.should have( 3 ).apps
      end


      it "should remember filter settings" do
        Trema::PacketinFilter.add mock( "filter", :name => "filter" )

        @context.packetin_filter.name.should == "filter"
      end


      it "should remember switch manager" do
        Trema::SwitchManager.add mock( "switch manager", :name => "switch manager" )

        @context.switch_manager.name.should == "switch manager"
      end


      it "should return default switch manager" do
        switch_manager = @context.switch_manager

        switch_manager.rule[ :port_status ].should == "default"
        switch_manager.rule[ :packet_in ].should == "default"
        switch_manager.rule[ :state_notify ].should == "default"
      end


      it "should route events to an app" do
        Trema::App.add mock( "app", :name => "App" )

        switch_manager = @context.switch_manager
        switch_manager.rule[ :port_status ].should == "App"
        switch_manager.rule[ :packet_in ].should == "App"
        switch_manager.rule[ :state_notify ].should == "App"
      end


      it "should raise if no event routing is given" do
        Trema::App.add mock( "app #0", :name => "App #0" )
        Trema::App.add mock( "app #1", :name => "App #1" )

        lambda do
          @context.switch_manager
        end.should raise_error
      end
    end
  end
end


### Local variables:
### mode: Ruby
### coding: utf-8-unix
### indent-tabs-mode: nil
### End:
