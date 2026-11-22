/*
 * PUFVFD — аппаратный слой для STM32F411CEU6 (Black Pill), без HAL.
 *
 * Назначение выводов:
 *   PA8/PA9/PA10   TIM1_CH1..3   ШИМ верхних ключей U, V, W   (через оптроны 6N137)
 *   PB13/PB14/PB15 TIM1_CH1N..3N ШИМ нижних ключей U, V, W
 *   PB12           TIM1_BKIN     авария силового модуля (активный 0)
 *   PA0, PA1       ADC1_IN0/1    токи фаз U, W (ACS712-30A через делитель 2/3)
 *   PA4, PA5       ADC1_IN4/5    напряжение шины: выходы VOUT+ / VOUT- изолирующего усилителя (через делители 1/2)
 *   PA6            ADC1_IN6      термистор радиатора NTC 10 кОм (B=3950), подтяжка 10 кОм к 3,3 В
 *   PA7            ADC1_IN7      аналоговый вход задания 0..3,3 В (после делителя)
 *   PB6/PB7        USART1 TX/RX  RS-485 (Modbus RTU), PB5 — DE/RE
 *   PA2/PA3        USART2 TX/RX  модуль ESP32 (Wi-Fi/Bluetooth)
 *   PB8/PB9        I2C1 SCL/SDA  ЖКИ (PCF8574, 0x27) и кнопки (PCF8574, 0x20)
 *   PB0, PB1, PB10 дискретные входы DI1 (вперёд), DI2 (назад), DI3 (активный 0)
 *   PB2            реле предзаряда;  PA15 — вентилятор;  PB3 — выходное реле;  PC13 — светодиод
 */
#include "stm32f4xx.h"
#include "hw.h"
#include "app.h"
#include <math.h>

#define F_CLK       96000000u
#define KEYS_I2C_ADDR 0x20            /* PCF8574T; для PCF8574AT — 0x38 */
#define K_CUR       (3.3f / 4095.0f / 0.044f)             /* А/ед.: ACS712-30A 66 мВ/А, делитель 2/3 */
#define K_VDC       (3.3f / 4095.0f / (8.0f * 0.5f * 390.0f / 990390.0f))  /* В/ед.: делитель 990к/390 Ом, усиление 8, делитель 1/2 */
#define OFFS_NOM    2068.0f
#define OFFS_TOL    300.0f
#define FLASH_STORE_ADDR   0x08060000u   /* сектор 7 (128 кБ) */
#define FLASH_STORE_SECTOR 7u
#define FLASH_STORE_SIZE   0x20000u
#define REC_SIZE    256u

static volatile uint32_t ms_ticks;
static uint16_t pwm_arr = 6000;
static float offs_u = OFFS_NOM, offs_w = OFFS_NOM, cal_i = 1.0f, cal_u = 1.0f;
static volatile uint8_t ipm_fault, sensor_fault;
static volatile uint16_t adc_ntc = 2048, adc_ai;

typedef struct { USART_TypeDef *u; volatile uint8_t rx[260]; volatile uint16_t rx_n; volatile uint32_t rx_t;
                 uint8_t tx[260]; volatile uint16_t tx_n, tx_i; uint16_t gap_ms; } port_t;
static port_t ports[2];

uint32_t hw_millis(void) { return ms_ticks; }
void hw_delay_ms(uint32_t ms) { uint32_t t = ms_ticks; while ((uint32_t)(ms_ticks - t) < ms + 1) IWDG->KR = 0xAAAA; }

void SysTick_Handler(void)
{
    static uint8_t ph;
    ms_ticks++;
    /* медленные каналы АЦП (термистор, аналоговый вход): поочерёдно, программный запуск */
    if (ADC1->SR & ADC_SR_EOC) { uint16_t v = (uint16_t)ADC1->DR; if (ph) adc_ai += (int16_t)(v - adc_ai) / 8; else adc_ntc += (int16_t)(v - adc_ntc) / 8; }
    ph ^= 1;
    ADC1->SQR3 = ph ? 7 : 6;
    ADC1->CR2 |= ADC_CR2_SWSTART;
}

/* ---------------- ШИМ ---------------- */
static uint8_t dtg_code(uint32_t ticks)
{
    if (ticks < 128) return (uint8_t)ticks;
    if (ticks < 256) return (uint8_t)(0x80 | ((ticks / 2 - 64) & 0x3F));
    if (ticks < 512) return (uint8_t)(0xC0 | ((ticks / 8 - 32) & 0x1F));
    return (uint8_t)(0xE0 | ((ticks / 16 - 32) & 0x1F));
}

void hw_pwm_set_freq(float f_pwm, float dead_time)
{
    if (TIM1->BDTR & TIM_BDTR_MOE) return;
    pwm_arr = (uint16_t)((float)F_CLK / (2.0f * f_pwm));
    TIM1->ARR = pwm_arr;
    TIM1->BDTR = (TIM1->BDTR & ~TIM_BDTR_DTG) | dtg_code((uint32_t)(dead_time * (float)F_CLK));
    TIM1->EGR = TIM_EGR_UG;
}

static void pwm_off(void) { TIM1->BDTR &= ~TIM_BDTR_MOE; TIM1->CCR1 = TIM1->CCR2 = TIM1->CCR3 = 0; }

/* Прерывание АЦП: инжектированная группа запускается от TIM1 в середине нулевого вектора, раз в период ШИМ */
void ADC_IRQHandler(void)
{
    static uint16_t idle_cnt;
    float duty[3];
    if (!(ADC1->SR & ADC_SR_JEOC)) return;
    ADC1->SR = ~ADC_SR_JEOC;
    float ru = (float)ADC1->JDR1, rw = (float)ADC1->JDR2;
    float vdc = (float)((int32_t)ADC1->JDR3 - (int32_t)ADC1->JDR4) * K_VDC * cal_u;
    if (vdc < 0) vdc = 0;
    if (TIM1->SR & TIM_SR_BIF) { TIM1->SR = ~TIM_SR_BIF; ipm_fault = 1; pwm_off(); }
    if (!(TIM1->BDTR & TIM_BDTR_MOE)) {      /* ШИМ выключен: подстройка нулей датчиков тока */
        if (idle_cnt < 2000) idle_cnt++;
        else {
            offs_u += 0.002f * (ru - offs_u); offs_w += 0.002f * (rw - offs_w);
            sensor_fault = (fabsf(offs_u - OFFS_NOM) > OFFS_TOL || fabsf(offs_w - OFFS_NOM) > OFFS_TOL);
        }
    } else idle_cnt = 0;
    float iu = (ru - offs_u) * K_CUR * cal_i, iw = (rw - offs_w) * K_CUR * cal_i;
    int en = app_isr(iu, iw, vdc, duty);
    if (en && !ipm_fault) {
        float a = (float)pwm_arr;
        TIM1->CCR1 = (uint16_t)(duty[0] * a); TIM1->CCR2 = (uint16_t)(duty[1] * a); TIM1->CCR3 = (uint16_t)(duty[2] * a);
        if (!(TIM1->BDTR & TIM_BDTR_MOE)) { TIM1->SR = ~TIM_SR_BIF; TIM1->BDTR |= TIM_BDTR_MOE; }
    } else pwm_off();
}

int  hw_ipm_fault(void) { if (!(GPIOB->IDR & (1u << 12))) ipm_fault = 1; return ipm_fault; }
void hw_ipm_fault_clear(void) { if (GPIOB->IDR & (1u << 12)) { TIM1->SR = ~TIM_SR_BIF; ipm_fault = 0; } }
int  hw_sensor_fault(void) { return sensor_fault; }
void hw_set_cal(float k_i, float k_u) { cal_i = k_i; cal_u = k_u; }

/* ---------------- дискретные сигналы ---------------- */
void hw_precharge(int on) { GPIOB->BSRR = on ? (1u << 2) : (1u << 18); }
void hw_fan(int on)       { GPIOA->BSRR = on ? (1u << 15) : (1u << 31); }
void hw_relay(int on)     { GPIOB->BSRR = on ? (1u << 3) : (1u << 19); }
uint8_t hw_di(void)
{
    uint32_t v = ~GPIOB->IDR;
    return (uint8_t)(((v >> 0) & 1) | (((v >> 1) & 1) << 1) | (((v >> 10) & 1) << 2));
}
uint8_t hw_keys(void)
{
    static uint8_t last; uint8_t b = 0xFF, ff = 0xFF;
    hw_i2c_write(KEYS_I2C_ADDR, &ff, 1);                 /* входы PCF8574: записать 1 */
    if (hw_i2c_read(KEYS_I2C_ADDR, &b, 1) != 0) { last = 0; return 0; }
    uint8_t k = (uint8_t)(~b & 0x1F), r = (uint8_t)(k & last);   /* подавление дребезга: два совпавших опроса */
    last = k;
    return r;
}
float hw_ai(void) { float v = (float)adc_ai / 4095.0f * 1.02f - 0.01f; return v < 0 ? 0 : (v > 1 ? 1 : v); }
float hw_temp(void)
{
    float r = (float)adc_ntc; if (r < 20) return 200.0f; if (r > 4070) return -50.0f;   /* КЗ / обрыв датчика */
    float rt = 10000.0f * r / (4095.0f - r);
    return 1.0f / (1.0f / 298.15f + logf(rt / 10000.0f) / 3950.0f) - 273.15f;
}

/* ---------------- UART / Modbus ---------------- */
void hw_uart_cfg(int port, uint32_t baud, int parity)
{
    port_t *p = &ports[port];
    USART_TypeDef *u = p->u;
    uint32_t fck = (port == PORT_RS485) ? F_CLK : F_CLK / 2;
    u->CR1 = 0;
    u->BRR = (fck + baud / 2) / baud;
    uint32_t cr1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE | USART_CR1_UE;
    if (parity == 1) cr1 |= USART_CR1_M | USART_CR1_PCE;
    if (parity == 2) cr1 |= USART_CR1_M | USART_CR1_PCE | USART_CR1_PS;
    u->CR1 = cr1;
    p->gap_ms = (uint16_t)(38500u / baud + 2);        /* пауза 3,5 символа, не менее 2 мс */
    p->rx_n = 0;
}

static void uart_irq(port_t *p, int rs485)
{
    USART_TypeDef *u = p->u;
    uint32_t sr = u->SR;
    if (sr & (USART_SR_RXNE | USART_SR_ORE)) {
        uint8_t c = (uint8_t)u->DR;
        if (sr & (USART_SR_PE | USART_SR_FE)) p->rx_n = 0xFFFF;        /* кадр испорчен */
        else if (p->rx_n < sizeof p->rx) p->rx[p->rx_n++] = c;
        p->rx_t = ms_ticks;
    }
    if ((u->CR1 & USART_CR1_TXEIE) && (sr & USART_SR_TXE)) {
        if (p->tx_i < p->tx_n) u->DR = p->tx[p->tx_i++];
        else u->CR1 = (u->CR1 & ~USART_CR1_TXEIE) | USART_CR1_TCIE;
    } else if ((u->CR1 & USART_CR1_TCIE) && (sr & USART_SR_TC)) {
        u->CR1 &= ~USART_CR1_TCIE;
        if (rs485) GPIOB->BSRR = 1u << (5 + 16);                        /* DE = 0: приём */
        u->CR1 |= USART_CR1_RE;
    }
}
void USART1_IRQHandler(void) { uart_irq(&ports[0], 1); }
void USART2_IRQHandler(void) { uart_irq(&ports[1], 0); }

int hw_uart_rx_frame(int port, uint8_t *buf, int max)
{
    port_t *p = &ports[port];
    int n = 0;
    if (!p->rx_n || (uint32_t)(ms_ticks - p->rx_t) < p->gap_ms) return 0;
    __disable_irq();
    if (p->rx_n != 0xFFFF && p->rx_n <= max) { n = p->rx_n; for (int i = 0; i < n; i++) buf[i] = p->rx[i]; }
    p->rx_n = 0;
    __enable_irq();
    return n;
}

void hw_uart_tx(int port, const uint8_t *buf, int len)
{
    port_t *p = &ports[port];
    if (len <= 0 || len > (int)sizeof p->tx) return;
    for (int i = 0; i < len; i++) p->tx[i] = buf[i];
    p->tx_n = (uint16_t)len; p->tx_i = 0;
    if (port == PORT_RS485) { p->u->CR1 &= ~USART_CR1_RE; GPIOB->BSRR = 1u << 5; }   /* DE = 1: передача */
    p->u->CR1 |= USART_CR1_TXEIE;
}

/* ---------------- I2C (блокирующий, с тайм-аутами) ---------------- */
static int i2c_wait(uint32_t flag)
{
    for (uint32_t i = 0; i < 20000; i++) {
        uint32_t sr = I2C1->SR1;
        if (sr & flag) return 0;
        if (sr & (I2C_SR1_AF | I2C_SR1_BERR | I2C_SR1_ARLO)) break;
    }
    I2C1->CR1 |= I2C_CR1_STOP; I2C1->SR1 = 0;
    return -1;
}
static int i2c_start(uint8_t addr_rw)
{
    for (uint32_t i = 0; (I2C1->SR2 & I2C_SR2_BUSY) && i < 20000; i++) {}
    if (I2C1->SR2 & I2C_SR2_BUSY) { I2C1->CR1 |= I2C_CR1_SWRST; I2C1->CR1 &= ~I2C_CR1_SWRST;
        I2C1->CR2 = 48; I2C1->CCR = 240; I2C1->TRISE = 49; I2C1->CR1 = I2C_CR1_PE; }
    I2C1->CR1 |= I2C_CR1_START;
    if (i2c_wait(I2C_SR1_SB)) return -1;
    I2C1->DR = addr_rw;
    if (i2c_wait(I2C_SR1_ADDR)) return -1;
    return 0;
}
int hw_i2c_write(uint8_t addr, const uint8_t *data, int len)
{
    if (i2c_start((uint8_t)(addr << 1))) return -1;
    (void)I2C1->SR2;
    for (int i = 0; i < len; i++) { if (i2c_wait(I2C_SR1_TXE)) return -1; I2C1->DR = data[i]; }
    if (i2c_wait(I2C_SR1_BTF)) return -1;
    I2C1->CR1 |= I2C_CR1_STOP;
    return 0;
}
int hw_i2c_read(uint8_t addr, uint8_t *data, int len)
{
    if (len != 1) return -1;                              /* используется только чтение одного байта */
    I2C1->CR1 &= ~I2C_CR1_ACK;
    if (i2c_start((uint8_t)((addr << 1) | 1))) return -1;
    (void)I2C1->SR2;
    I2C1->CR1 |= I2C_CR1_STOP;
    if (i2c_wait(I2C_SR1_RXNE)) return -1;
    data[0] = (uint8_t)I2C1->DR;
    return 0;
}

/* ---------------- хранение параметров во флеш (журнал записей по 256 байт в секторе 7) ---------------- */
static int rec_used(uint32_t i) { return *(const uint32_t *)(FLASH_STORE_ADDR + i * REC_SIZE) != 0xFFFFFFFFu; }
int hw_flash_load(void *buf, int len)
{
    uint32_t n = FLASH_STORE_SIZE / REC_SIZE, i = 0;
    while (i < n && rec_used(i)) i++;
    if (i == 0 || len > (int)REC_SIZE) return -1;
    const uint8_t *s = (const uint8_t *)(FLASH_STORE_ADDR + (i - 1) * REC_SIZE);
    for (int k = 0; k < len; k++) ((uint8_t *)buf)[k] = s[k];
    return 0;
}
static void flash_wait(void) { while (FLASH->SR & FLASH_SR_BSY) IWDG->KR = 0xAAAA; }
int hw_flash_save(const void *buf, int len, int may_erase)
{
    uint32_t n = FLASH_STORE_SIZE / REC_SIZE, i = 0;
    if (len > (int)REC_SIZE) return -1;
    while (i < n && rec_used(i)) i++;
    if (i >= n && !may_erase) return -1;
    FLASH->KEYR = 0x45670123u; FLASH->KEYR = 0xCDEF89ABu;
    FLASH->SR = FLASH_SR_PGAERR | FLASH_SR_PGPERR | FLASH_SR_PGSERR | FLASH_SR_WRPERR;
    if (i >= n) {                                         /* сектор заполнен: стирание (только при остановленном приводе) */
        flash_wait();
        FLASH->CR = FLASH_CR_SER | (FLASH_STORE_SECTOR << FLASH_CR_SNB_Pos) | FLASH_CR_PSIZE_1;
        FLASH->CR |= FLASH_CR_STRT;
        flash_wait();
        i = 0;
    }
    const uint8_t *s = (const uint8_t *)buf;
    uint32_t a = FLASH_STORE_ADDR + i * REC_SIZE;
    for (int k = 0; k < len; k += 4) {
        uint32_t w = 0xFFFFFFFFu;
        for (int b = 0; b < 4 && k + b < len; b++) w = (w & ~(0xFFu << (8 * b))) | ((uint32_t)s[k + b] << (8 * b));
        flash_wait();
        FLASH->CR = FLASH_CR_PG | FLASH_CR_PSIZE_1;
        *(volatile uint32_t *)(a + (uint32_t)k) = w;
        flash_wait();
    }
    FLASH->CR = FLASH_CR_LOCK;
    return (FLASH->SR & (FLASH_SR_PGAERR | FLASH_SR_PGPERR | FLASH_SR_PGSERR | FLASH_SR_WRPERR)) ? -1 : 0;
}

/* ---------------- инициализация ---------------- */
static void gpio_mode(GPIO_TypeDef *g, int pin, int mode, int af, int pull, int od)
{
    g->MODER = (g->MODER & ~(3u << (pin * 2))) | ((uint32_t)mode << (pin * 2));
    g->PUPDR = (g->PUPDR & ~(3u << (pin * 2))) | ((uint32_t)pull << (pin * 2));
    g->OTYPER = (g->OTYPER & ~(1u << pin)) | ((uint32_t)od << pin);
    g->OSPEEDR |= (2u << (pin * 2));
    g->AFR[pin >> 3] = (g->AFR[pin >> 3] & ~(15u << ((pin & 7) * 4))) | ((uint32_t)af << ((pin & 7) * 4));
}
enum { IN = 0, OUT = 1, AF = 2, AN = 3, NOPULL = 0, PU = 1, PD = 2 };

static void clock_init(void)
{
    uint32_t pllm = 25, src = RCC_PLLCFGR_PLLSRC_HSE;
    RCC->CR |= RCC_CR_HSEON;
    for (uint32_t i = 0; !(RCC->CR & RCC_CR_HSERDY) && i < 2000000; i++) {}
    if (!(RCC->CR & RCC_CR_HSERDY)) { pllm = 16; src = RCC_PLLCFGR_PLLSRC_HSI; }   /* нет кварца — работа от HSI */
    FLASH->ACR = FLASH_ACR_LATENCY_3WS | FLASH_ACR_PRFTEN | FLASH_ACR_ICEN | FLASH_ACR_DCEN;
    RCC->PLLCFGR = pllm | (192u << RCC_PLLCFGR_PLLN_Pos) | (0u << RCC_PLLCFGR_PLLP_Pos) | src | (4u << RCC_PLLCFGR_PLLQ_Pos);
    RCC->CR |= RCC_CR_PLLON;
    while (!(RCC->CR & RCC_CR_PLLRDY)) {}
    RCC->CFGR = RCC_CFGR_PPRE1_DIV2 | RCC_CFGR_SW_PLL;                  /* HCLK 96 МГц, APB1 48 МГц, APB2 96 МГц */
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL) {}
}

void hw_init(void)
{
    SCB->CPACR |= (0xFu << 20);                                         /* включить FPU */
    clock_init();
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN | RCC_AHB1ENR_GPIOCEN;
    RCC->APB2ENR |= RCC_APB2ENR_TIM1EN | RCC_APB2ENR_ADC1EN | RCC_APB2ENR_USART1EN;
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN | RCC_APB1ENR_I2C1EN;
    (void)RCC->APB1ENR;

    /* выходы */
    GPIOB->BSRR = (1u << 18) | (1u << 19) | (1u << 21); GPIOA->BSRR = 1u << 31;
    gpio_mode(GPIOB, 2, OUT, 0, NOPULL, 0); gpio_mode(GPIOB, 3, OUT, 0, NOPULL, 0); gpio_mode(GPIOB, 5, OUT, 0, NOPULL, 0);
    gpio_mode(GPIOA, 15, OUT, 0, NOPULL, 0); gpio_mode(GPIOC, 13, OUT, 0, NOPULL, 0);
    /* входы */
    gpio_mode(GPIOB, 0, IN, 0, PU, 0); gpio_mode(GPIOB, 1, IN, 0, PU, 0); gpio_mode(GPIOB, 10, IN, 0, PU, 0);
    /* аналоговые */
    for (int p = 0; p <= 7; p++) if (p != 2 && p != 3) gpio_mode(GPIOA, p, AN, 0, NOPULL, 0);
    /* TIM1: PA8..10 (AF1), PB13..15 (AF1), BKIN PB12 (AF1, подтяжка вверх) */
    for (int p = 8; p <= 10; p++) gpio_mode(GPIOA, p, AF, 1, PD, 0);
    for (int p = 13; p <= 15; p++) gpio_mode(GPIOB, p, AF, 1, PD, 0);
    gpio_mode(GPIOB, 12, AF, 1, PU, 0);
    /* USART1 PB6/PB7 (AF7), USART2 PA2/PA3 (AF7), I2C1 PB8/PB9 (AF4, открытый сток) */
    gpio_mode(GPIOB, 6, AF, 7, NOPULL, 0); gpio_mode(GPIOB, 7, AF, 7, PU, 0);
    gpio_mode(GPIOA, 2, AF, 7, NOPULL, 0); gpio_mode(GPIOA, 3, AF, 7, PU, 0);
    gpio_mode(GPIOB, 8, AF, 4, PU, 1); gpio_mode(GPIOB, 9, AF, 4, PU, 1);

    /* TIM1: центрированный ШИМ, комплементарные выходы, мёртвое время, вход аварии */
    TIM1->CR1 = TIM_CR1_CMS_0 | TIM_CR1_ARPE;
    TIM1->PSC = 0; TIM1->ARR = pwm_arr; TIM1->RCR = 1;                  /* событие обновления раз в период */
    TIM1->CCMR1 = (6u << TIM_CCMR1_OC1M_Pos) | TIM_CCMR1_OC1PE | (6u << TIM_CCMR1_OC2M_Pos) | TIM_CCMR1_OC2PE;
    TIM1->CCMR2 = (6u << TIM_CCMR2_OC3M_Pos) | TIM_CCMR2_OC3PE;
    TIM1->CCER = TIM_CCER_CC1E | TIM_CCER_CC1NE | TIM_CCER_CC2E | TIM_CCER_CC2NE | TIM_CCER_CC3E | TIM_CCER_CC3NE;
    TIM1->CR2 = TIM_CR2_MMS_1;                                          /* TRGO = обновление -> запуск АЦП */
    TIM1->BDTR = TIM_BDTR_BKE | TIM_BDTR_OSSI | TIM_BDTR_OSSR | dtg_code(240);   /* BKP = 0: активный низкий уровень */
    TIM1->EGR = TIM_EGR_UG;
    TIM1->CR1 |= TIM_CR1_CEN;

    /* ADC1: инжектированная группа из 4 каналов по TIM1_TRGO, регулярный канал — программно */
    ADC->CCR = ADC_CCR_ADCPRE_0;                                        /* PCLK2/4 = 24 МГц */
    ADC1->SMPR2 = 0x00FFFFFF & 0x00492492;                              /* каналы 0..7: 28 тактов */
    ADC1->SMPR2 |= (7u << 18) | (7u << 21);                             /* каналы 6, 7: 480 тактов */
    ADC1->JSQR = (3u << ADC_JSQR_JL_Pos) | (0u << ADC_JSQR_JSQ1_Pos) | (1u << ADC_JSQR_JSQ2_Pos) | (4u << ADC_JSQR_JSQ3_Pos) | (5u << ADC_JSQR_JSQ4_Pos);
    ADC1->CR1 = ADC_CR1_SCAN | ADC_CR1_JEOCIE;
    ADC1->CR2 = ADC_CR2_ADON | (1u << ADC_CR2_JEXTSEL_Pos) | ADC_CR2_JEXTEN_0;
    ADC1->SQR1 = 0; ADC1->SQR3 = 6;

    /* I2C1: 100 кГц */
    I2C1->CR1 = I2C_CR1_SWRST; I2C1->CR1 = 0;
    I2C1->CR2 = 48; I2C1->CCR = 240; I2C1->TRISE = 49; I2C1->CR1 = I2C_CR1_PE;

    ports[0].u = USART1; ports[1].u = USART2;

    SysTick_Config(F_CLK / 1000);
    NVIC_SetPriority(ADC_IRQn, 0);                                      /* управление двигателем — высший приоритет */
    NVIC_SetPriority(USART1_IRQn, 2); NVIC_SetPriority(USART2_IRQn, 2); NVIC_SetPriority(SysTick_IRQn, 3);
    NVIC_EnableIRQ(USART1_IRQn); NVIC_EnableIRQ(USART2_IRQn);

    /* сторожевой таймер ~3 с (перекрывает стирание сектора флеш) */
    IWDG->KR = 0x5555; IWDG->PR = 4; IWDG->RLR = 1500; IWDG->KR = 0xAAAA; IWDG->KR = 0xCCCC;
}

void hw_start(void) { NVIC_EnableIRQ(ADC_IRQn); }
void hw_led(int on) { GPIOC->BSRR = on ? (1u << 29) : (1u << 13); }
