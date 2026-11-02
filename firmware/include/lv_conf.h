/**
 * LVGL 8.3.11 configuration for RPMLOOP Navigator
 * ESP32-S3, 320x240 landscape, PSRAM available
 * 
 * This file is included by LVGL via -DLV_CONF_INCLUDE_SIMPLE
 */

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/*====================
   COLOR SETTINGS
 *====================*/
#define LV_COLOR_DEPTH     16
#define LV_COLOR_16_SWAP   1       // SPI displays need byte-swap

/*====================
   MEMORY SETTINGS
 *====================*/
#define LV_MEM_CUSTOM      1       // Use stdlib malloc (routes to PSRAM)
#define LV_MEM_CUSTOM_INCLUDE <stdlib.h>
#define LV_MEM_CUSTOM_ALLOC   malloc
#define LV_MEM_CUSTOM_FREE    free
#define LV_MEM_CUSTOM_REALLOC realloc

/*====================
   HAL SETTINGS
 *====================*/
#define LV_TICK_CUSTOM     1
#define LV_TICK_CUSTOM_INCLUDE "Arduino.h"
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())

/*====================
   DISPLAY
 *====================*/
#define LV_HOR_RES_MAX     320
#define LV_VER_RES_MAX     240
#define LV_DPI_DEF         130

/*====================
   FEATURE CONFIG
 *====================*/
#define LV_USE_PERF_MONITOR    0
#define LV_USE_MEM_MONITOR     0
#define LV_USE_LOG             0
#define LV_USE_ASSERT_NULL          1
#define LV_USE_ASSERT_MALLOC        1
#define LV_USE_ASSERT_STYLE         0
#define LV_USE_ASSERT_MEM_INTEGRITY 0
#define LV_USE_ASSERT_OBJ           0

/*====================
   FONT CONFIG
 *====================*/
#define LV_FONT_MONTSERRAT_8   0
#define LV_FONT_MONTSERRAT_10  0
#define LV_FONT_MONTSERRAT_12  1
#define LV_FONT_MONTSERRAT_14  1
#define LV_FONT_MONTSERRAT_16  1
#define LV_FONT_MONTSERRAT_18  1
#define LV_FONT_MONTSERRAT_20  1
#define LV_FONT_MONTSERRAT_22  0
#define LV_FONT_MONTSERRAT_24  1
#define LV_FONT_MONTSERRAT_26  0
#define LV_FONT_MONTSERRAT_28  1
#define LV_FONT_MONTSERRAT_30  0
#define LV_FONT_MONTSERRAT_32  1
#define LV_FONT_MONTSERRAT_34  0
#define LV_FONT_MONTSERRAT_36  0
#define LV_FONT_MONTSERRAT_38  0
#define LV_FONT_MONTSERRAT_40  0
#define LV_FONT_MONTSERRAT_42  0
#define LV_FONT_MONTSERRAT_44  0
#define LV_FONT_MONTSERRAT_46  0
#define LV_FONT_MONTSERRAT_48  0
#define LV_FONT_MONTSERRAT_28_COMPRESSED 0
#define LV_FONT_MONTSERRAT_12_SUBPX     0
#define LV_FONT_DEJAVU_16_PERSIAN_HEBREW 0
#define LV_FONT_SIMSUN_16_CJK           0
#define LV_FONT_UNSCII_8                0
#define LV_FONT_UNSCII_16               0
#define LV_FONT_DEFAULT  &lv_font_montserrat_14
#define LV_FONT_FMT_TXT_LARGE 0
#define LV_USE_FONT_COMPRESSED 0
#define LV_USE_FONT_SUBPX     0

/*====================
   THEME
 *====================*/
#define LV_USE_THEME_DEFAULT  1
#define LV_THEME_DEFAULT_DARK 1    // Dark theme by default
#define LV_USE_THEME_BASIC    1

/*====================
   TEXT CONFIG
 *====================*/
#define LV_TXT_ENC LV_TXT_ENC_UTF8
#define LV_TXT_BREAK_CHARS " ,.;:-_"
#define LV_TXT_LINE_BREAK_LONG_LEN 0
#define LV_TXT_LINE_BREAK_LONG_PRE_MIN_LEN 3
#define LV_TXT_LINE_BREAK_LONG_POST_MIN_LEN 3
#define LV_TXT_COLOR_CMD "#"

/*====================
   WIDGETS
 *====================*/
#define LV_USE_ARC        1
#define LV_USE_BAR        1
#define LV_USE_BTN        1
#define LV_USE_BTNMATRIX  1
#define LV_USE_CANVAS     1
#define LV_USE_CHECKBOX   1
#define LV_USE_DROPDOWN   1
#define LV_USE_IMG        1
#define LV_USE_LABEL      1
#define LV_USE_LINE       1
#define LV_USE_ROLLER     1
#define LV_USE_SLIDER     1
#define LV_USE_SWITCH     1
#define LV_USE_TEXTAREA   0
#define LV_USE_TABLE      0

/*====================
   EXTRA WIDGETS
 *====================*/
#define LV_USE_ANIMIMG    1
#define LV_USE_CALENDAR   0
#define LV_USE_CHART      0
#define LV_USE_COLORWHEEL 0
#define LV_USE_IMGBTN     1
#define LV_USE_KEYBOARD   0
#define LV_USE_LED        1
#define LV_USE_LIST       1
#define LV_USE_MENU       0
#define LV_USE_METER      0
#define LV_USE_MSGBOX     1
#define LV_USE_SPAN       1
#define LV_USE_SPINBOX    0
#define LV_USE_SPINNER    1
#define LV_USE_TABVIEW    0
#define LV_USE_TILEVIEW   0
#define LV_USE_WIN        0

/*====================
   LAYOUTS
 *====================*/
#define LV_USE_FLEX       1
#define LV_USE_GRID       1

/*====================
   DRAW
 *====================*/
#define LV_USE_DRAW_MASKS 1

/*====================
   GPU
 *====================*/
#define LV_USE_GPU_ESP32_S3 0

/*====================
   IMAGE DECODER
 *====================*/
#define LV_IMG_CACHE_DEF_SIZE 4
#define LV_USE_SJPG          0   // We use our own JPEG decoder for tiles

/*====================
   ANIMATIONS
 *====================*/
#define LV_USE_ANIM       1

/*====================
   FILE SYSTEM
 *====================*/
#define LV_USE_FS_STDIO   0
#define LV_USE_FS_POSIX   0
#define LV_USE_FS_WIN32   0
#define LV_USE_FS_FATFS   0

/*====================
   SNAPSHOT
 *====================*/
#define LV_USE_SNAPSHOT    0

#endif /*LV_CONF_H*/
