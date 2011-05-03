/*
 * Unit tests for timer.[ch]
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


#include <errno.h>
#include <limits.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "checks.h"
#include "cmockery.h"
#include "doubly_linked_list.h"
#include "timer.h"
#include "unittest.h"


/********************************************************************************
 * Static data and types
 ********************************************************************************/

// FIXME
typedef struct timer_callback {
  void ( *function )( void *user_data );
  struct timespec expires_at;
  struct timespec interval;
  void *user_data;
} timer_callback;


extern dlist_element *timer_callbacks;


/********************************************************************************
 * Mocks.
 ********************************************************************************/

int
mock_clock_gettime( clockid_t clk_id, struct timespec *tp ) {
  UNUSED( clk_id );
  UNUSED( tp );

  return ( int ) mock();
}


void
mock_error( const char *format, ... ) {
  // Do nothing.
  UNUSED( format );
}


void
mock_debug( const char *format, ... ) {
  // Do nothing.
  UNUSED( format );
}


/********************************************************************************
 * Helper functions.
 ********************************************************************************/

static timer_callback *
find_timer_callback( void ( *callback )( void *user_data ) ) {
  dlist_element *e;
  timer_callback *cb;

  cb = NULL;
  for ( e = timer_callbacks->next; e; e = e->next ) {
    cb = e->data;
    if ( cb->function == callback ) {
      return cb;
    }
  }
  return NULL;
}


/********************************************************************************
 * Tests
 ********************************************************************************/

static void
mock_timer_event_callback( void *user_data ) {
  UNUSED( user_data );
}


static void
test_timer_event_callback() {
  init_timer();

  will_return_count( mock_clock_gettime, 0, -1 );

  struct itimerspec interval;
  interval.it_value.tv_sec = 1;
  interval.it_value.tv_nsec = 1000;
  interval.it_interval.tv_sec = 2;
  interval.it_interval.tv_nsec = 2000;
  assert_true( add_timer_event_callback( &interval, mock_timer_event_callback, ( void * ) "It's time!!!" ) );

  timer_callback *callback = find_timer_callback( mock_timer_event_callback );
  assert_true( callback != NULL );
  assert_true( callback->function == mock_timer_event_callback );
  assert_string_equal( callback->user_data, "It's time!!!" );
  assert_int_equal( callback->interval.tv_sec, 2 );
  assert_int_equal( callback->interval.tv_nsec, 2000 );

  delete_timer_event_callback( mock_timer_event_callback );
  assert_true( find_timer_callback( mock_timer_event_callback ) == NULL );

  finalize_timer();
}


static void
test_periodic_event_callback() {
  init_timer();

  will_return_count( mock_clock_gettime, 0, -1 );
  assert_true( add_periodic_event_callback( 1, mock_timer_event_callback, ( void * ) "It's time!!!" ) );

  timer_callback *callback = find_timer_callback( mock_timer_event_callback );
  assert_true( callback != NULL );
  assert_true( callback->function == mock_timer_event_callback );
  assert_string_equal( callback->user_data, "It's time!!!" );
  assert_int_equal( callback->interval.tv_sec, 1 );
  assert_int_equal( callback->interval.tv_nsec, 0 );

  delete_periodic_event_callback( mock_timer_event_callback );
  assert_true( find_timer_callback( mock_timer_event_callback ) == NULL );

  finalize_timer();
}


static void
test_add_timer_event_callback_fail_with_invalid_timespec() {
  init_timer();

  will_return_count( mock_clock_gettime, 0, -1 );

  struct itimerspec interval;
  interval.it_value.tv_sec = 0;
  interval.it_value.tv_nsec = 0;
  interval.it_interval.tv_sec = 0;
  interval.it_interval.tv_nsec = 0;
  assert_false( add_timer_event_callback( &interval, mock_timer_event_callback, ( void * ) "USER_DATA" ) );

  finalize_timer();
}


static void
test_nonexistent_timer_event_callback() {
  assert_false( delete_timer_event_callback( mock_timer_event_callback ) );
}


static void
test_clock_gettime_fail_einval() {
  init_timer();

  will_return_count( mock_clock_gettime, -1, -1 );
  assert_false( add_periodic_event_callback( 1, mock_timer_event_callback, ( void * ) "USER_DATA" ) );

  finalize_timer();
}


/********************************************************************************
 * Run tests.
 ********************************************************************************/

int
main() {
  const UnitTest tests[] = {
    unit_test( test_timer_event_callback ),
    unit_test( test_periodic_event_callback ),
    unit_test( test_add_timer_event_callback_fail_with_invalid_timespec ),
    unit_test( test_nonexistent_timer_event_callback ),
    unit_test( test_clock_gettime_fail_einval ),
  };
  return run_tests( tests );
}


/*
 * Local variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */
