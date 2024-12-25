/*
  	EcoStation.h

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
#ifndef _EcoStation_H
#define _EcoStation_H

#include "AWSOTA.h"
#include "AWSRTC.h"
#include "config_server.h"
#include "sensor_manager.h"
#include "AWSNetwork.h"

const byte LOW_BATTERY_COUNT_MIN = 5;
const byte LOW_BATTERY_COUNT_MAX = 10;

const unsigned short 	BAT_V_MAX		= 4200;		// in mV
const unsigned short	BAT_V_MIN		= 3000;		// in mV
const byte 				BAT_LEVEL_MIN	= 33;		// in %, corresponds to ~3.4V for a typical Li-ion battery
const unsigned short	VCC				= 3300;		// in mV
const unsigned int		V_DIV_R1		= 82000;	// voltage divider R1 in ohms
const unsigned int		V_DIV_R2		= 300000;	// voltage divider R2 in ohms
const unsigned short	ADC_MAX			= 4096;		// 12 bits resolution
const float				V_MAX_IN		= ( BAT_V_MAX*V_DIV_R2 )/( V_DIV_R1+V_DIV_R2 );	// in mV
const float				V_MIN_IN		= ( BAT_V_MIN*V_DIV_R2 )/( V_DIV_R1+V_DIV_R2 );	// in mV
const unsigned short	ADC_V_MAX		= ( V_MAX_IN*ADC_MAX / VCC );
const unsigned short	ADC_V_MIN		= ( V_MIN_IN*ADC_MAX / VCC );

#define UNSELECT_SPI_DEVICES()	\
	do {\
		digitalWrite( GPIO_SD_CS, HIGH ); \
		digitalWrite( GPIO_LORA_CS, HIGH ); \
	} while(0)

enum struct aws_ip_info : uint8_t
{
	ETH_DNS,
	ETH_GW,
	ETH_IP,
	WIFI_STA_DNS,
	WIFI_STA_GW,
	WIFI_STA_IP
};
using aws_ip_info_t = aws_ip_info;

enum struct boot_mode : uint8_t
{
	NORMAL,
	MAINTENANCE,
	FACTORY_RESET	
};
using aws_boot_mode_t = boot_mode;

struct ota_setup_t {

	etl::string<24>	board;
	etl::string<32>	config;
	etl::string<18>	device;
	etl::string<26>	version;
	ota_status_t	status_code		= ota_status_t::UNKNOWN;
	int32_t			status_ts		= 0;
	time_t			last_update_ts	= 0;
};

void OTA_callback( int, int );

class EcoStation {

	private:

		TaskHandle_t				aws_periodic_task_handle;
		AWSRTC						aws_rtc;
		
		aws_boot_mode_t				boot_mode					= aws_boot_mode_t::NORMAL;
		compact_data_t				compact_data;
		AWSConfig					config;
		bool						debug_mode					= false;
		bool						force_ota_update			= false;
		etl::string<1096>			json_sensor_data;
		size_t						json_sensor_data_len;
		etl::string<128>			location;
		AWSNetwork					network;
		bool						ntp_synced					= false;
		AWSOTA						ota;
		ota_setup_t					ota_setup;
		bool						ready						= false;
		AWSSensorManager 			sensor_manager;
		AWSWebServer 				server;
		bool						solar_panel;
		station_data_t				station_data;

		void 			determine_boot_mode( void );
		void			display_banner( void );
		bool			enter_maintenance_mode( void );
		void			factory_reset( void );
		bool			fixup_timestamp( void );
		template<typename... Args>
		etl::string<96>	format_helper( const char *, Args... );
		void			periodic_tasks( void * );
		bool			post_content( const char *, const char * );
		template<typename... Args>
		void			print_config_string( const char *, Args... );
		void			print_runtime_config( void );
		void			read_battery_level( void );
		int				reformat_ca_root_line( std::array<char,116> &, int, int, int, const char * );
		bool			store_unsent_data( etl::string_view );

	public:

							EcoStation( void );
		bool				activate_sensors( void );
		void				check_ota_updates( bool );
		AWSConfig			&get_config( void );
		sensor_data_t		*get_sensor_data( void );
		etl::string_view	get_json_sensor_data( void );
		uint32_t			get_uptime( void );
		bool				initialise( void );
		void				initialise_sensors( void );
		bool				is_ready( void );
		bool				on_solar_panel();
		bool				poll_sensors( void );
		void				prepare_for_deep_sleep( int );
		void				reboot( void );
		void				read_sensors( void );
		void				report_unavailable_sensors( void );
		void				send_alarm( const char *, const char * );
		void				send_data( void );
		bool				sync_time( bool );
		void				trigger_ota_update( void );
};

#endif
