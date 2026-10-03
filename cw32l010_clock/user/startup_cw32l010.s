/**
  ******************************************************************************
  * @file      startup_cw32l010.s
  * @author    Ankali-Aylina
  * @brief     CW32L010 devices vector table GCC toolchain.
  *            This module performs:
  *                - Set the initial SP
  *                - Set the initial PC == Reset_Handler,
  *                - Set the vector table entries with the exceptions ISR address
  *                - Branches to main in the C library (which eventually
  *                  calls main()).
  *            After Reset the Cortex-M0+ processor is in Thread mode,
  *            priority is Privileged, and the Stack is set to Main.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025-2029 Ankali-Aylina
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  * P.s: I hope this document can help you and make it easier for you to learn CW32L010.
  *      At the same time, when sharing this document, I hope you will not delete this paragraph.
  *      Only when everyone starts to maintain the open source environment can our open source spirit continue.
  *      Thank you for reading here!
  ******************************************************************************
  */

.syntax unified // 启用统一的ARM/Thumb汇编语法。
.cpu cortex-m0plus // 使用Cortex-M0+处理器
.fpu softvfp // 使用软件浮点运算，M0+无硬件FPU
.thumb // 使用Thumb指令集

.global g_pfnVectors // 声明全局变量g_pfnVectors
.global Default_Handler // 声明全局函数Default_Handler

/**
 * @brief  This is the code that gets called when the processor first
 *          starts execution following a reset event. Only the absolutely
 *          necessary set is performed, after which the application
 *          supplied main() routine is called.
 * @param  None
 * @retval None
*/

  /*定义一个名为 .text.Reset_Handler 的代码段（Section），专门存放 Reset_Handler 函数的代码 */
  .section .text.Reset_Handler

  /*将 Reset_Handler 声明为 弱符号（Weak Symbol） */
  .weak Reset_Handler

  /*将 Reset_Handler 声明为 函数类型（Function Type） */
  /*%function：表示符号是函数类型 */
  .type Reset_Handler, %function
Reset_Handler:
  ldr   r0, =_estack    /* 获取栈顶地址 */
  mov   sp, r0          /* set stack pointer 设置栈指针 */

/* Copy the data segment initializers from flash to SRAM */
/*将数据段初始化从闪存复制到SRAM */
ApplicationStart:
  ldr r0, =_sdata       // 加载_sdata的地址到r0寄存器（data段起始地址）
  ldr r1, =_edata       // 加载_edata的地址到r1寄存器（data段终止地址）
  ldr r2, =_sidata      // 加载_sidata的地址到r2寄存器（Flash中的初始化数据）
  movs r3, #0           // 将0移动到r3寄存器
  b LoopCopyDataInit    // 跳转到LoopCopyDataInit标签

CopyDataInit:
  ldr r4, [r2, r3]      // 从r2寄存器加上r3寄存器的值所指向的内存地址中，将数据加载到r4寄存器中（加载Flash中的初始化数据到r4）
  str r4, [r0, r3]      // 将r4寄存器中的数据存储到r0寄存器加上r3寄存器的值所指向的内存地址中(将r4中的数据存储到RAM中)
  adds r3, r3, #4       // 将r3寄存器的值加上4，并将结果存储到r3寄存器中(移动到下一个地址继续复制)


LoopCopyDataInit:
  adds r4, r0, r3       // 将RAM数据段起始地址与r3寄存器的值相加，并将结果存储到r4寄存器中(将r4的数据替换为data段下一位地址)
  cmp r4, r1            // 对比r4寄存器与RAM数据段结束地址（查看当前是否到达了RAM 数据段的结束地址）
  bcc CopyDataInit      // 如果r4寄存器的值小于r1寄存器的值（未到达RAM的数据段结束地址），则跳转到CopyDataInit标签


/* Zero fill the bss segment. */
/*零填充bss区段。*/
  ldr r2, =_sbss        // 将_sbss的地址加载到r2寄存器中（bss段起始地址）
  ldr r4, =_ebss        // 将_ebss的地址加载到r4寄存器中（bss段终止地址）
  movs r3, #0           // 将0移动到r3寄存器中（bss段的填充数值）
  b LoopFillZerobss     // 跳转到LoopFillZerobss标签处

FillZerobss:
  str  r3, [r2]         // 将r3寄存器中的值存储到r2寄存器所指向的内存地址中（将r2地址指向的数值填充为0）
  adds r2, r2, #4       // 将r2寄存器的值加上4，并将结果存储到r2寄存器中（移动到下一个地址继续填充）

LoopFillZerobss:
  cmp r2, r4          // 比较r2寄存器与_ebss的地址（如果到达结束地址的下一位地址则取消填充）
  bcc FillZerobss     // 如果r2寄存器的值小于_ebss的地址（是否达到bss段的结束地址），则跳转到FillZerobss标签处

/* Call the clock system initialization function.*/
/*调用时钟系统初始化函数。*/
  bl  SystemInit

/* Freestanding C application: no C++ constructors or libc runtime. */

/* Call the application s entry point.*/
/*调用应用程序的入口点。*/
  bl main

LoopForever:
  b LoopForever

 /*设置Reset_Handle的大小，值为：当前位置-（减去）Reset_Handler的起始位置 */
 /*.表示当前地址 Reset_Handler 表示 Reset_Handler 的起始位置*/
.size Reset_Handler, .-Reset_Handler
/**
 * @brief  This is the code that gets called when the processor receives an
 *         unexpected interrupt.  This simply enters an infinite loop, preserving
 *         the system state for examination by a debugger.
 *
 * @param  None
 * @retval None
*/

/**
* @brief这是当处理器收到
*意外中断。这只是进入一个无限循环，保留
*由调试器检查的系统状态。
*
* @param None
* @retval None
*/

/*定义一个名为 .text.Default_Handler 的代码段（Section） */
/*.text段用于存储可执行代码 */
/*"ax"为段属性标志 a:表示该段需要分配内存，x:表示该段可执行 */
/*%progbits：段类型，表示该段包含实际数据（代码或初始化数据） */
  .section .text.Default_Handler,"ax",%progbits
  .type Default_Handler, %function
  .thumb_func

/*Default_Handler：定义一个默认中断符号，出现未定义的中断触发时将跳转到这里 */
Default_Handler:

/*Infinite_Loop：定义一个循环符号， b Infinite_Loop：无条件跳转到Infinite_Loop实现死循环。 */
/*用途：当出现未被定义的中断时跳转至此进行死循环防止程序跑飞 */
Infinite_Loop:
  b Infinite_Loop

/*设置 Default_Handler 的大小，值为：当前位置-（减去）Default_Handler的起始位置 */
/*.表示当前地址 Default_Handler 表示 Default_Handler 的起始位置*/
  .size Default_Handler, .-Default_Handler

/******************************************************************************
*
* The minimal vector table for a Cortex M0.  Note that the proper constructs
* must be placed on this to ensure that it ends up at physical address
* 0x0000.0000.
*
******************************************************************************/
/******************************************************************************
*
*Cortex M0的最小向量表。请注意，正确的构造
*必须放置在此，以确保它最终到达物理地址
* 0x0000.0000。
*
******************************************************************************/
  /*定义一个名为 .isr_vector 的段（Section），专门用于存放中断向量表 */
  /*"a"段属性标志 a:表示该段需要分配内存 */
  /*%progbits：段类型，表示该段包含实际数据（中断服务函数的地址） */
  .section .isr_vector,"a",%progbits

  /*声明符号 g_pfnVectors 的类型为数据对象 */
  /*%object：表示符号对应的是一个数据对象（如数组、结构体等） */
  /*g_pfnVectors 是一个函数指针数组，每个元素指向一个中断服务函数 */
  .type g_pfnVectors, %object

g_pfnVectors:
  .word  _estack                        /* Top of Stack */
  .word  Reset_Handler                  /* Reset Handler */
  .word  NMI_Handler                    /* NMI Handler */
  .word  HardFault_Handler              /* Hard Fault Handler */
  .word  Default_Handler                /* Reserved */
  .word  Default_Handler                /* Reserved */
  .word  Default_Handler                /* Reserved */
  .word  Default_Handler                /* Reserved */
  .word  Default_Handler                /* Reserved */
  .word  Default_Handler                /* Reserved */
  .word  Default_Handler                /* Reserved */
  .word  SVC_Handler                    /* SVCall Handler */
  .word  Default_Handler                /* Reserved */
  .word  Default_Handler                /* Reserved */
  .word  PendSV_Handler                 /* PendSV Handler */
  .word  SysTick_Handler                /* SysTick Handler */

  .word WDT_IRQHandler            /* 0 Watch Dog Timer Interrupt Handler         */
  .word LVD_IRQHandler            /* 1 Low Voltage Detect Interrupt Handler      */
  .word RTC_IRQHandler            /* 2 Real Time Clock Interrupt Handler         */
  .word FLASHRAM_IRQHandler       /* 3 Flash/RAM Interrupt Handler               */
  .word SYSCTRL_IRQHandler        /* 4 System Control Interrupt Handler          */
  .word GPIOA_IRQHandler          /* 5 GPIOA Interrupt Handler                   */
  .word GPIOB_IRQHandler          /* 6 GPIOB Interrupt Handler                   */
  .word Default_Handler           /* 7 Reserved                                  */
  .word Default_Handler           /* 8 Reserved                                  */
  .word Default_Handler           /* 9 Reserved                                  */
  .word Default_Handler           /* 10 Reserved                                 */
  .word Default_Handler           /* 11 Reserved                                 */
  .word ADC_IRQHandler            /* 12 ADC Interrupt Handler                    */
  .word ATIM_IRQHandler           /* 13 Advanced Timer Interrupt Handler         */
  .word VC1_IRQHandler            /* 14 Voltage Comparator 1 Interrupt Handler   */
  .word VC2_IRQHandler            /* 15 Voltage Comparator 2 Interrupt Handler   */
  .word GTIM1_IRQHandler          /* 16 General Timer1 Interrupt Handler         */
  .word Default_Handler           /* 17 Reserved                                 */
  .word Default_Handler           /* 18 Reserved                                 */
  .word LPTIM_IRQHandler          /* 19 Low Power Timer Interrupt Handler        */
  .word BTIM1_IRQHandler          /* 20 Base Timer1 Interrupt Handler            */
  .word BTIM2_IRQHandler          /* 21 Base Timer2 Interrupt Handler            */
  .word BTIM3_IRQHandler          /* 22 Base Timer3 Interrupt Handler            */
  .word I2C1_IRQHandler           /* 23 I2C1 Interrupt Handler                   */
  .word Default_Handler           /* 24 Reserved                                 */
  .word SPI1_IRQHandler           /* 25 SPI Interrupt Handler                    */
  .word Default_Handler           /* 26 Reserved                                 */
  .word UART1_IRQHandler          /* 27 UART1 Interrupt Handler                  */
  .word UART2_IRQHandler          /* 28 UART2 Interrupt Handler                  */
  .word Default_Handler           /* 29 Reserved                                 */
  .word Default_Handler           /* 30 Reserved                                 */
  .word CLKFAULT_IRQHandler       /* 31 Clock Fault Interrupt Handler            */

  /*设置 g_pfnVectors 的大小为当前位置减去 g_pfnVectors 的起始位置 */
  /*设置符号 g_pfnVectors 的大小为整个中断向量表的字节长度 */
  .size g_pfnVectors, .-g_pfnVectors

/*******************************************************************************
*
*为Default_Handler的每个异常处理程序提供弱别名。
*由于它们是弱别名，任何同名的函数都将被覆盖
*这个定义。
*禁止在此重复弱定义Reset_Handler！！！
*******************************************************************************/
  .weak      NMI_Handler
  .thumb_set NMI_Handler,Default_Handler

  .weak      HardFault_Handler
  .thumb_set HardFault_Handler,Default_Handler

  .weak      SVC_Handler
  .thumb_set SVC_Handler,Default_Handler

  .weak      PendSV_Handler
  .thumb_set PendSV_Handler,Default_Handler

  .weak      SysTick_Handler
  .thumb_set SysTick_Handler,Default_Handler

  .weak      WDT_IRQHandler
  .thumb_set WDT_IRQHandler,Default_Handler

  .weak      LVD_IRQHandler
  .thumb_set LVD_IRQHandler,Default_Handler

  .weak      RTC_IRQHandler
  .thumb_set RTC_IRQHandler,Default_Handler

  .weak      FLASHRAM_IRQHandler
  .thumb_set FLASHRAM_IRQHandler,Default_Handler

  .weak      SYSCTRL_IRQHandler
  .thumb_set SYSCTRL_IRQHandler,Default_Handler

  .weak      GPIOA_IRQHandler
  .thumb_set GPIOA_IRQHandler,Default_Handler

  .weak      GPIOB_IRQHandler
  .thumb_set GPIOB_IRQHandler,Default_Handler

  .weak      ADC_IRQHandler
  .thumb_set ADC_IRQHandler,Default_Handler

  .weak      ATIM_IRQHandler
  .thumb_set ATIM_IRQHandler,Default_Handler

  .weak      VC1_IRQHandler
  .thumb_set VC1_IRQHandler,Default_Handler

  .weak      VC2_IRQHandler
  .thumb_set VC2_IRQHandler,Default_Handler

  .weak      GTIM1_IRQHandler
  .thumb_set GTIM1_IRQHandler,Default_Handler

  .weak      LPTIM_IRQHandler
  .thumb_set LPTIM_IRQHandler,Default_Handler

  .weak      BTIM1_IRQHandler
  .thumb_set BTIM1_IRQHandler,Default_Handler

  .weak      BTIM2_IRQHandler
  .thumb_set BTIM2_IRQHandler,Default_Handler

  .weak      BTIM3_IRQHandler
  .thumb_set BTIM3_IRQHandler,Default_Handler

  .weak      I2C1_IRQHandler
  .thumb_set I2C1_IRQHandler,Default_Handler

  .weak      SPI1_IRQHandler
  .thumb_set SPI1_IRQHandler,Default_Handler

  .weak      UART1_IRQHandler
  .thumb_set UART1_IRQHandler,Default_Handler

  .weak      UART2_IRQHandler
  .thumb_set UART2_IRQHandler,Default_Handler

  .weak      CLKFAULT_IRQHandler
  .thumb_set CLKFAULT_IRQHandler,Default_Handler
