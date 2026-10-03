/*
 * See display.h.
 *
 *   BSP     : FMC/SDRAM, LTDC timings and PLLSAI, the LTDC pins, SPI5,
 *             the ILI9341 register sequence
 *   here    : the two layers, the flip, and an SDRAM read-back check
 */
#include "display.h"
#include "stm32f429i_discovery_lcd.h"
#include "ili9341.h"

/* Two full frames back to back at the start of SDRAM. */
#define FB0 ((uint32_t)LCD_FRAME_BUFFER)
#define FB1 ((uint32_t)LCD_FRAME_BUFFER + DISPLAY_FB_BYTES)

static uint32_t s_front;   /* layer index currently on screen */
static uint32_t s_back;    /* layer index we draw into        */
static uint32_t *s_fb[2];

/* Writes a pattern into both framebuffers and reads it back. Catches a
 * dead or misconfigured FMC before anything blames the panel.
 * BSP_LCD_Init() discards the SDRAM status, this is the only check. */
static int sdram_readback_ok(void)
{
    static const uint32_t k_pattern[4] = {
        0x00000000U, 0xFFFFFFFFU, 0xA5A5A5A5U, 0x5A5A5A5AU
    };
    volatile uint32_t *p = (volatile uint32_t *)FB0;
    uint32_t i;

    for (i = 0; i < 4U; i++)
        p[i] = k_pattern[i];
    for (i = 0; i < 4U; i++) {
        if (p[i] != k_pattern[i])
            return 0;
    }
    p = (volatile uint32_t *)FB1;
    for (i = 0; i < 4U; i++)
        p[i] = ~k_pattern[i];
    for (i = 0; i < 4U; i++) {
        if (p[i] != ~k_pattern[i])
            return 0;
    }
    return 1;
}

/*
 * Overrides the __weak BSP_SDRAM_MspInit(): same GPIO and clock setup as
 * the ST version, but without claiming DMA2 Stream0 for SDRAM transfers
 * nobody here makes. In the INSLIB firmware that stream belonged to the
 * IMU and the BSP silently reprogrammed it; here it would merely be dead
 * weight, but keeping the verified version means one less difference.
 */
void BSP_SDRAM_MspInit(SDRAM_HandleTypeDef *hsdram, void *Params)
{
    GPIO_InitTypeDef g = {0};

    (void)Params;
    if (hsdram == NULL)
        return;

    __HAL_RCC_FMC_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();

    /* The board's FMC wiring, verbatim from the ST BSP. */
    g.Mode      = GPIO_MODE_AF_PP;
    g.Speed     = GPIO_SPEED_FAST;
    g.Pull      = GPIO_NOPULL;
    g.Alternate = GPIO_AF12_FMC;

    g.Pin = GPIO_PIN_5 | GPIO_PIN_6;                       /* SDCKE1, SDNE1 */
    HAL_GPIO_Init(GPIOB, &g);

    g.Pin = GPIO_PIN_0;                                    /* SDNWE */
    HAL_GPIO_Init(GPIOC, &g);

    g.Pin = GPIO_PIN_0  | GPIO_PIN_1  | GPIO_PIN_8  | GPIO_PIN_9 |
            GPIO_PIN_10 | GPIO_PIN_14 | GPIO_PIN_15;
    HAL_GPIO_Init(GPIOD, &g);

    g.Pin = GPIO_PIN_0  | GPIO_PIN_1  | GPIO_PIN_7  | GPIO_PIN_8  |
            GPIO_PIN_9  | GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12 |
            GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
    HAL_GPIO_Init(GPIOE, &g);

    g.Pin = GPIO_PIN_0  | GPIO_PIN_1  | GPIO_PIN_2  | GPIO_PIN_3  |
            GPIO_PIN_4  | GPIO_PIN_5  | GPIO_PIN_11 | GPIO_PIN_12 |
            GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
    HAL_GPIO_Init(GPIOF, &g);

    g.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_4 | GPIO_PIN_5 |
            GPIO_PIN_8 | GPIO_PIN_15;
    HAL_GPIO_Init(GPIOG, &g);
}

int display_init(void)
{
    s_fb[0] = (uint32_t *)FB0;
    s_fb[1] = (uint32_t *)FB1;

    if (BSP_LCD_Init() != LCD_OK)
        return -1;

#if DISPLAY_ROTATE_180
    /* MADCTL after the BSP's init sequence, which writes 0xC8 = MY | MX |
     * BGR. Clearing MY and MX turns both scan directions around. BGR is
     * the panel's colour order and stays. */
    ili9341_WriteReg(LCD_MAC);
    ili9341_WriteData(0x08);
#endif

    if (!sdram_readback_ok())
        return -2;

    BSP_LCD_LayerDefaultInit(0, FB0);
    BSP_LCD_LayerDefaultInit(1, FB1);

    /* Layer 1 stays off until something has been drawn into it, both
     * layers on over uninitialized SDRAM shows noise. */
    s_front = 0;
    s_back  = 1;
    BSP_LCD_SetLayerVisible(1, DISABLE);
    BSP_LCD_SetLayerVisible(0, ENABLE);
    BSP_LCD_SelectLayer(s_front);
    BSP_LCD_Clear(LCD_COLOR_BLACK);
    BSP_LCD_SelectLayer(s_back);
    BSP_LCD_Clear(LCD_COLOR_BLACK);

    BSP_LCD_DisplayOn();
    return 0;
}

/* BSP_LCD_Relaod() is not used: it calls HAL_LTDC_Relaod(), a typo that
 * only exists in the older HAL the BSP shipped with. */
int display_ready(void)
{
    /* SRCR.VBR stays set until the LTDC has applied the reload at the next
     * vertical blanking. */
    return (LTDC->SRCR & LTDC_SRCR_VBR) == 0U;
}

uint32_t *display_back_buffer(void)
{
    return s_fb[s_back];
}

int display_back_index(void)
{
    return (int)s_back;
}

void display_present(void)
{
    /* Both layer changes without reload, then ONE reload, so the swap is
     * atomic and there is no frame with both layers off. */
    BSP_LCD_SetLayerVisible_NoReload(s_front, DISABLE);
    BSP_LCD_SetLayerVisible_NoReload(s_back,  ENABLE);
    LTDC->SRCR = LTDC_SRCR_VBR;

    const uint32_t shown = s_back;
    s_back  = s_front;
    s_front = shown;
    BSP_LCD_SelectLayer(s_back);
}
