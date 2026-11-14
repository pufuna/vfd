/* ЖКИ 20x4 (HD44780 через PCF8574) с русскими буквами (динамическая загрузка до 8 знаков в CGRAM) */
#ifndef LCD_H
#define LCD_H
#include <stdint.h>
#define LCD_COLS 20
#define LCD_ROWS 4
#define LCD_I2C_ADDR 0x27      /* PCF8574T; для PCF8574AT — 0x3F */
void lcd_init(void);
void lcd_begin_frame(void);                 /* начать формирование нового кадра */
void lcd_print(int row, int col, const char *utf8);
void lcd_task(void);                        /* вывод одного изменившегося знакоместа за вызов */
/* для тестов: число специальных (загружаемых) знаков в строке UTF-8; -1 — неподдерживаемый символ */
int  lcd_count_special(const char *utf8, uint32_t *set);
const uint8_t *lcd_glyph(int index);        /* index = код буквы - 0x410 */
#endif
