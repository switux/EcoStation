/*	
	EcoStation.ino

	(c) 2024-2025 F.Lesage

	1.0.0 - Initial version.
	
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

#include "gpio_config.h"
#include "common.h"
#include "EcoStation.h"

const etl::string<12>		REV					= "1.1.0";
const unsigned long long	US_HIBERNATE		= 1 * 24 * 60 * 60 * 1000000ULL;	// 1 day

EcoStation station;

void setup()
{
    btStop();
	setCpuFrequencyMhz( 80 );

	Serial.begin( 115200 );
	delay( 500 );

	if ( !station.initialise()) {
		
		Serial.printf( "[CORE      ] [PANIC] ===> EcoStation did not properly initialise. Stopping here! <===\n" );
		while( true ) { delay( 100000 ); }
	}

	if ( station.on_solar_panel() ) {


		station.read_sensors();
		station.send_data();

		uint16_t	sleep_minutes = station.get_config().get_parameter<uint16_t>( "sleep_minutes" );

		station.prepare_for_deep_sleep( sleep_minutes * 60 );

		esp_sleep_enable_timer_wakeup( sleep_minutes * 60 * 1000000 );
		esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF );
		esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_FAST_MEM, ESP_PD_OPTION_ON );
		esp_sleep_pd_config(ESP_PD_DOMAIN_XTAL, ESP_PD_OPTION_OFF);

		Serial.printf( "[CORE      ] [INFO ] Entering deep sleep mode for %d minutes.\n", sleep_minutes );
		esp_deep_sleep_start();

	}
}

void loop()
{
	if ( station.on_solar_panel() ) {

		while (true) {

			Serial.printf( "[CORE      ] [PANIC] Must not execute this code when running on solar panel!\n" );
			delay( 10000 );
		}
	}

	while( true );
}
