/*
******************************************************************************
**
**  File        : CW32L010_FLASH.ld
**
**  Author		: akako
**
**  Abstract    : Linker script for CW32L010 series
**                64Kbytes FLASH and 4Kbytes RAM
**
**                Set heap size, stack size and stack location according
**                to application requirements.
**
**                Set memory bank area and size if external memory is used.
**
**  Target      : Wuhan Xinyuan Semiconductor Co.,Ltd  CW32
**
**  Distribution: The file is distributed “as is,” without any warranty
**                of any kind.
**
**
*****************************************************************************
*/

/*入口点*/
ENTRY(Reset_Handler)

/*用户模式堆栈的最高地址*/
_estack = 0x20001000; /*ORIGIN(RAM) + LENGTH(RAM); 最大RAM地址 */
/*如果堆栈和堆不适应RAM，则生成链接错误*/
_Min_Heap_Size = 0;          /* No dynamic allocation. */
_Min_Stack_Size = 0x400; /* 堆栈所需的最小大小 */

/*指定内存区域*/
MEMORY
{
RAM (xrw)      : ORIGIN = 0x20000000, LENGTH = 4K
FLASH (rx)     : ORIGIN = 0x00000000, LENGTH = 0xFA00 /* two settings pages; keep final security page untouched */
}

/*指定代码和数据的放置位置*/
SECTIONS
{
    /*启动代码首先进入FLASH*/
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector)) /* Startup code */
    . = ALIGN(4);
  } >FLASH

  /*程序代码和其他数据进入FLASH*/
  .text :
  {
    . = ALIGN(4);
    *(.text)           /* .text sections (code) */
    *(.text*)          /* .text* sections (code) */
    *(.glue_7)         /* glue arm to thumb code */
    *(.glue_7t)        /* glue thumb to arm code */
    *(.eh_frame)

    KEEP (*(.init))
    KEEP (*(.fini))

    . = ALIGN(4);
    _etext = .;        /* define a global symbols at end of code */
    __etext = _etext;  /* alias for compatibility */
  } >FLASH

  /*常量数据进入FLASH*/
  .rodata :
  {
    . = ALIGN(4);
    *(.rodata)         /* .rodata sections (constants, strings, etc.) */
    *(.rodata*)        /* .rodata* sections (constants, strings, etc.) */
    . = ALIGN(4);
  } >FLASH

  /*确保异常表和异常索引表被正确地放置在 FLASH 内存区域中*/
  .ARM.extab   : { *(.ARM.extab* .gnu.linkonce.armextab.*) } >FLASH
  .ARM : {
    __exidx_start = .;
    *(.ARM.exidx*)
    __exidx_end = .;
  } >FLASH

  .preinit_array     :
  {
    PROVIDE_HIDDEN (__preinit_array_start = .);
    KEEP (*(.preinit_array*))
    PROVIDE_HIDDEN (__preinit_array_end = .);
  } >FLASH
  .init_array :
  {
    PROVIDE_HIDDEN (__init_array_start = .);
    KEEP (*(SORT(.init_array.*)))
    KEEP (*(.init_array*))
    PROVIDE_HIDDEN (__init_array_end = .);
  } >FLASH
  .fini_array :
  {
    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP (*(SORT(.fini_array.*)))
    KEEP (*(.fini_array*))
    PROVIDE_HIDDEN (__fini_array_end = .);
  } >FLASH

  /*由启动用来初始化数据*/
  _sidata = LOADADDR(.data);

  /* 初始化的数据部分进入RAM，在代码后加载LMA副本 */
  .data :
  {
    . = ALIGN(4);
    _sdata = .;        /* 在数据开始处创建一个全局符号 */
    *(.data)           /* .data 符号 */
    *(.data*)          /* .data* 符号 */

    . = ALIGN(4);
    _edata = .;        /* 在数据结束处定义一个全局符号 */
  } >RAM AT> FLASH


  /*未初始化数据节*/
  .bss :
  {
    . = ALIGN(4);
    /*启动代码使用此符号以初始化.bss节*/
    _sbss = .;         /* 在bss开始处定义一个全局符号 */
    __bss_start__ = _sbss;
    *(.bss)
    *(.bss*)
    *(COMMON)

    . = ALIGN(4);
    _ebss = .;         /* 在bss结束处定义一个全局符号 */
    __bss_end__ = _ebss;
  } >RAM

  /*用户堆栈节，用于检查是否有足够的RAM*/
  ._user_heap_stack :
  {
    . = ALIGN(8);
    PROVIDE ( end = . );
    PROVIDE ( _end = . );
    . = . + _Min_Heap_Size;
    . = . + _Min_Stack_Size;
    . = ALIGN(8);
  } >RAM

  /*从标准库中删除未使用的信息*/
  /* 注意: 请勿丢弃 libgcc.a，因为 Cortex-M0+ 没有硬件除法器，
     编译器会依赖其中的 __aeabi_uidiv / __aeabi_idiv 等辅助函数 */
  .ARM.attributes 0 : { *(.ARM.attributes) }
}
