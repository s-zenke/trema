/*
 * Link log_stubs.o against test executables that uses logging
 * functions. This file defines some stub (null) functions that
 * bypasses actual logging.
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


#include <stdarg.h>
#include "checks.h"


void
mock_vsyslog( int priority, const char *format, va_list ap ) {
  UNUSED( priority );
  UNUSED( format );
  UNUSED( ap );
}


int
mock_vprintf( const char *format, va_list ap ) {
  UNUSED( format );
  UNUSED( ap );

  return 0;
}


int
mock_printf( const char *format, ... ) {
  UNUSED( format );

  return 0;
}


/*
 * Local variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */
