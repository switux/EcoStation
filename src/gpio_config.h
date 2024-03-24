/*	
  	gpio_config.h
  	
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

#pragma once
#ifndef _GPIO_CONFIG_H
#define _GPIO_CONFIG_H

#ifndef _defaults_H
#include "defaults.h"
#endif

// Misc
#define GPIO_DEBUG		GPIO_NUM_34

// MOSFET SWITCHES
#define GPIO_ENABLE_3_3V		GPIO_NUM_33
#define GPIO_ENABLE_5V			GPIO_NUM_26
#define GPIO_ENABLE_LTE			GPIO_NUM_32

// Battery level
#define GPIO_BAT_ADC			GPIO_NUM_13
#define GPIO_BAT_ADC_EN			GPIO_NUM_25

// SD Card reader
#define	GPIO_SPI_MOSI			GPIO_NUM_23
#define	GPIO_SPI_MISO			GPIO_NUM_19
#define	GPIO_SPI_SCK			GPIO_NUM_18
#define	GPIO_SPI_CS				GPIO_NUM_5
#define	GPIO_SPI_INT			GPIO_NUM_4

// LTE
#define	LTE_RX					GPIO_NUM_17
#define	LTE_TX					GPIO_NUM_16

#endif
