
/* Linker script for STM32L4 firmware, 200K flash, 64K RAM. */
/* 8k for config split out of the flash */

/* Define memory regions. */
MEMORY
{
    rom (rx) : ORIGIN  = 0x08002000, LENGTH = 200K
    ram (rwx) : ORIGIN = 0x20000000, LENGTH = 64K
}


/* Include the common ld script. */
INCLUDE cortex-m-generic.ld
