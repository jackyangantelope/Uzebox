/* BSD-3-Clause. Exact existing RGB555 projection, Cortex-M33 little endian. */
#ifndef UZEBOX_RENDER_FAST_H
#define UZEBOX_RENDER_FAST_H
#include <stdint.h>
#include <stddef.h>
/* One shared worker is called by production and startup comparison. */
static void __attribute__((noinline,noclone))
uzebox_render_fast_line(const uint8_t *pixels,void *destination,const uint16_t *palette){
    uint8_t *dst=(uint8_t *)destination;
    if((uintptr_t)dst&3u){
        volatile uint8_t *out=dst;
        for(unsigned x=0;x<320u;x++){
            uint16_t color=palette[pixels[291u+(x*1456u)/320u]];
            out[x*2u]=(uint8_t)color;out[x*2u+1u]=(uint8_t)(color>>8u);
        }
        return;
    }
    const uint8_t *src=pixels+291u;
    for(unsigned group=0;group<16u;group++){
#define UZ_RENDER_PAIR(n,a,b) do{ \
        uint32_t pair=(uint32_t)palette[src[a]]|((uint32_t)palette[src[b]]<<16u); \
        __builtin_memcpy(dst+(n)*4u,&pair,4u); \
        }while(0)
        UZ_RENDER_PAIR(0,0,4);UZ_RENDER_PAIR(1,9,13);
        UZ_RENDER_PAIR(2,18,22);UZ_RENDER_PAIR(3,27,31);
        UZ_RENDER_PAIR(4,36,40);UZ_RENDER_PAIR(5,45,50);
        UZ_RENDER_PAIR(6,54,59);UZ_RENDER_PAIR(7,63,68);
        UZ_RENDER_PAIR(8,72,77);UZ_RENDER_PAIR(9,81,86);
#undef UZ_RENDER_PAIR
        src+=91u;dst+=40u;
    }
}
static inline void uzebox_render_fast(const uint8_t *pixels,unsigned pno,
                                     void *framebuffer,const uint16_t *palette){
    if(pno<19u||pno>=244u)return;
    uzebox_render_fast_line(pixels,(uint8_t *)framebuffer+(7u+pno-19u)*640u,palette);
}
#endif
