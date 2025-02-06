/*
  	AT24C.h

	(c) 2025 F.Lesage

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
#ifndef _ATC24C_h
#define _ATC24C_h

#include "Wire.h"
#include <array>

class AT24C {

	private:

		uint8_t			eeprom_address;
		uint8_t			error			= 0;
		const uint16_t	max_page_size	= 32;

	public:

					AT24C( void );
		explicit	AT24C( uint8_t );

		uint8_t	get_error( void );
		template <typename T>
		bool	read( uint16_t, T * );
		bool	read_buffer( uint16_t, uint8_t *, uint8_t );
		bool	read_page( uint16_t, uint8_t *, uint8_t );
		template <typename T>
		bool	write( uint16_t, T * );
		bool	write_buffer( uint16_t, const uint8_t *, uint8_t );
		bool	write_page( uint16_t, const uint8_t *, uint8_t );

};

template <typename T>
bool AT24C::read( uint16_t data_addr, T *data )
{
	constexpr size_t			size = sizeof( T );
	std::array<uint8_t,size>	buffer;

	Wire.beginTransmission( eeprom_address );
	Wire.write( ( data_addr >> 8 ) & 0xFF );
	Wire.write( data_addr & 0xFF );

	delay( 10 );

	if ( ( error = Wire.endTransmission() ) != 0 )
		return false;

	if ( Wire.requestFrom( eeprom_address, size ) != size )
		return false;

	for ( size_t i = 0; ( i < size ) && Wire.available(); i++ )
		buffer[ i ] = Wire.read();

	memcpy( data, buffer.data(), size );

	return true;
}

template <typename T>
bool AT24C::write( uint16_t data_addr, T *data )
{
	constexpr size_t			size = sizeof( T );
	std::array<uint8_t,size>	buffer;

	memcpy( buffer.data(), &data, size );

	return write_buffer( data_addr, buffer.data(), size );
}

#endif
