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

class AT24C {

	private:

		uint8_t			eeprom_address	= 0x50;
		const uint16_t	max_page_size	= 32;

	public:

			AT24C( void ) = default;
			AT24C( uint8_t );
			
		bool read_buffer( uint16_t, uint8_t *, uint8_t );
		bool read_byte( uint16_t, uint8_t * );
		bool read_page( uint16_t, uint8_t *, uint8_t );
		bool write_buffer( uint16_t, const uint8_t *, uint8_t );
		bool write_byte( uint16_t, uint8_t );
		bool write_page( uint16_t, const uint8_t *, uint8_t );


};

#endif
