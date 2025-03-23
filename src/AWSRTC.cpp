/*
  	AWSRTC.cpp

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

#include <Wire.h>
#include <time.h>
#include <HardwareSerial.h>

#include "AWSRTC.h"

const uint8_t DS3231_I2C_ADDRESS = 0x68;

uint8_t AWSRTC::bcd_to_decimal( uint8_t i )
{
	return ((( i / 16 ) * 10 ) + ( i % 16 ));
}

uint8_t AWSRTC::decimal_to_bcd( uint8_t i )
{
	return ((( i / 10 ) * 16 ) + ( i % 10 ));
}

bool AWSRTC::begin( void )
{
	Wire.begin();
	Wire.beginTransmission( DS3231_I2C_ADDRESS );
	uint8_t error = Wire.endTransmission();
	return ( error == 0 );
}

void AWSRTC::set_datetime( time_t *now )
{
	struct tm	dummy;
	struct tm	*utc_time = gmtime_r( now, &dummy );

	Wire.beginTransmission( DS3231_I2C_ADDRESS );
	Wire.write( 0 );
	Wire.write( decimal_to_bcd( utc_time->tm_sec ));
	Wire.write( decimal_to_bcd( utc_time->tm_min ));
	Wire.write( decimal_to_bcd( utc_time->tm_hour ));
	Wire.write( decimal_to_bcd( utc_time->tm_wday ));
	Wire.write( decimal_to_bcd( utc_time->tm_mday ));
	Wire.write( decimal_to_bcd( utc_time->tm_mon ) + 1 );
	Wire.write( decimal_to_bcd( utc_time->tm_year - 100 ));
	Wire.endTransmission();

	Serial.printf( "[RTC       ] [INFO ] Setting time: %04d-%02d-%02d %02d:%02d:%02d\n", 1900+utc_time->tm_year, utc_time->tm_mon + 1, utc_time->tm_mday, utc_time->tm_hour, utc_time->tm_min, utc_time->tm_sec );
}

void AWSRTC::get_datetime( struct tm *utc_time )
{
	Wire.beginTransmission( DS3231_I2C_ADDRESS );
	Wire.write( 0 );
	Wire.endTransmission();
	Wire.requestFrom( DS3231_I2C_ADDRESS, static_cast<uint8_t>(7) );
	utc_time->tm_sec = bcd_to_decimal( Wire.read() & 0x7F );
	utc_time->tm_min = bcd_to_decimal( Wire.read() );
	utc_time->tm_hour = bcd_to_decimal( Wire.read() & 0x3F );
	utc_time->tm_wday = bcd_to_decimal( Wire.read() );
	utc_time->tm_mday = bcd_to_decimal( Wire.read() );
	utc_time->tm_mon = bcd_to_decimal( Wire.read() - 1 );
	utc_time->tm_year = 100 + bcd_to_decimal( Wire.read() );
}
