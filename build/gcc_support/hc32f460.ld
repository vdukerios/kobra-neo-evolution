/* Linker script for Anycubic Kobra Neo app (HC32F460), replicates Keil firmware.sct:
 *   LR_IROM1 0x00008000 0x00078000 ; RW_IRAM1 0x20000000 0x10000 ; RW_IRAM2 0x20010000 0x10000
 * Startup stack/heap sizes as in HDSC startup_hc32f46x.s: Stack 0x4400, Heap 0x3200. */
ENTRY(Reset_Handler)

MEMORY
{
  FLASH (rx)  : ORIGIN = 0x00008000, LENGTH = 0x00078000
  RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 0x00020000
}

_Min_Heap_Size  = 0x3200;
_Min_Stack_Size = 0x4400;

SECTIONS
{
  .isr_vector : { . = ALIGN(4); KEEP(*(.isr_vector)) . = ALIGN(4); } >FLASH

  .text : {
    . = ALIGN(4);
    *(.text .text.* )
    *(.rodata .rodata.* .gnu.linkonce.r.*)
    *(.glue_7) *(.glue_7t) *(.eh_frame)
    KEEP(*(.init)) KEEP(*(.fini))
    . = ALIGN(4);
  } >FLASH

  .ARM.extab : { *(.ARM.extab* .gnu.linkonce.armextab.*) } >FLASH
  .ARM.exidx : { __exidx_start = .; *(.ARM.exidx* .gnu.linkonce.armexidx.*) __exidx_end = .; } >FLASH

  .preinit_array : { . = ALIGN(4); PROVIDE_HIDDEN(__preinit_array_start = .); KEEP(*(.preinit_array*)) PROVIDE_HIDDEN(__preinit_array_end = .); } >FLASH
  .init_array : { . = ALIGN(4); PROVIDE_HIDDEN(__init_array_start = .); KEEP(*(SORT(.init_array.*))) KEEP(*(.init_array*)) PROVIDE_HIDDEN(__init_array_end = .); } >FLASH
  .fini_array : { . = ALIGN(4); PROVIDE_HIDDEN(__fini_array_start = .); KEEP(*(SORT(.fini_array.*))) KEEP(*(.fini_array*)) PROVIDE_HIDDEN(__fini_array_end = .); } >FLASH

  _sidata = LOADADDR(.data);

  .data : {
    . = ALIGN(4);
    _sdata = .;
    *(.data .data.*)
    *(.ramfunc .ramfunc.*)
    . = ALIGN(4);
    _edata = .;
  } >RAM AT> FLASH

  .bss (NOLOAD) : {
    . = ALIGN(4);
    _sbss = .;
    __bss_start__ = _sbss;
    *(.bss .bss.*)
    *(COMMON)
    . = ALIGN(4);
    _ebss = .;
    __bss_end__ = _ebss;
  } >RAM

  .heap (NOLOAD) : {
    . = ALIGN(8);
    __heap_base = .;
    . = . + _Min_Heap_Size;
    __heap_limit = .;
  } >RAM

  .stack (NOLOAD) : {
    . = ALIGN(8);
    . = . + _Min_Stack_Size;
    . = ALIGN(8);
    _estack = .;
  } >RAM

  /DISCARD/ : { *(.note.GNU-stack) *(.icg_sec) }
  .ARM.attributes 0 : { *(.ARM.attributes) }
}
