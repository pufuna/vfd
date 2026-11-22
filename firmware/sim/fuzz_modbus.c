/* Стресс-тест разбора Modbus: случайные и искажённые кадры (сборка с -fsanitize=address,undefined). make fuzz */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "modbus.h"
#include "params.h"

static uint16_t regs[0x300];
int mb_reg_read(uint16_t a, uint16_t *v) { if (a >= 0x300) return 2; *v = regs[a]; return 0; }
int mb_reg_write(uint16_t a, uint16_t v) { if (a >= 0x300) return 2; if (a < P_COUNT && params_set(a, (int16_t)v, 0)) return 3; regs[a] = v; return 0; }

int main(void)
{
    uint8_t req[300], resp[300]; long ok = 0, ex = 0, none = 0;
    srand(12345); params_defaults();
    for (long it = 0; it < 3000000; it++) {
        int len = rand() % 270;
        for (int i = 0; i < len; i++) req[i] = (uint8_t)rand();
        if (rand() % 2 && len >= 8) {                     /* правдоподобный кадр с верной CRC */
            static const uint8_t fcs[] = {3, 4, 6, 16, 1, 5, 0x2B};
            req[0] = (uint8_t)(rand() % 3); req[1] = fcs[rand() % 7];
            if (req[1] == 16 && rand() % 2) { int n = rand() % 130; req[4] = (uint8_t)(n >> 8); req[5] = (uint8_t)n; req[6] = (uint8_t)(n * 2); len = 9 + 2 * n; if (len > 256) len = 256; }
            uint16_t c = mb_crc16(req, len - 2); req[len - 2] = (uint8_t)c; req[len - 1] = (uint8_t)(c >> 8);
        }
        memset(resp, 0xAA, sizeof resp);
        int r = mb_process(1, req, len, resp);
        if (r < 0 || r > 256) { printf("ОШИБКА: длина ответа %d\n", r); return 1; }
        if (r) {
            if (mb_crc16(resp, r - 2) != (uint16_t)(resp[r - 2] | resp[r - 1] << 8)) { printf("ОШИБКА: CRC ответа\n"); return 1; }
            if (resp[1] & 0x80) ex++; else ok++;
        } else none++;
    }
    for (int i = 0; i < P_COUNT; i++)
        if ((int16_t)regs[i] && ((int16_t)regs[i] < param_defs[i].min || (int16_t)regs[i] > param_defs[i].max)) { printf("ОШИБКА: P%02d вне диапазона\n", i); return 1; }
    printf("ИТОГ: 3 млн кадров, ответов %ld, исключений %ld, без ответа %ld — нарушений нет\n", ok, ex, none);
    return 0;
}
