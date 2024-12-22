/*
  	AWSRTC_h.h

	Mininal library for DS3231 RTC
	
	(c) 2024 F.Lesage

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
#ifndef _AWSRTC_h
#define _AWSRTC_h


class AWSRTC
{

	private:

		uint8_t bcd_to_decimal( uint8_t );
		uint8_t decimal_to_bcd( uint8_t );

	public:
				AWSRTC( void ) = default;
		bool	begin( void );
		void	get_datetime( struct tm * );
		void	set_datetime( time_t * );

};

#endif
