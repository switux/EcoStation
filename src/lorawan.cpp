/*
  	lorawan.cpp

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

#include "gpio_config.h"
#include "lorawan.h"
#include "common.h"
#include "EcoStation.h"

extern EcoStation	station;

const lmic_pinmap lmic_pins = {
	.nss	= GPIO_LORA_CS,
	.rxtx	= LMIC_UNUSED_PIN,
	.rst	= GPIO_LORA_RST,
	.dio	= { GPIO_LORA_DIO0, GPIO_LORA_DIO1, GPIO_LORA_DIO2 },
};

RTC_DATA_ATTR lmic_t RTC_LMIC;

void os_getArtEui( u1_t* buf )
{
	memcpy_P( buf, APPEUI, 8 );
}

void os_getDevEui( u1_t* buf )
{
	memcpy_P( buf, DEVEUI, 8 );
}

void os_getDevKey( u1_t* buf )
{
	memcpy_P( buf, APPKEY, 16 );
}

bool AWSLoraWAN::begin( std::array<uint8_t,8> deveui, std::array<uint8_t,16> appkey, bool _debug_mode )
{
	debug_mode = _debug_mode;

	memcpy_P( DEVEUI, deveui.data(), 8 );
	memcpy_P( APPKEY, appkey.data(), 16 );

	os_init();
	LMIC_reset();
	restore_after_deep_sleep();

	if ( !joined ) {

		LMIC_setupChannel( 0, 868100000, DR_RANGE_MAP( DR_SF12, DR_SF7 ),  BAND_CENTI );
		LMIC_setupChannel( 1, 868300000, DR_RANGE_MAP( DR_SF12, DR_SF7B ), BAND_CENTI );
		LMIC_setupChannel( 2, 868500000, DR_RANGE_MAP( DR_SF12, DR_SF7 ),  BAND_CENTI );
		LMIC_setupChannel( 3, 867100000, DR_RANGE_MAP( DR_SF12, DR_SF7 ),  BAND_CENTI );
		LMIC_setupChannel( 4, 867300000, DR_RANGE_MAP( DR_SF12, DR_SF7 ),  BAND_CENTI );
		LMIC_setupChannel( 5, 867500000, DR_RANGE_MAP( DR_SF12, DR_SF7 ),  BAND_CENTI );
		LMIC_setupChannel( 6, 867700000, DR_RANGE_MAP( DR_SF12, DR_SF7 ),  BAND_CENTI );
		LMIC_setupChannel( 7, 867900000, DR_RANGE_MAP( DR_SF12, DR_SF7 ),  BAND_CENTI );
		LMIC_setupChannel( 8, 868800000, DR_RANGE_MAP( DR_FSK,  DR_FSK ),  BAND_MILLI );
		LMIC_setLinkCheckMode( 0 );
		LMIC.dn2Dr = DR_SF9;
		LMIC_setDrTxpow( DR_SF12, 14 );
		Serial.printf( "[LORAWAN   ] [INFO ] Need to rejoin.\n" );

	} else {

		Serial.printf( "[LORAWAN   ] [INFO ] No need to rejoin.\n" );
		LMIC_setLinkCheckMode( 1 );
	}
	return true;
}

bool AWSLoraWAN::check_joined( void )
{
	os_runloop_once();

	if (( LMIC.devaddr != 0  ) && (( LMIC.opmode & OP_JOINING ) == 0 )) {

		Serial.printf( "[LORAWAN   ] [INFO ] Joined LoRaWAN network with devaddr 0x%04lX\n", LMIC.devaddr );
		LMIC_setLinkCheckMode( 1 );

		return true;
	}
	return false;
}

bool AWSLoraWAN::join( void )
{
	if ( joined ) {

		LMIC_setLinkCheckMode( 1 );
		return true;
	}

	LMIC_setLinkCheckMode( 0 );
	LMIC_startJoining();

	unsigned long	start = millis();

	while ( (!( joined = check_joined() )) && ( ( millis() - start ) < 20000 )) { /* wait 20s before giving up */ };
	if ( !joined )
		Serial.printf( "[LORAWAN   ] [ERROR] Could not join the network\n" );
	else
		LMIC_setLinkCheckMode( 1 );

	return joined;
}

bool AWSLoraWAN::has_joined( void )
{
	return joined;
}
 
void AWSLoraWAN::prepare_for_deep_sleep( int deep_sleep_time_secs )
{
	RTC_LMIC = LMIC;

	//
	// https://github.com/JackGruber/ESP32-LMIC-DeepSleep-example
	// Otherwise stuck with opmode 0x100 set and no packet is ever transmitted
	//
	unsigned long now = millis();
	for( int i = 0; i < MAX_BANDS; i++ ) {

		ostime_t correctedAvail = RTC_LMIC.bands[i].avail - ( (( now / 1000.0 ) + deep_sleep_time_secs ) * OSTICKS_PER_SEC );
		if ( correctedAvail < 0 )
			correctedAvail = 0;
		RTC_LMIC.bands[i].avail = correctedAvail;

	}

	RTC_LMIC.globalDutyAvail = RTC_LMIC.globalDutyAvail - ( (( now / 1000.0 ) + deep_sleep_time_secs) * OSTICKS_PER_SEC );
	if (RTC_LMIC.globalDutyAvail < 0)
		RTC_LMIC.globalDutyAvail = 0;
}

void AWSLoraWAN::restore_after_deep_sleep( void )
{
	if ( !RTC_LMIC.seqnoUp )
		return;

	LMIC = RTC_LMIC;
	joined = true;
}

void AWSLoraWAN::send( osjob_t *job )
{
	if ( LMIC.opmode & OP_TXRXPEND ) {

		if ( debug_mode )
			Serial.print( "[LORAWAN   ] [DEBUG] RX/TX pending, not sending packet\n" );
		return;
	}

	LMIC_setTxData2( 1, mydata.data(), mylen, 0 );
	if ( debug_mode ) {

		Serial.printf( "[LORAWAN   ] [DEBUG] Queuing packet of %d bytes [", mylen );
		for( int i = 0; i < mylen; i++ )
			Serial.printf( "%02x ", mydata[ i ] );
		Serial.printf( "]\n" );
	}

	unsigned long	start			= millis();
	bool			message_sent	= false;
	bool			msg_recv		= false;

	while (( millis() - start ) < 5000 ) {

		os_runloop_once();

		if ( !message_sent && !( LMIC.opmode & OP_TXRXPEND ))  {
			message_sent = true;
			Serial.println("[LORAWAN   ] [DEBUG] Transmission completed.");
		}

		if ( ( LMIC.txrxFlags & ( TXRX_DNW1 | TXRX_DNW2 )) && ( LMIC.dataLen > 0 ) ) {

			Serial.printf( "[LORAWAN   ] [DEBUG] Downlink payload: [" );
			for ( uint8_t i = 0; i < LMIC.dataLen; i++ )
				Serial.printf( "%02X ", LMIC.frame[ LMIC.dataBeg + i ] );
			Serial.printf( "]\n" );
			uint8_t commandID = LMIC.frame[ LMIC.dataBeg ];
			Serial.printf("[LORAWAN   ] [DEBUG] Command ID: 0x%02X\n", commandID );

		}

		if (( LMIC.txrxFlags & TXRX_NOPORT ) && ( LMIC.dataLen > 0 )) {

			Serial.printf( "[LORAWAN   ] [DEBUG] No PORT Downlink payload: [" );
			for ( uint8_t i = 0; i < LMIC.dataLen; i++ )
				Serial.printf( "%02X ", LMIC.frame[ LMIC.dataBeg + i ] );
			Serial.printf( "]\n" );
			uint8_t commandID = LMIC.frame[ LMIC.dataBeg ];
			Serial.printf("[LORAWAN   ] [DEBUG] Command ID: 0x%02X\n", commandID );
                    
	    }

		delay( 100 );

	}

	Serial.printf( "[LORAWAN   ] [DEBUG] Packet %ssent\n", message_sent?"":"not "  );
}

void AWSLoraWAN::send_data( uint8_t *buffer, uint8_t len )
{
	std::fill( mydata.begin(), mydata.end(), 0 );
	if ( len <= mydata.max_size() ) {
		
		memcpy( mydata.data(), buffer, len );
		mylen = len;

	} else

		mylen = 0;

	send( &sendjob );
}
