/* Extra linker script (CMake: -Wl,-T,tools/link_pad.ld).  The vitasdk linker script starts the data segment at ALIGN(0x10000) after the text segment
   ("hacky hackity hack"), and vita-elf-create then needs ~4 KB of room at the end of the text segment for the SCE module info / NID tables:
   "Cannot allocate 3960 bytes for SCE data at end of segment 0; segment 1 overlaps" whenever the text ends within 4 KB below a 64 KB boundary (6 % of builds).
   This pads the text segment up to the next 64 KB boundary + 4 KB when that happens, so the build can no longer hit it. */
SECTIONS
{
  .dqiv_pad (READONLY) :
  {
    BYTE(0)
    . = . + ((((. & 0xFFFF) > 0xF000) || ((. & 0xFFFF) == 0)) ? (0x10000 - (. & 0xFFFF) + 0x1000) : 0);
  }
}
INSERT AFTER .eh_frame;
