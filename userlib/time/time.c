/******************************************************************************\
Copyright (C) 2017-2020 Peter Bosch

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
\******************************************************************************/

/**
 * @file userlib/time/time.c
 *
 * Part of posnk kernel
 *
 * Written by Peter Bosch <peterbosc@gmail.com>
 *
 */

#include <sys/types.h>
#include <sys/syscall.h>
 
time_t	time( time_t *t )
{
	time_t v;
	
	v = ( time_t ) syscall( SYS_TIME, 0, 0, 0, 0, 0, 0 );
	
	if ( t )
		*t = v;
		
	return v;
}
