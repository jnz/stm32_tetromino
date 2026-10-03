/*
 * See periph.h.
 */
#include "periph.h"

I2C_HandleTypeDef hi2c3;

/*
 * Core clock, "make CPU_MHZ=90" (default) or 180. Both from the 8 MHz HSE
 * through PLLM 8 (1 MHz), and both give the peripherals the same clocks:
 * APB1 45 MHz (I2C3), APB2 90 MHz (SPI5 to the panel), RNG 45 MHz. The
 * pixel clock comes from PLLSAI and does not depend on this at all.
 *
 *   90 MHz:  VCO 180, PLLP 2, PLLQ 4. Voltage scale 3, the lowest core
 *            voltage, which allows up to 120 MHz. No over-drive, 2 flash
 *            wait states. The default: the game needs at most a third of
 *            each 50 ms step at this clock, and the clock trees and the
 *            core voltage cost power even while the core sleeps.
 *   180 MHz: VCO 360, PLLP 2, PLLQ 8. Voltage scale 1 plus over-drive, 5
 *            wait states. What the INSLIB firmware ran at, because the
 *            BSP's SDRAM timings asked for it; kept for comparison.
 *
 * PLLQ makes 45 MHz in both, the RNG wants at most 48.
 */
#ifndef CPU_MHZ
#define CPU_MHZ 90
#endif
#if CPU_MHZ != 90 && CPU_MHZ != 180
#error "CPU_MHZ must be 90 or 180"
#endif

void periph_clock_config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    /* Only takes while the PLL is off, which it is out of reset. */
#if CPU_MHZ == 90
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);
#else
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
#endif

    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState       = RCC_HSE_ON;
    osc.PLL.PLLState   = RCC_PLL_ON;
    osc.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLM       = 8;
    osc.PLL.PLLN       = 2U * CPU_MHZ;
    osc.PLL.PLLP       = RCC_PLLP_DIV2;
    osc.PLL.PLLQ       = (2U * CPU_MHZ) / 45U;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK)
        Error_Handler();

#if CPU_MHZ == 180
    if (HAL_PWREx_EnableOverDrive() != HAL_OK)
        Error_Handler();
#endif

    clk.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                         RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;
#if CPU_MHZ == 90
    clk.APB1CLKDivider = RCC_HCLK_DIV2;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_2) != HAL_OK)
        Error_Handler();
#else
    clk.APB1CLKDivider = RCC_HCLK_DIV4;
    clk.APB2CLKDivider = RCC_HCLK_DIV2;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_5) != HAL_OK)
        Error_Handler();
#endif
}

static void gpio_init(void)
{
    GPIO_InitTypeDef g = {0};

    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();

    /* On-board L3GD20 shares SPI5 with the panel: keep it deselected. */
    HAL_GPIO_WritePin(NCS_MEMS_SPI_GPIO_Port, NCS_MEMS_SPI_Pin, GPIO_PIN_SET);
    g.Pin   = NCS_MEMS_SPI_Pin;
    g.Mode  = GPIO_MODE_OUTPUT_PP;
    g.Pull  = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(NCS_MEMS_SPI_GPIO_Port, &g);

    /* LEDs off until the game has something to show (app_leds()). */
    HAL_GPIO_WritePin(LD3_GPIO_Port, LD3_Pin | LD4_Pin, GPIO_PIN_RESET);
    g.Pin = LD3_Pin | LD4_Pin;
    HAL_GPIO_Init(LD3_GPIO_Port, &g);
}

static void i2c3_init(void)
{
    hi2c3.Instance             = I2C3;
    hi2c3.Init.ClockSpeed      = 400000;
    hi2c3.Init.DutyCycle       = I2C_DUTYCYCLE_2;
    hi2c3.Init.OwnAddress1     = 0;
    hi2c3.Init.AddressingMode  = I2C_ADDRESSINGMODE_7BIT;
    hi2c3.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c3.Init.OwnAddress2     = 0;
    hi2c3.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c3.Init.NoStretchMode   = I2C_NOSTRETCH_DISABLE;
    if (HAL_I2C_Init(&hi2c3) != HAL_OK)
        Error_Handler();
    if (HAL_I2CEx_ConfigAnalogFilter(&hi2c3, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
        Error_Handler();
    if (HAL_I2CEx_ConfigDigitalFilter(&hi2c3, 0) != HAL_OK)
        Error_Handler();
}

void periph_init(void)
{
    gpio_init();
    i2c3_init();
}

uint32_t periph_random(void)
{
    uint32_t v = 0;
    uint32_t n;

    __HAL_RCC_RNG_CLK_ENABLE();
    RNG->CR |= RNG_CR_RNGEN;
    /* A word is ready after 40 RNG clocks. The bound only matters if the
     * generator reports a seed or clock error, then the caller gets 0. */
    for (n = 0; n < 100000U; n++) {
        if ((RNG->SR & (RNG_SR_SECS | RNG_SR_CECS)) != 0U)
            break;
        if ((RNG->SR & RNG_SR_DRDY) != 0U) {
            v = RNG->DR;
            break;
        }
    }
    RNG->CR &= ~RNG_CR_RNGEN;
    return v;
}

/*
 * LSI (~32 kHz) / 64 = 500 Hz, reload 2000: about 4 s. The longest thing
 * the main loop ever waits for is a flash sector erase of the high score
 * store, a few hundred milliseconds. A hang anywhere else resets the board
 * and the ornament comes back by itself.
 *
 * Frozen while the core is halted by a debugger, so stepping through code
 * does not reset the board under the probe.
 */
void periph_watchdog_start(void)
{
    DBGMCU->APB1FZ |= DBGMCU_APB1_FZ_DBG_IWDG_STOP;
    IWDG->KR = 0x5555U;            /* unlock PR and RLR */
    IWDG->PR = 4U;                 /* /64 */
    IWDG->RLR = 2000U;
    IWDG->KR = 0xCCCCU;            /* start */
    IWDG->KR = 0xAAAAU;
}

void periph_watchdog_kick(void)
{
    IWDG->KR = 0xAAAAU;
}

/* --------------------------------------------------------------------- */
/* HAL MSP callbacks                                                     */
/* --------------------------------------------------------------------- */

void HAL_MspInit(void)
{
    __HAL_RCC_SYSCFG_CLK_ENABLE();
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_0);
}

void HAL_I2C_MspInit(I2C_HandleTypeDef *hi2c)
{
    GPIO_InitTypeDef g = {0};

    if (hi2c->Instance != I2C3)
        return;

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    /* PA8 SCL, PC9 SDA */
    g.Pin       = GPIO_PIN_8;
    g.Mode      = GPIO_MODE_AF_OD;
    g.Pull      = GPIO_NOPULL;
    g.Speed     = GPIO_SPEED_FREQ_LOW;
    g.Alternate = GPIO_AF4_I2C3;
    HAL_GPIO_Init(GPIOA, &g);

    g.Pin = GPIO_PIN_9;
    HAL_GPIO_Init(GPIOC, &g);

    __HAL_RCC_I2C3_CLK_ENABLE();
}
