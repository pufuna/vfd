#include "modbus.h"

uint16_t mb_crc16(const uint8_t *p, int len)
{
    uint16_t crc = 0xFFFF;
    while (len--) {
        crc ^= *p++;
        for (int i = 0; i < 8; i++) crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : crc >> 1;
    }
    return crc;
}

static int finish(uint8_t *resp, int n)
{
    uint16_t crc = mb_crc16(resp, n);
    resp[n++] = (uint8_t)(crc & 0xFF); resp[n++] = (uint8_t)(crc >> 8);
    return n;
}

static int except(uint8_t *resp, uint8_t code)
{
    resp[1] |= 0x80; resp[2] = code;
    return finish(resp, 3);
}

int mb_process(uint8_t own_addr, const uint8_t *req, int len, uint8_t *resp)
{
    if (len < 8 || len > 256) return 0;
    if (req[0] != own_addr && req[0] != 0) return 0;
    if (mb_crc16(req, len - 2) != (uint16_t)(req[len - 2] | (req[len - 1] << 8))) return 0;
    int bcast = (req[0] == 0);
    uint8_t fc = req[1];
    uint16_t addr = (uint16_t)((req[2] << 8) | req[3]), n = (uint16_t)((req[4] << 8) | req[5]);
    int e, out = 0;
    resp[0] = req[0]; resp[1] = fc;
    switch (fc) {
    case 3: case 4:
        if (bcast) return 0;
        if (n < 1 || n > 100) return except(resp, 3);
        resp[2] = (uint8_t)(n * 2);
        for (int i = 0; i < n; i++) {
            uint16_t v;
            if ((e = mb_reg_read((uint16_t)(addr + i), &v)) != 0) return except(resp, (uint8_t)e);
            resp[3 + 2 * i] = (uint8_t)(v >> 8); resp[4 + 2 * i] = (uint8_t)v;
        }
        out = finish(resp, 3 + 2 * n);
        break;
    case 6:
        if ((e = mb_reg_write(addr, n)) != 0) return bcast ? 0 : except(resp, (uint8_t)e);
        for (int i = 2; i < 6; i++) resp[i] = req[i];
        out = finish(resp, 6);
        break;
    case 16:
        if (n < 1 || n > 100 || req[6] != n * 2 || len != 9 + n * 2) return bcast ? 0 : except(resp, 3);
        for (int i = 0; i < n; i++)
            if ((e = mb_reg_write((uint16_t)(addr + i), (uint16_t)((req[7 + 2 * i] << 8) | req[8 + 2 * i]))) != 0)
                return bcast ? 0 : except(resp, (uint8_t)e);
        for (int i = 2; i < 6; i++) resp[i] = req[i];
        out = finish(resp, 6);
        break;
    default:
        return bcast ? 0 : except(resp, 1);
    }
    return bcast ? 0 : out;
}
