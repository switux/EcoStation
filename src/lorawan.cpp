/*
  	lorawan.cpp

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

#include "gpio_config.h"
#include "common.h"
#include "lorawan.h"
#include "EcoStation.h"

extern EcoStation station;

#if defined(ARDUINO_LMIC_OVERRIDE_INITIAL_JOIN_DR)
extern _dr_eu868_t LMIC_join_dr;
#endif


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

AWSLoraWAN	*AWSLoraWAN::me = nullptr;

AWSLoraWAN::AWSLoraWAN( void )
{
	me = this;
}

bool AWSLoraWAN::begin( std::array<uint8_t,8> deveui, std::array<uint8_t,16> appkey, _dr_eu868_t join_dr, bool _debug_mode )
{
	debug_mode = _debug_mode;

	memcpy_P( DEVEUI, deveui.data(), 8 );
	memcpy_P( APPKEY, appkey.data(), 16 );

#if defined(ARDUINO_LMIC_OVERRIDE_INITIAL_JOIN_DR)
	LMIC_join_dr = join_dr;
#endif

	os_init();
	LMIC_reset();
	restore_after_deep_sleep();

	if ( !joined )

		Serial.printf( "[LORAWAN   ] [INFO ] Need to rejoin network.\n" );

	else {

		Serial.printf( "[LORAWAN   ] [INFO ] Already joined with addr 0x%04lx.\n", LMIC.devaddr );
		LMIC_setLinkCheckMode( 1 );
	}

	std::function<void(void *)> _loop = std::bind( &AWSLoraWAN::loop, this, std::placeholders::_1 );

	if ( xTaskCreatePinnedToCore(
		[](void *param) {	// NOSONAR
			std::function<void(void*)>* periodic_tasks_proxy = static_cast<std::function<void(void*)>*>( param );	// NOSONAR
			(*periodic_tasks_proxy)( NULL );
		}, "LORALOOP Task", 5000, &_loop, 5, &loop_handle, 1 ) != pdPASS )

		Serial.printf( "[LORAWAN   ] [ERROR] Failed to start LoRaWAN event loop.\n" );

	return true;
}

void AWSLoraWAN::empty_queue( void )
{
	if ( !msg_waiting )
		return;

	if ( !msg_port ) {

		Serial.printf( "[LORAWAN   ] [INFO ] Queued message is on port 0, dropping.\n" );
		return;
	}

	unsigned long	start = millis();
	while( ( LMIC.opmode & ( OP_POLL | OP_TXDATA | OP_JOINING | OP_TXRXPEND ) != 0 )
			&& ( ( millis() - start ) < ( 30*1000 ) ) ) {
		delay( 5000 );
	}

	// Well, it is actually desirable to notify back but sometimes it cannot be helped and we
	// do not want to draw on the batteries too much
	if ( LMIC.opmode & ( OP_POLL | OP_TXDATA | OP_JOINING | OP_TXRXPEND ) != 0 ) {

		Serial.printf( "[LORAWAN   ] [INFO ] Timeout waiting for clear TX path! Dropping message.\n" );
		return;
	}

	_message_sent = false;
	memcpy( mydata.data(), &msg, 8 );
	if ( debug_mode ) {

		Serial.printf( "[LORAWAN   ] [DEBUG] Queuing packet of %d bytes on port %d [ ", 8, msg_port );
		for( int i = 0; i < 8; i++ )
			Serial.printf( "%02x ", mydata[ i ] );
		Serial.printf( "]\n" );
	}

	LMIC_setTxData2( msg_port, mydata.data(), 8, 0 );

	while( !_message_sent )
		delay( 100 );
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

	// At SF7, 3 attempts per minute or so
	// At SF8, 3 attempts every two minutes or so => starts at t0 + 3'
	// At SF9, 3 attempts every four minutes or so => starts at t0 + 9'
	// At SF10, 3 attempts every eight minutes or so => starts at t0 + 21'
	// At SF11, 3 attempts every 16 minutes or so => starts at t0 + 45'
	// At SF12, 3 attempts every 32 minutes or so => starts at t0 + 87' and stays here

	while (( !joined ) && ( ( millis() - start ) < (120*60*1000) )) { /* Wait until we try 2x at SF12 */ };

	if ( !joined )

		Serial.printf( "[LORAWAN   ] [ERROR] Could not join the network\n" );

	else {

		Serial.printf( "[LORAWAN   ] [INFO ] Joined with addr 0x%04lx.\n", LMIC.devaddr );
		LMIC_setLinkCheckMode( 1 );
	}

	return joined;
}

bool AWSLoraWAN::has_joined( void )
{
	return joined;
}

void AWSLoraWAN::loop( void *dummy )	// NOSONAR
{
	while( true ) {

		delay( 1 );
		os_runloop_once();
	}
}

void AWSLoraWAN::message_sent( void )
{
	_message_sent = true;
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

void AWSLoraWAN::process_downlink( void )
{
	Serial.printf( "[LORAWAN   ] [DEBUG] Downlink payload: [" );
	for ( uint8_t i = 0; i < LMIC.dataLen; i++ )
		Serial.printf( "%02X ", LMIC.frame[ LMIC.dataBeg + i ] );
	Serial.printf( "]\n" );
}

void AWSLoraWAN::queue_message( uint8_t port, uint64_t _msg )
{
	msg = _msg;
	msg_waiting = true;
	msg_port = port;
}

void AWSLoraWAN::request_network_time( void )
{
	uint32_t	utc_time;

	Serial.printf( "[LORAWAN   ] [INFO ] Requesting network time.\n" );
	LMIC_requestNetworkTime( &AWSLoraWAN::static_request_network_time_callback, &utc_time );
}

void AWSLoraWAN::request_network_time_callback( time_t utc_time, int result )
{
  lmic_time_reference_t 	lmic_time_ref;

	if ( result != 1 ) {

		Serial.printf( "[LORAWAN   ] [INFO ] Network time request failed.\n" );
		return;
	}

	if ( LMIC_getNetworkTimeReference( &lmic_time_ref ) != 1 ) {

		Serial.printf( "[LORAWAN   ] [INFO ] Could not get network time.\n" );
		return;
	}

    utc_time = lmic_time_ref.tNetwork + 315964800;

	Serial.printf( "[LORAWAN   ] [INFO ] Network time request, got UTC timestamp: %d\n", utc_time );

    utc_time += osticks2ms( os_getTime() - lmic_time_ref.tLocal ) / 1000;;

	station.set_rtc_time( utc_time );
}

void AWSLoraWAN::restore_after_deep_sleep( void )
{
	if ( !RTC_LMIC.seqnoUp )
		return;

	LMIC = RTC_LMIC;
	LMIC.opmode = 0x800;
	joined = true;
}

void AWSLoraWAN::send( osjob_t *job )
{
	if ( debug_mode ) {

		Serial.printf( "[LORAWAN   ] [DEBUG] Queuing packet of %d bytes [", mylen );
		for( int i = 0; i < mylen; i++ )
			Serial.printf( "%02x ", mydata[ i ] );
		Serial.printf( "]\n" );
	}

	_message_sent = false;
	LMIC_setTxData2( 1, mydata.data(), mylen, 0 );

	while( !_message_sent )
		delay( 100 );
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

void AWSLoraWAN::set_joined( bool b )
{
	joined = b;
}

void AWSLoraWAN::static_request_network_time_callback( void *_utc_time, int status ) // NOSONAR
{
	const auto *utc_time = static_cast<const uint32_t*>( _utc_time );
	me->request_network_time_callback( *utc_time, status );
}

void onEvent( ev_t event )
{
	switch( event ) {

		case EV_SCAN_TIMEOUT:
		case EV_BEACON_FOUND:
		case EV_BEACON_MISSED:
		case EV_BEACON_TRACKED:
        case EV_JOINING:
		case EV_LOST_TSYNC:
		case EV_RESET:
        case EV_RXCOMPLETE:
		case EV_LINK_DEAD:
		case EV_LINK_ALIVE:
		case EV_TXSTART:
		case EV_TXCANCELED:
		case EV_RXSTART:
			break;

		case EV_JOINED:
			station.set_LoRaWAN_joined( true );
			break;

		case EV_JOIN_FAILED:
		case EV_REJOIN_FAILED:
			station.set_LoRaWAN_joined( false );
            break;

		case EV_TXCOMPLETE:

			if ( LMIC.txrxFlags & TXRX_ACK )
				Serial.println( "[LORAWAN   ] [INFO ] Received ACK.\n" );

			Serial.printf( "[LORAWAN   ] [INFO ] Packet sent\n" );

			if ( LMIC.dataLen )
				station.LoRaWAN_process_downlink();

			station.LoRaWAN_message_sent();

			break;

		case EV_JOIN_TXCOMPLETE:
			station.set_LoRaWAN_joined( false );
            break;

		default:
			Serial.printf( "[LORAWAN   ] [INFO ] Unknown event: %d\n", event  );
            break;
	}
}
