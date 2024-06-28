/*	
	EcoStation.ino

	(c) 2024 F.Lesage

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

const etl::string<12>		REV					= "1.0.0";
const unsigned long			US_SLEEP			= 15 * 60 * 1000000;				// 15 minutes
const unsigned long long	US_HIBERNATE		= 1 * 24 * 60 * 60 * 1000000ULL;	// 1 day

EcoStation station;

void setup()
{
	Serial.begin( 115200 );
	delay( 500 );
	        
	if ( !station.initialise()) {
		
		Serial.printf( "[CORE      ] [PANIC] ===> EcoStation did not properly initialise. Stopping here! <===\n" );
		while( true ) { delay( 100000 ); }
	}

	if ( station.on_solar_panel() ) {

		station.read_sensors();
		station.send_data();
		station.prepare_for_deep_sleep( static_cast<int>( US_SLEEP / 1000000 ) );
		esp_sleep_enable_timer_wakeup( US_SLEEP );
		Serial.printf( "[CORE      ] [INFO ] Entering sleep mode.\n" );
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
