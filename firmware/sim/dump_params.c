/* Генерация docs/parameters.md из таблицы параметров прошивки: make docs */
#include <stdio.h>
#include "params.h"
#include "app.h"
static void val(char *b, int v, int dec) {
    static const int p10[4] = {1, 10, 100, 1000}; int a = v < 0 ? -v : v;
    if (dec) sprintf(b, "%s%d,%0*d", v < 0 ? "-" : "", a / p10[dec], dec, a % p10[dec]); else sprintf(b, "%d", v);
}
static void js(const char *t) { putchar('"'); for (; *t; t++) { if (*t == '"' || *t == '\\') putchar('\\'); putchar(*t); } putchar('"'); }
static int json(void)
{
    char o[64];
    printf("{\"params\":[");
    for (int i = 0; i < P_COUNT; i++) {
        const param_def_t *d = &param_defs[i];
        printf("%s{\"n\":%d,\"ru\":", i ? "," : "", i); js(d->name_ru); printf(",\"en\":"); js(d->name_en);
        printf(",\"unit\":"); js(d->unit);
        printf(",\"min\":%d,\"max\":%d,\"def\":%d,\"dec\":%d,\"stop\":%d,\"cmd\":%d,\"opts\":[", d->min, d->max, d->def, d->dec, (d->flags & PF_STOP) ? 1 : 0, (d->flags & PF_CMD) ? 1 : 0);
        if (d->opts_ru) for (int v = d->min; v <= d->max; v++) { if (v > d->min) putchar(','); js(param_opt(i, v, 0, o, sizeof o)); }
        printf("]}");
    }
    printf("],\"faults\":[");
    for (int f = 1; f < VFD_F_COUNT; f++) { if (f > 1) putchar(','); js(fault_text(f, 0)); }
    printf("],\"version\":%d}\n", FW_VERSION);
    return 0;
}

int main(int argc, char **argv)
{
    char a[32], b[32], c[32], o[64];
    (void)argv;
    if (argc > 1) return json();
    printf("# Таблица параметров PUFVFD\n\nСоздано автоматически из `firmware/stm32/lib/core/params.c` (`make docs` в `firmware/sim`).\n\n"
           "Номер параметра совпадает с адресом регистра хранения Modbus. Значение в регистре — целое число: "
           "показанное значение, умноженное на 10^(число знаков после запятой). "
           "«Стоп» — параметр изменяется только при остановленном двигателе.\n\n"
           "| № / регистр | Название на дисплее | Диапазон | По умолч. | Ед. | Стоп | Варианты |\n|---|---|---|---|---|---|---|\n");
    for (int i = 0; i < P_COUNT; i++) {
        const param_def_t *d = &param_defs[i];
        val(a, d->min, d->dec); val(b, d->max, d->dec); val(c, d->def, d->dec);
        printf("| P%02d | %s | %s … %s | %s | %s | %s | ", i, d->name_ru, a, b, c, d->unit, (d->flags & PF_STOP) ? "да" : "");
        if (d->opts_ru) for (int v = d->min; v <= d->max; v++) printf("%s%d — %s", v > d->min ? "; " : "", v, param_opt(i, v, 0, o, sizeof o));
        printf(" |\n");
    }
    printf("\n## Коды аварий\n\n| Код | Сообщение на дисплее |\n|---|---|\n");
    for (int f = 1; f < VFD_F_COUNT; f++) printf("| E%02d | %s |\n", f, fault_text(f, 0));
    return 0;
}
