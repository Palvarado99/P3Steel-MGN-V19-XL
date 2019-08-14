/**
 * Marlin 3D Printer Firmware
 * Copyright (C) 2019 MarlinFirmware [https://github.com/MarlinFirmware/Marlin]
 *
 * Based on Sprinter and grbl.
 * Copyright (C) 2011 Camiel Gubbels / Erik van der Zalm
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

/**
 * Based on u8g_com_st7920_hw_spi.c
 *
 * Universal 8bit Graphics Library
 *
 * Copyright (c) 2011, olikraus@gmail.com
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice, this list
 *    of conditions and the following disclaimer.
 *
 *  * Redistributions in binary form must reproduce the above copyright notice, this
 *    list of conditions and the following disclaimer in the documentation and/or other
 *    materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef ARDUINO_ARCH_SAM

#include "../../inc/MarlinConfigPre.h"

#if HAS_GRAPHICAL_LCD

#include "../shared/Marduino.h"
#include "../shared/Delay.h"

#include <U8glib.h>

void u8g_SetPIOutput_DUE(u8g_t *u8g, uint8_t pin_index) {
  PIO_Configure(g_APinDescription[u8g->pin_list[pin_index]].pPort, PIO_OUTPUT_1,
    g_APinDescription[u8g->pin_list[pin_index]].ulPin, g_APinDescription[u8g->pin_list[pin_index]].ulPinConfiguration);  // OUTPUT
}

void u8g_SetPILevel_DUE(u8g_t *u8g, uint8_t pin_index, uint8_t level) {
  volatile Pio* port = g_APinDescription[u8g->pin_list[pin_index]].pPort;
  uint32_t mask = g_APinDescription[u8g->pin_list[pin_index]].ulPin;
  if (level) port->PIO_SODR = mask; else port->PIO_CODR = mask;
}

Pio *SCK_pPio, *MOSI_pPio;
uint32_t SCK_dwMask, MOSI_dwMask;

static void spiSend_sw_DUE(uint8_t val) { // 800KHz
  for (uint8_t i = 0; i < 8; i++) {
    if (val & 0x80)
      MOSI_pPio->PIO_SODR = MOSI_dwMask;
    else
      MOSI_pPio->PIO_CODR = MOSI_dwMask;
    DELAY_NS(48);
    SCK_pPio->PIO_SODR = SCK_dwMask;
    DELAY_NS(905); // 762 dead, 810 garbage, 858/0 900kHz, 905/1 825k, 953/1 800k, 1000/2 725KHz
    val <<= 1;
    SCK_pPio->PIO_CODR = SCK_dwMask;
  }
}

static uint8_t rs_last_state = 255;

static void u8g_com_DUE_st7920_write_byte_sw_spi(uint8_t rs, uint8_t val) {
  if (rs != rs_last_state) {  // time to send a command/data byte
    rs_last_state = rs;
    spiSend_sw_DUE(rs ? 0x0FA : 0x0F8); // Command or Data
    DELAY_US(40); // give the controller some time to process the data: 20 is bad, 30 is OK, 40 is safe
  }
  spiSend_sw_DUE(val & 0xF0);
  spiSend_sw_DUE(val << 4);
}

uint8_t u8g_com_HAL_DUE_ST7920_sw_spi_fn(u8g_t *u8g, uint8_t msg, uint8_t arg_val, void *arg_ptr) {
  switch (msg) {
    case U8G_COM_MSG_INIT:
      SCK_pPio = g_APinDescription[u8g->pin_list[U8G_PI_SCK]].pPort;
      SCK_dwMask = g_APinDescription[u8g->pin_list[U8G_PI_SCK]].ulPin;
      MOSI_pPio = g_APinDescription[u8g->pin_list[U8G_PI_MOSI]].pPort;
      MOSI_dwMask = g_APinDescription[u8g->pin_list[U8G_PI_MOSI]].ulPin;

      u8g_SetPILevel_DUE(u8g, U8G_PI_CS, 0);
      u8g_SetPIOutput_DUE(u8g, U8G_PI_CS);
      u8g_SetPILevel_DUE(u8g, U8G_PI_SCK, 0);
      u8g_SetPIOutput_DUE(u8g, U8G_PI_SCK);
      u8g_SetPILevel_DUE(u8g, U8G_PI_MOSI, 0);
      u8g_SetPIOutput_DUE(u8g, U8G_PI_MOSI);

      SCK_pPio->PIO_CODR = SCK_dwMask;   //SCK low - needed at power up but not after reset
      MOSI_pPio->PIO_CODR = MOSI_dwMask; //MOSI low - needed at power up but not after reset

      u8g_Delay(5);

      u8g->pin_list[U8G_PI_A0_STATE] = 0;       /* inital RS state: command mode */
      break;

    case U8G_COM_MSG_STOP:
      break;

    case U8G_COM_MSG_RESET:
       if (U8G_PIN_NONE != u8g->pin_list[U8G_PI_RESET]) u8g_SetPILevel_DUE(u8g, U8G_PI_RESET, arg_val);
      break;

    case U8G_COM_MSG_ADDRESS:                     /* define cmd (arg_val = 0) or data mode (arg_val = 1) */
      u8g->pin_list[U8G_PI_A0_STATE] = arg_val;
      break;

    case U8G_COM_MSG_CHIP_SELECT:
      if (U8G_PIN_NONE != u8g->pin_list[U8G_PI_CS])
        u8g_SetPILevel_DUE(u8g, U8G_PI_CS, arg_val);  //note: the st7920 has an active high chip select
      break;

    case U8G_COM_MSG_WRITE_BYTE:

      u8g_com_DUE_st7920_write_byte_sw_spi(u8g->pin_list[U8G_PI_A0_STATE], arg_val);
      break;

    case U8G_COM_MSG_WRITE_SEQ: {
        uint8_t *ptr = (uint8_t*) arg_ptr;
        while (arg_val > 0) {
          u8g_com_DUE_st7920_write_byte_sw_spi(u8g->pin_list[U8G_PI_A0_STATE], *ptr++);
          arg_val--;
        }
      }
      break;

      case U8G_COM_MSG_WRITE_SEQ_P: {
        uint8_t *ptr = (uint8_t*) arg_ptr;
        while (arg_val > 0) {
          u8g_com_DUE_st7920_write_byte_sw_spi(u8g->pin_list[U8G_PI_A0_STATE], *ptr++);
          arg_val--;
        }
      }
      break;
  }
  return 1;
}

#if ENABLED(LIGHTWEIGHT_UI)
  #include "../../lcd/ultralcd.h"
  #include "../shared/HAL_ST7920.h"

  #define ST7920_CS_PIN LCD_PINS_RS

  #if DOGM_SPI_DELAY_US > 0
    #define U8G_DELAY() DELAY_US(DOGM_SPI_DELAY_US)
  #else
    #define U8G_DELAY() DELAY_US(10)
  #endif

  void ST7920_cs() {
    WRITE(ST7920_CS_PIN, HIGH);
    U8G_DELAY();
  }

  void ST7920_ncs() {
    WRITE(ST7920_CS_PIN, LOW);
  }

  void ST7920_set_cmd() {
    spiSend_sw_DUE(0xF8);
    DELAY_US(40);
  }

  void ST7920_set_dat() {
    spiSend_sw_DUE(0xFA);
    DELAY_US(40);
  }

  void ST7920_write_byte(const uint8_t val) {
    spiSend_sw_DUE(val & 0xF0);
    spiSend_sw_DUE(val << 4);
  }
#endif // LIGHTWEIGHT_UI

#endif // HAS_GRAPHICAL_LCD
#endif // ARDUINO_ARCH_SAM
