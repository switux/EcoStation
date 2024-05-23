/*
  	dbmeter.h

	(c) 2024 F.Lesage

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
#ifndef _dbmeter_h
#define _dbmeter_h

#define	DBM_I2C_ADDR			0x48
#define DBM_REG_VERSION			0x00
#define DBM_REG_ID3				0x01
#define DBM_REG_ID2				0x02
#define DBM_REG_ID1				0x03
#define DBM_REG_ID0				0x04
#define DBM_REG_SCRATCH			0x05
#define DBM_REG_CONTROL			0x06
#define DBM_REG_TAVGH			0x07
#define DBM_REG_TAVGL			0x08
#define DBM_REG_RESET			0x09
#define DBM_REG_DECIBEL			0x0A
#define DBM_REG_MIN				0x0B
#define DBM_REG_MAX				0x0C
#define DBM_REG_THR_MIN			0x0D
#define DBM_REG_DBHISTORY_0		0x14
#define DBM_REG_DBHISTORY_99	0x77
#define DBM_REG_FREQ_64BINS_0	0x78
#define DBM_REG_FREQ_64BINS_63	0xB7
#define DBM_REG_FREQ_16BINS_0	0xB8
#define DBM_REG_FREQ_16BINS_15	0xC7

struct dbm_control_t {
	
	uint8_t	reserved			: 3;
	uint8_t interrupt_type		: 1;
	uint8_t interrupt_enabled	: 1;
	uint8_t filter_selection 	: 2;
	uint8_t	power_down			: 1;
};

class dbmeter {

	private:

		bool		i2c_ok;
		uint8_t		version;
		uint8_t	device_id[4];

		bool		control( dbm_control_t );
		bool		get_device_id( void );
		bool		get_version( void );
		bool		read_register( uint8_t, uint8_t, uint8_t * );
		bool		write_register( uint8_t, uint8_t );

	public:

		bool	begin( void );
		uint8_t read( void );
};

#endif
