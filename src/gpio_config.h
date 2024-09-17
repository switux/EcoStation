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
#define GPIO_DEBUG				GPIO_NUM_34

// MOSFET SWITCHES
#define GPIO_ENABLE_3_3V		GPIO_NUM_5

// Battery level
#define GPIO_BAT_ADC			GPIO_NUM_4
#define GPIO_BAT_ADC_EN			GPIO_NUM_26

// Solar panel voltage
#define GPIO_PANEL_ADC			GPIO_NUM_2

// SD Card reader
#define	GPIO_SD_MOSI			GPIO_NUM_23	// Module MOSI pin
#define	GPIO_SD_MISO			GPIO_NUM_19	// Module MISO pin
#define	GPIO_SD_SCK				GPIO_NUM_18
#define	GPIO_SD_CS				GPIO_NUM_14

// LoRa RFM95
#define	GPIO_LORA_MOSI			GPIO_NUM_23	// Module MOSI pin
#define	GPIO_LORA_MISO			GPIO_NUM_19	// Module MISO pin
#define	GPIO_LORA_SCK			GPIO_NUM_18
#define	GPIO_LORA_CS			GPIO_NUM_25
#define	GPIO_LORA_RST			GPIO_NUM_33
#define	GPIO_LORA_DIO0			GPIO_NUM_32
#define	GPIO_LORA_DIO1			GPIO_NUM_27
#define	GPIO_LORA_DIO2			GPIO_NUM_13

#endif
