/*
  	config_manager.cpp

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

#include <ESPAsyncWebServer.h>
#include <FS.h>
#include <LittleFS.h>

#include "defaults.h"
#include "common.h"
#include "AT24C.h"
#include "config_manager.h"
#include "EcoStation.h"

extern HardwareSerial Serial1;	// NOSONAR
extern EcoStation station;

RTC_DATA_ATTR char	_can_rollback	= 0;	// NOSONAR

template <size_t N>
etl::string<N * 2> AWSConfig::bytes_to_hex_string( const uint8_t* bytes, size_t length, bool reverse ) const
{
	etl::string<N * 2> hex_str;

	if ( reverse )

		for ( int i = length - 1; i >= 0; i-- ) {

			uint8_t byte = bytes[ i ];
			hex_str.push_back( nibble_to_hex_char( ( byte >> 4 ) & 0x0F ));
			hex_str.push_back( nibble_to_hex_char( byte & 0x0F ));
		}

	  else

		for ( int i = 0; i < length; ++i ) {

			uint8_t byte = bytes[ i ];
			hex_str.push_back( nibble_to_hex_char( ( byte >> 4 ) & 0x0F ));
			hex_str.push_back( nibble_to_hex_char( byte & 0x0F ));
		}

	return hex_str;
}

bool AWSConfig::can_rollback( void )
{
	return _can_rollback;
}

uint8_t AWSConfig::char2int( char c )
{
	if (( c >= '0' ) && ( c <= '9' ))
		return ( c - '0' );

	if (( c >= 'A' ) && ( c <= 'F' ))
		return ( c - 'A' + 10 );

	return 0;
}

void AWSConfig::factory_reset( etl::string<64> &firmware_sha256 )
{
	if ( !read_eeprom_and_nvs_config( firmware_sha256 ) ) {

		Preferences nvs;
		nvs.begin( "aws", false );
		nvs.putString( "product", "ES" );
		nvs.putString( "product_version", "1.0" );
		nvs.putChar( "pwr_mode", 0 );
		nvs.putChar( "has_ethernet", 0 );
		nvs.putChar( "has_rtc", 1 );
		nvs.putChar( "has_sdcard", 1 );
		nvs.putChar( "has_lte", 0 );
		nvs.putChar( "has_lorawan", 1 );
		nvs.end();
	}
	
	if ( !LittleFS.begin( true )) {

		Serial.printf( "[CONFIGMNGR] [ERROR] Could not access flash filesystem, bailing out!\n" );
		return;
	}

	File x = LittleFS.open( "/aws.conf", "w" );
	if ( x ) {

		x.print( "{}" );
		x.close();
		if ( debug_mode )
			Serial.printf( "[CONFIGMNGR] [ERROR] Config file %ssuccessfully truncated.\n", x ? "" : "un" );
	}

	LittleFS.end();
}

uint32_t AWSConfig::get_fs_free_space( void )
{
	return fs_free_space;
}

etl::string_view AWSConfig::get_pcb_version( void )
{
	return etl::string_view( pcb_version );
}

etl::string_view AWSConfig::get_product( void )
{
	return etl::string_view( product );
}

etl::string_view AWSConfig::get_product_version( void )
{
	return etl::string_view( product_version );
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

std::array<uint8_t, 16> AWSConfig::get_lora_appkey( void )
{
	return lora_appkey;
}

etl::string_view AWSConfig::get_lora_appkey_str( void )
{
	auto hex_string	= bytes_to_hex_string<16>( lora_appkey.data(), 16, false );
	return hex_string;
}

std::array<uint8_t, 8> AWSConfig::get_lora_deveui( void )
{
	return lora_eui;
}

etl::string_view AWSConfig::get_lora_deveui_str( void )
{
	auto hex_string	= bytes_to_hex_string<8>( lora_eui.data(), 8, true );
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

	while ( file ) {

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

char AWSConfig::nibble_to_hex_char(uint8_t nibble) const
{
	return (nibble < 10) ? ('0' + nibble) : ('A' + nibble - 10);
}

bool AWSConfig::read_config( etl::string<64> &firmware_sha256 )
{
	read_eeprom_and_nvs_config( firmware_sha256 );

	read_root_ca();

	if ( !read_file( "/aws.conf"  ) ) {

		if ( !read_file( "/aws.conf.dfl" )) {

			Serial.printf( "\n[CONFIGMNGR] [ERROR] Could not read config file.\n" );
			return false;
		}
		Serial.printf( "[CONFIGMNGR] [INFO ] Using minimal/factory config file.\n" );
	}

	devices |= aws_device_t::BME_SENSOR * ( json_config["has_bme"].is<int>() ? json_config["has_bme"].as<int>() : DEFAULT_HAS_BME );
	devices |= aws_device_t::MLX_SENSOR * ( json_config["has_mlx"].is<int>() ? json_config["has_mlx"].as<int>() : DEFAULT_HAS_MLX );
	devices |= aws_device_t::TSL_SENSOR * ( json_config["has_tsl"].is<int>() ? json_config["has_tsl"].as<int>() : DEFAULT_HAS_TSL );
	devices |= aws_device_t::SPL_SENSOR * ( json_config["has_spl"].is<int>() ? json_config["has_tsl"].as<int>() : DEFAULT_HAS_SPL );

	set_missing_parameters_to_default_values();

	// Add fixed hardware config
	json_config["has_rtc"] = ( ( devices & aws_device_t::RTC_DEVICE ) == aws_device_t::RTC_DEVICE );
	json_config["has_lorawan"] = ( ( devices & aws_device_t::LORAWAN_DEVICE ) == aws_device_t::LORAWAN_DEVICE );
	json_config["has_sdcard"] = ( ( devices & aws_device_t::SDCARD_DEVICE ) == aws_device_t::SDCARD_DEVICE );
	json_config["lorawan_deveui"] = bytes_to_hex_string<8>( lora_eui.data(), 8, true );
	json_config["lorawan_appkey"] = bytes_to_hex_string<16>( lora_appkey.data(), 16, false );

	return true;
}

bool AWSConfig::read_eeprom_and_nvs_config( etl::string<64> &firmware_sha56 )
{
	char			c;
	AT24C			eeprom( AT24C_ADDRESS );
	JsonDocument	eeprom_config;
	bool			lorawan	= false;
	uint32_t		magic;
	Preferences	nvs;
	bool			use_nvs	= true;
	bool			rtc		= false;
	bool			sdcard	= false;

	if ( eeprom.read( 0, &magic ) ) {

		if ( magic == AWSConfig::EEPROM_MAGIC ) {

			if ( debug_mode )
				Serial.printf( "[CONFIGMNGR] [DEBUG] Found valid EEPROM signature.\n" );

			uint16_t			config_size = 0;
			etl::string<256>	serialised_config;
			serialised_config.clear();

			eeprom.read( 4, &config_size );
			if ( config_size <= 256 ) {

				eeprom.read_buffer( 32, reinterpret_cast<uint8_t*>(serialised_config.data()), config_size );
				if ( debug_mode )
					Serial.printf( "[CONFIGMNGR] [DEBUG] EEPROM: %d bytes (%s)\n", config_size, serialised_config.data() );
				serialised_config.repair();
				deserializeJson( eeprom_config, serialised_config.data() );
				use_nvs = false;

			} else {

				Serial.printf( "[CONFIGMNGR] [ERROR] Invalid EEPROM config size, bailing out.\n" );
				return false;
			}

		} else {

			Serial.printf( "[CONFIGMNGR] [ERROR] Cannot find valid EEPROM signature, bailing out.\n" );
			return false;
		}

	} else

		Serial.printf( "[CONFIGMNGR] [INFO ] Cannot find/access EEPROM, trying to use NVS.\n" );

	nvs.begin( "firmware", false );

	if ( !nvs.getString( "sha256", ota_sha256.data(), ota_sha256.capacity() ))
		nvs.putString( "sha256", firmware_sha56.data() );

	nvs.end();

	if ( use_nvs ) {

		Serial.printf( "[CONFIGMNGR] [INFO ] Reading NVS.\n" );

		nvs.begin( "aws", true );
		if ( !nvs.getString( "pcb_version", pcb_version.data(), pcb_version.capacity() )) {

			if ( !nvs.getString( "product", product.data(), product.capacity() )) {

				Serial.printf( "[CONFIGMNGR] [PANIC] Could not get product or PCB version from NVS. Please contact support.\n" );
				nvs.end();
				return false;
			}

			if ( !nvs.getString( "product_version", product_version.data(), product_version.capacity() )) {

				Serial.printf( "[CONFIGMNGR] [PANIC] Could not get product version from NVS. Please contact support.\n" );
				nvs.end();
				return false;
			}

		}

		if ( static_cast<byte>(( pwr_mode = (aws_pwr_src) nvs.getChar( "pwr_mode", 127 ))) == 127 ) {

			Serial.printf( "[CONFIGMNGR] [PANIC] Could not get Power Mode from NVS. Please contact support.\n" );
			nvs.end();
			return false;
		}

		if ( ( c = nvs.getChar( "has_rtc", 127 )) == 127 )
			Serial.printf( "[CONFIGMNGR] [ERROR] Could not get RTC presence from NVS. Please contact support.\n" );
		else
			rtc = ( c == 1 );

		if ( ( c = nvs.getChar( "has_sdcard", 127 )) == 127 )
			Serial.printf( "[CONFIGMNGR] [ERROR] Could not get SDCARD presence from NVS. Please contact support.\n" );
		else
			sdcard = ( c == 1 );

		if ( ( c = nvs.getChar( "has_lorawan", 127 )) == 127 )
			Serial.printf( "[CONFIGMNGR] [ERROR] Could not get LoRaWAN presence from NVS. Please contact support.\n" );
		else
			lorawan = ( c == 1 );

		if ( lorawan ) {

			if ( !nvs.getBytes( "lorawan_deveui", lora_eui.data(), 8 )) {

				Serial.printf( "[CONFIGMNGR] [PANIC] Could not get LoRaWAN DEVEUI from NVS. Please contact support.\n" );
				nvs.end();
				return false;
			}

			if ( !nvs.getBytes( "lorawan_appkey", lora_appkey.data(), 16 )) {

				Serial.printf( "[CONFIGMNGR] [PANIC] Could not get LoRaWAN APPKEY from NVS. Please contact support.\n" );
				nvs.end();
				return false;
			}

		}
		nvs.end();

	} else {

		rtc = ( eeprom_config["has_rtc"] == 1 );
		sdcard = ( eeprom_config["has_sdcard"] == 1 );
		lorawan = ( eeprom_config["has_lorawan"] == 1 );
		pwr_mode = (aws_pwr_src) eeprom_config["pwr_mode"].as<uint8_t>();
		memcpy( product.data(), eeprom_config["product"].as<String>().c_str(), eeprom_config["product"].as<String>().length() );
		product.repair();
		memcpy( product_version.data(), eeprom_config["product_version"].as<String>().c_str(), eeprom_config["product_version"].as<String>().length() );
		product_version.repair();
		
		if ( lorawan ) {

			to_hex_array( eeprom_config["lorawan_deveui"].as<String>().length(), eeprom_config["lorawan_deveui"], lora_eui.data(), true );
			to_hex_array( eeprom_config["lorawan_appkey"].as<String>().length(), eeprom_config["lorawan_appkey"], lora_appkey.data(), false );
		}

	}

	devices |= rtc ? aws_device_t::RTC_DEVICE : aws_device_t::NO_SENSOR;
	devices |= sdcard ? aws_device_t::SDCARD_DEVICE : aws_device_t::NO_SENSOR;
	devices |= lorawan ? aws_device_t::LORAWAN_DEVICE : aws_device_t::NO_SENSOR;

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
		if ( DEFAULT_ROOT_CA_LEN > root_ca.capacity() )
			Serial.printf( "[CONFIGMNGR] [BUG  ] Size of default ROOT CA is too big [%d > %d].\n", DEFAULT_ROOT_CA_LEN, root_ca.capacity() );
		else
			root_ca.assign( DEFAULT_ROOT_CA );
		return;
	}

	if (!( s = file.size() )) {

		if ( debug_mode )
			Serial.printf( "[CONFIGMNGR] [DEBUG] Empty ROOT CA file. Using default CA.\n" );
		if ( DEFAULT_ROOT_CA_LEN > root_ca.capacity() )
			Serial.printf( "[CONFIGMNGR] [BUG  ] Size of default ROOT CA is too big [%d > %d].\n", DEFAULT_ROOT_CA_LEN, root_ca.capacity() );
		else
			root_ca.assign( DEFAULT_ROOT_CA );
		return;
	}

	if ( s > root_ca.capacity() ) {

		Serial.printf( "[CONFIGMNGR] [ERROR] ROOT CA file is too big. Using default CA.\n" );
		if ( DEFAULT_ROOT_CA_LEN > root_ca.capacity() )
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

bool AWSConfig::save_current_configuration( void )
{
	JsonVariant v = json_config.as<JsonVariant>();
	return save_runtime_configuration( v );
}

bool AWSConfig::save_runtime_configuration( JsonVariant &_json_config )
{
	size_t	s;

	_json_config.remove( "has_rtc" );
	_json_config.remove( "has_lorawan" );
	_json_config.remove( "has_sdcard" );

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
	if ( !json_config["join_dr"].is<JsonVariant>())
		json_config["join_dr"] = DEFAULT_JOIN_DR;

	if ( !json_config["wifi_ap_ssid"].is<JsonVariant>())
		json_config["wifi_ap_ssid"] = DEFAULT_WIFI_AP_SSID;

	if ( !json_config["config_port"].is<JsonVariant>( ))
		json_config["config_port"] = DEFAULT_CONFIG_PORT;

	if ( !json_config["pref_iface"].is<JsonVariant>( ))
		json_config["pref_iface"] = static_cast<int>( aws_iface::wifi_ap );

	if ( !json_config["remote_server"].is<JsonVariant>( ))
		json_config["remote_server"] = DEFAULT_SERVER;

	if ( !json_config["wifi_sta_ssid"].is<JsonVariant>( ))
		json_config["wifi_sta_ssid"] = DEFAULT_WIFI_STA_SSID;

	if ( !json_config["url_path"].is<JsonVariant>( ))
		json_config["url_path"] = DEFAULT_URL_PATH;

	if ( !json_config["wifi_ap_dns"].is<JsonVariant>( ))
		json_config["wifi_ap_dns"] = DEFAULT_WIFI_AP_DNS;

	if ( !json_config["wifi_ap_gw"].is<JsonVariant>( ))
		json_config["wifi_ap_gw"] = DEFAULT_WIFI_AP_GW;

	if ( !json_config["wifi_ap_ip"].is<JsonVariant>( ))
		json_config["wifi_ap_ip"] = DEFAULT_WIFI_AP_IP;

	if ( !json_config["wifi_ap_password"].is<JsonVariant>( ))
		json_config["wifi_ap_password"] = DEFAULT_WIFI_AP_PASSWORD;

	if ( !json_config["wifi_mode"].is<JsonVariant>( ))
		json_config["wifi_mode"] = static_cast<int>( DEFAULT_WIFI_MODE );

	if ( !json_config["wifi_sta_dns"].is<JsonVariant>( ))
		json_config["wifi_sta_dns"] = DEFAULT_WIFI_STA_DNS;

	if ( !json_config["wifi_sta_gw"].is<JsonVariant>( ))
		json_config["wifi_sta_gw"] = DEFAULT_WIFI_STA_GW;

	if ( !json_config["wifi_sta_ip"].is<JsonVariant>( ))
		json_config["wifi_sta_ip"] = DEFAULT_WIFI_STA_IP;

	if ( !json_config["wifi_sta_ip_mode"].is<JsonVariant>( ))
		json_config["wifi_sta_ip_mode"] = static_cast<int>( DEFAULT_WIFI_STA_IP_MODE );

	if ( !json_config["wifi_sta_password"].is<JsonVariant>( ))
		json_config["wifi_sta_password"] = DEFAULT_WIFI_STA_PASSWORD;
}

void AWSConfig::set_missing_parameters_to_default_values( void )
{
	set_missing_network_parameters_to_default_values();

	if ( !json_config["k1"].is<JsonVariant>( ))
		json_config["k1"] = DEFAULT_K1;

	if ( !json_config["k2"].is<JsonVariant>( ))
		json_config["k2"] = DEFAULT_K3;

	if ( !json_config["k3"].is<JsonVariant>( ))
		json_config["k3"] = DEFAULT_K3;

	if ( !json_config["k4"].is<JsonVariant>( ))
		json_config["k4"] = DEFAULT_K4;

	if ( !json_config["k5"].is<JsonVariant>( ))
		json_config["k5"] = DEFAULT_K5;

	if ( !json_config["k6"].is<JsonVariant>( ))
		json_config["k6"] = DEFAULT_K6;

	if ( !json_config["k7"].is<JsonVariant>( ))
		json_config["k7"] = DEFAULT_K7;

	if ( !json_config["cc_aag_cloudy"].is<JsonVariant>( ))
		json_config["cc_aag_cloudy"] = DEFAULT_CC_AAG_CLOUDY;

	if ( !json_config["cc_aag_overcast"].is<JsonVariant>( ))
		json_config["cc_aag_overcast"] = DEFAULT_CC_AAG_OVERCAST;

	if ( !json_config["cc_aws_cloudy"].is<JsonVariant>( ))
		json_config["cc_aws_cloudy"] = DEFAULT_CC_AWS_CLOUDY;

	if ( !json_config["cc_aws_overcast"].is<JsonVariant>( ))
		json_config["cc_aws_overcast"] = DEFAULT_CC_AWS_OVERCAST;

	if ( !json_config["msas_calibration_offset"].is<JsonVariant>( ))
		json_config["msas_calibration_offset"] = DEFAULT_MSAS_CORRECTION;

	if ( !json_config["tzname"].is<JsonVariant>( ))
		json_config["tzname"] = DEFAULT_TZNAME;

	if ( !json_config["automatic_updates"].is<JsonVariant>( ))
		json_config["automatic_updates"] = DEFAULT_AUTOMATIC_UPDATES;

	if ( !json_config["data_push"].is<JsonVariant>( ))
		json_config["data_push"] = DEFAULT_DATA_PUSH;

	if ( !json_config["push_freq"].is<JsonVariant>( ))
		json_config["push_freq"] = DEFAULT_PUSH_FREQ;

	if ( !json_config["ota_url"].is<JsonVariant>( ))
		json_config["ota_url"] = DEFAULT_OTA_URL;

	if ( !json_config["check_certificate"].is<JsonVariant>( ))
		json_config["check_certificate"] = DEFAULT_CHECK_CERTIFICATE;

	if ( !json_config["sleep_minutes"].is<JsonVariant>( ))
		json_config["sleep_minutes"] = DEFAULT_SLEEP_MINUTES;

	if ( !json_config["spl_duration"].is<JsonVariant>( ))
		json_config["spl_duration"] = DEFAULT_SPL_DURATION;

	if ( !json_config["spl_mode"].is<JsonVariant>( ))
		json_config["spl_mode"] = DEFAULT_SPL_MODE;
}

void AWSConfig::set_root_ca( JsonVariant &_json_config )
{
	if ( _json_config["root_ca"].is<JsonVariant>( )) {

		_json_config[ "root_ca" ][ 4096 - 1 ] = 0;	// Must be updated along DYNAMIC_JSON_DOCUMENT_SIZE
		if ( strlen( _json_config["root_ca"] ) <= root_ca.capacity() ) {

			root_ca.assign( _json_config["root_ca"].as<const char *>() );
			_json_config.remove( "root_ca" );
			return;
		}
	}
	root_ca.empty();
}

void AWSConfig::to_hex_array( size_t len, const char* s, uint8_t *tmp, bool reverse )
{
	int i = reverse ? ( len / 2 ) - 1 : 0;
	int j = reverse ? -1 : 1;

	while ( *s && s[ 1 ] ) {

		tmp[ i ] = char2int( *s ) * 16 + char2int( s[ 1 ] );
		i += j;
		s += 2;
	}
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

	for ( JsonPair item : config_items ) {

		// Cloud coverage & SQM
		switch ( str2int( item.key().c_str() )) {

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
			case str2int( "msas_calibration_offset" ):
				continue;
			default:
				break;
		}

		// General
		switch( str2int( item.key().c_str() )) {

			case str2int( "sleep_minutes" ):
			case str2int( "spl_duration" ):
			case str2int( "spl_mode" ):
				continue;

			default:
				break;
		}

		// Network
		switch ( str2int( item.key().c_str() )) {

			case str2int( "config_port" ):
			case str2int( "join_dr" ):
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
				continue;
			case str2int( "automatic_updates" ):
			case str2int( "check_certificate" ):
			case str2int( "data_push" ):
				proposed_config[ item.key().c_str() ] = 1;
				continue;
				break;

			default:
				break;
		}

		// Sensors
		switch ( str2int( item.key().c_str() )) {

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
