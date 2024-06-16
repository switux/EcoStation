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

const lmic_pinmap lmic_pins = {
  .nss	= GPIO_LORA_CS,
  .rxtx = LMIC_UNUSED_PIN,
  .rst	= GPIO_LORA_RST,
  .dio	= { GPIO_LORA_DIO0, GPIO_LORA_DIO1, GPIO_LORA_DIO2 },
};

const unsigned TX_INTERVAL = 60;
extern unsigned long			US_SLEEP;

RTC_DATA_ATTR uint32_t fcnt = 0;

// Only for OTAA
void os_getArtEui (u1_t* buf) { }
void os_getDevEui (u1_t* buf) { }
void os_getDevKey (u1_t* buf) { }

AWSLoraWAN::AWSLoraWAN( uint8_t *_appskey, uint8_t *_nwkskey, uint32_t _devaddr )
{
	memcpy( appskey, _appskey, 16 );
	memcpy( nwkskey, _nwkskey, 16 );
	devaddr = _devaddr;
}

bool AWSLoraWAN::begin( bool _debug_mode )
{
	debug_mode = _debug_mode;
	os_init();
	LMIC_reset();
	LMIC_setSession( TTN_NET_ID, devaddr, nwkskey, appskey );
	LMIC_setupChannel( 0, 868100000, DR_RANGE_MAP( DR_SF12, DR_SF7 ),  BAND_CENTI );
	LMIC_setupChannel( 1, 868300000, DR_RANGE_MAP( DR_SF12, DR_SF7B ), BAND_CENTI );
	LMIC_setupChannel( 2, 868500000, DR_RANGE_MAP( DR_SF12, DR_SF7 ),  BAND_CENTI );
	LMIC_setupChannel( 3, 867100000, DR_RANGE_MAP( DR_SF12, DR_SF7 ),  BAND_CENTI );
	LMIC_setupChannel( 4, 867300000, DR_RANGE_MAP( DR_SF12, DR_SF7 ),  BAND_CENTI );
	LMIC_setupChannel( 5, 867500000, DR_RANGE_MAP( DR_SF12, DR_SF7 ),  BAND_CENTI );
	LMIC_setupChannel( 6, 867700000, DR_RANGE_MAP( DR_SF12, DR_SF7 ),  BAND_CENTI );
	LMIC_setupChannel( 7, 867900000, DR_RANGE_MAP( DR_SF12, DR_SF7 ),  BAND_CENTI );
	LMIC_setupChannel( 8, 868800000, DR_RANGE_MAP( DR_FSK,  DR_FSK ),  BAND_MILLI );
	LMIC_setLinkCheckMode( 1 );
	LMIC.dn2Dr = DR_SF9;
	LMIC_setDrTxpow( DR_SF7, 14 );
	LMIC.seqnoUp = fcnt;
	return true;
}

void AWSLoraWAN::prepare_for_deep_sleep( void )
{
    fcnt = LMIC.seqnoUp;
}

void AWSLoraWAN::send( osjob_t *job )
{
	if ( LMIC.opmode & OP_TXRXPEND ) {

		if ( debug_mode )
			Serial.print( "[LORAWAN   ] [DEBUG] RX/TX pending, not sending packet\n" );

	} else {

		LMIC.seqnoUp++;
		Serial.printf("LORA SEQ #%d\n",LMIC.seqnoUp);
		LMIC_setTxData2( 1, reinterpret_cast<unsigned char *>(mydata), mylen, 0 );
		if ( debug_mode ) {

			Serial.printf( "[LORAWAN   ] [DEBUG] Queuing packet of %d bytes [", mylen );
			for( int i = 0; i < mylen; i++ )
				Serial.printf( "%02x ", mydata[ i ] );
			Serial.printf( "]\n" );
		}

		unsigned long	start			= millis();
		bool			message_sent	= false;

		while (( !message_sent ) && (( millis() - start ) < 10000 )) {

			os_runloop_once();

			if ( ( LMIC.opmode & OP_TXRXPEND ))
				delay( 100 );
			else
				message_sent = true;
		}
		if ( message_sent )
			Serial.printf( "[LORAWAN   ] [DEBUG] Packet sent\n"  );
	}
}

void AWSLoraWAN::send_data( uint8_t *buffer, uint8_t len )
{
	memset( mydata, 0, len );
	memcpy( mydata, buffer, len );
	mylen = len;

	send( &sendjob );
}
