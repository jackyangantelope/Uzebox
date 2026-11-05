/* BSD-3-Clause. A local guarded line checks the actual projection worker. */
#define UZ_RP16(b) (b)+0,(b)+1,(b)+2,(b)+3,(b)+4,(b)+5,(b)+6,(b)+7,(b)+8,(b)+9,(b)+10,(b)+11,(b)+12,(b)+13,(b)+14,(b)+15
#define UZ_RP256 UZ_RP16(0),UZ_RP16(16),UZ_RP16(32),UZ_RP16(48),UZ_RP16(64),UZ_RP16(80),UZ_RP16(96),UZ_RP16(112),UZ_RP16(128),UZ_RP16(144),UZ_RP16(160),UZ_RP16(176),UZ_RP16(192),UZ_RP16(208),UZ_RP16(224),UZ_RP16(240)
static const uint8_t uzebox_render_test_pixels[2048]={UZ_RP256,UZ_RP256,UZ_RP256,UZ_RP256,UZ_RP256,UZ_RP256,UZ_RP256,UZ_RP256};
#undef UZ_RP256
#undef UZ_RP16
static bool __attribute__((noinline,noclone)) uzebox_y_render_selftest(void){
    _Alignas(4) uint8_t guarded[648];uint16_t palette[256];
    for(unsigned i=0;i<256u;i++)palette[i]=rgb332_to_rgb555((uint8_t)i);
    for(unsigned align=0;align<4u;align++){
        memset(guarded,0xd3,sizeof(guarded));uint8_t *dst=guarded+4u+align;
        uzebox_render_fast_line(uzebox_render_test_pixels,dst,palette);
        for(unsigned x=0;x<320u;x++){
            uint16_t actual;memcpy(&actual,dst+x*2u,2u);
            if(actual!=palette[uzebox_render_test_pixels[291u+(x*1456u)/320u]])return false;
        }
        for(unsigned i=0;i<sizeof(guarded);i++)if((i<4u+align||i>=644u+align)&&guarded[i]!=0xd3u)return false;
    }
    return true;
}
