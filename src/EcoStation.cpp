/*
	EcoStation.cpp

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

#include <rom/rtc.h>
#include <esp_ota_ops.h>
#include <esp_task_wdt.h>
#include <Preferences.h>
#include <ESPAsyncWebServer.h>
#include <time.h>
#include <thread>
#include <stdio.h>
#include <stdarg.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include <LittleFS.h>
#include <charconv>
#include <optional>

#include "Embedded_Template_Library.h"
#include "etl/string.h"
#include "AWSOTA.h"

#include "defaults.h"
#include "gpio_config.h"
#include "common.h"
#include "sensor_manager.h"
#include "config_manager.h"
#include "config_server.h"
#include "AWSNetwork.h"
#include "EcoStation.h"
//#include "manifest.h"

extern SemaphoreHandle_t	sensors_read_mutex;

const std::array<etl::string<10>, 3> PWR_MODE_STR = { "SolarPanel", "12VDC", "PoE" };

const bool				FORMAT_LITTLEFS_IF_FAILED = true;
const unsigned long		FACTORY_RESET_GUARD			= 15000000;	// 15 seconds
const unsigned long		MAINTENANCE_MODE_GUARD		= 5000000;	// 5 seconds

RTC_DATA_ATTR time_t 	boot_timestamp = 0;				// NOSONAR
RTC_DATA_ATTR time_t 	last_ntp_time = 0;				// NOSONAR
RTC_DATA_ATTR uint16_t	ntp_time_misses = 0;			// NOSONAR
RTC_DATA_ATTR uint16_t 	low_battery_event_count = 0;	// NOSONAR
RTC_NOINIT_ATTR bool	ota_update_ongoing = false;		// NOSONAR

EcoStation::EcoStation( void )
{
	station_data.health.init_heap_size = xPortGetFreeHeapSize();
	station_data.health.current_heap_size = station_data.health.init_heap_size;
	station_data.health.largest_free_heap_block = heap_caps_get_largest_free_block( MALLOC_CAP_8BIT );
	location = DEFAULT_LOCATION;
	compact_data.format_version = COMPACT_DATA_FORMAT_VERSION;
}

bool EcoStation::activate_sensors( void )
{
	Serial.printf( "[STATION   ] [INFO ] Activating sensors.\n" );
	return ( ready = sensor_manager.initialise( &config, &compact_data, true ));
}

void EcoStation::check_ota_updates( bool force_update = false )
{
	ota_status_t	ota_retcode;

	Serial.printf( "[STATION   ] [INFO ] Checking for OTA firmware update.\n" );

	ota_update_ongoing = true;
	ota.set_aws_board_id( ota_setup.board );
	ota.set_aws_device_id( ota_setup.device );
	ota.set_aws_config( ota_setup.config );
	ota.set_progress_callback( OTA_callback );
	time( &ota_setup.last_update_ts );

	ota_setup.status_code = ota.check_for_update( config.get_parameter<const char *>( "ota_url" ), config.get_root_ca().data(), ota_setup.version, force_update ? ota_action_t::UPDATE_AND_BOOT : ota_action_t::CHECK_ONLY );
	ota_update_ongoing = false;
}

void EcoStation::determine_boot_mode( void )
{
	unsigned long start					= micros();
	unsigned long button_pressed_secs	= 0;

	pinMode( GPIO_DEBUG, INPUT );

	debug_mode = static_cast<bool>( 1 - gpio_get_level( GPIO_DEBUG )) || DEBUG_MODE;
	while ( !( gpio_get_level( GPIO_DEBUG ))) {

		if (( micros() - start ) >= FACTORY_RESET_GUARD	) {
			boot_mode = aws_boot_mode_t::FACTORY_RESET;
			break;
		}

		if (( micros() - start ) >= MAINTENANCE_MODE_GUARD )
			boot_mode = aws_boot_mode_t::MAINTENANCE;

		delay( 100 );
		button_pressed_secs = micros() - start;
	}
}

void EcoStation::display_banner()
{
	if ( !debug_mode )
		return;

	uint8_t	*wifi_mac	= network.get_wifi_mac();
	int		i;

	Serial.printf( "\n[STATION   ] [INFO ] #############################################################################################\n" );
	Serial.printf( "[STATION   ] [INFO ] # EcoStation                                                                                #\n" );
	Serial.printf( "[STATION   ] [INFO ] #  (c) lesage@loads.ch                                                                      #\n" );
	Serial.printf( "[STATION   ] [INFO ] #-------------------------------------------------------------------------------------------#\n" );
	Serial.printf( "[STATION   ] [INFO ] # HARDWARE SETUP                                                                            #\n" );
	Serial.printf( "[STATION   ] [INFO ] #-------------------------------------------------------------------------------------------#\n" );

	print_config_string( "# Board              : %s", ota_setup.board.data() );
	print_config_string( "# Model              : %s", ota_setup.config.data() );
	print_config_string( "# WIFI Mac           : %s", ota_setup.device.data() );
	print_config_string( "# LoRaWAN            : %s", config.get_has_device( aws_device_t::LORAWAN_DEVICE ) ? "Yes" : "No" );
	if ( config.get_has_device( aws_device_t::LORAWAN_DEVICE ) )
  		print_config_string( "# LoRaWAN DEVEUI     : %s", config.get_lora_deveui_str().data() );
	print_config_string( "# Firmware           : %s", ota_setup.version.data() );

	Serial.printf( "[STATION   ] [INFO ] #-------------------------------------------------------------------------------------------#\n" );
	Serial.printf( "[STATION   ] [INFO ] # GPIO PIN CONFIGURATION                                                                    #\n" );
	Serial.printf( "[STATION   ] [INFO ] #-------------------------------------------------------------------------------------------#\n" );

	if ( solar_panel ) {

		print_config_string( "# 3.3V SWITCH   : %d", GPIO_ENABLE_3_3V );
		print_config_string( "# BAT LVL       : SW=%d ADC=%d", GPIO_BAT_ADC_EN, GPIO_BAT_ADC );
		print_config_string( "# PANEL VOLTAGE : ADC=%d", GPIO_PANEL_ADC );

	}
	print_config_string( "# DEBUG/CONFIG  : %d", GPIO_DEBUG );

	Serial.printf( "[STATION   ] [INFO ] #-------------------------------------------------------------------------------------------#\n" );
	Serial.printf( "[STATION   ] [INFO ] # RUNTIME CONFIGURATION                                                                     #\n" );
	Serial.printf( "[STATION   ] [INFO ] #-------------------------------------------------------------------------------------------#\n" );

	print_runtime_config();

//	Serial.printf( "[STATION   ] [INFO ] #-------------------------------------------------------------------------------------------#\n" );
//	Serial.printf( "[STATION   ] [INFO ] # LIBRARIES                                                                                 #\n" );
//	Serial.printf( "[STATION   ] [INFO ] #-------------------------------------------------------------------------------------------#\n" );

//	for( i = 0; i < NBLIB; i++ )
//		print_config_string( "# %35s : %s", libraries[i].data(), libversions[i].data() );

	Serial.printf( "[STATION   ] [INFO ] #############################################################################################\n" );
}

bool EcoStation::enter_maintenance_mode( void )
{
	if ( debug_mode )
		Serial.printf( "[STATION   ] [INFO ] Trying to enter maintenance mode...\n" );

	if ( !network.is_wifi_connected() ) {

		if ( !network.start_hotspot() ) {

			Serial.printf( "[STATION   ] [ERROR] Failed to start WiFi AP and STA, cannot enter maintenance mode.\n" );
			return false;

		} else

			Serial.printf( "[STATION   ] [INFO ] Failed to start WiFi STA, fallen back to AP.\n" );

	}

	if ( !server.initialise( debug_mode )) {

		Serial.printf( "[STATION   ] [PANIC ] Failed to start WiFi AP, cannot enter maintenance mode.\n" );
		return false;

	} else

		while ( true );
}

void EcoStation::factory_reset( void )
{
	Serial.printf( "[STATION   ] [INFO ] Performing factory reset.\n ");
	config.factory_reset();
	reboot();
}

bool EcoStation::fixup_timestamp( void )
{
	if ( !config.get_has_device( aws_device_t::RTC_DEVICE )) {

		Serial.printf( "[STATION   ] [INFO ] RTC not present.\n");
		return false;
	}
	if ( !aws_rtc.begin() ) {

		Serial.printf( "[STATION   ] [ERROR] RTC not found.\n");
		return false;
	}
	struct tm timeinfo;
	aws_rtc.get_datetime( &timeinfo );
	time_t now = mktime( &timeinfo );
	struct timeval now2 = { .tv_sec = now };
	settimeofday( &now2, NULL );
	etl::string<50> d;
	strftime( d.data(), d.capacity(), "%Y-%m-%d %H:%M:%S", &timeinfo );
	d.repair();
	Serial.printf( "[STATION   ] [INFO ] RTC time: %s\n", d.data() );
	return true;
}

template<typename... Args>
etl::string<96> EcoStation::format_helper( const char *fmt, Args... args )
{
	char buf[96];	// NOSONAR
	snprintf( buf, 95, fmt, args... );
	return etl::string<96>( buf );
}

AWSConfig &EcoStation::get_config( void )
{
	return config;
}

etl::string_view EcoStation::get_json_sensor_data( void )
{
	JsonDocument		json_data;
	sensor_data_t		*sensor_data = sensor_manager.get_sensor_data();
	int					l;

	json_data["available_sensors"] = static_cast<unsigned long>( sensor_data->available_sensors );
	json_data["battery_level"] = station_data.health.battery_level;
	json_data["timestamp"] = sensor_data->timestamp;
	json_data["temperature"] = sensor_data->weather.temperature;
	json_data["pressure"] = sensor_data->weather.pressure;
	json_data["sl_pressure"] = sensor_data->weather.sl_pressure;
	json_data["rh"] = sensor_data->weather.rh;
	json_data["db"] = sensor_data->db;
	json_data["ota_board"] = ota_setup.board.data();
	json_data["ota_device"] = ota_setup.device.data();
	json_data["ota_config"] = ota_setup.config.data();
	json_data["build_id" ] = ota_setup.version.data();
	json_data["dew_point"] = sensor_data->weather.dew_point;
	json_data["raw_sky_temperature"] = sensor_data->weather.raw_sky_temperature;
	json_data["sky_temperature"] = sensor_data->weather.sky_temperature;
	json_data["ambient_temperature"] = sensor_data->weather.ambient_temperature;
	json_data["cloud_coverage"] = sensor_data->weather.cloud_coverage;
	json_data["msas"] = sensor_data->sqm.msas;
	json_data["nelm"] = sensor_data->sqm.nelm;
	json_data["integration_time"] = sensor_data->sqm.integration_time;
	json_data["gain"] = sensor_data->sqm.gain;
	json_data["ir_luminosity"] = sensor_data->sqm.ir_luminosity;
	json_data["full_luminosity"] = sensor_data->sqm.full_luminosity;
	json_data["lux"] = sensor_data->sun.lux;
	json_data["irradiance"] = sensor_data->sun.irradiance;
	json_data["ntp_time_sec"] = station_data.ntp_time.tv_sec;
	json_data["ntp_time_usec"] = station_data.ntp_time.tv_usec;
	json_data["uptime"] = get_uptime();
	json_data["init_heap_size"] = station_data.health.init_heap_size;
	json_data["current_heap_size"] = station_data.health.current_heap_size;
	json_data["largest_free_heap_block" ] = station_data.health.largest_free_heap_block;
	json_data["ota_code" ] = static_cast<int>( ota_setup.status_code );
	json_data["ota_status_ts" ] = ota_setup.status_ts;
	json_data["ota_last_update_ts" ] = ota_setup.last_update_ts;
	json_data["reset_reason"] = station_data.reset_reason;

	if ( ( l = measureJson( json_data )) > json_sensor_data.capacity() ) {

		etl::string<64> tmp;
		snprintf( tmp.data(), tmp.capacity(), "sensor_data json is too small ( %d > %d )", l, json_sensor_data.capacity() );
		send_alarm( "[STATION] BUG", tmp.data() );
		Serial.printf( "[STATION   ] [BUG  ] sensor_data json is too small ( %d > %d ). Please report to support!\n", l, json_sensor_data.capacity() );
		return etl::string_view( "" );

	}
	json_sensor_data_len = serializeJson( json_data, json_sensor_data.data(), json_sensor_data.capacity() );
	if ( debug_mode )
		Serial.printf( "[STATION   ] [DEBUG] sensor_data is %d bytes long, max size is %d bytes.\n", json_sensor_data_len, json_sensor_data.capacity() );

	return etl::string_view( json_sensor_data );
}

sensor_data_t *EcoStation::get_sensor_data( void )
{
	return sensor_manager.get_sensor_data();
}

uint32_t EcoStation::get_uptime( void )
{
	if ( !on_solar_panel() )

		station_data.health.uptime = round( esp_timer_get_time() / 1000000 );

	else {

  		time_t now;
  		time( &now );
  		if ( !boot_timestamp ) {

  			station_data.health.uptime = round( esp_timer_get_time() / 1000000 );
  			boot_timestamp = now - station_data.health.uptime;

  		} else

  			station_data.health.uptime = now - boot_timestamp;

  		compact_data.uptime = station_data.health.uptime;
  	}
	return station_data.health.uptime;
}

bool EcoStation::initialise( void )
{
	std::array<uint8_t, 6>	mac;
	byte					offset = 0;
	std::array<uint8_t, 32>	sha_256;

	determine_boot_mode();

	esp_partition_get_sha256( esp_ota_get_running_partition(), sha_256.data() );
	for ( uint8_t _byte : sha_256 ) {

		etl::string<3> h;
		snprintf( h.data(), 3, "%02x", _byte );
		station_data.firmware_sha56 += h.data();
	}

	Serial.printf( "\n\n[STATION   ] [INFO ] Firmware checksum = [%s]\n", station_data.firmware_sha56.data() );
	Serial.printf( "[STATION   ] [INFO ] EcoStation [REV %s, BUILD %s, BASE %s] is booting...\n", REV.data(), BUILD_ID, GITHASH );

	station_data.reset_reason = esp_reset_reason();
	compact_data.reset_reason = station_data.reset_reason;

	if ( solar_panel ) {

		pinMode( GPIO_ENABLE_3_3V, OUTPUT );
		digitalWrite( GPIO_ENABLE_3_3V, HIGH );
	}

	if ( !config.load( station_data.firmware_sha56, debug_mode ) )
			return false;

	if ( solar_panel )
		digitalWrite( GPIO_ENABLE_3_3V, LOW );

	station_data.health.fs_free_space = config.get_fs_free_space();
	compact_data.fs_free_space = station_data.health.fs_free_space;
	Serial.printf( "[STATION   ] [INFO ] Free space on config partition: %d bytes\n", station_data.health.fs_free_space );

	solar_panel = ( static_cast<aws_pwr_src>( config.get_pwr_mode()) == aws_pwr_src::panel );
	sensor_manager.set_solar_panel( solar_panel );
	sensor_manager.set_debug_mode( debug_mode );

	if ( ( esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_UNDEFINED ) && solar_panel )
		boot_timestamp = 0;

	ota_setup.board = "ECO_";
	ota_setup.board += ESP.getChipModel();

	esp_read_mac( mac.data(), ESP_MAC_WIFI_STA );
	snprintf( ota_setup.device.data(), ota_setup.device.capacity(), "%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5] );

	ota_setup.config = PWR_MODE_STR[ static_cast<int>( config.get_pwr_mode()) ];
	ota_setup.config += "_";
	ota_setup.config += config.get_pcb_version().data();

	ota_setup.version = REV;
	ota_setup.version += ".";
	ota_setup.version += BUILD_ID;

	if ( boot_mode == aws_boot_mode_t::FACTORY_RESET )
		factory_reset();

	read_battery_level();

	network.initialise( &config, debug_mode );

	if ( solar_panel ) {

		// Issue #143
		esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

		pinMode( GPIO_ENABLE_3_3V, OUTPUT );
		digitalWrite( GPIO_ENABLE_3_3V, HIGH );

		fixup_timestamp();

		if ( network.is_wifi_connected() || ( boot_mode == aws_boot_mode_t::MAINTENANCE ))
			enter_maintenance_mode();
		else
			WiFi.mode ( WIFI_OFF );

	} else {

		server.initialise( debug_mode );
		sync_time( true );
		fixup_timestamp();
	}
	
	if ( ota_update_ongoing ) {

		Serial.printf( "[STATION   ] [INFO ] Expected OTA firmware sha256 is [%s]\n", config.get_ota_sha256().data() );
		ota_update_ongoing = false;
	}

	display_banner();

	if ( !sensor_manager.initialise( &config, &compact_data, false ))
		return false;

	if ( solar_panel )
		return true;

	std::function<void(void *)> _periodic_tasks = std::bind( &EcoStation::periodic_tasks, this, std::placeholders::_1 );
	xTaskCreatePinnedToCore(
		[](void *param) {	// NOSONAR
			std::function<void(void*)>* periodic_tasks_proxy = static_cast<std::function<void(void*)>*>( param );	// NOSONAR
			(*periodic_tasks_proxy)( NULL );
		}, "AWSCoreTask", 10000, &_periodic_tasks, 5, &aws_periodic_task_handle, 1 );

	ready = true;
	return true;
}

void EcoStation::initialise_sensors( void )
{
	sensor_manager.initialise_sensors();
}

bool EcoStation::is_ready( void )
{
	return ready;
}

bool EcoStation::on_solar_panel( void )
{
	return solar_panel;
}

void OTA_callback( int offset, int total_length )
{
	static float	percentage = 0.F;
	float			p = ( 100.F * offset / total_length );

	if ( p - percentage > 10.F ) {

		esp_task_wdt_reset();
		Serial.printf("[STATION   ] [INFO ] Updating %d of %d (%02d%%)...\n", offset, total_length, 100 * offset / total_length );
		percentage = p;
	}
	esp_task_wdt_reset();
}

void EcoStation::periodic_tasks( void *dummy )	// NOSONAR
{
	unsigned long	sync_time_millis			= 0;
	unsigned long	sync_time_timer				= 5000;
	unsigned long	ota_millis					= 0;
	unsigned long	ota_timer					= 30 * 60 * 1000;
	unsigned long	data_push_millis			= 0;
	int				data_push_timer				= config.get_parameter<int>( "push_freq" );
	bool			auto_ota_updates			= config.get_parameter<bool>( "automatic_updates" );

	if ( !config.get_parameter<bool>( "data_push" ))
		data_push_timer = 0;

	while ( true ) {

		if (( millis() - sync_time_millis ) > sync_time_timer ) {

			station_data.health.current_heap_size =	xPortGetFreeHeapSize();
			station_data.health.largest_free_heap_block = heap_caps_get_largest_free_block( MALLOC_CAP_8BIT );
			sync_time( false );
			sync_time_millis = millis();
		}

		if ( force_ota_update || ( auto_ota_updates && (( millis() - ota_millis ) > ota_timer ))) {

			force_ota_update = false;
			check_ota_updates( true );
			ota_millis = millis();
		}

		if ( data_push_timer && (( millis() - data_push_millis ) > 1000 * data_push_timer )) {

			send_data();
			data_push_millis = millis();
		}

		delay( 500 );
	}
}

bool EcoStation::poll_sensors( void )
{
	return sensor_manager.poll_sensors();
}

void EcoStation::prepare_for_deep_sleep( int deep_sleep_secs )
{
	network.prepare_for_deep_sleep( deep_sleep_secs );	
}

template void EcoStation::print_config_string<>( const char *fmt );

template<typename... Args>
void EcoStation::print_config_string( const char *fmt, Args... args )
{
	etl::string<96>	string;
	byte 			i;
	int				l;

	string = format_helper( fmt, args... );

	l = string.size();
	if ( l >= 0 ) {

		string.append( string.capacity() - l - 4, ' ' );
		string.append( "#\n" );
	}
	Serial.printf( "[STATION   ] [INFO ] %s", string.data() );
}

void EcoStation::print_runtime_config( void )
{
	std::array<char, 116>	string;
	const char			*root_ca = config.get_root_ca().data();
	int					ca_pos = 0;
	    
	if ( config.get_has_device( aws_device_t::LORAWAN_DEVICE ) ) {

		print_config_string( "# LoRaWAN APPKEY : %s", config.get_lora_appkey_str().data() );
		if ( network.has_joined() )
			print_config_string( "# LoRaWAN DEVADDR: 0x%lx", LMIC.devaddr );
		else
			print_config_string( "# LoRaWAN DEVADDR: <not joined>" );

	}
	print_config_string( "# AP SSID        : %s", config.get_parameter<const char *>( "wifi_ap_ssid" ));
	print_config_string( "# AP PASSWORD    : %s", config.get_parameter<const char *>( "wifi_ap_password" ));
	print_config_string( "# AP IP          : %s", config.get_parameter<const char *>( "wifi_ap_ip" ));
	print_config_string( "# AP Gateway     : %s", config.get_parameter<const char *>( "wifi_ap_gw" ));
	print_config_string( "# STA SSID       : %s", config.get_parameter<const char *>( "wifi_sta_ssid" ));
	print_config_string( "# STA PASSWORD   : %s", config.get_parameter<const char *>( "wifi_sta_password" ));
	print_config_string( "# STA IP         : %s", config.get_parameter<const char *>( "wifi_sta_ip" ));
	print_config_string( "# STA Gateway    : %s", config.get_parameter<const char *>( "wifi_sta_gw" ));
	print_config_string( "# SERVER         : %s", config.get_parameter<const char *>( "remote_server" ));
	print_config_string( "# URL PATH       : /%s", config.get_parameter<const char *>( "url_path" ));
	print_config_string( "# TZNAME         : %s", config.get_parameter<const char *>( "tzname" ));

	memset( string.data(), 0, string.size() );
	int str_len = snprintf( string.data(), string.size() - 1, "[STATION   ] [INFO ] # ROOT CA        : " );

	int ca_len = config.get_root_ca().size();

	while ( ca_pos < ca_len ) {

		ca_pos = reformat_ca_root_line( string, str_len, ca_pos, ca_len, root_ca );
		Serial.printf( "%s", string.data() );
		memset( string.data(), 0, string.size() );
		str_len = snprintf( string.data(), string.size() - 1, "[STATION   ] [INFO ] # " );
	}

	Serial.printf( "[STATION   ] [INFO ] #-------------------------------------------------------------------------------------------#\n" );
	Serial.printf( "[STATION   ] [INFO ] # SENSORS & CONTROLS                                                                        #\n" );
	Serial.printf( "[STATION   ] [INFO ] #-------------------------------------------------------------------------------------------#\n" );

	print_config_string( "# SQM/IRRADIANCE   : %s", config.get_has_device( aws_device_t::TSL_SENSOR ) ? "Yes" : "No" );
	print_config_string( "# CLOUD SENSOR     : %s", config.get_has_device( aws_device_t::MLX_SENSOR ) ? "Yes" : "No" );
	print_config_string( "# RH/TEMP/PRES.    : %s", config.get_has_device( aws_device_t::BME_SENSOR ) ? "Yes" : "No" );
	print_config_string( "# SPL              : %s", config.get_has_device( aws_device_t::SPL_SENSOR ) ? "Yes" : "No" );
}

void EcoStation::read_battery_level( void )
{
	if ( !solar_panel )
		return;

	int	adc_value = 0;

	WiFi.mode ( WIFI_OFF );

	digitalWrite( GPIO_BAT_ADC_EN, HIGH );

	for ( uint8_t i = 0; i < 5; i++ ) {

		delay( 500 );
		adc_value += analogRead( GPIO_BAT_ADC );
	}

	digitalWrite( GPIO_BAT_ADC_EN, LOW );

	adc_value /= 5;
	station_data.health.battery_level = ( adc_value >= ADC_V_MIN ) ? map( adc_value, ADC_V_MIN, ADC_V_MAX, 0, 100 ) : 0;
	compact_data.battery_level = sensor_manager.float_to_int16_encode( station_data.health.battery_level, 0, 100 );

	if ( debug_mode ) {

		Serial.print( "[STATION   ] [DEBUG] Battery level: " );
		float adc_v_in = adc_value * VCC / ADC_V_MAX;
		float bat_v = adc_v_in * ( V_DIV_R1 + V_DIV_R2 ) / V_DIV_R2;
		Serial.printf( "%03.2f%% (ADC value=%d, ADC voltage=%1.3fV, battery voltage=%1.3fV)\n", station_data.health.battery_level, adc_value, adc_v_in / 1000.F, bat_v / 1000.F );
	}
}

void EcoStation::read_sensors( void )
{
	sensor_manager.read_sensors();

	if ( station_data.health.battery_level <= BAT_LEVEL_MIN ) {

		etl::string<64> string;
		snprintf( string.data(), string.capacity(), "LOW Battery level = %03.2f%%\n", station_data.health.battery_level );

		// Deal with ADC output accuracy, no need to stress about it, a few warnings are enough to get the message through :-)
		if (( low_battery_event_count >= LOW_BATTERY_COUNT_MIN ) && ( low_battery_event_count <= LOW_BATTERY_COUNT_MAX ))
			send_alarm( "Low battery", string.data() );

		low_battery_event_count++;
		Serial.printf( "[CORE      ] [INFO ] %s", string.data() );
	}
}

void EcoStation::reboot( void )
{
	ESP.restart();
}

int EcoStation::reformat_ca_root_line( std::array<char, 116> &string, int str_len, int ca_pos, int ca_len, const char *root_ca )
{
	int string_pos;

	strlcat( string.data(), root_ca + ca_pos, 113 );
	for ( string_pos = str_len; string_pos < 113; string_pos++ ) {

		if ( string[ string_pos ] == '\n' ) {

			memcpy( string.data() + string_pos, root_ca + ca_pos + 1, 115 - string_pos - 3 );
			ca_pos++;

		}
		ca_pos++;
		if ( ca_pos > ca_len )
			break;
	}
	ca_pos--;
	for ( int j = string_pos; j < 113; string[ j ] = ' ', j++ );
	memset( string.data() + 112, 0, 3 );
	strlcat( string.data(), " #\n", string.size() );
	return ca_pos;
}

void EcoStation::report_unavailable_sensors( void )
{
	std::array<std::string, 7>	sensor_name			= { "MLX96014 ", "TSL2591 ", "BME280 ", "DB_METER" };
	std::array<char, 96>			unavailable_sensors;
	aws_device_t				j					= sensor_manager.get_available_sensors();
	unsigned long				k;

	strlcpy( unavailable_sensors.data(), "Unavailable sensors: ", unavailable_sensors.size() );

	k = static_cast<unsigned long>( j );
	for ( uint8_t i = 0; i < 7; i++ ) {

		if ( ( k & aws_device_t::MLX_SENSOR ) == aws_device_t::NO_SENSOR )		// Flaky, MLX_SENSOR = 1, maybe need to rework this ....
			strlcat( unavailable_sensors.data(), sensor_name[i].c_str(), 13 );
		k >>= 1;
	}

	if ( debug_mode ) {

		Serial.printf( "[STATION   ] [DEBUG] %s", unavailable_sensors.data() );

		if ( j == ALL_SENSORS )
			Serial.printf( "none.\n" );
	}

	if ( j != ALL_SENSORS )
		send_alarm( "[STATION   ] Unavailable sensors report", unavailable_sensors.data() );
}

void EcoStation::send_alarm( const char *subject, const char *message )
{
	JsonDocument content;
	// flawfinder: ignore
	char jsonString[ 600 ];	// NOSONAR
	content["subject"] = subject;
	content["message"] = message;

	serializeJson( content, jsonString );
	network.post_content( "alarm.php", strlen( "alarm.php" ), jsonString );
	content.clear();
	content["username"] = "TestStation";
	content["content"] = message;
	serializeJson( content, jsonString );
}

void EcoStation::send_data( void )
{
	bool lora_data_sent = false;

	if ( !solar_panel ) {

		while ( xSemaphoreTake( sensors_read_mutex, 5000 /  portTICK_PERIOD_MS ) != pdTRUE )

			if ( debug_mode )
				Serial.printf( "[STATION   ] [DEBUG] Waiting for sensor data update to complete.\n" );

	}

	get_json_sensor_data();
	if ( debug_mode )
		Serial.printf( "[STATION   ] [DEBUG] Sensor data: %s\n", json_sensor_data.data() );
	sensor_manager.encode_sensor_data();

	if ( config.get_has_device( aws_device_t::LORAWAN_DEVICE ) )
		network.send_raw_data( reinterpret_cast<uint8_t *>( &compact_data ), sizeof( compact_data_t ) );
	else
		network.post_content( "newData.php", strlen( "newData.php" ), json_sensor_data.data() );

	store_unsent_data( etl::string_view( json_sensor_data ));

	digitalWrite( GPIO_ENABLE_3_3V, LOW );

	if ( !solar_panel )
		xSemaphoreGive( sensors_read_mutex );
}

bool EcoStation::store_unsent_data( etl::string_view data )
{
	bool ok;

	UNSELECT_SPI_DEVICES();

	if ( !SD.begin( GPIO_SD_CS ) ) {

		Serial.printf( "[STATION   ] [ERROR] Cannot open SDCard.\n" );
		return false;
	}

	File backlog = SD.open( "/backlog.txt", FILE_APPEND );
	if ( !backlog ) {

		Serial.printf( "[STATION   ] [ERROR] Cannot store data.\n" );
		return false;
	}

	if (( ok = ( backlog.printf( "%s\n", data.data()) == ( 1 + json_sensor_data_len )) )) {

		if ( debug_mode )
			Serial.printf( "[STATION   ] [DEBUG] Data stored: [%s]\n", data.data() );

	} else

		Serial.printf( "[STATION   ] [ERROR] Could not store data.\n" );

	backlog.close();
	return ok;
}

bool EcoStation::sync_time( bool verbose )
{
	const char	*ntp_server = "pool.ntp.org";
	uint8_t		ntp_retries = 5;
	struct 		tm timeinfo;

	if ( debug_mode && verbose )
		Serial.printf( "[STATION   ] [DEBUG] Connecting to NTP server " );

	configTzTime( config.get_parameter<const char *>( "tzname" ), ntp_server );

	while ( !( ntp_synced = getLocalTime( &timeinfo )) && ( --ntp_retries > 0 ) ) {	// NOSONAR

		if ( debug_mode && verbose )
			Serial.printf( "." );
		delay( 1000 );
		configTzTime( config.get_parameter<const char *>( "tzname" ), ntp_server );
	}
	if ( debug_mode && verbose ) {

		Serial.printf( "\n[STATION   ] [DEBUG] %sNTP Synchronised. ", ntp_synced ? "" : "NOT " );
		Serial.print( "Time and date: " );
		Serial.print( &timeinfo, "%Y-%m-%d %H:%M:%S\n" );
		Serial.printf( "\n" );
	}

	if ( ntp_synced ) {

		time( &sensor_manager.get_sensor_data()->timestamp );
		station_data.ntp_time.tv_sec = sensor_manager.get_sensor_data()->timestamp;
		if ( ntp_time_misses )
			ntp_time_misses = 0;
		last_ntp_time = sensor_manager.get_sensor_data()->timestamp;

	} else {

		if ( !fixup_timestamp() ) {

			// Not proud of this but it should be sufficient if the number of times we miss ntp sync is not too big
			ntp_time_misses++;
			sensor_manager.get_sensor_data()->timestamp =  last_ntp_time + ( US_SLEEP / 1000000 ) * ntp_time_misses;

		}
	}
	return ntp_synced;
}

void EcoStation::trigger_ota_update( void )
{
	force_ota_update = true;
}
