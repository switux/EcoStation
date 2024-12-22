/*
  	lorawan.h

	(c) 2023-2024 F.Lesage

	This program is free software: you can redistribute it and/or modify it
	under the terms of the GNU General Public License as published by the
	Free Software Foundation, either version 3 of the License, or (at your option)
	any later version.

	This program is distributed in the hope that it will be useful, but
	WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
	or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
	more details.

	You should have received a copy of the GNU General Public License along
	with this program. If not, see <https://www.gnu.org/licenses/>.
*/

#pragma once
#ifndef _lorawan_H
#define _lorawan_H

#include <array>
#include <HardwareSerial.h>
#include <SPI.h>

#include <lmic.h>
#include <hal/hal.h>

static uint8_t			DEVEUI[8]	= { 0x00 };	// NOSONAR
static uint8_t			APPKEY[16]	= { 0x00 };	// NOSONAR
static const uint8_t	APPEUI[8]	= { 0x00 };	// NOSONAR

class AWSLoraWAN
{
	private:

		bool		debug_mode		= false;
		bool		joined 			= false;
		osjob_t		sendjob;
		uint8_t		mydata[64];
		uint32_t	mylen;
	
		bool 	check_joined( void );

	public:

				AWSLoraWAN( void ) = default;
		bool	begin( const uint8_t *, const uint8_t *, bool );
		bool	join( void );
		bool	has_joined( void );
		void	prepare_for_deep_sleep( int );
		void	restore_after_deep_sleep( void );
		void	send( osjob_t * );
		void	send_data( uint8_t *, uint8_t );

};

#endif
