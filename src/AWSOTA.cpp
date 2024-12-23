/*
  	AWSOTA.cpp

	(c) 2024 F.Lesage and inspired from ESP32-OTA-Pull by M.Hart

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

#include <esp_task_wdt.h>
#include <ESPAsyncWebServer.h>
#include <HTTPClient.h>
#include <Update.h>
#include <WiFi.h>

#include "Embedded_Template_Library.h"
#include "etl/string.h"

#include "common.h"
#include "EcoStation.h"
#include "AWSOTA.h"

extern bool	ota_update_ongoing;			// NOSONAR
extern EcoStation	station;

ota_status_t AWSOTA::check_for_update( const char *url, const char *root_ca, etl::string<26> &current_version, ota_action_t action = ota_action_t::CHECK_ONLY )
{
	etl::string<32>	board;
	etl::string<32>	config;
	int				deserialisation_status;
	etl::string<32>	device;
	bool			profile_match = false;
	etl::string<32>	version;

	if ( !download_json( url, root_ca ))
		return status_code;

	if ( !aws_board_id.size() || !aws_config.size() || !aws_device_id.size() )
		return ota_status_t::CONFIG_ERROR;

	for( auto ota_config : json_ota_config["Configurations"].as<JsonArray>() ) {

		if ( ota_config.containsKey( "Board" ))
			board.assign( ota_config["Board"].as<const char *>() );

		if ( ota_config.containsKey( "Device" ))
			device.assign( ota_config["Device"].as<const char *>() );

		if ( ota_config.containsKey( "Config" ))
			config.assign( ota_config["Config"].as<const char *>() );

		if ( ota_config.containsKey( "Version" ))
			version.assign( ota_config["Version"].as<const char *>() );

		if (( !board.size() || ( board == aws_board_id )) &&
			( !device.size() || ( device == aws_device_id )) &&
			( !config.size() || ( config == aws_config )) &&
			( !version.size() || ( version > current_version ))) {

			if ( action == ota_action_t::CHECK_ONLY )
				return ota_status_t::UPDATE_AVAILABLE;
				
			if ( action == ota_action_t::UPDATE_AND_BOOT ) {

				ota_update_ongoing = true;
				save_firmware_sha256( ota_config["SHA256"].as<const char *>() );
			}

			if ( do_ota_update( ota_config["URL"], root_ca, action ))
				return status_code;
					
			profile_match = true;
		}
	}
	return profile_match ? ota_status_t::NO_UPDATE_AVAILABLE : ota_status_t::NO_UPDATE_PROFILE_FOUND;
}

bool AWSOTA::do_ota_update( const char *url, const char *root_ca, ota_action_t action )
{
	HTTPClient	http;
	
	if ( !http.begin( url, root_ca )) {

		status_code = ota_status_t::HTTP_FAILED;
		return false;

	}

	http_status = http.GET();

	if ( http_status != 200 ) {

		http.end();
		return false;
	}

	int total_length = http.getSize();

	if ( !Update.begin( UPDATE_SIZE_UNKNOWN )) {

		http.end();
		status_code = ota_status_t::OTA_UPDATE_FAIL;
		return false;
	}

	std::array<uint8_t,1280>	buffer;

	WiFiClient *stream = http.getStreamPtr();

	int offset = 0;
	while ( http.connected() && offset < total_length ) {

		size_t	bytes_available = stream->available();
		if ( bytes_available > 0 ) {

			size_t bytes_to_read = min( bytes_available, buffer.max_size() );
			size_t bytes_read = stream->readBytes( buffer.data(), bytes_to_read );
			esp_task_wdt_reset();
			size_t bytes_written = Update.write( buffer.data(), bytes_read );
			if ( bytes_read != bytes_written )
				break;
			offset += bytes_written;
			if ( progress_callback != nullptr )
				progress_callback( offset, total_length );
		}
		esp_task_wdt_reset();
	}

	esp_task_wdt_reset();
	http.end();
	esp_task_wdt_reset();

	if ( offset == total_length ) {

		Update.end( true );
		esp_task_wdt_reset();
		delay( 1000 );
		if ( action == ota_action_t::UPDATE_ONLY ) {

			status_code = ota_status_t::UPDATE_OK;
			return true;
		}
		ESP.restart();
	}
	
	status_code = ota_status_t::WRITE_ERROR;
	return false;
}

bool AWSOTA::download_json( const char *url, const char *root_ca )
{
	HTTPClient	http;

	if ( !http.begin( url, root_ca )) {

		status_code = ota_status_t::HTTP_FAILED;
		return false;
	}
	
	if ( ( http_status = http.GET()) == 200 )
		deserialisation_status = deserializeJson( json_ota_config, http.getString() );

	http.end();
	return (( http_status == 200 ) && ( deserialisation_status == DeserializationError::Ok ));
}

void AWSOTA::save_firmware_sha256( const char *sha256 )
{
	Preferences nvs;
	if ( !nvs.begin( "firmware", false )) {

		Serial.printf( "[OTA       ] [ERROR] Could not open firmware NVS.\n" );
		station.send_alarm( "[Station] OTA NVS", "Could not open firmware NVS to save sha256" );
		return;
	}

	if ( nvs.putString( "sha256", sha256 ) ) {

		nvs.end();
		return;
	}

	nvs.end();
	Serial.printf( "[OTA       ] [ERROR] Could not save firmware SHA256 on NVS.\n" );
	station.send_alarm( "[Station] OTA NVS", "Could not save firmware SHA256 on NVS" );
}

void AWSOTA::set_aws_board_id( etl::string<24> &board )
{
	aws_board_id = etl::string_view( board.data() );
}

void AWSOTA::set_progress_callback( std::function<void(int, int)> callback )
{
	progress_callback = callback;
}

void AWSOTA::set_aws_config( etl::string<32> &config )
{
	aws_config = etl::string_view( config.data() );
}

void AWSOTA::set_aws_device_id( etl::string<18> &device )
{
	aws_device_id = etl::string_view( device.data() );
}
