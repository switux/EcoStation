#include <Arduino.h>
#include <Wire.h>
#include "dbmeter.h"

bool dbmeter::begin( void )
{
	Wire.begin();
	Wire.beginTransmission( static_cast<int>( spl_hw_t::DBM_I2C_ADDR ));
	if ( Wire.endTransmission() != 0 )
		return false;
	if ( !get_version() )
		return false;
	if ( !get_device_id() )
		return false;
	return true;
}

uint8_t dbmeter::read( void )
{
	uint8_t	spl;
	read_register( static_cast<uint8_t>( spl_hw_t::DBM_REG_DECIBEL ), 1, &spl );
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
	return ( Wire.endTransmission() == 0 );
}
