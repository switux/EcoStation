/*
   aws.js

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

let dashboardRefresh;
const MLX_SENSOR = 0x01;
const TSL_SENSOR = 0x02;
const BME_SENSOR = 0x04;

const	GPS_DEVICE	= 0x40;

// 12=After OTA Update
const	RESET_REASON = [ 'Unknown', 'Power on', 'PIN reset', 'Reboot', 'Exception/Panic reset', 'Interrupt WD', 'Task WD', 'Other WD', 'Deepsleep', 'Brownout', 'SDIO reset', 'USB reset', 'JTAG reset' ];
const	PANELS = [ 'general', 'network', 'sensors', 'devices', 'dashboard' ];
const	SENSORS = [ 'bme', 'tsl', 'mlx' ];
const	WIFI_PARAMETERS = [ 'wifi_mode', 'sta_ssid', 'wifi_sta_password', 'wifi_sta_ip_mode', 'wifi_sta_ip', 'wifi_sta_gw', 'wifi_sta_dns', 'ap_ssid', 'wifi_ap_password', 'wifi_ap_ip', 'wifi_ap_gw', 'wifi_ap_dns' ];
const	CLOUD_COVERAGE = [ 'Clear', 'Cloudy', 'Overcast' ];

let sleepSetTimeout_ctrl;

function sleep(ms) {
    clearInterval(sleepSetTimeout_ctrl);
    return new Promise(resolve => sleepSetTimeout_ctrl = setTimeout(resolve, ms));
}

function config_section_active( section_name, yes_no )
{
	let _class = '';
	if ( yes_no ) {

		document.getElementById( "banner_"+section_name ).className = "top_menu selected_top_menu";
		_class='panel active_panel';
		if ( section_name == 'general' )
			_class += ' left_panel';
		document.getElementById( section_name ).className = _class;

	} else {

		document.getElementById( section_name ).className = 'panel';
		document.getElementById( "banner_"+section_name ).className = "top_menu unselected_top_menu";

	}
}

function fill_network_values( values )
{
	switch( values['wifi_mode'] ) {
		case 'AP':
			document.getElementById("AP").checked = true;
			break;
		case 'Client':
			document.getElementById("Client").checked = true;
			break;
		case 'Both':
			document.getElementById("Both").checked = true;
			break;
	}
	let parameters = [ "ap_ssid", "remote_server", "sta_ssid", "url_path", "wifi_ap_dns", "wifi_ap_gw", "wifi_ap_ip", "wifi_ap_password", "wifi_sta_dns", "wifi_sta_gw", "wifi_sta_ip", "wifi_sta_password" ];
	parameters.forEach(( parameter ) => {
		document.getElementById( parameter ).value = values[ parameter ];
	});

	show_wifi();
	document.getElementById("wifi").checked = true;


	toggle_sta_ipgw(  values['show_wifi_sta_ip_mode'] );
	switch( values['wifi_mode'] ) {
		case 0:
			document.getElementById("AP").checked = true;
			break;
		case 1:
			document.getElementById("Client").checked = true;
			break;
		case 2:
			document.getElementById("Both").checked = true;
			break;
	}

}



function fill_sensor_values( values )
{
	SENSORS.forEach((sensor) => {
		document.getElementById("has_"+sensor).checked = values['has_'+sensor];
	});
}

function fill_cloud_coverage_parameter_values( values )
{
	for( let i = 1; i<8; i++ )
		document.getElementById("k"+i).value = values['k'+i];
	document.getElementById("cc_aws_cloudy").value = values['cc_aws_cloudy'];
	document.getElementById("cc_aws_overcast").value = values['cc_aws_overcast'];
	document.getElementById("cc_aag_cloudy").value = values['cc_aag_cloudy'];
	document.getElementById("cc_aag_overcast").value = values['cc_aag_overcast'];
}

function makeRequest() {
	return new Promise((resolve,reject) => {
		clearInterval( dashboardRefresh );
		sleep( 3000 );
		let req = new XMLHttpRequest();
		req.open( "GET", "/ota_update", true );
		req.onload = function () {
			if ( req.status == 200 ) {
				resolve( req.responseText );
			} else {
				reject( new Error( req.status ));
			}
		};
		req.onerror = function () {
			reject( new Error( "Network error" ));
		};
		req.send();
	});
}

function force_ota()
{
	makeRequest()
		.then( response => {
			document.getElementById("ota_msg").textContent = response;
		})
		.catch( error => {
			document.getElementById("ota_msg").textContent = 'Error: '+error;
		});
}

function hide_wifi()
{
	WIFI_PARAMETERS.forEach( (parameter) => {
		document.getElementById("show_"+parameter).style.display = "none";
	});
	ETH_PARAMETERS.forEach( (parameter) => {
		document.getElementById("show_"+parameter).style.display = "table-row";
	});

}

function retrieve_data()
{
	toggle_panel( 'general' );

	let req = new XMLHttpRequest();
	req.onreadystatechange = function() {

		if ( this.readyState == 4 && this.status == 200 ) {

			let values = JSON.parse( req.responseText );
			document.getElementById("tzname").value = values['tzname'];
			document.getElementById("automatic_updates").checked = values['automatic_updates'];
			document.getElementById("push_freq").value = values['push_freq'];
			document.getElementById("data_push").checked = values['data_push'];
			fill_network_values( values );
			fill_sensor_values( values );
			fill_cloud_coverage_parameter_values( values );
			fill_lookout_values( values );

		}
	};
	req.open( "GET", "/get_config", true );
	req.send();

	let req2 = new XMLHttpRequest();
	req2.onreadystatechange = function() {
		if ( this.readyState == 4 && this.status == 200 ) {
			document.getElementById("root_ca").value = req2.responseText;
		}
	};
	req2.open( "GET", "/get_root_ca", true );
	req2.send();
}

function toggle_panel( panel_id )
{
	PANELS.forEach(( panel ) => {
		if ( panel == panel_id )
			config_section_active( panel, true );
		else
			config_section_active( panel, false );
	}); 
	if ( panel_id == 'dashboard' ) {

		fetch_station_data();
		dashboardRefresh = setInterval( fetch_station_data, 10000 );

	} else
		clearInterval( dashboardRefresh );
}

function toggle_wifi_mode( wifi_mode )
{
	switch( wifi_mode ) {
		case 0:
			document.getElementById("show_ap_ssid").style.display = "none";
			document.getElementById("show_wifi_ap_password").style.display = "none";
			document.getElementById("show_wifi_ap_ip_mode").style.display = "none";
			document.getElementById("show_wifi_ap_ip").style.display = "none";
			document.getElementById("show_wifi_ap_gw").style.display = "none";
			document.getElementById("show_wifi_ap_dns").style.display = "none";
			document.getElementById("show_sta_ssid").style.display = "table-row";
			document.getElementById("show_wifi_sta_password").style.display = "table-row";
			document.getElementById("show_wifi_sta_ip_mode").style.display = "table-row";
			document.getElementById("show_wifi_sta_ip").style.display = "table-row";
			document.getElementById("show_wifi_sta_gw").style.display = "table-row";
			document.getElementById("show_wifi_sta_dns").style.display = "table-row";
			break;
		case 1:
			document.getElementById("show_sta_ssid").style.display = "none";
			document.getElementById("show_wifi_sta_password").style.display = "none";
			document.getElementById("show_wifi_sta_ip_mode").style.display = "none";
			document.getElementById("show_wifi_sta_ip").style.display = "none";
			document.getElementById("show_wifi_sta_gw").style.display = "none";
			document.getElementById("show_wifi_sta_dns").style.display = "none";
			document.getElementById("show_wifi_ap_password").style.display = "table-row";
			document.getElementById("show_wifi_ap_ip").style.display = "table-row";
			document.getElementById("show_wifi_ap_gw").style.display = "table-row";
			document.getElementById("show_wifi_ap_dns").style.display = "table-row";
			break;
		case 2:
			show_wifi();
			break;
	}
}

function toggle_sta_ipgw( show )
{
	if ( show ) {
		document.getElementById("wifi_sta_fixed").checked = true;
		document.getElementById("wifi_sta_ip").removeAttribute("readonly");
		document.getElementById("wifi_sta_gw").removeAttribute("readonly");
		document.getElementById("wifi_sta_dns").removeAttribute("readonly");
		document.getElementById("wifi_sta_ip").style.display.background = "#d0d0d0";
		document.getElementById("wifi_sta_gw").style.display.background = "#d0d0d0";
		document.getElementById("wifi_sta_dns").style.display.background = "#d0d0d0";
	} else {
		document.getElementById("wifi_sta_dhcp").checked = true;
		document.getElementById("wifi_sta_ip").setAttribute("readonly","true");
		document.getElementById("wifi_sta_gw").setAttribute("readonly","true");
		document.getElementById("wifi_sta_dns").setAttribute("readonly","true");
		document.getElementById("wifi_sta_ip").style.display.background = "white";
		document.getElementById("wifi_sta_gw").style.display.background = "white";
		document.getElementById("wifi_sta_dns").style.display.background = "white";
	}
}

function reboot()
{
	let req = new XMLHttpRequest();
	req.open( "GET", "/reboot", true );
	req.send();
}

function send_config()
{
	let req = new XMLHttpRequest();
	req.open( "POST", "/set_config", true );
	req.setRequestHeader( "Content-Type", "application/json;charset=UTF-8" );
	req.send( JSON.stringify(Object.fromEntries( ( new FormData(document.querySelector('#config') )).entries())) );
}

function show_wifi()
{
	WIFI_PARAMETERS.forEach( (parameter) => {
		document.getElementById("show_"+parameter).style.display = "table-row";
	});
	ETH_PARAMETERS.forEach( (parameter) => {
		document.getElementById("show_"+parameter).style.display = "none";
	});
}

function fetch_station_data()
{
	let req = new XMLHttpRequest();
	req.onreadystatechange = function() {
		if ( this.readyState == 4 ) {
			if ( this.status == 200 ) {
				let values = JSON.parse( req.responseText );
				document.getElementById("status").textContent = "";
				update_dashboard( values );
			} else if ( this.status == 503 ) {
				document.getElementById("status").textContent = "Station not ready";
			}
		}
	};
	req.open( "GET", "/get_station_data", true );
	req.send();
}

function update_dashboard( values )
{
	document.getElementById("battery_level").textContent = values['battery_level']+'%';
	let uptime = values['uptime'];
	let days = Math.floor( uptime / ( 3600 * 24 ));
	let hours = Math.floor( ( uptime % ( 3600 * 24 )) / 3600 );
	let mins = Math.floor( ( uptime % 3600 ) / 60 );
	let secs = uptime % 60;

	document.getElementById("build_id").textContent = 'V'+values['build_id'];

	document.getElementById("uptime").textContent = days+' days '+hours+' hours '+mins+' minutes '+secs+' seconds';
	document.getElementById("reset_reason").textContent = RESET_REASON[ values['reset_reason'] ];
	document.getElementById("initial_heap").textContent = values['init_heap_size']+' bytes';
	document.getElementById("current_heap").textContent = values['current_heap_size']+' bytes';
	document.getElementById("largest_heap_block").textContent = values['largest_free_heap_block']+' bytes';

	document.getElementById("ota_board").textContent = values['ota_board'];
	document.getElementById("ota_device").textContent = values['ota_device'];
	document.getElementById("ota_config").textContent = values['ota_config'];
	let str;
	switch( values['ota_code'] ) {
		case -3:
			str = 'Update available';
			break;
		case -2:
			str = 'No update profile';
			break;
		case -1:
			str = 'No update available';
			break;
		case 0:
			str = 'Ok';
			break;
		case 1:
			str = 'Network error (HTTP)';
			break;
		case 2:
			str = 'Write error';
			break;
		case 3:
			str = 'Profile error';
			break;
		case 4:
			str = 'Profile Failed';
			break;
		default:
			str = 'Unknown';
			break;
	}
	document.getElementById("ota_status").textContent = str;

	document.getElementById("gps_fix").textContent = ( values['gps_fix'] ) ? 'Yes':'No';
	document.getElementById("gps_fix").style = ( values['gps_fix'] ) ? 'color:green':'color:red';
	document.getElementById("gps_longitude").textContent = values['gps_longitude'];
	document.getElementById("gps_latitude").textContent = values['gps_latitude'];
	document.getElementById("gps_altitude").textContent = values['gps_altitude'];

	let date = new Date( values['gps_time_sec']*1000 + values['gps_time_usec'] );
	let iso = /(\d{4}-\d{2}-\d{2})T(\d{2}:\d{2}:\d{2}.\d{3})/.exec(date.toISOString());
	document.getElementById("gps_time").textContent = iso[1]+' '+iso[2];

	update_sensor_dashboard( values );
}


function update_sensor_dashboard( values )
{
	document.getElementById("temperature").textContent = values['temperature'].toFixed(2);
	document.getElementById("dewpoint").textContent = values['dew_point'].toFixed(2);
	document.getElementById("pressure").textContent = values['pressure'].toFixed(2);
	document.getElementById("rh").textContent = values['rh'].toFixed(2);

	document.getElementById("msas").textContent = values['msas'].toFixed(2);
	document.getElementById("nelm").textContent = values['nelm'].toFixed(2);

	document.getElementById("illuminance").textContent = values['lux'].toFixed(2);
	document.getElementById("irradiance").textContent = values['irradiance'].toFixed(2);

	document.getElementById("ambient_temperature").textContent = values['ambient_temperature'].toFixed(2);
	document.getElementById("sky_temperature").textContent = values['sky_temperature'].toFixed(2);
	document.getElementById("raw_sky_temperature").textContent = values['raw_sky_temperature'].toFixed(2);
	document.getElementById("cloud_coverage").textContent = CLOUD_COVERAGE[ values['cloud_coverage'] ];

	if ( values['available_sensors'] & BME_SENSOR ) {
		document.getElementById('temp_led').querySelector('.led_color').style.backgroundColor = 'green';
		document.getElementById('pres_led').querySelector('.led_color').style.backgroundColor = 'green';
		document.getElementById('rh_led').querySelector('.led_color').style.backgroundColor = 'green';
	} else {
		document.getElementById('temp_led').querySelector('.led_color').style.backgroundColor = 'red';
		document.getElementById('pres_led').querySelector('.led_color').style.backgroundColor = 'red';
		document.getElementById('rh_led').querySelector('.led_color').style.backgroundColor = 'red';
	}

	if ( values['available_sensors'] & TSL_SENSOR ) {
		document.getElementById('tsl_led').querySelector('.led_color').style.backgroundColor = 'green';
		document.getElementById('sqm_led').querySelector('.led_color').style.backgroundColor = 'green';
	} else {
		document.getElementById('tsl_led').querySelector('.led_color').style.backgroundColor = 'red';
		document.getElementById('sqm_led').querySelector('.led_color').style.backgroundColor = 'red';
	}
	if ( values['available_sensors'] & MLX_SENSOR )
		document.getElementById('cloud_led').querySelector('.led_color').style.backgroundColor = 'green';
	else
		document.getElementById('cloud_led').querySelector('.led_color').style.backgroundColor = 'red';
}
