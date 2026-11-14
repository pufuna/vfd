#include "lcd.h"
#include "hw.h"
#include <string.h>

#define BL 0x08
#define EN 0x04
#define RS 0x01

/* Буквы, совпадающие по начертанию с латинскими/цифрами (0 — нужен загружаемый знак). Индекс: U+0410.. */
static const char look[32] = {'A', 0, 'B', 0, 0, 'E', 0, '3', 0, 0, 'K', 0, 'M', 'H', 'O', 0,
                              'P', 'C', 'T', 'Y', 0, 'X', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
#define G(a,b,c,d,e,f,g) {0x##a,0x##b,0x##c,0x##d,0x##e,0x##f,0x##g,0}
static const uint8_t glyphs[32][8] = {
    [1]  = G(1F,10,10,1E,11,11,1E), /* Б */  [3]  = G(1F,10,10,10,10,10,10), /* Г */
    [4]  = G(06,0A,0A,0A,0A,1F,11), /* Д */  [6]  = G(15,15,0E,04,0E,15,15), /* Ж */
    [8]  = G(11,11,13,15,19,11,11), /* И */  [9]  = G(0A,04,11,13,15,19,11), /* Й */
    [11] = G(07,09,09,09,09,09,11), /* Л */  [15] = G(1F,11,11,11,11,11,11), /* П */
    [20] = G(04,0E,15,15,0E,04,04), /* Ф */  [22] = G(12,12,12,12,12,1F,01), /* Ц */
    [23] = G(11,11,11,0F,01,01,01), /* Ч */  [24] = G(15,15,15,15,15,15,1F), /* Ш */
    [25] = G(15,15,15,15,15,1F,01), /* Щ */  [26] = G(18,08,08,0E,09,09,0E), /* Ъ */
    [27] = G(11,11,11,19,15,15,19), /* Ы */  [28] = G(10,10,10,1E,11,11,1E), /* Ь */
    [29] = G(0E,11,01,07,01,11,0E), /* Э */  [30] = G(12,15,15,1D,15,15,12), /* Ю */
    [31] = G(0F,11,11,0F,05,09,11), /* Я */
};
const uint8_t *lcd_glyph(int index) { return glyphs[index & 31]; }

static uint8_t want[LCD_ROWS][LCD_COLS], have[LCD_ROWS][LCD_COLS];
static uint8_t slot_ch[8];      /* индекс буквы (1..31) в слоте CGRAM, 0 — свободен */
static uint8_t slot_dirty, slot_used;
static int scan;

static void wr4(uint8_t nib, uint8_t rs)
{
    uint8_t b[2] = {(uint8_t)(nib | rs | BL | EN), (uint8_t)(nib | rs | BL)};
    hw_i2c_write(LCD_I2C_ADDR, b, 2);
}
static void wr8(uint8_t v, uint8_t rs)
{
    uint8_t b[4] = {(uint8_t)((v & 0xF0) | rs | BL | EN), (uint8_t)((v & 0xF0) | rs | BL),
                    (uint8_t)((v << 4) | rs | BL | EN), (uint8_t)((v << 4) | rs | BL)};
    hw_i2c_write(LCD_I2C_ADDR, b, 4);
}

void lcd_init(void)
{
    hw_delay_ms(50);
    wr4(0x30, 0); hw_delay_ms(5); wr4(0x30, 0); hw_delay_ms(1); wr4(0x30, 0); hw_delay_ms(1);
    wr4(0x20, 0); hw_delay_ms(1);
    wr8(0x28, 0); wr8(0x08, 0); wr8(0x01, 0); hw_delay_ms(3); wr8(0x06, 0); wr8(0x0C, 0);
    memset(want, ' ', sizeof want); memset(have, ' ', sizeof have);
    memset(slot_ch, 0, sizeof slot_ch); slot_dirty = 0;
}

void lcd_begin_frame(void) { memset(want, ' ', sizeof want); slot_used = 0; }

/* декодирование одного символа UTF-8: ASCII или кириллица U+0400..U+047F */
static const char *next(const char *s, int *ascii, int *cyr)
{
    uint8_t c = (uint8_t)*s;
    *ascii = 0; *cyr = -1;
    if (c < 0x80) { *ascii = c; return s + 1; }
    if ((c == 0xD0 || c == 0xD1) && s[1]) {
        int u = ((c & 0x1F) << 6) | ((uint8_t)s[1] & 0x3F);
        if (u == 0x401) *ascii = 'E';                       /* Ё */
        else if (u >= 0x410 && u <= 0x42F) { if (look[u - 0x410]) *ascii = look[u - 0x410]; else *cyr = u - 0x410; }
        else *ascii = '?';
        return s + 2;
    }
    *ascii = '?';
    return s + 1;
}

int lcd_count_special(const char *s, uint32_t *set)
{
    int a, c, bad = 0;
    while (*s) { s = next(s, &a, &c); if (c >= 0) *set |= 1u << c; else if (a == '?') bad = 1; }
    int n = 0; for (int i = 0; i < 32; i++) if (*set & (1u << i)) n++;
    return bad ? -1 : n;
}

static uint8_t slot_for(int cyr)
{
    int i;
    for (i = 0; i < 8; i++) if (slot_ch[i] == cyr) { slot_used |= 1 << i; return (uint8_t)(8 + i); }
    for (i = 0; i < 8; i++) if (!(slot_used & (1 << i))) {
        slot_ch[i] = (uint8_t)cyr; slot_used |= 1 << i; slot_dirty |= 1 << i;
        return (uint8_t)(8 + i);
    }
    return '?';
}

void lcd_print(int row, int col, const char *s)
{
    int a, c;
    while (*s && col < LCD_COLS) {
        s = next(s, &a, &c);
        want[row][col++] = c >= 0 ? slot_for(c) : (uint8_t)a;
    }
}

void lcd_task(void)
{
    static const uint8_t row_addr[4] = {0x00, 0x40, 0x14, 0x54};
    if (slot_dirty) {
        for (int i = 0; i < 8; i++) if (slot_dirty & (1 << i)) {
            wr8((uint8_t)(0x40 | (i << 3)), 0);
            for (int r = 0; r < 8; r++) wr8(glyphs[slot_ch[i]][r], RS);
            slot_dirty &= (uint8_t)~(1 << i);
            return;
        }
    }
    for (int n = 0; n < LCD_ROWS * LCD_COLS; n++) {
        int r = scan / LCD_COLS, c = scan % LCD_COLS;
        scan = (scan + 1) % (LCD_ROWS * LCD_COLS);
        if (want[r][c] != have[r][c]) {
            wr8((uint8_t)(0x80 | (row_addr[r] + c)), 0);
            wr8(want[r][c], RS);
            have[r][c] = want[r][c];
            return;
        }
    }
}
