/* Script for ld -pie: link position independent executable */
SECTIONS
{
  .pcode ALIGN(0x1000) : { 
    __pcode_start = .;
    *(.pcode)
    . = ALIGN(0x1000);
    __pcode_end = .; 
  }
  .tcode ALIGN(0x1000) : { 
    __tcode_start = .;
    *(.tcode)
    . = ALIGN(0x1000);
    __tcode_end = .; 
  }
  .pdata ALIGN(0x1000) : { 
    __pdata_start = .;
    *(.pdata)
    . = ALIGN(0x1000);
    __pdata_end = .; 
  }
  .idtd ALIGN(0x1000) : { 
    *(.idtd)
    . = ALIGN(0x1000);
  }
}
