# PUFVFD. Готовые файлы прошивки

Собраны из исходников этого репозитория (`firmware/stm32`, `firmware/esp32`). Если исходники менялись, пересоберите: `pio run` в соответствующей папке.

| Файл | Что это | Куда записывать |
|---|---|---|
| `pufvfd_stm32.bin` | прошивка контроллера STM32F411 (Black Pill) | с адреса `0x08000000` |
| `pufvfd_stm32.hex` | то же в формате Intel HEX (адрес внутри файла) | — |
| `pufvfd_stm32.elf` | то же с отладочной информацией (для ST-Link/отладчика) | — |
| `pufvfd_esp32_full.bin` | полный образ модуля ESP32 (загрузчик, таблица разделов, программа), флеш 4 МБ | с адреса `0x0` |
| `pufvfd_esp32_app.bin` | только программа ESP32 (для обновления уже прошитого модуля) | с адреса `0x10000` |

## Контроллер STM32F411 без PlatformIO

**Через USB (DFU), программатор не нужен.** Удерживая кнопку BOOT0, нажмите и отпустите NRST, отпустите BOOT0, затем:

```bash
dfu-util -a 0 -s 0x08000000:leave -D pufvfd_stm32.bin
```

На Windows то же делается программой STM32CubeProgrammer: подключение «USB», файл `pufvfd_stm32.hex`, кнопка «Download».

**Через ST-Link:**

```bash
st-flash --reset write pufvfd_stm32.bin 0x08000000
```

## Модуль ESP32

Нужен esptool (`pip install esptool`). Порт — тот, под которым определился модуль (`COM3` на Windows, `/dev/ttyUSB0` на Linux).

```bash
esptool.py --chip esp32 --port /dev/ttyUSB0 --baud 460800 write_flash 0x0 pufvfd_esp32_full.bin
```

На Windows можно использовать программу Flash Download Tools (Espressif): файл `pufvfd_esp32_full.bin`, адрес `0x0`, SPI MODE DIO.

После прошивки модуль создаёт точку доступа Wi-Fi «PUFVFD» и устройство Bluetooth «PUFVFD» (подробности — в руководстве по эксплуатации, `docs/03_user_manual.md`).
