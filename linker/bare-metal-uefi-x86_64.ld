/* UEFI ELF: PT_LOAD aralıkları 4 KiB ayrık; heap/yığın da memsz ile ayrılır. */
ENTRY(uefi_entry)
PHDRS { kod PT_LOAD FLAGS(5); sabit PT_LOAD FLAGS(4); veri PT_LOAD FLAGS(6); }
SECTIONS {
 . = 0x2000000; /* OVMF: 8..23,5 MiB ayrılmış; 32 MiB boş bölgede ölçüldü. */
 .text : { *(.text*) } :kod
 . = ALIGN(4096);
 .rodata : { __rodata_start = .; *(.rodata*) __rodata_end = .; } :sabit
 . = ALIGN(4096);
 .data : { *(.data*) } :veri
 .bss (NOLOAD) : { __bss_start = .; *(.bss*) *(COMMON) __bss_end = .; } :veri
 .alan (NOLOAD) : {
  . = ALIGN(16); . += 0x4000; __stack_top = .;
  . = ALIGN(4096); __heap_start = .; . += 0x1C00000; __heap_end = .;
 } :veri
 /DISCARD/ : { *(.note*) *(.comment*) *(.eh_frame*) }
}
