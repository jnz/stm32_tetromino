/*
 * See display.h.
 *
 *   BSP     : the LTDC pins (BSP_LCD_MspInit), SPI5 and the ILI9341
 *             register sequence (ili9341_drv)
 *   here    : LTDC timings and pixel clock, the two L8 layers with their
 *             palette, the flip, and keeping the SDRAM asleep
 *
 * BSP_LCD_Init() is not used any more: it brings up the SDRAM as well,
 * which this firmware no longer needs. The LTDC part of it is repeated
 * below with the same timings.
 */
#include "display.h"
#include "assets.h"
#include "ili9341.h"
#include "stm32f429i_discovery_lcd.h"
#include <string.h>

/* Both framebuffers in internal SRAM, see the linker script: the LTDC
 * cannot read the CCM, where everything else lives. */
static uint8_t s_fb[2][DISPLAY_WIDTH * DISPLAY_HEIGHT]
    __attribute__((section(".framebuffer"), aligned(8)));

static LTDC_HandleTypeDef s_ltdc;
static int      s_rotated; /* MADCTL as last written          */
static uint32_t s_front;   /* layer index currently on screen */
static uint32_t s_back;    /* layer index we draw into        */

static LTDC_Layer_TypeDef *layer(uint32_t i)
{
    return (i == 0U) ? LTDC_Layer1 : LTDC_Layer2;
}

/*
 * The board's SDRAM (IS42S16400J) is powered whether it is used or not.
 * Left alone its control lines float. Clock enable (SDCKE1, PB5) held low
 * puts it into power-down, the state with the least standby current, and
 * chip select (SDNE1, PB6) held high keeps it deselected. Nothing else of
 * the FMC is set up, the controller stays unclocked.
 */
static void sdram_sleep(void)
{
    GPIO_InitTypeDef g = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);
    g.Pin   = GPIO_PIN_5 | GPIO_PIN_6;
    g.Mode  = GPIO_MODE_OUTPUT_PP;
    g.Pull  = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &g);
}

/* The palette into a layer's CLUT. The CLUT may only be written while its
 * layer is off (or in vertical blanking): written into a live layer,
 * entries get lost and the two layers end up with different palettes,
 * which flickers. The caller makes sure the layer is off. */
static void clut_write(uint32_t i)
{
    uint32_t k;

    for (k = 0; k < 256U; k++)
        layer(i)->CLUTWR = (k << 24) | (asset_pal[k] & 0x00FFFFFFU);
}

static int layer_init(uint32_t i)
{
    LTDC_LayerCfgTypeDef cfg = {0};

    cfg.WindowX0        = 0;
    cfg.WindowX1        = DISPLAY_WIDTH;
    cfg.WindowY0        = 0;
    cfg.WindowY1        = DISPLAY_HEIGHT;
    cfg.PixelFormat     = LTDC_PIXEL_FORMAT_L8;
    cfg.FBStartAdress   = (uint32_t)s_fb[i];
    cfg.Alpha           = 255;
    cfg.Alpha0          = 0;
    cfg.BlendingFactor1 = LTDC_BLENDING_FACTOR1_PAxCA;
    cfg.BlendingFactor2 = LTDC_BLENDING_FACTOR2_PAxCA;
    cfg.ImageWidth      = DISPLAY_WIDTH;
    cfg.ImageHeight     = DISPLAY_HEIGHT;
    if (HAL_LTDC_ConfigLayer(&s_ltdc, &cfg, i) != HAL_OK)
        return -1;

    /* HAL_LTDC_ConfigLayer() has just switched the layer on. Off for the
     * CLUT (see clut_write()), then CLUT on, layer stays off until
     * present() shows it. */
    layer(i)->CR &= ~LTDC_LxCR_LEN;
    LTDC->SRCR = LTDC_SRCR_IMR;
    while ((LTDC->SRCR & LTDC_SRCR_IMR) != 0U) {
    }
    clut_write(i);
    layer(i)->CR |= LTDC_LxCR_CLUTEN;
    LTDC->SRCR = LTDC_SRCR_IMR;
    while ((LTDC->SRCR & LTDC_SRCR_IMR) != 0U) {
    }
    return 0;
}

int display_init(void)
{
    RCC_PeriphCLKInitTypeDef clk = {0};

    sdram_sleep();

    /* Black in both buffers before anything is shown. */
    memset(s_fb, asset_grey[0], sizeof s_fb);

    /* Timings of the ILI9341 in RGB mode, as BSP_LCD_Init() sets them:
     * HSYNC 10, HBP 20, 240 active, HFP 10; VSYNC 2, VBP 2, 320 active,
     * VFP 4. The HAL wants them accumulated. */
    s_ltdc.Instance                = LTDC;
    s_ltdc.Init.HorizontalSync     = ILI9341_HSYNC;
    s_ltdc.Init.VerticalSync       = ILI9341_VSYNC;
    s_ltdc.Init.AccumulatedHBP     = ILI9341_HBP;
    s_ltdc.Init.AccumulatedVBP     = ILI9341_VBP;
    s_ltdc.Init.AccumulatedActiveW = 269;
    s_ltdc.Init.AccumulatedActiveH = 323;
    s_ltdc.Init.TotalWidth         = 279;
    s_ltdc.Init.TotalHeigh         = 327;
    s_ltdc.Init.Backcolor.Red      = 0;
    s_ltdc.Init.Backcolor.Green    = 0;
    s_ltdc.Init.Backcolor.Blue     = 0;
    s_ltdc.Init.HSPolarity         = LTDC_HSPOLARITY_AL;
    s_ltdc.Init.VSPolarity         = LTDC_VSPOLARITY_AL;
    s_ltdc.Init.DEPolarity         = LTDC_DEPOLARITY_AL;
    s_ltdc.Init.PCPolarity         = LTDC_PCPOLARITY_IPC;

    /* Pixel clock: 1 MHz * 192 / 4 / 8 = 6 MHz, as the BSP sets it. */
    clk.PeriphClockSelection = RCC_PERIPHCLK_LTDC;
    clk.PLLSAI.PLLSAIN       = 192;
    clk.PLLSAI.PLLSAIR       = 4;
    clk.PLLSAIDivR           = RCC_PLLSAIDIVR_8;
    if (HAL_RCCEx_PeriphCLKConfig(&clk) != HAL_OK)
        return -1;

    BSP_LCD_MspInit();             /* LTDC clock and pins */
    __HAL_RCC_DMA2D_CLK_DISABLE(); /* switched on there, not used here */
    if (HAL_LTDC_Init(&s_ltdc) != HAL_OK)
        return -1;

    /* Panel register sequence over SPI5. It leaves the panel unturned. */
    ili9341_drv.Init();
    s_rotated = 0;
    display_set_rotated(DISPLAY_ROTATE_180);

    if (layer_init(0) != 0 || layer_init(1) != 0)
        return -2;

    /* Layer 1 stays off until something has been drawn into it. */
    s_front = 0;
    s_back  = 1;
    layer(1)->CR &= ~LTDC_LxCR_LEN;
    layer(0)->CR |= LTDC_LxCR_LEN;
    LTDC->SRCR = LTDC_SRCR_IMR;

    ili9341_DisplayOn();
    return 0;
}

int display_ready(void)
{
    /* SRCR.VBR stays set until the LTDC has applied the reload at the next
     * vertical blanking. While it is set, the buffer we are about to draw
     * into is still the one being scanned out. */
    return (LTDC->SRCR & LTDC_SRCR_VBR) == 0U;
}

uint8_t *display_back_buffer(void)
{
    return s_fb[s_back];
}

int display_back_index(void)
{
    return (int)s_back;
}

void display_set_rotated(int rotated)
{
    rotated = (rotated != 0);
    if (rotated == s_rotated)
        return;
    /* MADCTL. The init sequence writes 0xC8 = MY | MX | BGR, clearing MY
     * and MX turns both scan directions around. BGR is the panel's colour
     * order and stays. */
    ili9341_WriteReg(LCD_MAC);
    ili9341_WriteData(rotated ? 0x08 : 0xC8);
    s_rotated = rotated;
}

int display_rotated(void)
{
    return s_rotated;
}

void display_present(void)
{
    /* Both layer changes, then ONE reload at vertical blanking: the swap
     * is atomic and there is no frame with both layers off. */
    layer(s_front)->CR &= ~LTDC_LxCR_LEN;
    layer(s_back)->CR  |= LTDC_LxCR_LEN;
    LTDC->SRCR = LTDC_SRCR_VBR;

    {
        const uint32_t shown = s_back;

        s_back  = s_front;
        s_front = shown;
    }
}
