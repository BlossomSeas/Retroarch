/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2015 - Daniel De Matteis
 *  Copyright (C) 2014-2015 - Ali Bouhlel
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "../input_joypad_driver.h"
#include "../input_autodetect.h"
#include "../../general.h"
#include "../../driver.h"
#include "../../configuration.h"
#include "../../retroarch.h"
#include "string.h"
#include "3ds.h"

#ifndef MAX_PADS
#define MAX_PADS 1
#endif

static uint64_t pad_state;
static int16_t analog_state[1][2][2];

static const char *ctr_joypad_name(unsigned pad)
{
   return "3DS Controller";
}

static void ctr_joypad_autodetect_add(unsigned autoconf_pad)
{
   settings_t *settings = config_get_ptr();
   autoconfig_params_t params = {{0}};

   strlcpy(settings->input.device_names[autoconf_pad],
         ctr_joypad_name(autoconf_pad),
         sizeof(settings->input.device_names[autoconf_pad]));

   /* TODO - implement VID/PID? */
   params.idx = autoconf_pad;
   strlcpy(params.name, ctr_joypad_name(autoconf_pad), sizeof(params.name));
   strlcpy(params.driver, ctr_joypad.ident, sizeof(params.driver));
   input_config_autoconfigure_joypad(&params);
}

static bool ctr_joypad_init(void)
{
   ctr_joypad_autodetect_add(0);

   return true;
}

static bool ctr_joypad_button(unsigned port_num, uint16_t joykey)
{
   if (port_num >= MAX_PADS)
      return false;

   return (pad_state & (1ULL << joykey));
}

static uint64_t ctr_joypad_get_buttons(unsigned port_num)
{
   return pad_state;
}

static int16_t ctr_joypad_axis(unsigned port_num, uint32_t joyaxis)
{
   int    val  = 0;
   int    axis = -1;
   bool is_neg = false;
   bool is_pos = false;

   if (joyaxis == AXIS_NONE || port_num >= MAX_PADS)
      return 0;

   if (AXIS_NEG_GET(joyaxis) < 4)
   {
      axis = AXIS_NEG_GET(joyaxis);
      is_neg = true;
   }
   else if (AXIS_POS_GET(joyaxis) < 4)
   {
      axis = AXIS_POS_GET(joyaxis);
      is_pos = true;
   }

   switch (axis)
   {
      case 0:
         val = analog_state[port_num][0][0];
         break;
      case 1:
         val = analog_state[port_num][0][1];
         break;
      case 2:
         val = analog_state[port_num][1][0];
         break;
      case 3:
         val = analog_state[port_num][1][1];
         break;
   }

   if (is_neg && val > 0)
      val = 0;
   else if (is_pos && val < 0)
      val = 0;

   return val;
}

static void ctr_joypad_poll(void)
{
   int32_t ret;
   uint32_t state_tmp;
   circlePosition state_tmp_analog;

   global_t *global          = global_get_ptr();
   uint64_t *lifecycle_state = (uint64_t*)&global->lifecycle_state;

   hidScanInput();

   state_tmp = hidKeysHeld();
   hidCircleRead(&state_tmp_analog);

   analog_state[0][0][0] = analog_state[0][0][1] =
      analog_state[0][1][0] = analog_state[0][1][1] = 0;
   pad_state = 0;
   pad_state |= (state_tmp & KEY_DLEFT) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_LEFT) : 0;
   pad_state |= (state_tmp & KEY_DDOWN) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_DOWN) : 0;
   pad_state |= (state_tmp & KEY_DRIGHT) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_RIGHT) : 0;
   pad_state |= (state_tmp & KEY_DUP) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_UP) : 0;
   pad_state |= (state_tmp & KEY_START) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_START) : 0;
   pad_state |= (state_tmp & KEY_SELECT) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_SELECT) : 0;
   pad_state |= (state_tmp & KEY_X) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_X) : 0;
   pad_state |= (state_tmp & KEY_Y) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_Y) : 0;
   pad_state |= (state_tmp & KEY_B) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_B) : 0;
   pad_state |= (state_tmp & KEY_A) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_A) : 0;
   pad_state |= (state_tmp & KEY_R) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_R) : 0;
   pad_state |= (state_tmp & KEY_L) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_L) : 0;

   analog_state[0][RETRO_DEVICE_INDEX_ANALOG_LEFT] [RETRO_DEVICE_ID_ANALOG_X] =  (state_tmp_analog.dx * 200);
   analog_state[0][RETRO_DEVICE_INDEX_ANALOG_LEFT] [RETRO_DEVICE_ID_ANALOG_Y] = -(state_tmp_analog.dy * 200);

   for (int i = 0; i < 2; i++)
      for (int j = 0; j < 2; j++)
         if (analog_state[0][i][j] == -0x8000)
            analog_state[0][i][j] = -0x7fff;

   *lifecycle_state &= ~((1ULL << RARCH_MENU_TOGGLE));

   if(state_tmp & KEY_TOUCH)
      *lifecycle_state |= (1ULL << RARCH_MENU_TOGGLE);

   /* panic button */
   if((state_tmp & KEY_START) &&
      (state_tmp & KEY_SELECT) &&
      (state_tmp & KEY_L) &&
      (state_tmp & KEY_R))
      rarch_main_command(RARCH_CMD_QUIT);

}

static bool ctr_joypad_query_pad(unsigned pad)
{
   /* FIXME */
   return pad < MAX_USERS && pad_state;
}


static void ctr_joypad_destroy(void)
{
}

rarch_joypad_driver_t ctr_joypad = {
   ctr_joypad_init,
   ctr_joypad_query_pad,
   ctr_joypad_destroy,
   ctr_joypad_button,
   ctr_joypad_get_buttons,
   ctr_joypad_axis,
   ctr_joypad_poll,
   NULL,
   ctr_joypad_name,
   "ctr",
};
