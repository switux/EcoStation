#include "Wire.h"
#include "AT24C.h"

AT24C::AT24C( uint8_t addr ) : eeprom_address( addr )
{
}

bool AT24C::write_byte( uint16_t data_addr, uint8_t data )
{
	Wire.beginTransmission( eeprom_address );
	Wire.write( ( data_addr >> 8 ) & 0xFF );
	Wire.write(  data_addr & 0xFF );
	Wire.write( data );
	return ( Wire.endTransmission() == 0 );
}

bool AT24C::read_byte( uint16_t data_addr, uint8_t *data )
{
	Wire.beginTransmission( eeprom_address );
	Wire.write( ( data_addr >> 8 ) & 0xFF );
	Wire.write(  data_addr & 0xFF );
	if ( Wire.endTransmission( false ) != 0 )
		return false;
		
	Wire.requestFrom( eeprom_address, 1 );
	if ( Wire.available() ) {

		*data = Wire.read();
		return true;
	}
	return false;
}

bool AT24C::read_buffer( uint16_t data_addr, uint8_t *data, uint8_t len )
{
	uint16_t	bytes_read = 0;

	while( bytes_read < len ) {

		uint8_t offset		= data_addr % max_page_size;
		uint8_t used		= max_page_size - offset;
		uint8_t to_read		= ( used < ( len - to_read )) ? used : len - to_read;

		if ( !read_page( data_addr, data + bytes_read, to_read ))
			return false;

		bytes_read += to_read;
		data_addr += to_read;
	}
	return true; 
}

bool AT24C::read_page( uint16_t data_addr, uint8_t *data, uint8_t len )
{
	if ( len > max_page_size )
		return false;

	Wire.beginTransmission( eeprom_address );
	Wire.write( ( data_addr >> 8 ) & 0xFF );
	Wire.write(  data_addr & 0xFF );
	if ( Wire.endTransmission( false ) != 0 )
		return false;

	Wire.requestFrom( eeprom_address, len );
	if ( Wire.available() >= len ) {

		for( uint8_t i = 0; i < len; data[i++] = Wire.read() );
		return true;
	}

	return false;
}

bool AT24C::write_page( uint16_t data_addr, const uint8_t *data, uint8_t len )
{
	if ( len > max_page_size )
		return false;

	Wire.beginTransmission( eeprom_address );
	Wire.write( ( data_addr >> 8 ) & 0xFF );
	Wire.write(  data_addr & 0xFF );
	Wire.write( data, len );
	return ( Wire.endTransmission() == 0 );
}

bool AT24C::write_buffer( uint16_t data_addr, const uint8_t *data, uint8_t len )
{
	uint16_t	written = 0;

	while( written < len ) {

		uint8_t offset		= data_addr % max_page_size;
		uint8_t used		= max_page_size - offset;
		uint8_t to_write	= ( used < ( len - written )) ? used : len - written;

		if ( !write_page( data_addr, data + written, to_write ))
			return false;

		written += to_write;
		data_addr += to_write;
	}
	return true;
}
