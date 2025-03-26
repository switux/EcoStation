/*
  	AWSSensorManager.cpp

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

#include <esp_task_wdt.h>
#include <ESP32Time.h>
// Keep these two to get rid of compile time errors because of incompatibilities between libraries
#include <ESPAsyncWebServer.h>

#include "defaults.h"
#include "gpio_config.h"
#include "common.h"
#include "config_manager.h"
#include "SQM.h"
#include "device.h"
#include "sensor_manager.h"
#include "EcoStation.h"

RTC_DATA_ATTR long	prev_available_sensors = 0;	// NOSONAR
RTC_DATA_ATTR long	available_sensors = 0;		// NOSONAR

SemaphoreHandle_t sensors_read_mutex = NULL;	// Issue #7
const aws_device_t ALL_SENSORS	= ( aws_device_t::MLX_SENSOR |
									aws_device_t::TSL_SENSOR |
									aws_device_t::BME_SENSOR |
									aws_device_t::SPL_SENSOR );

const std::array<etl::string_view,3> CLOUD_COVERAGE_STR = { etl::string_view( "Clear" ), etl::string_view( "Cloudy" ), etl::string_view( "Overcast" ) };

extern EcoStation station;

aws_device_t operator&( aws_device_t a, aws_device_t b )
{
	return static_cast<aws_device_t>( static_cast<unsigned long>(a) & static_cast<unsigned long>(b) );
}

bool operator!=( unsigned long a, aws_device_t b )
{
	return a != static_cast<unsigned long>(b);
}

aws_device_t operator&( unsigned long a, aws_device_t b )
{
	return static_cast<aws_device_t>( a & static_cast<unsigned long>(b) );
}

aws_device_t operator~( aws_device_t b )
{
	return static_cast<aws_device_t>( ~static_cast<unsigned long>(b) );
}

aws_device_t operator|( aws_device_t a, aws_device_t b )
{
	return static_cast<aws_device_t>( static_cast<unsigned long>(a) | static_cast<unsigned long>(b) );
}

aws_device_t operator*( aws_device_t a, int b )
{
	return static_cast<aws_device_t>( static_cast<unsigned long>(a) * b );
}

aws_device_t& operator|=( aws_device_t& a, aws_device_t b )
{
	return a = static_cast<aws_device_t>( static_cast<unsigned long>(a) | static_cast<unsigned long>(b) );
}

aws_device_t& operator&=( aws_device_t& a, aws_device_t b )
{
	return a = static_cast<aws_device_t>( static_cast<unsigned long>(a) & static_cast<unsigned long>(b) );
}

AWSSensorManager::AWSSensorManager( void ) :
	i2c_mutex( xSemaphoreCreateMutex() ),
	tsl ( 2591 )
{
	memset( &sensor_data, 0, sizeof( sensor_data_t ));
	sensor_data.available_sensors	= aws_device_t::NO_SENSOR;

}

int16_t AWSSensorManager::float_to_int16_encode( float v, float min, float max )
{
	if ( v < min )
		v = min;
	else if ( v > max )
		v = max;

	return static_cast<int16_t>( v * 100 );
}

int32_t AWSSensorManager::float_to_int32_encode( float v, float min, float max )
{
	if ( v < min )
		v = min;
	else if ( v > max )
		v = max;

	return static_cast<int32_t>( v * 100 );
}

aws_device_t AWSSensorManager::get_available_sensors( void )
{
	return sensor_data.available_sensors;
}

bool AWSSensorManager::get_debug_mode( void )
{
	return debug_mode;
}

SemaphoreHandle_t AWSSensorManager::get_i2c_mutex( void )
{
	return i2c_mutex;
}

sensor_data_t *AWSSensorManager::get_sensor_data( void )
{
	return &sensor_data;
}

bool AWSSensorManager::initialise( AWSConfig *_config, compact_data_t *_compact_data, bool create_mutex )
{
	config = _config;
	compact_data = _compact_data;

	initialise_sensors();

	if ( !solar_panel || create_mutex ) {

		sensors_read_mutex = xSemaphoreCreateMutex();
		std::function<void(void *)> _poll_sensors_task = std::bind( &AWSSensorManager::poll_sensors_task, this, std::placeholders::_1 );
		xTaskCreatePinnedToCore(
			[](void *param) {	// NOSONAR
				std::function<void(void*)>* poll_proxy = static_cast<std::function<void(void*)>*>( param );
				(*poll_proxy)( NULL );
			}, "SensorManagerTask", 10000, &_poll_sensors_task, 5, &sensors_task_handle, 1 );
	}
	k[0] = config->get_parameter<int>( "k1" );
	k[1] = config->get_parameter<int>( "k2" );
	k[2] = config->get_parameter<int>( "k3" );
	k[3] = config->get_parameter<int>( "k4" );
	k[4] = config->get_parameter<int>( "k5" );
	k[5] = config->get_parameter<int>( "k6" );
	k[6] = config->get_parameter<int>( "k7" );

	initialised = true;
	return true;
}

void AWSSensorManager::initialise_dbmeter( void )
{
	uint8_t seconds = config->get_parameter<uint8_t>( "spl_duration" );
	uint8_t spl_mode = config->get_parameter<uint8_t>( "spl_mode" );

	if ( !spl.begin( spl_mode, seconds ) )
		Serial.printf( "[SENSORMNGR] [ERROR] Could not find DBMETER.\n" );

	else {

		if ( debug_mode )
			Serial.printf( "[SENSORMNGR] [INFO ] Found DBMETER.\n" );

		sensor_data.available_sensors |= aws_device_t::SPL_SENSOR;
	}
}

void AWSSensorManager::initialise_BME( void )
{
	if ( !bme.begin( 0x76 ) )

		Serial.printf( "[SENSORMNGR] [ERROR] Could not find BME280.\n" );

	else {

		if ( debug_mode )
			Serial.printf( "[SENSORMNGR] [INFO ] Found BME280.\n" );

		sensor_data.available_sensors |= aws_device_t::BME_SENSOR;
	}
}

void AWSSensorManager::initialise_MLX( void )
{
	if ( !mlx.begin() )

		Serial.printf( "[SENSORMNGR] [ERROR] Could not find MLX90614.\n" );

	else {

		if ( debug_mode )
			Serial.printf( "[SENSORMNGR] [INFO ] Found MLX90614.\n" );

		sensor_data.available_sensors |= aws_device_t::MLX_SENSOR;
	}
}

void AWSSensorManager::initialise_sensors( void )
{
	if ( config->get_has_device( aws_device_t::BME_SENSOR ) )
		initialise_BME();

	if ( config->get_has_device( aws_device_t::MLX_SENSOR ) )
		initialise_MLX();

	if ( config->get_has_device( aws_device_t::TSL_SENSOR ) ) {

		initialise_TSL();
		sqm.initialise( &tsl, &sensor_data.sqm, config->get_parameter<float>( "msas_calibration_offset" ), debug_mode );
	}

	initialise_dbmeter();
}

void AWSSensorManager::initialise_TSL( void )
{
	if ( !tsl.begin() ) {

		if ( debug_mode )
			Serial.printf( "[SENSORMNGR] [ERROR] Could not find TSL2591.\n" );

	} else {

		if ( debug_mode )
			Serial.printf( "[SENSORMNGR] [INFO ] Found TSL2591.\n" );

		tsl.setGain( TSL2591_GAIN_LOW );
		tsl.setTiming( TSL2591_INTEGRATIONTIME_100MS );
		sensor_data.available_sensors |= aws_device_t::TSL_SENSOR;
	}
}

bool AWSSensorManager::poll_sensors( void )
{
	if ( xSemaphoreTake( sensors_read_mutex, 2000 / portTICK_PERIOD_MS ) == pdTRUE ) {

		retrieve_sensor_data();
		xSemaphoreGive( sensors_read_mutex );
		return true;
	}
	return false;
}

void AWSSensorManager::poll_sensors_task( void *dummy )	// NOSONAR
{
	while( true ) {

		if ( xSemaphoreTake( sensors_read_mutex, 5000 / portTICK_PERIOD_MS ) == pdTRUE ) {

			retrieve_sensor_data();
			xSemaphoreGive( sensors_read_mutex );
		}
		delay( polling_ms_interval );
	}
}

void AWSSensorManager::read_dbmeter( void  )
{
	if ( ( sensor_data.available_sensors & aws_device_t::SPL_SENSOR ) == aws_device_t::SPL_SENSOR ) {

		sensor_data.db = spl.read();
		if ( debug_mode )
			Serial.printf( "[SENSORMNGR] [DEBUG] SPL = %ddB\n", sensor_data.db  );
		return;
	}
	sensor_data.db = 0;
}

void AWSSensorManager::read_BME( void  )
{
	if ( ( sensor_data.available_sensors & aws_device_t::BME_SENSOR ) == aws_device_t::BME_SENSOR ) {

		sensor_data.weather.temperature = bme.readTemperature();
		sensor_data.weather.pressure = bme.readPressure() / 100.F;
		sensor_data.weather.rh = bme.readHumidity();

		// "Arden Buck" equation
		float gammaM = log( ( sensor_data.weather.rh / 100 )*exp( ( 18.68 - sensor_data.weather.temperature / 234.5 ) * ( sensor_data.weather.temperature / ( 257.14 + sensor_data.weather.temperature )) ));
		if ( sensor_data.weather.temperature >= 0 )
			sensor_data.weather.dew_point = ( 238.88 * gammaM ) / ( 17.368 - gammaM );
		else
			sensor_data.weather.dew_point = ( 247.15 * gammaM ) / ( 17.966 - gammaM );

		if ( debug_mode ) {

			Serial.printf( "[SENSORMNGR] [DEBUG] Temperature = %2.2f °C\n", sensor_data.weather.temperature  );
			Serial.printf( "[SENSORMNGR] [DEBUG] Pressure = %4.2f hPa\n", sensor_data.weather.pressure );
			Serial.printf( "[SENSORMNGR] [DEBUG] RH = %3.2f %%\n", sensor_data.weather.rh );
			Serial.printf( "[SENSORMNGR] [DEBUG] Dew point = %2.2f °C\n", sensor_data.weather.dew_point );
		}
		return;
	}

	sensor_data.weather.temperature = -99.F;
	sensor_data.weather.pressure = 0.F;
	sensor_data.weather.rh = 0.F;
	sensor_data.weather.dew_point = -99.F;
}

void AWSSensorManager::read_MLX( void )
{
	if ( ( sensor_data.available_sensors & aws_device_t::MLX_SENSOR ) == aws_device_t::MLX_SENSOR ) {

		sensor_data.weather.ambient_temperature = mlx.readAmbientTempC();
		sensor_data.weather.sky_temperature = mlx.readObjectTempC();
		sensor_data.weather.raw_sky_temperature = mlx.readObjectTempC();

		if ( config->get_parameter<int>( "cloud_coverage_formula" ) == 0 ) {

			sensor_data.weather.sky_temperature -= sensor_data.weather.ambient_temperature;
			sensor_data.weather.cloud_coverage = ( sensor_data.weather.sky_temperature <= -15 ) ? 0 : 2;
			if ( sensor_data.weather.sky_temperature < config->get_parameter<int>( "cc_aws_cloudy" ) )
				sensor_data.weather.cloud_coverage = static_cast<uint8_t>( cloud_coverage::CLEAR );
			else if ( sensor_data.weather.sky_temperature < config->get_parameter<int>( "cc_aws_overcast" ) )
				sensor_data.weather.cloud_coverage = static_cast<uint8_t>( cloud_coverage::CLOUDY );
			else
				sensor_data.weather.cloud_coverage = static_cast<uint8_t>( cloud_coverage::OVERCAST );

		}
		else {

			float t = ( k[0] / 100 ) * ( sensor_data.weather.ambient_temperature - k[1] / 10 ) * ( k[2] / 100 ) * pow( exp( k[3] / 1000 * sensor_data.weather.ambient_temperature ), k[4]/100 );
			float t67;
			if ( abs( k[1] / 10 - sensor_data.weather.ambient_temperature ) < 1 )
				t67 = sign<int>( k[5] ) * sign<float>( sensor_data.weather.ambient_temperature - k[1]/10) * ( k[1] / 10 - sensor_data.weather.ambient_temperature );
			else
				t67 = k[5] / 10 * sign( sensor_data.weather.ambient_temperature - k[1]/10 ) * ( log( abs( k[1]/10 - sensor_data.weather.ambient_temperature ))/log(10) + k[6] / 100 );
			t += t67;
			sensor_data.weather.sky_temperature -= t;

			if ( sensor_data.weather.sky_temperature < config->get_parameter<int>( "cc_aag_cloudy" ) )
				sensor_data.weather.cloud_coverage = static_cast<uint8_t>( cloud_coverage::CLEAR );
			else if ( sensor_data.weather.sky_temperature < config->get_parameter<int>( "cc_aag_overcast" ) )
				sensor_data.weather.cloud_coverage = static_cast<uint8_t>( cloud_coverage::CLOUDY );
			else
				sensor_data.weather.cloud_coverage = static_cast<uint8_t>( cloud_coverage::OVERCAST );

		}
		if ( debug_mode )
			Serial.printf( "[SENSORMNGR] [DEBUG] Ambient temperature = %2.2f °C / Raw sky temperature = %2.2f °C / Corrected sky temperature = %2.2f / Cloud coverage = %s (%d)\n", sensor_data.weather.ambient_temperature, sensor_data.weather.raw_sky_temperature, sensor_data.weather.sky_temperature, CLOUD_COVERAGE_STR[sensor_data.weather.cloud_coverage].data(), sensor_data.weather.cloud_coverage );
		return;
	}
	sensor_data.weather.ambient_temperature = -99.F;
	sensor_data.weather.raw_sky_temperature = -99.F;
	sensor_data.weather.sky_temperature = -99.F;
}

void AWSSensorManager::read_sensors( void )
{
	retrieve_sensor_data();

	if ( prev_available_sensors != sensor_data.available_sensors ) {

		prev_available_sensors = static_cast<unsigned long>( sensor_data.available_sensors );
		station.report_unavailable_sensors();
	}
}

void AWSSensorManager::read_TSL( void )
{
	int			lux = -1;

	if ( ( sensor_data.available_sensors & aws_device_t::TSL_SENSOR ) == aws_device_t::TSL_SENSOR ) {

		uint32_t lum = tsl.getFullLuminosity();
		uint16_t ir = lum >> 16;
		uint16_t full = lum & 0xFFFF;
		lux = tsl.calculateLux( full, ir );

		if ( debug_mode )
			Serial.printf( "[SENSORMNGR] [DEBUG] Infrared=%05d Full=%05d Visible=%05d Lux = %05d\n", ir, full, full - ir, lux );
	}

	// Avoid aberrant readings
	sensor_data.sun.lux = ( lux < TSL_MAX_LUX ) ? lux : -1;
	sensor_data.sun.irradiance = ( lux == -1 ) ? 0 : lux * LUX_TO_IRRADIANCE_FACTOR;
}

void AWSSensorManager::encode_sensor_data( void )
{
	if ( debug_mode )
		Serial.printf( "[SENSORMNGR] [DEBUG] Compact sensor data format version: %02x\n", compact_data->format_version );

	compact_data->timestamp = sensor_data.timestamp;

	compact_data->lux = float_to_int32_encode( sensor_data.sun.lux, 0, 80000 );
	compact_data->irradiance = float_to_int16_encode( sensor_data.sun.irradiance, 0, 1000 );

	compact_data->temperature = float_to_int16_encode( sensor_data.weather.temperature, -40, 50 );
	compact_data->pressure = float_to_int32_encode( sensor_data.weather.pressure, 700, 1050 );
	compact_data->rh = float_to_int16_encode( sensor_data.weather.rh, 0, 100 );

	compact_data->ambient_temperature = float_to_int16_encode( sensor_data.weather.ambient_temperature, -40, 50 );
	compact_data->raw_sky_temperature = float_to_int16_encode( sensor_data.weather.raw_sky_temperature, -100, 50 );
	compact_data->sky_temperature = float_to_int16_encode( sensor_data.weather.sky_temperature, -100, 50 );
	compact_data->cloud_cover = float_to_int16_encode( sensor_data.weather.cloud_cover, -100, 50 );
	compact_data->cloud_coverage = sensor_data.weather.cloud_coverage;

	compact_data->msas = float_to_int16_encode( sensor_data.sqm.msas, 0, 30);
	compact_data->nelm = float_to_int16_encode( sensor_data.sqm.nelm, -15, 10 );

	compact_data->db = sensor_data.db;

	compact_data->available_sensors = sensor_data.available_sensors;
}

void AWSSensorManager::retrieve_sensor_data( void )
{
	if ( xSemaphoreTake( i2c_mutex, 500 / portTICK_PERIOD_MS ) == pdTRUE ) {

		time( &sensor_data.timestamp );

		if ( config->get_has_device( aws_device_t:: BME_SENSOR ) )
			read_BME();

		esp_task_wdt_reset();

		if ( config->get_has_device( aws_device_t::MLX_SENSOR ) )
			read_MLX();

		esp_task_wdt_reset();

		if ( config->get_has_device( aws_device_t::TSL_SENSOR ) ) {

			read_TSL();
			if ( ( sensor_data.available_sensors & aws_device_t::TSL_SENSOR ) == aws_device_t::TSL_SENSOR )
				sqm.read( sensor_data.weather.ambient_temperature );
		}

		esp_task_wdt_reset();

		read_dbmeter();

		esp_task_wdt_reset();

		xSemaphoreGive( i2c_mutex );

	}
}

void AWSSensorManager::set_debug_mode( bool b )
{
	debug_mode = b;
}

void AWSSensorManager::set_solar_panel( bool b )
{
	solar_panel = b;

	if ( solar_panel ) {

		pinMode( GPIO_BAT_ADC_EN, OUTPUT );
		pinMode( GPIO_BAT_ADC, INPUT );
		pinMode( GPIO_PANEL_ADC, INPUT );
	}
}

void AWSSensorManager::resume( void )
{
	if ( initialised )
		vTaskResume( sensors_task_handle );
}

bool AWSSensorManager::sensor_is_available( aws_device_t sensor )
{
	return (( sensor_data.available_sensors & sensor ) == sensor );
}

void AWSSensorManager::suspend( void )
{
	if ( initialised )
		vTaskSuspend( sensors_task_handle );
}

void AWSSensorManager::update_available_sensors( aws_device_t dev, bool is_available )
{
	if ( is_available )
		sensor_data.available_sensors |= dev;
	else
		sensor_data.available_sensors &= ~dev;
}
