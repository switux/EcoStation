/*
  	AWSNetwork.h

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

#pragma once
#ifndef _AWSNetwork_h
#define _AWSNetwork_h

#include <ESPping.h>
#include "lorawan.h"

class AWSNetwork {

	private:

		AWSConfig			*config;
		AWSLoraWAN			lorawan;
		aws_iface			current_pref_iface;
		aws_wifi_mode		current_wifi_mode;
		bool				debug_mode;
		SSLClient			*ssl_eth_client;
		IPAddress			wifi_ap_dns;
		IPAddress			wifi_ap_gw;
		IPAddress			wifi_ap_ip;
		IPAddress			wifi_ap_subnet;
		uint8_t				wifi_mac[6];		// NOSONAR
		IPAddress			wifi_sta_dns;
		IPAddress			wifi_sta_gw;
		IPAddress			wifi_sta_ip;
		IPAddress			wifi_sta_subnet;

		bool wifi_post_content( const char *, etl::string<128> &, const char * );

	public:

					AWSNetwork( void );
		IPAddress	cidr_to_mask( byte cidr );
		bool 		connect_to_wifi( void );
		void		empty_queue( void );
		uint8_t		*get_wifi_mac( void );
		bool		has_joined( void );
		void		initialise( AWSConfig *, bool );
		bool		initialise_wifi( void );
		bool		is_wifi_connected( void );
		void		LoRaWAN_message_sent( void );
		bool		post_content( const char *, size_t, const char * );
		void		queue_message( uint8_t, uint64_t );
		void		prepare_for_deep_sleep( int );
		void		request_lorawan_network_time( void );
		void		send_raw_data( uint8_t *, uint8_t );
		void		set_LoRaWAN_joined( bool );
		bool		start_hotspot( void );

};

#endif
