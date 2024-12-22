/*
	common.h

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
#ifndef _common_H
#define _common_H

#include "Embedded_Template_Library.h"
#include "etl/string.h"
#include "etl/string_utilities.h"

#include "build_id.h"

// Force DEBUG output even if not activated by external button
const uint8_t DEBUG_MODE = 1;

#define COMPACT_DATA_FORMAT_VERSION	0x01

extern const unsigned long 		US_SLEEP;
extern const etl::string<12>	REV;
extern HardwareSerial			Serial1;

enum class aws_device_t : unsigned long {
	NO_SENSOR			= 0x00000000,
	MLX_SENSOR			= 0x00000001,
	TSL_SENSOR			= 0x00000002,
	BME_SENSOR			= 0x00000004,
	WIND_VANE_SENSOR	= 0x00000008,	// UNUSED
	ANEMOMETER_SENSOR	= 0x00000010,	// UNUSED
	RAIN_SENSOR			= 0x00000020,	// UNUSED
	GPS_SENSOR			= 0x00000040,	// UNUSED
	DOME_DEVICE			= 0x00000080,	// UNUSED
	ETHERNET_DEVICE		= 0x00000100,	// UNUSED
	SC16IS750_DEVICE	= 0x00000200,	// UNUSED
	LTE_DEVICE			= 0x00000400,	// FUTURE
	SPL_SENSOR			= 0x00000800,
	RTC_DEVICE			= 0x00001000,
	SDCARD_DEVICE		= 0x00002000,
	LORAWAN_DEVICE		= 0x00004000
};

extern aws_device_t	operator&( aws_device_t, aws_device_t );
extern bool			operator!=( unsigned long, aws_device_t );
extern aws_device_t operator&( unsigned long, aws_device_t );
extern aws_device_t operator~( aws_device_t );
extern aws_device_t operator|( aws_device_t, aws_device_t );
extern aws_device_t operator*( aws_device_t, int );
extern aws_device_t &operator|=( aws_device_t &, aws_device_t );
extern aws_device_t &operator&=( aws_device_t &, aws_device_t );

extern const aws_device_t ALL_SENSORS;

struct health_data_t {

	float			battery_level;
	uint32_t		fs_free_space;
	uint32_t		uptime;
	uint32_t		init_heap_size;
	uint32_t		current_heap_size;
	uint32_t		largest_free_heap_block;

};

struct sqm_data_t {

	float		msas;
	float		nelm;
	uint16_t	gain;
	uint16_t	integration_time;
	uint16_t	ir_luminosity;
	uint16_t	vis_luminosity;
	uint16_t	full_luminosity;

};

struct weather_data_t {

	float	temperature;
	float	pressure;
	float	sl_pressure;
	float	rh;
	float	dew_point;

	float	ambient_temperature;
	float	raw_sky_temperature;
	float	sky_temperature;
	float	cloud_cover;
	uint8_t	cloud_coverage;
};

struct sun_data_t {

	int		lux;
	float	irradiance;
};

struct sensor_data_t {

	time_t			timestamp;
	sun_data_t		sun;
	weather_data_t	weather;
	sqm_data_t		sqm;
	uint8_t			db;
	aws_device_t	available_sensors;

};

struct station_data_t {

	health_data_t	health;				// 6 bytes
	struct timeval	ntp_time;
	int				reset_reason;		// 2 bytes int
	etl::string<64>	firmware_sha56;
};

struct compact_data_t {

	uint8_t			format_version;		// Because of alignment made by the compiler,
										// any change in the data may lead to a different byte arrangement
										// that needs to be taken into account for decoding
	time_t			timestamp;

	int32_t			lux;
	int16_t			irradiance;

	int16_t			temperature;
	int32_t			pressure;
	int16_t			rh;

	int16_t			ambient_temperature;
	int16_t			raw_sky_temperature;
	int16_t			sky_temperature;
	int16_t			cloud_cover;
	uint8_t			cloud_coverage;

	int16_t			msas;
	int16_t			nelm;

	uint8_t			db;

	aws_device_t	available_sensors;

	int16_t			battery_level;
	uint32_t		uptime;
	uint32_t		fs_free_space;

	int				reset_reason;
} __attribute__ ((packed));

void loop( void );
void setup( void );

template <typename T>
int sign( T val )
{
	return static_cast<int>(T(0) < val) - static_cast<int>(val < T(0));
}

#endif // _common_H
