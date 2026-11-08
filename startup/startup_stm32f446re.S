.syntax unified
.cpu cortex-m4
.thumb

.section .isr_vector, "a", %progbits
.align 2
.global _vector_table
.type _vector_table, %object
_vector_table:
    .word _estack
    .word Reset_Handler
    .word NMI_Handler
    .word HardFault_Handler
    .word MemManage_Handler
    .word BusFault_Handler
    .word UsageFault_Handler
    .word 0
    .word 0
    .word 0
    .word 0
    .word SVC_Handler
    .word DebugMon_Handler
    .word 0
    .word PendSV_Handler
    .word SysTick_Handler
    .word WWDG_IRQHandler
    .word PVD_IRQHandler
    .word TAMP_STAMP_IRQHandler
    .word RTC_WKUP_IRQHandler
    .word FLASH_IRQHandler
    .word RCC_IRQHandler
    .word EXTI0_IRQHandler
    .word EXTI1_IRQHandler
    .word EXTI2_IRQHandler
    .word EXTI3_IRQHandler
    .word EXTI4_IRQHandler
    .word DMA1_Stream0_IRQHandler
    .word DMA1_Stream1_IRQHandler
    .word DMA1_Stream2_IRQHandler
    .word DMA1_Stream3_IRQHandler
    .word DMA1_Stream4_IRQHandler
    .word DMA1_Stream5_IRQHandler
    .word DMA1_Stream6_IRQHandler
    .word ADC_IRQHandler
    .word CAN1_TX_IRQHandler
    .word CAN1_RX0_IRQHandler
    .word CAN1_RX1_IRQHandler
    .word CAN1_SCE_IRQHandler
    .word EXTI9_5_IRQHandler
    .word TIM1_BRK_TIM9_IRQHandler
    .word TIM1_UP_TIM10_IRQHandler
    .word TIM1_TRG_COM_TIM11_IRQHandler
    .word TIM1_CC_IRQHandler
    .word TIM2_IRQHandler
    .word TIM3_IRQHandler
    .word TIM4_IRQHandler
    .word I2C1_EV_IRQHandler
    .word I2C1_ER_IRQHandler
    .word I2C2_EV_IRQHandler
    .word I2C2_ER_IRQHandler
    .word SPI1_IRQHandler
    .word SPI2_IRQHandler
    .word USART1_IRQHandler
    .word USART2_IRQHandler
    .word USART3_IRQHandler
    .word EXTI15_10_IRQHandler
    .word RTC_Alarm_IRQHandler
    .word OTG_FS_WKUP_IRQHandler
    .word TIM8_BRK_TIM12_IRQHandler
    .word TIM8_UP_TIM13_IRQHandler
    .word TIM8_TRG_COM_TIM14_IRQHandler
    .word TIM8_CC_IRQHandler
    .word DMA1_Stream7_IRQHandler
    .word FMC_IRQHandler
    .word SDIO_IRQHandler
    .word TIM5_IRQHandler
    .word SPI3_IRQHandler
    .word UART4_IRQHandler
    .word UART5_IRQHandler
    .word TIM6_DAC_IRQHandler
    .word TIM7_IRQHandler
    .word DMA2_Stream0_IRQHandler
    .word DMA2_Stream1_IRQHandler
    .word DMA2_Stream2_IRQHandler
    .word DMA2_Stream3_IRQHandler
    .word DMA2_Stream4_IRQHandler
    .word 0
    .word 0
    .word CAN2_TX_IRQHandler
    .word CAN2_RX0_IRQHandler
    .word CAN2_RX1_IRQHandler
    .word CAN2_SCE_IRQHandler
    .word OTG_FS_IRQHandler
    .word DMA2_Stream5_IRQHandler
    .word DMA2_Stream6_IRQHandler
    .word DMA2_Stream7_IRQHandler
    .word USART6_IRQHandler
    .word I2C3_EV_IRQHandler
    .word I2C3_ER_IRQHandler
    .word OTG_HS_EP1_OUT_IRQHandler
    .word OTG_HS_EP1_IN_IRQHandler
    .word OTG_HS_WKUP_IRQHandler
    .word OTG_HS_IRQHandler
    .word DCMI_IRQHandler
    .word 0
    .word 0
    .word FPU_IRQHandler
    .word 0
    .word 0
    .word SPI4_IRQHandler
    .word 0
    .word 0
    .word SAI1_IRQHandler
    .word 0
    .word 0
    .word 0
    .word SAI2_IRQHandler
    .word QuadSPI_IRQHandler
    .word HDMI_CEC_IRQHandler
    .word SPDIF_RX_IRQHandler
    .word FMPI2C1_EV_IRQHandler
    .word FMPI2C1_ER_IRQHandler
.size _vector_table, . - _vector_table

.section .text.Reset_Handler, "ax", %progbits
.align 2
.global Reset_Handler
.type Reset_Handler, %function
.thumb_func
Reset_Handler:
    ldr r0, =0xE000ED08
    ldr r1, =_vector_table
    str r1, [r0]
    dsb
    isb
    ldr r0, =_sidata
    ldr r1, =_sdata
    ldr r2, =_edata
1:
    cmp r1, r2
    bcs 2f
    ldr r3, [r0], #4
    str r3, [r1], #4
    b 1b
2:
    ldr r1, =_sbss
    ldr r2, =_ebss
    movs r3, #0
3:
    cmp r1, r2
    bcs 4f
    str r3, [r1], #4
    b 3b
4:
    bl main
    b Default_Handler
.size Reset_Handler, . - Reset_Handler

.macro weak_alias handler
    .weak \handler
    .thumb_set \handler, Default_Handler
.endm

weak_alias DebugMon_Handler
weak_alias WWDG_IRQHandler
weak_alias PVD_IRQHandler
weak_alias TAMP_STAMP_IRQHandler
weak_alias RTC_WKUP_IRQHandler
weak_alias FLASH_IRQHandler
weak_alias RCC_IRQHandler
weak_alias EXTI0_IRQHandler
weak_alias EXTI1_IRQHandler
weak_alias EXTI2_IRQHandler
weak_alias EXTI3_IRQHandler
weak_alias EXTI4_IRQHandler
weak_alias DMA1_Stream0_IRQHandler
weak_alias DMA1_Stream1_IRQHandler
weak_alias DMA1_Stream2_IRQHandler
weak_alias DMA1_Stream3_IRQHandler
weak_alias DMA1_Stream4_IRQHandler
weak_alias DMA1_Stream5_IRQHandler
weak_alias DMA1_Stream6_IRQHandler
weak_alias ADC_IRQHandler
weak_alias CAN1_TX_IRQHandler
weak_alias CAN1_RX0_IRQHandler
weak_alias CAN1_RX1_IRQHandler
weak_alias CAN1_SCE_IRQHandler
weak_alias EXTI9_5_IRQHandler
weak_alias TIM1_BRK_TIM9_IRQHandler
weak_alias TIM1_UP_TIM10_IRQHandler
weak_alias TIM1_TRG_COM_TIM11_IRQHandler
weak_alias TIM1_CC_IRQHandler
weak_alias TIM2_IRQHandler
weak_alias TIM3_IRQHandler
weak_alias TIM4_IRQHandler
weak_alias I2C1_EV_IRQHandler
weak_alias I2C1_ER_IRQHandler
weak_alias I2C2_EV_IRQHandler
weak_alias I2C2_ER_IRQHandler
weak_alias SPI1_IRQHandler
weak_alias SPI2_IRQHandler
weak_alias USART1_IRQHandler
weak_alias USART2_IRQHandler
weak_alias USART3_IRQHandler
weak_alias EXTI15_10_IRQHandler
weak_alias RTC_Alarm_IRQHandler
weak_alias OTG_FS_WKUP_IRQHandler
weak_alias TIM8_BRK_TIM12_IRQHandler
weak_alias TIM8_UP_TIM13_IRQHandler
weak_alias TIM8_TRG_COM_TIM14_IRQHandler
weak_alias TIM8_CC_IRQHandler
weak_alias DMA1_Stream7_IRQHandler
weak_alias FMC_IRQHandler
weak_alias SDIO_IRQHandler
weak_alias TIM5_IRQHandler
weak_alias SPI3_IRQHandler
weak_alias UART4_IRQHandler
weak_alias UART5_IRQHandler
weak_alias TIM6_DAC_IRQHandler
weak_alias TIM7_IRQHandler
weak_alias DMA2_Stream0_IRQHandler
weak_alias DMA2_Stream1_IRQHandler
weak_alias DMA2_Stream2_IRQHandler
weak_alias DMA2_Stream3_IRQHandler
weak_alias DMA2_Stream4_IRQHandler
weak_alias CAN2_TX_IRQHandler
weak_alias CAN2_RX0_IRQHandler
weak_alias CAN2_RX1_IRQHandler
weak_alias CAN2_SCE_IRQHandler
weak_alias OTG_FS_IRQHandler
weak_alias DMA2_Stream5_IRQHandler
weak_alias DMA2_Stream6_IRQHandler
weak_alias DMA2_Stream7_IRQHandler
weak_alias USART6_IRQHandler
weak_alias I2C3_EV_IRQHandler
weak_alias I2C3_ER_IRQHandler
weak_alias OTG_HS_EP1_OUT_IRQHandler
weak_alias OTG_HS_EP1_IN_IRQHandler
weak_alias OTG_HS_WKUP_IRQHandler
weak_alias OTG_HS_IRQHandler
weak_alias DCMI_IRQHandler
weak_alias FPU_IRQHandler
weak_alias SPI4_IRQHandler
weak_alias SAI1_IRQHandler
weak_alias SAI2_IRQHandler
weak_alias QuadSPI_IRQHandler
weak_alias HDMI_CEC_IRQHandler
weak_alias SPDIF_RX_IRQHandler
weak_alias FMPI2C1_EV_IRQHandler
weak_alias FMPI2C1_ER_IRQHandler

.section .note.GNU-stack, "", %progbits
