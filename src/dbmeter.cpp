#include <Arduino.h>
#include <Wire.h>
#include "dbmeter.h"

bool dbmeter::begin( uint8_t _int_mode, uint8_t seconds )
{
	Wire.begin();
	Wire.beginTransmission( static_cast<int>( spl_hw_t::DBM_I2C_ADDR ));
	if ( Wire.endTransmission() != 0 )
		return false;
	if ( !get_version() )
		return false;
	if ( !get_device_id() )
		return false;

	wait_ms = seconds * 1000;

	switch( int_mode ) {

		case 1:
			wait_ms += 2000;
			write_register( static_cast<uint8_t>( spl_hw_t::DBM_REG_TAVGH ), ( wait_ms >> 8 ) & 0x0F );
			write_register( static_cast<uint8_t>( spl_hw_t::DBM_REG_TAVGL ), ( wait_ms ) & 0x0F );
			break;

		case 2:
			write_register( static_cast<uint8_t>( spl_hw_t::DBM_REG_TAVGH ), 0 );
			write_register( static_cast<uint8_t>( spl_hw_t::DBM_REG_TAVGL ), 125 );
			break;

		default:
			break;
	}

	int_mode = _int_mode;
	Serial.printf( "[DBMETER   ] [INFO ] DB meter configured in mode %d, integration time=%ds\n", int_mode, seconds );
	return true;
}

uint8_t dbmeter::read( void )
{
	float	e = 0;
	int 	i;
	uint8_t	spl;

	switch( int_mode ) {

		case 0:
		case 1:
			if ( wait_ms )
				delay( wait_ms );
			read_register( static_cast<uint8_t>( spl_hw_t::DBM_REG_DECIBEL ), 1, &spl );
			break;

		case 2:
			for( i = 0; i < wait_ms / 500; i++ ) {

				delay( 500 );
				read_register( static_cast<uint8_t>( spl_hw_t::DBM_REG_DECIBEL ), 1, &spl );
				e += pow( 10, spl/10.0 );
			}
			spl = 10*log10(e/i);
			break;

		default:
			read_register( static_cast<uint8_t>( spl_hw_t::DBM_REG_DECIBEL ), 1, &spl );
			break;
	}

	return spl;
}

bool dbmeter::get_version( void )
{
	return read_register( static_cast<uint8_t>( spl_hw_t::DBM_REG_VERSION ), 1, &version );
}

bool dbmeter::get_device_id( void )
{
	return read_register( static_cast<uint8_t>( spl_hw_t::DBM_REG_ID3 ), 4, device_id.data() );
}

bool dbmeter::read_register( uint8_t reg, uint8_t sz, uint8_t *buf )
{
	uint8_t	i = 0;

	Wire.beginTransmission( static_cast<uint8_t>( spl_hw_t::DBM_I2C_ADDR ));
	Wire.write( reg );
	if ( Wire.endTransmission() != 0 )
		return false;
	Wire.requestFrom( static_cast<uint8_t>( spl_hw_t::DBM_I2C_ADDR ), sz );
	while( i < sz && Wire.available() )
		buf[ i++ ] = Wire.read();
	return true;
}

bool dbmeter::write_register( uint8_t reg, uint8_t value )
{
	Wire.beginTransmission( static_cast<uint8_t>( spl_hw_t::DBM_I2C_ADDR ));
	Wire.write( reg );
	Wire.write( value );
	return ( Wire.endTransmission() == 0 );
}
