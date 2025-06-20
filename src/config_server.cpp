/*
	config_server.cpp

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

#define DYNAMIC_JSON_DOCUMENT_SIZE  4096	// NOSONAR

#include <Arduino.h>
#include <esp_task_wdt.h>
#include <AsyncTCP.h>
#include <SSLClient.h>
#include <ESPAsyncWebServer.h>
#include <FS.h>
#include <SD.h>
#include <LittleFS.h>

#include "AsyncJson.h"
#include "ArduinoJson.h"
#include "defaults.h"
#include "gpio_config.h"
#include "common.h"
#include "config_manager.h"
#include "config_server.h"
#include "EcoStation.h"

extern HardwareSerial Serial1;	// NOSONAR
extern EcoStation station;
extern SemaphoreHandle_t sensors_read_mutex;	// Issue #7

void AWSWebServer::activate_sensors( AsyncWebServerRequest *request )
{
	station.activate_sensors();
	request->send( 200, "text/plain", "Activated sensors" );
}

void AWSWebServer::attempt_ota_update( AsyncWebServerRequest *request )
{
	station.trigger_ota_update();
	request->send( 200, "text/plain", "Scheduled immediate OTA update" );
	delay( 500 );
}

void AWSWebServer::get_backlog( AsyncWebServerRequest *request )
{
	UNSELECT_SPI_DEVICES();
	
	if ( !SD.begin( GPIO_SD_CS )) {

		etl::string<64> msg;
		Serial.printf( "[WEBSERVER ] [ERROR] Cannot open SDCard to serve [backlog.txt]." );
		snprintf( msg.data(), msg.capacity(), "[ERROR] Cannot open SDCard to serve [backlog.txt]." );
		request->send( 500, "text/html", msg.data() );
		return;

	}
	if ( !SD.exists( "/backlog.txt" )) {

		etl::string<64> msg;
		Serial.printf( "[WEBSERVER ] [ERROR] SDCard file [backlog.txt] not found." );
		snprintf( msg.data(), msg.capacity(), "[ERROR] SDCard file [backlog.txt] not found." );
		request->send( 500, "text/html", msg.data() );
		return;
	}

	request->send( SD, "/backlog.txt" );
	delay(500);
}

void AWSWebServer::get_configuration( AsyncWebServerRequest *request )
{
	if ( station.get_config().get_json_string_config().size() ) {

		request->send( 200, "application/json", station.get_config().get_json_string_config().data() );

	} else

		request->send( 500, "text/plain", "[ERROR] get_configuration() had a problem, please contact support." );
}

void AWSWebServer::get_station_data( AsyncWebServerRequest *request )
{
	if ( !station.is_ready() ) {

		request->send( 503, "text/plain", "Station is not ready yet" );
		return;
	}

	while ( xSemaphoreTake( sensors_read_mutex, 100 /  portTICK_PERIOD_MS ) != pdTRUE ) { esp_task_wdt_reset(); }
		if ( debug_mode )
			Serial.printf( "[WEBSERVER ] [DEBUG] Waiting for sensor data update to complete.\n" );

	request->send( 200, "application/json", station.get_json_sensor_data().data() );
	xSemaphoreGive( sensors_read_mutex );
}

void AWSWebServer::get_root_ca( AsyncWebServerRequest *request )
{
	request->send( 200, "text/plain", station.get_config().get_root_ca().data() );
}

void AWSWebServer::get_uptime( AsyncWebServerRequest *request )
{
	int32_t			uptime	= station.get_uptime();
	int				days	= floor( uptime / ( 3600 * 24 ));
	int				hours	= floor( fmod( uptime, 3600 * 24 ) / 3600 );
	int				minutes	= floor( fmod( uptime, 3600 ) / 60 );
	int				seconds	= fmod( uptime, 60 );
	etl::string<16>	str;

	snprintf( str.data(), str.capacity(), "%03dd:%02dh:%02dm:%02ds", days, hours, minutes, seconds );
	request->send( 200, "text/plain", str.data() );
}

void AWSWebServer::index( AsyncWebServerRequest *request )
{
	if ( !LittleFS.begin()) {

		etl::string<64> msg;
		Serial.printf( "[WEBSERVER ] [ERROR] Cannot open filesystem to serve index.html." );
		snprintf( msg.data(), msg.capacity(), "[ERROR] Cannot open filesystem to serve index.html" );
		request->send( 500, "text/html", msg.data() );
		return;

	}
	request->send( LittleFS, "/ui/index.html" );
}

bool AWSWebServer::initialise( bool _debug_mode )
{
	int port = station.get_config().get_parameter<int>( "config_port" );

	debug_mode = _debug_mode;
	Serial.printf( "[WEBSERVER ] [INFO ] Server on port [%d].\n", port );
	if (( server = new AsyncWebServer(port))) {
			if ( debug_mode )
				Serial.printf( "[WEBSERVER ] [INFO ] AWS Server up.\n" );
	} else {

		Serial.printf( "[WEBSERVER ] [ERROR] Could not start AWS Server.\n" );
		return false;
	}

	initialised = true;
	start();
	return true;
}

void AWSWebServer::join_dr_override( AsyncWebServerRequest *request )
{
#if defined( ARDUINO_LMIC_OVERRIDE_INITIAL_JOIN_DR )
	request->send( 200, "text/plain", "true" );
#else
	request->send( 200, "text/plain", "false" );
#endif
}

void AWSWebServer::send_file( AsyncWebServerRequest *request )
{
	if ( !LittleFS.begin()) {

		etl::string<64> msg;
		Serial.printf( "[WEBSERVER ] [ERROR] Cannot open filesystem to serve [%s].", request->url().c_str() );
		snprintf( msg.data(), msg.capacity(), "[ERROR] Cannot open filesystem to serve [%s].", request->url().c_str() );
		request->send( 500, "text/html", msg.data() );
		return;

	}
	if ( !LittleFS.exists( request->url().c_str() )) {

		etl::string<64> msg;
		Serial.printf( "[WEBSERVER ] [ERROR] File [%s] not found.", request->url().c_str() );
		snprintf( msg.data(), msg.capacity(), "[ERROR] File [%s] not found.", request->url().c_str() );
		request->send( 500, "text/html", msg.data() );
		return;
	}

	request->send( LittleFS, request->url().c_str() );
	delay(500);
}

void AWSWebServer::send_sdcard_file( AsyncWebServerRequest *request )
{
	UNSELECT_SPI_DEVICES();
	
	if ( !SD.begin( GPIO_SD_CS )) {

		etl::string<64> msg;
		Serial.printf( "[WEBSERVER ] [ERROR] Cannot open SDCard to serve [%s].", request->url().c_str() );
		snprintf( msg.data(), msg.capacity(), "[ERROR] Cannot open SDCard to serve [%s].", request->url().c_str() );
		request->send( 500, "text/html", msg.data() );
		return;

	}
	if ( !SD.exists( request->url().c_str() )) {

		etl::string<64> msg;
		Serial.printf( "[WEBSERVER ] [ERROR] SDCard file [%s] not found.", request->url().c_str() );
		snprintf( msg.data(), msg.capacity(), "[ERROR] SDCard file [%s] not found.", request->url().c_str() );
		request->send( 500, "text/html", msg.data() );
		return;
	}

	request->send( SD, request->url().c_str() );
	delay(500);
}

void AWSWebServer::reboot( AsyncWebServerRequest *request )
{
	Serial.printf( "[WEBSERVER ] [INFO ] Rebooting...\n" );
	request->send( 200, "text/plain", "OK\n" );
	delay( 500 );
	ESP.restart();
}

void AWSWebServer::rm_file( AsyncWebServerRequest *request )
{
	if ( !LittleFS.begin()) {

		etl::string<64> msg;
		Serial.printf( "[WEBSERVER ] [ERROR] Cannot open filesystem." );
		snprintf( msg.data(), msg.capacity(), "[ERROR] Cannot open filesystem." );
		request->send( 500, "text/html", msg.data() );
		return;
	}
	if ( request->params() !=1 ) {

		etl::string<64> msg;
		Serial.printf( "[WEBSERVER ] [INFO ] rm_file has wrong count of parameter." );
		snprintf( msg.data(), msg.capacity(), "[ERROR] Wrong count of parameter." );
		request->send( 400, "text/html", msg.data() );
		return;
	}

	if ( LittleFS.remove( request->getParam(0)->value() )) {
		request->send( 200, "text/html", "File deleted." );
		return;
	}
	request->send( 500, "text/html", "File not deleted." );
}

void AWSWebServer::set_configuration( AsyncWebServerRequest *request, JsonVariant &json )
{
	if ( station.get_config().save_runtime_configuration( json )) {

		request->send( 200, "text/plain", "OK\n" );
		station.reboot();

	}
	else
		request->send( 500, "text/plain", "ERROR\n" );
}

void AWSWebServer::start( void )
{
	server->addHandler( new AsyncCallbackJsonWebHandler( "/set_config", std::bind( &AWSWebServer::set_configuration, this, std::placeholders::_1, std::placeholders::_2 )));
	server->on( "/activate_sensors", HTTP_GET, std::bind( &AWSWebServer::activate_sensors, this, std::placeholders::_1 ));
	server->on( "/ui/aws.js", HTTP_GET, std::bind( &AWSWebServer::send_file, this, std::placeholders::_1 ));
	server->on( "/favicon.ico", HTTP_GET, std::bind( &AWSWebServer::send_file, this, std::placeholders::_1 ));
	server->on( "/unsent.txt", HTTP_GET, std::bind( &AWSWebServer::send_sdcard_file, this, std::placeholders::_1 ));
	server->on( "/get_backlog", HTTP_GET, std::bind( &AWSWebServer::get_backlog, this, std::placeholders::_1 ));
	server->on( "/get_config", HTTP_GET, std::bind( &AWSWebServer::get_configuration, this, std::placeholders::_1 ));
	server->on( "/get_station_data", HTTP_GET, std::bind( &AWSWebServer::get_station_data, this, std::placeholders::_1 ));
	server->on( "/get_root_ca", HTTP_GET, std::bind( &AWSWebServer::get_root_ca, this, std::placeholders::_1 ));
	server->on( "/get_uptime", HTTP_GET, std::bind( &AWSWebServer::get_uptime, this, std::placeholders::_1 ));
	server->on( "/join_dr_override", HTTP_GET, std::bind( &AWSWebServer::join_dr_override, this, std::placeholders::_1 ));
	server->on( "/ota_update", HTTP_GET, std::bind( &AWSWebServer::attempt_ota_update, this, std::placeholders::_1 ));
	server->on( "/rm_file", HTTP_GET, std::bind( &AWSWebServer::rm_file, this, std::placeholders::_1 ));
	server->on( "/", HTTP_GET, std::bind( &AWSWebServer::index, this, std::placeholders::_1 ));
	server->on( "/index.html", HTTP_GET, std::bind( &AWSWebServer::index, this, std::placeholders::_1 ));
	server->on( "/reboot", HTTP_GET, std::bind( &AWSWebServer::reboot, this, std::placeholders::_1 ));
	server->onNotFound( std::bind( &AWSWebServer::handle404, this, std::placeholders::_1 ));
	server->begin();
}

void AWSWebServer::stop( void )
{
	server->reset();
	server->end();
}

void AWSWebServer::handle404( AsyncWebServerRequest *request )
{
	request->send( 404, "text/plain", "Not found\n" );
}
