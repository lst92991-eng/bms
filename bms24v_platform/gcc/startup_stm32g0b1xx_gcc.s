.syntax unified
.cpu cortex-m0plus
.thumb

.global g_pfnVectors
.global Default_Handler

.word _sidata
.word _sdata
.word _edata
.word _sbss
.word _ebss

.section .text.Reset_Handler
.weak Reset_Handler
.type Reset_Handler, %function
Reset_Handler:
  ldr r0, =_estack
  mov sp, r0

  ldr r0, =_sdata
  ldr r1, =_edata
  ldr r2, =_sidata
  movs r3, #0
  b LoopCopyDataInit

CopyDataInit:
  ldr r4, [r2, r3]
  str r4, [r0, r3]
  adds r3, r3, #4

LoopCopyDataInit:
  adds r4, r0, r3
  cmp r4, r1
  bcc CopyDataInit

  ldr r2, =_sbss
  ldr r4, =_ebss
  movs r3, #0
  b LoopFillZerobss

FillZerobss:
  str r3, [r2]
  adds r2, r2, #4

LoopFillZerobss:
  cmp r2, r4
  bcc FillZerobss

  bl SystemInit
  bl __libc_init_array
  bl main

LoopForever:
  b LoopForever
.size Reset_Handler, .-Reset_Handler

.section .text.Default_Handler,"ax",%progbits
Default_Handler:
Infinite_Loop:
  b Infinite_Loop
.size Default_Handler, .-Default_Handler

.macro weak_handler handler
  .weak \handler
  .set \handler, Default_Handler
.endm

weak_handler NMI_Handler
weak_handler HardFault_Handler
weak_handler SVC_Handler
weak_handler PendSV_Handler
weak_handler SysTick_Handler
weak_handler WWDG_IRQHandler
weak_handler PVD_VDDIO2_IRQHandler
weak_handler RTC_TAMP_IRQHandler
weak_handler FLASH_IRQHandler
weak_handler RCC_CRS_IRQHandler
weak_handler EXTI0_1_IRQHandler
weak_handler EXTI2_3_IRQHandler
weak_handler EXTI4_15_IRQHandler
weak_handler USB_UCPD1_2_IRQHandler
weak_handler DMA1_Channel1_IRQHandler
weak_handler DMA1_Channel2_3_IRQHandler
weak_handler DMA1_Ch4_7_DMA2_Ch1_5_DMAMUX1_OVR_IRQHandler
weak_handler ADC1_COMP_IRQHandler
weak_handler TIM1_BRK_UP_TRG_COM_IRQHandler
weak_handler TIM1_CC_IRQHandler
weak_handler TIM2_IRQHandler
weak_handler TIM3_TIM4_IRQHandler
weak_handler TIM6_DAC_LPTIM1_IRQHandler
weak_handler TIM7_LPTIM2_IRQHandler
weak_handler TIM14_IRQHandler
weak_handler TIM15_IRQHandler
weak_handler TIM16_FDCAN_IT0_IRQHandler
weak_handler TIM17_FDCAN_IT1_IRQHandler
weak_handler I2C1_IRQHandler
weak_handler I2C2_3_IRQHandler
weak_handler SPI1_IRQHandler
weak_handler SPI2_3_IRQHandler
weak_handler USART1_IRQHandler
weak_handler USART2_LPUART2_IRQHandler
weak_handler USART3_4_5_6_LPUART1_IRQHandler
weak_handler CEC_IRQHandler

.section .isr_vector,"a",%progbits
.type g_pfnVectors, %object
g_pfnVectors:
  .word _estack
  .word Reset_Handler
  .word NMI_Handler
  .word HardFault_Handler
  .word 0
  .word 0
  .word 0
  .word 0
  .word 0
  .word 0
  .word 0
  .word SVC_Handler
  .word 0
  .word 0
  .word PendSV_Handler
  .word SysTick_Handler
  .word WWDG_IRQHandler
  .word PVD_VDDIO2_IRQHandler
  .word RTC_TAMP_IRQHandler
  .word FLASH_IRQHandler
  .word RCC_CRS_IRQHandler
  .word EXTI0_1_IRQHandler
  .word EXTI2_3_IRQHandler
  .word EXTI4_15_IRQHandler
  .word USB_UCPD1_2_IRQHandler
  .word DMA1_Channel1_IRQHandler
  .word DMA1_Channel2_3_IRQHandler
  .word DMA1_Ch4_7_DMA2_Ch1_5_DMAMUX1_OVR_IRQHandler
  .word ADC1_COMP_IRQHandler
  .word TIM1_BRK_UP_TRG_COM_IRQHandler
  .word TIM1_CC_IRQHandler
  .word TIM2_IRQHandler
  .word TIM3_TIM4_IRQHandler
  .word TIM6_DAC_LPTIM1_IRQHandler
  .word TIM7_LPTIM2_IRQHandler
  .word TIM14_IRQHandler
  .word TIM15_IRQHandler
  .word TIM16_FDCAN_IT0_IRQHandler
  .word TIM17_FDCAN_IT1_IRQHandler
  .word I2C1_IRQHandler
  .word I2C2_3_IRQHandler
  .word SPI1_IRQHandler
  .word SPI2_3_IRQHandler
  .word USART1_IRQHandler
  .word USART2_LPUART2_IRQHandler
  .word USART3_4_5_6_LPUART1_IRQHandler
  .word CEC_IRQHandler
.size g_pfnVectors, .-g_pfnVectors
