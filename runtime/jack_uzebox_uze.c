/* BSD-3-Clause parser for external Uzebox UZE content. */
#include "jack_uzebox_uze.h"
#include <string.h>
static uint16_t le16(const uint8_t *p){return (uint16_t)((uint16_t)p[0]|((uint16_t)p[1]<<8u));}
static uint32_t le32(const uint8_t *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8u)|((uint32_t)p[2]<<16u)|((uint32_t)p[3]<<24u);}
uint32_t jack_uzebox_crc32(uint32_t previous,const void *data,size_t size){
    uint32_t crc=~previous;const uint8_t *p=(const uint8_t *)data;
    for(size_t i=0;i<size;++i){crc^=p[i];for(unsigned bit=0;bit<8u;++bit)crc=(crc>>1u)^(0xedb88320u&(0u-(crc&1u)));}
    return ~crc;
}
bool jack_uzebox_parse_uze(const uint8_t *data,size_t size,jack_uzebox_uze_info *info){
    static const uint8_t magic[6]={'U','Z','E','B','O','X'};
    if(!data||!info||size<512u||memcmp(data,magic,sizeof(magic))!=0)return false;
    const uint32_t program_size=le32(data+8u);
    if(data[6]!=1u||data[7]!=0u||program_size==0u||program_size>65535u||size<512u+(size_t)program_size)return false;
    memset(info,0,sizeof(*info));info->version=data[6];info->target=data[7];info->program_size=program_size;info->year=le16(data+12u);
    memcpy(info->name,data+14u,32u);info->name[32]='\0';memcpy(info->author,data+46u,32u);info->author[32]='\0';
    info->header_crc32=le32(data+334u);info->mouse=data[338u];memcpy(info->description,data+339u,64u);info->description[64]='\0';info->program=data+512u;
    return info->header_crc32==0u||jack_uzebox_crc32(0u,info->program,program_size)==info->header_crc32;
}