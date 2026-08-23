/**
 ******************************************************************************
 * @file    system_stm32g0xx.c
 * @author  MCD Application Team
 * @brief   CMSIS Cortex-M0+ Device Peripheral Access Layer System Source File
 *
 *   This file provides two functions and one global variable to be called from
 *   user application:
 *      - SystemInit(): This function is called at startup just after reset and
 *                      before branch to main program. This call is made inside
 *                      the "startup_stm32g0xx.s" file.
 *
 *      - SystemCoreClock variable: Contains the core clock (HCLK), it can be
 * used by the user application to setup the SysTick timer or configure other
 * parameters.
 *
 *      - SystemCoreClockUpdate(): Updates the variable SystemCoreClock and must
 *                                 be called whenever the core clock is changed
 *                                 during program execution.
 *
 *   After reset, SystemInit() applies any requested vector-table relocation.
 *   main.c later uses the 16 MHz HSI as the PLL source and selects the
 *   resulting 64 MHz PLL clock as SYSCLK.
 *
 *   This file configures the system clock as follows:
 *=============================================================================
 *-----------------------------------------------------------------------------
 *        System Clock source                    | PLL (HSI source)
 *-----------------------------------------------------------------------------
 *        SYSCLK(Hz)                             | 64000000
 *-----------------------------------------------------------------------------
 *        HCLK(Hz)                               | 64000000
 *-----------------------------------------------------------------------------
 *        AHB Prescaler                          | 1
 *-----------------------------------------------------------------------------
 *        APB Prescaler                          | 1
 *-----------------------------------------------------------------------------
 *        HSI Division factor                    | 1
 *-----------------------------------------------------------------------------
 *        PLL_M                                  | 1
 *-----------------------------------------------------------------------------
 *        PLL_N                                  | 8
 *-----------------------------------------------------------------------------
 *        PLL_P                                  | 2
 *-----------------------------------------------------------------------------
 *        PLL_Q                                  | 2
 *-----------------------------------------------------------------------------
 *        PLL_R                                  | 2
 *-----------------------------------------------------------------------------
 *        Require 48MHz for RNG                  | Disabled
 *-----------------------------------------------------------------------------
 *=============================================================================
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2018-2021 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/** @addtogroup CMSIS
 * @{
 */

/** @addtogroup stm32g0xx_system
 * @{
 */

/** @addtogroup STM32G0xx_System_Private_Includes
 * @{
 */

#include "stm32g0xx.h"

#if !defined(HSE_VALUE)
#define HSE_VALUE (8000000UL) /*!< Value of the External oscillator in Hz */
#endif                        /* HSE_VALUE */

#if !defined(HSI_VALUE)
#define HSI_VALUE (16000000UL) /*!< Value of the Internal oscillator in Hz*/
#endif                         /* HSI_VALUE */

#if !defined(LSI_VALUE)
#define LSI_VALUE (32000UL) /*!< Value of LSI in Hz*/
#endif                      /* LSI_VALUE */

#if !defined(LSE_VALUE)
#define LSE_VALUE (32768UL) /*!< Value of LSE in Hz*/
#endif                      /* LSE_VALUE */

/**
 * @}
 */

/** @addtogroup STM32G0xx_System_Private_TypesDefinitions
 * @{
 */

/**
 * @}
 */

/** @addtogroup STM32G0xx_System_Private_Defines
 * @{
 */

/************************* Miscellaneous Configuration ************************/
/* Note: Following vector table addresses must be defined in line with linker
         configuration. */
/*!< Uncomment the following line if you need to relocate the vector table
     anywhere in Flash or Sram, else the vector table is kept at the automatic
     remap of boot address selected */
/* #define USER_VECT_TAB_ADDRESS */

#if defined(USER_VECT_TAB_ADDRESS)
/*!< Uncomment the following line if you need to relocate your vector Table
     in Sram else user remap will be done in Flash. */
/* #define VECT_TAB_SRAM */
#if defined(VECT_TAB_SRAM)
#define VECT_TAB_BASE_ADDRESS                                                  \
  SRAM_BASE /*!< Vector Table base address field.                              \
                 This value must be a multiple of 0x200. */
#else
#define VECT_TAB_BASE_ADDRESS                                                  \
  FLASH_BASE /*!< Vector Table base address field.                             \
                  This value must be a multiple of 0x200. */
#endif       /* VECT_TAB_SRAM */

#if !defined(VECT_TAB_OFFSET)
#define VECT_TAB_OFFSET                                                        \
  0x00000000U /*!< Vector Table offset field.                                  \
                   This value must be a multiple of 0x200. */
#endif        /* VECT_TAB_OFFSET */

#endif /* USER_VECT_TAB_ADDRESS */
/******************************************************************************/
/**
 * @}
 */

/** @addtogroup STM32G0xx_System_Private_Macros
 * @{
 */

/**
 * @}
 */

/** @addtogroup STM32G0xx_System_Private_Variables
 * @{
 */
/** @brief Current configured HCLK frequency in hertz. */
uint32_t SystemCoreClock = 16000000UL;

/** @brief Lookup table of right-shift counts for RCC AHB prescaler encodings. */
const uint32_t AHBPrescTable[16UL] = {0UL, 0UL, 0UL, 0UL, 0UL, 0UL, 0UL, 0UL,
                                      1UL, 2UL, 3UL, 4UL, 6UL, 7UL, 8UL, 9UL};
/** @brief Lookup table of right-shift counts for RCC APB prescaler encodings. */
const uint32_t APBPrescTable[8UL] = {0UL, 0UL, 0UL, 0UL, 1UL, 2UL, 3UL, 4UL};

/**
 * @}
 */

/** @addtogroup STM32G0xx_System_Private_FunctionPrototypes
 * @{
 */

/**
 * @}
 */

/** @addtogroup STM32G0xx_System_Private_Functions
 * @{
 */

/**
 * @brief Apply the compile-time vector-table location during reset startup.
 * @note Called by startup_stm32g0b1xx.s before main().
 */
void SystemInit(void) {
  // Relocation is compile-time optional and must match the linker memory map.
#if defined(USER_VECT_TAB_ADDRESS)
  SCB->VTOR =
      VECT_TAB_BASE_ADDRESS | VECT_TAB_OFFSET; /* Vector Table Relocation */
#endif                                         /* USER_VECT_TAB_ADDRESS */
}

/**
 * @brief Recalculate SystemCoreClock from the live RCC clock registers.
 * @note Call this after clock changes that bypass HAL_RCC_ClockConfig().
 * @note The result uses nominal oscillator constants and is a configured
 *       frequency in hertz, not a physical frequency measurement.
 */
void SystemCoreClockUpdate(void) {
  uint32_t tmp;
  uint32_t pllvco;
  uint32_t pllr;
  uint32_t pllsource;
  uint32_t pllm;
  uint32_t hsidiv;

  // Decode the live RCC source and prescalers rather than assuming main.c's
  // startup clock is still active.
  switch (RCC->CFGR & RCC_CFGR_SWS) {
  case RCC_CFGR_SWS_0: /* HSE used as system clock */
    SystemCoreClock = HSE_VALUE;
    break;

  case (RCC_CFGR_SWS_1 | RCC_CFGR_SWS_0): /* LSI used as system clock */
    SystemCoreClock = LSI_VALUE;
    break;

  case RCC_CFGR_SWS_2: /* LSE used as system clock */
    SystemCoreClock = LSE_VALUE;
    break;

  case RCC_CFGR_SWS_1: /* PLL used as system clock */
    /* PLL_VCO = (HSE_VALUE or HSI_VALUE / PLLM) * PLLN
       SYSCLK = PLL_VCO / PLLR
       */
    pllsource = (RCC->PLLCFGR & RCC_PLLCFGR_PLLSRC);
    pllm = ((RCC->PLLCFGR & RCC_PLLCFGR_PLLM) >> RCC_PLLCFGR_PLLM_Pos) + 1UL;

    if (pllsource == 0x03UL) /* HSE used as PLL clock source */
    {
      pllvco = (HSE_VALUE / pllm);
    } else /* HSI used as PLL clock source */
    {
      pllvco = (HSI_VALUE / pllm);
    }
    pllvco =
        pllvco * ((RCC->PLLCFGR & RCC_PLLCFGR_PLLN) >> RCC_PLLCFGR_PLLN_Pos);
    pllr = (((RCC->PLLCFGR & RCC_PLLCFGR_PLLR) >> RCC_PLLCFGR_PLLR_Pos) + 1UL);

    SystemCoreClock = pllvco / pllr;
    break;

  case 0x00000000U: /* HSI used as system clock */
  default:          /* HSI used as system clock */
    hsidiv = (1UL << ((READ_BIT(RCC->CR, RCC_CR_HSIDIV)) >> RCC_CR_HSIDIV_Pos));
    SystemCoreClock = (HSI_VALUE / hsidiv);
    break;
  }
  /* Compute HCLK clock frequency --------------------------------------------*/
  /* Get HCLK prescaler */
  tmp = AHBPrescTable[((RCC->CFGR & RCC_CFGR_HPRE) >> RCC_CFGR_HPRE_Pos)];
  /* HCLK clock frequency */
  SystemCoreClock >>= tmp;
}

/**
 * @}
 */

/**
 * @}
 */

/**
 * @}
 */
