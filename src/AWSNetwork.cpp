/*
	AWSNetwork.cpp

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

#include <SSLClient.h>
#include <ESPAsyncWebServer.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <HTTPClient.h>
#include "Embedded_Template_Library.h"
#include "etl/string.h"

#include "common.h"
#include "config_manager.h"
#include "gpio_config.h"
#include "AWSNetwork.h"
#include "EcoStation.h"

extern EcoStation station;

AWSNetwork::AWSNetwork( void )
{
	current_wifi_mode = aws_wifi_mode::sta;
	current_pref_iface = aws_iface::wifi_sta;
	memset( wifi_mac, 0, 6 );
}

IPAddress AWSNetwork::cidr_to_mask( byte cidr )
{
	uint32_t		subnet;

	if ( cidr )
		subnet = htonl( ~(( 1 << ( 32 - cidr )) -1 ) );
	else
		subnet = htonl( 0 );

	return IPAddress( subnet );
}

bool AWSNetwork::connect_to_wifi()
{
	uint8_t		remaining_attempts	= 5;
	char		*ip = nullptr;
	char		*cidr = nullptr;
	const char	*ssid		= config->get_parameter<const char *>( "wifi_sta_ssid" );
	const char	*password	= config->get_parameter<const char *>( "wifi_sta_password" );
	char		*dummy;

	if (( WiFi.status () == WL_CONNECTED ) && !strcmp( ssid, WiFi.SSID().c_str() )) {

		if ( debug_mode )
			Serial.printf( "[NETWORK   ] [DEBUG] Already connected to SSID [%s].\n", ssid );
		return true;
	}

	WiFi.mode( WIFI_STA );
	Serial.printf( "[NETWORK   ] [INFO ] Attempting to connect to SSID [%s] ", ssid );

	if ( static_cast<aws_ip_mode>(config->get_parameter<int>( "wifi_sta_ip_mode" )) == aws_ip_mode::fixed ) {

		if ( etl::string_view( config->get_parameter<const char *>( "wifi_sta_ip" )).size() ) {

			etl::string<32> buf;
			strlcpy( buf.data(), config->get_parameter<const char *>( "wifi_sta_ip" ), buf.capacity() );
		 	ip = strtok_r( buf.data(), "/", &dummy );
		}
		if ( ip )
			cidr = strtok_r( nullptr, "/", &dummy );

  		wifi_sta_ip.fromString( ip );
  		wifi_sta_gw.fromString( config->get_parameter<const char *>( "wifi_sta_gw" ));
  		wifi_sta_dns.fromString( config->get_parameter<const char *>( "wifi_sta_dns" ));
		// flawfinder: ignore
  		wifi_sta_subnet = cidr_to_mask( static_cast<unsigned int>( atoi( cidr ) ));
		WiFi.config( wifi_sta_ip, wifi_sta_gw, wifi_sta_subnet, wifi_sta_dns );

	}
	WiFi.begin( ssid , password );

	while (( WiFi.status() != WL_CONNECTED ) && ( --remaining_attempts > 0 )) {	// NOSONAR

    Serial.print( "." );
		delay( 500 );
	}

	if ( WiFi.status () == WL_CONNECTED ) {

		wifi_sta_ip = WiFi.localIP();
		wifi_sta_subnet = WiFi.subnetMask();
		wifi_sta_gw = WiFi.gatewayIP();
		wifi_sta_dns = WiFi.dnsIP();
		Serial.printf( " OK. Using IP [%s]\n", WiFi.localIP().toString().c_str() );
		return true;
	}

	Serial.printf( "NOK.\n" );

	return false;
}

void AWSNetwork::empty_queue( void )
{
	lorawan.empty_queue();
}

uint8_t *AWSNetwork::get_wifi_mac( void )
{
	return wifi_mac;
}

bool AWSNetwork::has_joined( void )
{
	return lorawan.has_joined();
}

void AWSNetwork::initialise( AWSConfig *_config, bool _debug_mode )
{
	debug_mode = _debug_mode;
	config = _config;

	esp_read_mac( wifi_mac, ESP_MAC_WIFI_STA );

	UNSELECT_SPI_DEVICES();

	if ( config->get_has_device( aws_device_t::LORAWAN_DEVICE ))
		lorawan.begin( _config->get_lora_deveui(), _config->get_lora_appkey(), _debug_mode );

	initialise_wifi();
}


bool AWSNetwork::initialise_wifi( void )
{
	switch ( static_cast<aws_wifi_mode>( config->get_parameter<int>( "wifi_mode" )) ) {

		case aws_wifi_mode::ap:
			if ( debug_mode )
				Serial.printf( "[NETWORK   ] [DEBUG] Booting in AP mode.\n" );
			return start_hotspot();

		case aws_wifi_mode::both:
			if ( debug_mode )
				Serial.printf( "[NETWORK   ] [DEBUG] Booting in AP+STA mode.\n" );
			WiFi.mode( WIFI_AP_STA );
			return ( start_hotspot() && connect_to_wifi() );

		case aws_wifi_mode::sta:
			if ( debug_mode )
				Serial.printf( "[NETWORK   ] [DEBUG] Booting in STA mode.\n" );
			return connect_to_wifi();

		default:
			Serial.printf( "[NETWORK   ] [ERROR] Unknown wifi mode [%d]\n",  config->get_parameter<int>( "wifi_mode" ) );
	}
	return false;
}

bool AWSNetwork::is_wifi_connected( void )
{
	const char  *ssid       = config->get_parameter<const char *>( "wifi_sta_ssid" );

    return (( WiFi.status () == WL_CONNECTED ) && !strcmp( ssid, WiFi.SSID().c_str() ));
}

void AWSNetwork::LoRaWAN_message_sent( void )
{
	lorawan.message_sent();
}

bool AWSNetwork::post_content( const char *endpoint, size_t endpoint_len, const char *jsonString )
{
	uint8_t				fe_len;
	etl::string<128>	final_endpoint;
	int 				l;
	const char			*remote_server = config->get_parameter<const char *>( "remote_server" );
	etl::string<128>	url;
	uint8_t 			url_len = 8 + etl::string_view( config->get_parameter<const char *>( "remote_server" )).size() + 1 + etl::string_view( config->get_parameter<const char *>( "url_path" )).size() + 1;
	const char			*url_path = config->get_parameter<const char *>( "url_path" );

	if ( url_len > url.capacity() ) {

		etl::string<64> tmp;
		snprintf( tmp.data(), tmp.capacity(), "Content POST URL is too long (%d > %d)", url_len, url.capacity() );
		station.send_alarm( "[STATION] BUG", tmp.data() );
		Serial.printf( "[NETWORK   ] [BUG  ] URL is too long (%d > %d)\n", url_len, url.capacity() );
		return false;
	}

	url.assign( "https://" );
	url.append( remote_server );
	url.append( "/" );
	url.append( url_path );
	url.append( "/" );

	fe_len = l + endpoint_len + 2;
	if ( fe_len > final_endpoint.capacity() ) {

		etl::string<64> tmp;
		snprintf( tmp.data(), tmp.capacity(), "Content POST final endpoint is too long (%d > %d)", fe_len, final_endpoint.capacity() );
		station.send_alarm( "[STATION] BUG", tmp.data() );
		Serial.printf( "[NETWORK   ] [ERROR] Final endpoint is too long (%d > %d)\n", fe_len, final_endpoint.capacity() );
		return false;
	}

	final_endpoint.assign( url );
	final_endpoint.append( endpoint );

	if ( debug_mode )
		Serial.printf( "[NETWORK   ] [DEBUG] Connecting to server [%s:443] ...", remote_server );

	return wifi_post_content( remote_server, final_endpoint, jsonString );
}

void AWSNetwork::prepare_for_deep_sleep( int deep_sleep_secs )
{
	lorawan.prepare_for_deep_sleep( deep_sleep_secs );
}

void AWSNetwork::queue_message( uint64_t msg )
{
	lorawan.queue_message( msg );
}

void AWSNetwork::send_raw_data( uint8_t *buffer, uint8_t len )
{
	UNSELECT_SPI_DEVICES();
	if ( lorawan.join() )
		lorawan.send_data( buffer, len );
}

void AWSNetwork::set_LoRaWAN_joined( bool b )
{
	lorawan.set_joined( b );
}

bool AWSNetwork::start_hotspot( void )
{
	const char	*ssid		= config->get_parameter<const char *>( "wifi_ap_ssid" );
	const char	*password	= config->get_parameter<const char *>( "wifi_ap_password" );

	char *ip = nullptr;
	char *cidr = nullptr;
	char *dummy;

	if ( debug_mode )
		Serial.printf( "[NETWORK   ] [DEBUG] Trying to start AP on SSID [%s] with password [%s]\n", ssid, password );

	WiFi.mode( WIFI_AP );

	if ( WiFi.softAP( ssid, password )) {

		if ( etl::string_view( config->get_parameter<const char *>( "wifi_ap_ip" )).size() ) {

			etl::string<32> buf;
			strlcpy( buf.data(), config->get_parameter<const char *>( "wifi_ap_ip" ), buf.capacity() );
		 	ip = strtok_r( buf.data(), "/", &dummy );
		}
		if ( ip )
			cidr = strtok_r( nullptr, "/", &dummy );

		wifi_ap_ip.fromString( ip );
		wifi_ap_gw.fromString( config->get_parameter<const char *>( "wifi_ap_gw" ));
		// flawfinder: ignore
		wifi_ap_subnet = cidr_to_mask( static_cast<unsigned int>( atoi( cidr )) );

		WiFi.softAPConfig( wifi_ap_ip, wifi_ap_gw, wifi_ap_subnet );
		Serial.printf( "[NETWORK   ] [INFO ] Started hotspot on SSID [%s/%s] and configuration server @ IP=%s/%s\n", ssid, password, ip, cidr );
		return true;
	}
	return false;
}

bool AWSNetwork::wifi_post_content( const char *remote_server, etl::string<128> &final_endpoint, const char *jsonString )
{
	HTTPClient 			http;
	uint8_t				http_code;
	WiFiClientSecure	wifi_client;

	wifi_client.setCACert( config->get_root_ca().data() );
	if ( !wifi_client.connect( remote_server, 443 )) {

		if ( debug_mode )
			Serial.printf( "NOK.\n" );
		return false;
	}

	if ( debug_mode )
		Serial.printf( "OK.\n" );

	http.begin( wifi_client, final_endpoint.data() );
	http.setFollowRedirects( HTTPC_FORCE_FOLLOW_REDIRECTS );
	http.addHeader( "Content-Type", "application/json" );
	http_code = http.POST( jsonString );
	http.end();
	wifi_client.stop();

	if ( http_code == 200 )
		return true;
		
	if ( debug_mode )
		Serial.printf( "[NETWORK   ] [DEBUG] HTTP response: %d\n", http_code );

	return false;
}
