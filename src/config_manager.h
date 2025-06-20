/*
	config_manager.h

	(c) 2023-2025 F.Lesage

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
#ifndef _config_manager_H
#define _config_manager_H

#include <ArduinoJson.h>

#include "device.h"
enum struct aws_iface : int {

	wifi_ap,
	wifi_sta

};

enum struct aws_wifi_mode : int {

	sta,
	ap,
	both

};

enum struct aws_pwr_src : int {

	panel,
	dc12v,
	poe

};

enum struct aws_ip_mode : int {

	dhcp,
	fixed

};

const int				DEFAULT_CONFIG_PORT						= 80;
const uint8_t			DEFAULT_HAS_BME							= 0;
const uint8_t			DEFAULT_HAS_MLX							= 0;
const uint8_t			DEFAULT_HAS_TSL							= 0;
const uint8_t			DEFAULT_HAS_SPL							= 0;
const float				DEFAULT_MSAS_CORRECTION					= -0.55;
const aws_iface			DEFAULT_PREF_IFACE						= aws_iface::wifi_ap;

const int				DEFAULT_K1								= 33;
const int				DEFAULT_K2								= 0;
const int				DEFAULT_K3								= 8;
const int				DEFAULT_K4								= 100;
const int				DEFAULT_K5								= 100;
const int				DEFAULT_K6								= 0;
const int				DEFAULT_K7								= 0;
const int				DEFAULT_CC_AWS_OVERCAST					= -20;
const int				DEFAULT_CC_AWS_CLOUDY					= -25;
const int				DEFAULT_CC_AAG_OVERCAST					= -15;
const int				DEFAULT_CC_AAG_CLOUDY					= -20;

const uint8_t			DEFAULT_SPL_MODE						= 0;
const uint8_t			DEFAULT_SPL_DURATION					= 0;

const aws_wifi_mode		DEFAULT_WIFI_MODE						= aws_wifi_mode::both;
const aws_ip_mode		DEFAULT_WIFI_STA_IP_MODE				= aws_ip_mode::dhcp;
const _dr_eu868_t		DEFAULT_JOIN_DR							= EU868_DR_SF7;
const bool				DEFAULT_DATA_PUSH						= true;
const uint16_t			DEFAULT_PUSH_FREQ						= 300;
const bool				DEFAULT_CHECK_CERTIFICATE				= false;
const char				DEFAULT_OTA_URL[]						= "https://www.datamancers.net/images/AWS.json";

class AWSConfig {

	public:

								AWSConfig( void ) = default;
		bool					can_rollback( void );
		void					factory_reset( etl::string<64> & );
		uint32_t				get_fs_free_space( void );
		template <typename T>
		T 						get_parameter( const char * );
		bool					get_has_device( aws_device_t );
		etl::string_view		get_json_string_config( void );
		std::array<uint8_t,16>	get_lora_appkey( void );
		etl::string_view		get_lora_appkey_str( void );
		std::array<uint8_t,8>	get_lora_deveui( void );
		etl::string_view		get_lora_deveui_str( void );
		etl::string_view		get_ota_sha256( void );
		etl::string_view		get_pcb_version( void );
		etl::string_view		get_product( void );
		etl::string_view		get_product_version( void );
		aws_pwr_src				get_pwr_mode( void );
		etl::string_view		get_root_ca( void );
		bool 					load( etl::string<64> &, bool );
		void					reset_parameter( const char * );
		bool					rollback( void );
		template <typename T>
		void					set_parameter( const char *, T );
		bool					save_current_configuration( void );
		bool					save_runtime_configuration( JsonVariant & );
		bool 					update( JsonVariant &json );

	private:

		const size_t			MAX_CONFIG_FILE_SIZE	= 2048;
		bool					debug_mode				= false;
		aws_device_t			devices					= aws_device_t::NO_SENSOR;
        static const uint32_t	EEPROM_MAGIC			= 0xDEADBEEF;
		uint32_t				fs_free_space			= 0;
		bool					initialised				= false;
		JsonDocument			json_config;
		etl::string<65>			ota_sha256;
		std::array<uint8_t,16>	lora_appkey;
		std::array<uint8_t,8>	lora_eui;
		etl::string<8>			pcb_version;
		etl::string<8>			product;
		etl::string<8>			product_version;
		aws_pwr_src				pwr_mode				= aws_pwr_src::dc12v;
		etl::string<4096>		root_ca;

		template<size_t N>
		etl::string<N*2>	bytes_to_hex_string( const uint8_t *, size_t, bool  ) const;
		uint8_t				char2int( char );
		template <typename T>
		T 					get_aag_parameter( const char * );
		void				list_files( void );
		void				migrate_config_and_ui( void );
		char				nibble_to_hex_char(uint8_t) const;
		bool				read_config( etl::string<64> & );
		bool				read_file( const char * );
		bool				read_eeprom_and_nvs_config( etl::string<64> & );
		void				read_root_ca( void );
		void				set_missing_network_parameters_to_default_values( void );
		void				set_missing_parameters_to_default_values( void );
		void				set_root_ca( JsonVariant & );
		void				to_hex_array( size_t, const char*, uint8_t *, bool );
		void				update_fs_free_space( void );
		bool				verify_entries( JsonVariant & );
};

//
// Credits to: https://stackoverflow.com/a/16388610
//
constexpr unsigned int str2int( const char* str, int h = 0 )
{
	return !str[h] ? 5381 : (str2int(str, h + 1) * 33) ^ str[h];
}

template <typename T>
T AWSConfig::get_aag_parameter( const char *key )
{
	switch ( str2int( key )) {
		case str2int( "k1" ):
		case str2int( "k2" ):
		case str2int( "k3" ):
		case str2int( "k4" ):
		case str2int( "k5" ):
		case str2int( "k6" ):
		case str2int( "k7" ):
		case str2int( "cc_aws_cloudy" ):
		case str2int( "cc_aws_overcast" ):
		case str2int( "cc_aag_cloudy" ):
		case str2int( "cc_aag_overcast" ):
			return json_config[key].as<T>();
		default:
			break;
	}
	Serial.printf( "[CONFIGMNGR] [ERROR]: Unknown parameter [%s]\n", key );
	return 0;
}

template <typename T>
T AWSConfig::get_parameter( const char *key )
{
	if (( *key == 'k' ) || ( !strncmp( key, "cc_", 3  )))
		return get_aag_parameter<T>( key );

	switch ( str2int( key )) {

		case str2int( "cloud_coverage_formula" ):
		case str2int( "config_iface" ):
		case str2int( "config_port" ):
			return ( json_config[key].is<JsonVariant>() ? json_config[key].as<T>() : 0 );	// NOSONAR

		case str2int( "automatic_updates" ):
		case str2int( "check_certificate" ):
		case str2int( "data_push" ):
		case str2int( "join_dr" ):
		case str2int( "msas_calibration_offset" ):
		case str2int( "ota_url" ):
		case str2int( "pref_iface" ):
		case str2int( "push_freq" ):
		case str2int( "remote_server" ):
		case str2int( "sleep_minutes" ):
		case str2int( "spl_duration" ):
		case str2int( "spl_mode" ):
		case str2int( "tzname" ):
		case str2int( "url_path" ):
		case str2int( "wifi_ap_dns" ):
		case str2int( "wifi_ap_gw" ):
		case str2int( "wifi_ap_ip" ):
		case str2int( "wifi_ap_password" ):
		case str2int( "wifi_ap_ssid" ):
		case str2int( "wifi_mode" ):
		case str2int( "wifi_sta_dns" ):
		case str2int( "wifi_sta_gw" ):
		case str2int( "wifi_sta_ip" ):
		case str2int( "wifi_sta_ip_mode" ):
		case str2int( "wifi_sta_password" ):
		case str2int( "wifi_sta_ssid" ):
			return json_config[key].as<T>();
		default:
			break;

	}
	Serial.printf( "[CONFIGMNGR] [ERROR]: Unknown parameter [%s]\n", key );
	return 0;
}

template <typename T>
void AWSConfig::set_parameter( const char *key, T value )
{
	switch( str2int( key )) {

		case str2int( "sleep_minutes" ):
		case str2int( "spl_duration" ):
		case str2int( "spl_mode" ):
			json_config[key] = value;
			Serial.printf( "[CONFIGMNGR] [INFO ] Set %s=%d\n", key, value );
			break;

		default:
			break;
	}
}

#endif
