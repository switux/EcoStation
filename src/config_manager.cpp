/*
  	config_manager.cpp

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

#include <ESPAsyncWebServer.h>
#include <FS.h>
#include <LittleFS.h>

#include "defaults.h"
#include "common.h"
#include "config_manager.h"
#include "EcoStation.h"

extern HardwareSerial Serial1;	// NOSONAR


extern EcoStation station;

RTC_DATA_ATTR char _can_rollback = 0;	// NOSONAR

AWSConfig::AWSConfig( void ) : json_config( 3072 )
{
}

template <size_t N>
etl::string<N * 2> AWSConfig::bytes_to_hex_string(const uint8_t* bytes, size_t length) const {
    etl::string<N * 2> hex_str;
    for (size_t i = 0; i < length; ++i) {
		uint8_t byte = bytes[i];
        hex_str.push_back(nibble_to_hex_char((byte >> 4) & 0x0F));
        hex_str.push_back(nibble_to_hex_char(byte & 0x0F));
	}
    return hex_str;
}

bool AWSConfig::can_rollback( void )
{
	return _can_rollback;
}

uint32_t AWSConfig::get_fs_free_space( void )
{
	return fs_free_space;
}

etl::string_view AWSConfig::get_pcb_version( void )
{
	return etl::string_view( pcb_version );
}

aws_pwr_src	AWSConfig::get_pwr_mode( void )
{
	return pwr_mode;
}

etl::string_view AWSConfig::get_root_ca( void )
{
	return etl::string_view( root_ca );
}

bool AWSConfig::get_has_device( aws_device_t dev )
{
	return (( devices & dev ) == dev );
}

etl::string_view AWSConfig::get_json_string_config( void )
{
	static etl::string<5120>	json_string;
	int							i;

	if ( ( i = serializeJson( json_config, json_string.data(), json_string.capacity() )) >= json_string.capacity() ) {

		Serial.printf( "[CONFIGMNGR] [ERROR] Reached configuration string limit (%d > 1024). Please contact support\n", i );
		return etl::string_view( "" );
	}
	return etl::string_view( json_string.data() );
}

uint8_t *AWSConfig::get_lora_appkey( void )
{
	return lora_appkey;
}

etl::string_view AWSConfig::get_lora_appkey_str( void )
{
	auto hex_string	= bytes_to_hex_string<16>( lora_appkey, 16 );
	return hex_string;
}

uint8_t *AWSConfig::get_lora_deveui( void )
{
	return lora_eui;
}

etl::string_view AWSConfig::get_lora_deveui_str( void )
{
	auto hex_string	= bytes_to_hex_string<8>( lora_eui, 8 );
	return hex_string;
}

etl::string_view AWSConfig::get_ota_sha256( void )
{
	return etl::string_view( ota_sha256 );
}

void AWSConfig::list_files( void )
{
	File root = LittleFS.open( "/" );
	File file = root.openNextFile();

	while( file ) {

		Serial.printf( "[CONFIGMNGR] [DEBUG] Filename: %05d /%s\n", file.size(), file.name() );
		file = root.openNextFile();
	}
	close( root );
}

bool AWSConfig::load( etl::string<64> &firmware_sha256, bool _debug_mode  )
{
	debug_mode = _debug_mode;

	if ( !LittleFS.begin( true )) {

		Serial.printf( "[CONFIGMNGR] [ERROR] Could not access flash filesystem, bailing out!\n" );
		return false;
	}

	if ( debug_mode )
		list_files();

	update_fs_free_space();

	return read_config( firmware_sha256 );
}

char AWSConfig::nibble_to_hex_char(uint8_t nibble) const {
    return (nibble < 10) ? ('0' + nibble) : ('A' + nibble - 10);
}

bool AWSConfig::read_config( etl::string<64> &firmware_sha256 )
{
	read_hw_info_from_nvs( firmware_sha256 );

	read_root_ca();

	if ( !read_file( "/aws.conf"  ) ) {

		if ( !read_file( "/aws.conf.dfl" )) {

			Serial.printf( "\n[CONFIGMNGR] [ERROR] Could not read config file.\n" );
			return false;
		}
		Serial.printf( "[CONFIGMNGR] [INFO ] Using minimal/factory config file.\n" );
	}

	devices |= aws_device_t::BME_SENSOR * ( json_config.containsKey( "has_bme" ) ? json_config["has_bme"].as<int>() : DEFAULT_HAS_BME );
	devices |= aws_device_t::MLX_SENSOR * ( json_config.containsKey( "has_mlx" ) ? json_config["has_mlx"].as<int>() : DEFAULT_HAS_MLX );
	devices |= aws_device_t::TSL_SENSOR * ( json_config.containsKey( "has_tsl" ) ? json_config["has_tsl"].as<int>() : DEFAULT_HAS_TSL );
	devices |= aws_device_t::SPL_SENSOR * ( json_config.containsKey( "has_spl" ) ? json_config["has_tsl"].as<int>() : DEFAULT_HAS_SPL );
	
	set_missing_parameters_to_default_values();

	// Add fixed hardware config
	
	json_config["has_rtc"]= ( ( devices & aws_device_t::RTC_DEVICE ) == aws_device_t::RTC_DEVICE );
	json_config["has_lorawan"]= ( ( devices & aws_device_t::LORAWAN_DEVICE ) == aws_device_t::LORAWAN_DEVICE );
	json_config["has_sdcard"]= ( ( devices & aws_device_t::SDCARD_DEVICE ) == aws_device_t::SDCARD_DEVICE );

	return true;
}

void AWSConfig::read_root_ca( void )
{
	File 	file;
	int		s;

	if ( LittleFS.exists( "/root_ca.txt" ) )
		file = LittleFS.open( "/root_ca.txt", FILE_READ );

	if ( !file ) {

		Serial.printf( "[CONFIGMNGR] [ERROR] Cannot read ROOT CA file. Using default CA.\n");
		s = strlen( DEFAULT_ROOT_CA );
		if ( s > root_ca.capacity() )
			Serial.printf( "[CONFIGMNGR] [BUG  ] Size of default ROOT CA is too big [%d > %d].\n", s, root_ca.capacity() );
		else
			root_ca.assign( DEFAULT_ROOT_CA );
		return;
	}

	if (!( s = file.size() )) {

		if ( debug_mode )
			Serial.printf( "[CONFIGMNGR] [DEBUG] Empty ROOT CA file. Using default CA.\n" );
		s = strlen( DEFAULT_ROOT_CA );
		if ( s > root_ca.capacity() )
			Serial.printf( "[CONFIGMNGR] [BUG  ] Size of default ROOT CA is too big [%d > %d].\n", s, root_ca.capacity() );
		else
			root_ca.assign( DEFAULT_ROOT_CA );
		return;
	}

	if ( s > root_ca.capacity() ) {

		Serial.printf( "[CONFIGMNGR] [ERROR] ROOT CA file is too big. Using default CA.\n" );
		s = strlen( DEFAULT_ROOT_CA );
		if ( s > root_ca.capacity() )
			Serial.printf( "[CONFIGMNGR] [BUG  ] Size of default ROOT CA is too big [%d > %d].\n", s, root_ca.capacity() );
		else
			root_ca.assign( DEFAULT_ROOT_CA );
		return;
	}

	file.readBytes( root_ca.data(), s );
	file.close();
	root_ca.uninitialized_resize( s );
}

bool AWSConfig::read_file( const char *filename )
{
	int					s;

	// flawfinder: ignore
	File file = LittleFS.open( filename, FILE_READ );

	if ( !file ) {

		Serial.printf( "[CONFIGMNGR] [ERROR] Cannot read config file [%s]\n", filename );
		return false;
	}

	if (!( s = file.size() )) {

		if ( debug_mode )
			Serial.printf( "[CONFIGMNGR] [DEBUG] Empty config file [%s]\n", filename );
		return false;
	}

	if ( DeserializationError::Ok == deserializeJson( json_config, file )) {

		if ( debug_mode )
			Serial.printf( "[CONFIGMNGR] [DEBUG] Configuration is valid.\n");

		return true;
	}

	Serial.printf( "[CONFIGMNGR] [ERROR] Configuration file has been corrupted.\n");
	return false;
}

bool AWSConfig::read_hw_info_from_nvs( etl::string<64> &firmware_sha56 )
{
	Preferences nvs;
	char		x;

	Serial.printf( "[CONFIGMNGR] [INFO ] Reading NVS values.\n" );

	nvs.begin( "firmware", false );
		
	if ( !nvs.getString( "sha256", ota_sha256.data(), ota_sha256.capacity() ))
		nvs.putString( "sha256", firmware_sha56.data() );

	nvs.end();

	nvs.begin( "aws", true );
	if ( !nvs.getString( "pcb_version", pcb_version.data(), pcb_version.capacity() )) {

		Serial.printf( "[CONFIGMNGR] [PANIC] Could not get PCB version from NVS. Please contact support.\n" );
		nvs.end();
		return false;
	}

	if ( static_cast<byte>(( pwr_mode = (aws_pwr_src) nvs.getChar( "pwr_mode", 127 ))) == 127 ) {

		Serial.printf( "[CONFIGMNGR] [PANIC] Could not get Power Mode from NVS. Please contact support.\n" );
		nvs.end();
		return false;
	}

	if ( ( x = nvs.getChar( "has_rtc", 127 )) == 127 ) {

		Serial.printf( "[CONFIGMNGR] [PANIC] Could not get RTC presence from NVS. Please contact support.\n" );

	}
	devices |= ( x == 0 ) ? aws_device_t::NO_SENSOR : aws_device_t::RTC_DEVICE;

	if ( ( x = nvs.getChar( "has_sdcard", 127 )) == 127 ) {

		Serial.printf( "[CONFIGMNGR] [PANIC] Could not get SDCARD presence from NVS. Please contact support.\n" );

	}
	devices |= ( x == 0 ) ? aws_device_t::NO_SENSOR : aws_device_t::SDCARD_DEVICE;

	if ( ( x = nvs.getChar( "has_lorawan", 127 )) == 127 ) {

		Serial.printf( "[CONFIGMNGR] [PANIC] Could not get LoRaWAN presence from NVS. Please contact support.\n" );
	}
	devices |= ( x == 0 ) ? aws_device_t::NO_SENSOR : aws_device_t::LORAWAN_DEVICE;

	if ( x ) {

		if ( !nvs.getBytes( "lorawan_deveui", lora_eui, 8 )) {

			Serial.printf( "[CONFIGMNGR] [PANIC] Could not get LoRaWAN DEVEUI from NVS. Please contact support.\n" );
			nvs.end();
			return false;
		}

		if ( !nvs.getBytes( "lorawan_appkey", lora_appkey, 16 )) {

			Serial.printf( "[CONFIGMNGR] [PANIC] Could not get LoRaWAN APPKEY from NVS. Please contact support.\n" );
			nvs.end();
			return false;
		}

	}
	nvs.end();
	return true;
}

bool AWSConfig::rollback()
{
	if ( !_can_rollback ) {

		if ( debug_mode )
			Serial.printf( "[CONFIGMNGR] [DEBUG] No configuration to rollback.\n");
		return true;

	}

	Serial.printf( "[CONFIGMNGR] [INFO ] Rolling back last submitted configuration.\n" );

	if ( !LittleFS.begin( true )) {

		Serial.printf( "[CONFMGR] [ERROR] Could not open filesystem.\n" );
		return false;
	}

	LittleFS.remove( "/aws.conf" );
	LittleFS.rename( "/aws.conf.bak", "/aws.conf" );
	Serial.printf( "[CONFIGMNGR] [INFO ] Rollback successful.\n" );
	_can_rollback = 0;
	return true;
}

bool AWSConfig::save_runtime_configuration( JsonVariant &_json_config )
{
	size_t	s;

	if ( !verify_entries( _json_config ))
		return false;

	if ( debug_mode )
		list_files();

	set_root_ca( _json_config );

	Serial.printf( "[CONFIGMNGR] [INFO ] Saving submitted configuration.\n" );

	if ( !LittleFS.begin( true )) {

		Serial.printf( "[CONFIGMNGR] [ERROR] Could not open filesystem.\n" );
		return false;
	}

	update_fs_free_space();

	LittleFS.remove( "/aws.conf.bak.try" );
	LittleFS.rename( "/aws.conf", "/aws.conf.bak.try" );
	LittleFS.remove( "/aws.conf.try" );

	// flawfinder: ignore
	File file = LittleFS.open( "/aws.conf.try", FILE_WRITE );
	if ( !file ) {

		Serial.printf( "[CONFIGMNGR] [ERROR] Cannot write configuration file, rolling back.\n" );
		return false;
	}

	s = serializeJson( _json_config, file );
	file.close();
	if ( !s ) {

		LittleFS.remove( "/aws.conf" );
		LittleFS.remove( "/aws.conf.try" );
		LittleFS.rename( "/aws.conf.bak.try", "/aws.conf" );
		Serial.printf( "[CONFIGMNGR] [ERROR] Empty configuration file, rolling back.\n" );
		return false;

	}
	if ( s > MAX_CONFIG_FILE_SIZE ) {

		LittleFS.remove( "/aws.conf.try" );
		LittleFS.rename( "/aws.conf.bak.try", "/aws.conf" );
		Serial.printf( "[CONFIGMNGR] [ERROR] Configuration file is too big [%d bytes].\n" );
		station.send_alarm( "Configuration error", "Configuration file is too big. Not applying changes!" );
		return false;

	}

	LittleFS.remove( "/aws.conf" );

	if ( debug_mode )
		list_files();

	LittleFS.rename( "/aws.conf.try", "/aws.conf" );
	LittleFS.rename( "/aws.conf.bak.try", "/aws.conf.bak" );
	Serial.printf( "[CONFIGMNGR] [INFO ] Wrote %d bytes, configuration save successful.\n", s );

	LittleFS.remove( "/root_ca.txt.try" );
	if ( LittleFS.exists( "/root_ca.txt" ))
		LittleFS.rename( "/root_ca.txt", "/root_ca.txt.try" );

	file = LittleFS.open( "/root_ca.txt.try", FILE_WRITE );
	s = file.print( root_ca.data() );
	file.close();
	Serial.printf( "[CONFIGMNGR] [INFO ] Wrote %d bytes of ROOT CA.\n", s );
	LittleFS.rename( "/root_ca.txt.try", "/root_ca.txt" );

	_can_rollback = 1;
	if ( debug_mode )
		list_files();

	return true;
}

void AWSConfig::set_missing_network_parameters_to_default_values( void )
{
	if ( !json_config.containsKey( "wifi_ap_ssid" ))
		json_config["wifi_ap_ssid"] = DEFAULT_WIFI_AP_SSID;

	if ( !json_config.containsKey( "config_port" ))
		json_config["config_port"] = DEFAULT_CONFIG_PORT;

	if ( !json_config.containsKey( "pref_iface" ))
		json_config["pref_iface"] = static_cast<int>( aws_iface::wifi_ap );

	if ( !json_config.containsKey( "remote_server" ))
		json_config["remote_server"] = DEFAULT_SERVER;

	if ( !json_config.containsKey( "wifi_sta_ssid" ))
		json_config["wifi_sta_ssid"] = DEFAULT_WIFI_STA_SSID;

	if ( !json_config.containsKey( "url_path" ))
		json_config["url_path"] = DEFAULT_URL_PATH;

	if ( !json_config.containsKey( "wifi_ap_dns" ))
		json_config["wifi_ap_dns"] = DEFAULT_WIFI_AP_DNS;

	if ( !json_config.containsKey( "wifi_ap_gw" ))
		json_config["wifi_ap_gw"] = DEFAULT_WIFI_AP_GW;

	if ( !json_config.containsKey( "wifi_ap_ip" ))
		json_config["wifi_ap_ip"] = DEFAULT_WIFI_AP_IP;

	if ( !json_config.containsKey( "wifi_ap_password" ))
		json_config["wifi_ap_password"] = DEFAULT_WIFI_AP_PASSWORD;

	if ( !json_config.containsKey( "wifi_mode" ))
		json_config["wifi_mode"] = static_cast<int>( DEFAULT_WIFI_MODE );

	if ( !json_config.containsKey( "wifi_sta_dns" ))
		json_config["wifi_sta_dns"] = DEFAULT_WIFI_STA_DNS;

	if ( !json_config.containsKey( "wifi_sta_gw" ))
		json_config["wifi_sta_gw"] = DEFAULT_WIFI_STA_GW;

	if ( !json_config.containsKey( "wifi_sta_ip" ))
		json_config["wifi_sta_ip"] = DEFAULT_WIFI_STA_IP;

	if ( !json_config.containsKey( "wifi_sta_ip_mode" ))
		json_config["wifi_sta_ip_mode"] = static_cast<int>( DEFAULT_WIFI_STA_IP_MODE );

	if ( !json_config.containsKey( "wifi_sta_password" ))
		json_config["wifi_sta_password"] = DEFAULT_WIFI_STA_PASSWORD;
}

void AWSConfig::set_missing_parameters_to_default_values( void )
{
	set_missing_network_parameters_to_default_values();
	
	if ( !json_config.containsKey( "k1" ))
		json_config["k1"] = DEFAULT_K1;

	if ( !json_config.containsKey( "k2" ))
		json_config["k2"] = DEFAULT_K3;

	if ( !json_config.containsKey( "k3" ))
		json_config["k3"] = DEFAULT_K3;

	if ( !json_config.containsKey( "k4" ))
		json_config["k4"] = DEFAULT_K4;

	if ( !json_config.containsKey( "k5" ))
		json_config["k5"] = DEFAULT_K5;

	if ( !json_config.containsKey( "k6" ))
		json_config["k6"] = DEFAULT_K6;

	if ( !json_config.containsKey( "k7" ))
		json_config["k7"] = DEFAULT_K7;

	if ( !json_config.containsKey( "cc_aag_cloudy" ))
		json_config["cc_aag_cloudy"] = DEFAULT_CC_AAG_CLOUDY;

	if ( !json_config.containsKey( "cc_aag_overcast" ))
		json_config["cc_aag_overcast"] = DEFAULT_CC_AAG_OVERCAST;

	if ( !json_config.containsKey( "cc_aws_cloudy" ))
		json_config["cc_aws_cloudy"] = DEFAULT_CC_AWS_CLOUDY;

	if ( !json_config.containsKey( "cc_aws_overcast" ))
		json_config["cc_aws_overcast"] = DEFAULT_CC_AWS_OVERCAST;
		
	if ( !json_config.containsKey( "msas_calibration_offset" ))
		json_config["msas_calibration_offset"] = DEFAULT_MSAS_CORRECTION;

	if ( !json_config.containsKey( "tzname" ))
		json_config["tzname"] = DEFAULT_TZNAME;

	if ( !json_config.containsKey( "automatic_updates" ))
		json_config["automatic_updates"] = DEFAULT_AUTOMATIC_UPDATES;

	if ( !json_config.containsKey( "data_push" ))
		json_config["data_push"] = DEFAULT_DATA_PUSH;

	if ( !json_config.containsKey( "push_freq" ))
		json_config["push_freq"] = DEFAULT_PUSH_FREQ;

	if ( !json_config.containsKey( "ota_url" ))
		json_config["ota_url"] = DEFAULT_OTA_URL;
}

void AWSConfig::set_root_ca( JsonVariant &_json_config )
{
	if ( _json_config.containsKey( "root_ca" )) {

		if ( strlen( _json_config["root_ca"] ) <= root_ca.capacity() ) {
			root_ca.assign( _json_config["root_ca"].as<const char *>() );
			_json_config.remove( "root_ca" );
			return;
		}
	}
	root_ca.empty();
}

void AWSConfig::update_fs_free_space( void )
{
	fs_free_space = LittleFS.totalBytes() - LittleFS.usedBytes();
}

bool AWSConfig::verify_entries( JsonVariant &proposed_config )
{
	// proposed_config will de facto be modified in this function as
	// config_items is only a way of presenting the items in proposed_config

	JsonObject config_items = proposed_config.as<JsonObject>();
	aws_ip_mode x = aws_ip_mode::dhcp;

	for( JsonPair item : config_items ) {

		switch( str2int( item.key().c_str() )) {

			case str2int( "cc_aag_cloudy" ):
			case str2int( "cc_aag_overcast" ):
			case str2int( "cc_aws_cloudy" ):
			case str2int( "cc_aws_overcast" ):
			case str2int( "cloud_coverage_formula" ):
			case str2int( "k1" ):
			case str2int( "k2" ):
			case str2int( "k3" ):
			case str2int( "k4" ):
			case str2int( "k5" ):
			case str2int( "k6" ):
			case str2int( "k7" ):
			case str2int( "ota_url" ):
			case str2int( "pref_iface" ):
			case str2int( "push_freq" ):
			case str2int( "remote_server" ):
			case str2int( "root_ca" ):
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
				break;
			case str2int( "automatic_updates" ):
			case str2int( "data_push" ):
			case str2int( "has_bme" ):
			case str2int( "has_mlx" ):
			case str2int( "has_spl" ):
			case str2int( "has_tsl" ):
				proposed_config[ item.key().c_str() ] = 1;
				break;
			default:
				Serial.printf( "[CONFIGMNGR] [ERROR] Unknown configuration key [%s]\n",  item.key().c_str() );
				return false;
		}
	}

	return true;
}
