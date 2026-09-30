    .text
    .globl oracle_sound,oracle_sound_end
oracle_sound:
    move.b %d0,0x20(%a3)
    move.l %d1,%d3
    andi.b #0xfd,%d1
    move.b %d1,0x22(%a3)
    move.b %d3,0x22(%a3)
    move.b %d2,0x20(%a3)
    andi.b #0xfd,%d1
    ori.b #0x80,%d1
    move.b %d1,0x22(%a3)
    move.b %d3,0x22(%a3)
oracle_sound_end:
    nop
