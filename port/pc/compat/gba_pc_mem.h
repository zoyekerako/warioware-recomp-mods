/* Substitutes real memory for the addresses the GBA hardware answered at.
 *
 * Force-included ahead of the game's own headers, so include/gba/gba.h finds
 * WWPC_MEMORY_BASES_OVERRIDDEN defined and leaves the bases alone.  Everything
 * the game writes -- IO registers, VRAM, palette RAM, OAM -- lands in these
 * arrays, which is exactly what the compositor reads back.
 *
 * The bases are unsigned long, not unsigned int: the game's headers model
 * addresses as 32-bit values, and truncating a 64-bit host pointer to that
 * produces an address that is merely plausible -- it faults on first use.
 */
#ifndef WWPC_GBA_PC_MEM_H
#define WWPC_GBA_PC_MEM_H

#define WWPC_MEMORY_BASES_OVERRIDDEN 1

extern unsigned char wwpc_bios[0x4000];
extern unsigned char wwpc_ewram_block[0x40000];
extern unsigned char wwpc_iwram[0x8000];
extern unsigned char wwpc_ioram[0x400];
extern unsigned char wwpc_palram[0x400];
extern unsigned char gba_vram[0x18000];
extern unsigned char gba_oam[0x400];
extern unsigned char wwpc_cartram[0x10000];

#define BIOSROMBase       ((unsigned long)wwpc_bios)
#define ExternWorkRAMBase ((unsigned long)wwpc_ewram_block)
#define InternWorkRAMBase ((unsigned long)wwpc_iwram)
#define IORAMBase         ((unsigned long)wwpc_ioram)
#define PaletteRAMBase    ((unsigned long)wwpc_palram)
#define VRAMBase          ((unsigned long)gba_vram)
#define OAMBase           ((unsigned long)gba_oam)
#define GameROMBase       0x08000000
#define GameROMImage1Base 0x0A000000
#define GameROMImage2Base 0x0C000000
#define CartRAMBase       ((unsigned long)wwpc_cartram)

#define VideoBuffer     (VRAMBase + 0x0)
#define BackBuffer      (VRAMBase + 0xA000)
#define BGPaletteMem    (PaletteRAMBase + 0x0)
#define OBJPaletteMem   (PaletteRAMBase + 0x200)

#endif
