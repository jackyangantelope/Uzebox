/* GPL-3.0-or-later. Screened phase4 whole bitmap loop; other phases original P. */
static void __attribute__((noinline,section(".time_critical.cu_avr_bitmap_execute")))
uz_bitmap_original(cu_avr_bitmap_ctx_t *c)
{
 uint8 *p=c->pixels;
 auint i,j,color,grid=c->grid & c->ddrc;
 for (i=0U;i<c->n;i++){
  cu_avr_bitmap_store4(p,c->port); p[4]=(uint8)c->port;
  cu_avr_bitmap_store4(p+5,grid); p+=9;
  c->pos=(c->pos+1U)&255U;
  for (j=c->phase+1U;j<8U;j++){
   color=c->pixel & c->ddrc;
   cu_avr_bitmap_store4(p,color); cu_avr_bitmap_store4(p+4,color);
   cu_avr_bitmap_store4(p+8,color);
   if (j==7U){
    auint const a=c->x&4095U;
    c->bits=c->sram[a]; c->marks[a]|=CU_MEM_R;
    c->x=(c->x+1U)&65535U;
   }
   c->compare_pos=c->pos;
   c->pixel=cu_avr_bitmap_color(c,j==7U?128U:1U<<(6U-j));
   c->pos=(c->pos+1U)&255U;
   p[12]=(uint8)grid; p[13]=(uint8)grid; p[14]=(uint8)grid;
   if (j!=7U || i+1U!=c->n || c->tail_taken){ p[15]=(uint8)grid; p+=16; }
   else{ p+=15; }
  }
  c->port=grid;
  if (i+1U!=c->n || c->tail_taken){
   for(j=0U;j<=c->phase;j++){
    c->port=c->pixel & c->ddrc;
    cu_avr_bitmap_store4(p,c->port);p[4]=(uint8)c->port;
    p[5]=(uint8)c->port;p[6]=(uint8)c->port;
    c->compare_pos=c->pos;
    c->pixel=cu_avr_bitmap_color(c,1U<<(6U-j));
    if(j!=c->phase){
     p[7]=(uint8)c->port;cu_avr_bitmap_store4(p+8,c->port);
     cu_avr_bitmap_store4(p+12,grid);p+=16;
     c->pos=(c->pos+1U)&255U;
    }else{p+=7;}
   }
  }
 }
}
static inline void uz_bitmap_store4(uint8 *p,auint color){uint32 word=(uint32)(uint8)color*0x01010101U;memcpy(p,&word,4U);}
static void __attribute__((noinline,section(".time_critical.uz_bitmap_local"))) uz_bitmap_local(cu_avr_bitmap_ctx_t *c){
 uz_scan_bitmap_entries++;
 cu_avr_bitmap_ctx_t q=*c;q.phase=4U;
uint8 *p=q.pixels;auint i,color,grid=q.grid&q.ddrc;
for(i=0;i<q.n;i++){
uz_bitmap_store4(p,q.port);p[4]=(uint8)q.port;uz_bitmap_store4(p+5,grid);p+=9;q.pos=(q.pos+1U)&255U;
switch(q.phase){case 0U:goto Cell1;case 1U:goto Cell2;case 2U:goto Cell3;case 3U:goto Cell4;case 4U:goto Cell5;case 5U:goto Cell6;case 6U:goto Cell7;default:break;}
Cell1:
color=q.pixel&q.ddrc;uz_bitmap_store4(p,color);uz_bitmap_store4(p+4,color);uz_bitmap_store4(p+8,color);
q.compare_pos=q.pos;q.pixel=cu_avr_bitmap_color(&q,32U);q.pos=(q.pos+1U)&255U;
uz_bitmap_store4(p+12,grid);p+=16;
Cell2:
color=q.pixel&q.ddrc;uz_bitmap_store4(p,color);uz_bitmap_store4(p+4,color);uz_bitmap_store4(p+8,color);
q.compare_pos=q.pos;q.pixel=cu_avr_bitmap_color(&q,16U);q.pos=(q.pos+1U)&255U;
uz_bitmap_store4(p+12,grid);p+=16;
Cell3:
color=q.pixel&q.ddrc;uz_bitmap_store4(p,color);uz_bitmap_store4(p+4,color);uz_bitmap_store4(p+8,color);
q.compare_pos=q.pos;q.pixel=cu_avr_bitmap_color(&q,8U);q.pos=(q.pos+1U)&255U;
uz_bitmap_store4(p+12,grid);p+=16;
Cell4:
color=q.pixel&q.ddrc;uz_bitmap_store4(p,color);uz_bitmap_store4(p+4,color);uz_bitmap_store4(p+8,color);
q.compare_pos=q.pos;q.pixel=cu_avr_bitmap_color(&q,4U);q.pos=(q.pos+1U)&255U;
uz_bitmap_store4(p+12,grid);p+=16;
Cell5:
color=q.pixel&q.ddrc;uz_bitmap_store4(p,color);uz_bitmap_store4(p+4,color);uz_bitmap_store4(p+8,color);
q.compare_pos=q.pos;q.pixel=cu_avr_bitmap_color(&q,2U);q.pos=(q.pos+1U)&255U;
uz_bitmap_store4(p+12,grid);p+=16;
Cell6:
color=q.pixel&q.ddrc;uz_bitmap_store4(p,color);uz_bitmap_store4(p+4,color);uz_bitmap_store4(p+8,color);
q.compare_pos=q.pos;q.pixel=cu_avr_bitmap_color(&q,1U);q.pos=(q.pos+1U)&255U;
uz_bitmap_store4(p+12,grid);p+=16;
Cell7:
color=q.pixel&q.ddrc;uz_bitmap_store4(p,color);uz_bitmap_store4(p+4,color);uz_bitmap_store4(p+8,color);
q.bits=q.sram[q.x&4095U];q.marks[q.x&4095U]|=CU_MEM_R;q.x=(q.x+1U)&65535U;
q.compare_pos=q.pos;q.pixel=cu_avr_bitmap_color(&q,128U);q.pos=(q.pos+1U)&255U;
p[12]=(uint8)grid;p[13]=(uint8)grid;p[14]=(uint8)grid;if(i+1U!=q.n || q.tail_taken){p[15]=(uint8)grid;p+=16;}else p+=15;
q.port=grid;
if(i+1U!=q.n || q.tail_taken){
q.port=q.pixel&q.ddrc;uz_bitmap_store4(p,q.port);p[4]=(uint8)q.port;p[5]=(uint8)q.port;p[6]=(uint8)q.port;q.compare_pos=q.pos;q.pixel=cu_avr_bitmap_color(&q,64U);
if(q.phase==0U){p+=7;goto AfterTail;}
p[7]=(uint8)q.port;uz_bitmap_store4(p+8,q.port);uz_bitmap_store4(p+12,grid);p+=16;q.pos=(q.pos+1U)&255U;
q.port=q.pixel&q.ddrc;uz_bitmap_store4(p,q.port);p[4]=(uint8)q.port;p[5]=(uint8)q.port;p[6]=(uint8)q.port;q.compare_pos=q.pos;q.pixel=cu_avr_bitmap_color(&q,32U);
if(q.phase==1U){p+=7;goto AfterTail;}
p[7]=(uint8)q.port;uz_bitmap_store4(p+8,q.port);uz_bitmap_store4(p+12,grid);p+=16;q.pos=(q.pos+1U)&255U;
q.port=q.pixel&q.ddrc;uz_bitmap_store4(p,q.port);p[4]=(uint8)q.port;p[5]=(uint8)q.port;p[6]=(uint8)q.port;q.compare_pos=q.pos;q.pixel=cu_avr_bitmap_color(&q,16U);
if(q.phase==2U){p+=7;goto AfterTail;}
p[7]=(uint8)q.port;uz_bitmap_store4(p+8,q.port);uz_bitmap_store4(p+12,grid);p+=16;q.pos=(q.pos+1U)&255U;
q.port=q.pixel&q.ddrc;uz_bitmap_store4(p,q.port);p[4]=(uint8)q.port;p[5]=(uint8)q.port;p[6]=(uint8)q.port;q.compare_pos=q.pos;q.pixel=cu_avr_bitmap_color(&q,8U);
if(q.phase==3U){p+=7;goto AfterTail;}
p[7]=(uint8)q.port;uz_bitmap_store4(p+8,q.port);uz_bitmap_store4(p+12,grid);p+=16;q.pos=(q.pos+1U)&255U;
q.port=q.pixel&q.ddrc;uz_bitmap_store4(p,q.port);p[4]=(uint8)q.port;p[5]=(uint8)q.port;p[6]=(uint8)q.port;q.compare_pos=q.pos;q.pixel=cu_avr_bitmap_color(&q,4U);
if(q.phase==4U){p+=7;goto AfterTail;}
p[7]=(uint8)q.port;uz_bitmap_store4(p+8,q.port);uz_bitmap_store4(p+12,grid);p+=16;q.pos=(q.pos+1U)&255U;
q.port=q.pixel&q.ddrc;uz_bitmap_store4(p,q.port);p[4]=(uint8)q.port;p[5]=(uint8)q.port;p[6]=(uint8)q.port;q.compare_pos=q.pos;q.pixel=cu_avr_bitmap_color(&q,2U);
if(q.phase==5U){p+=7;goto AfterTail;}
p[7]=(uint8)q.port;uz_bitmap_store4(p+8,q.port);uz_bitmap_store4(p+12,grid);p+=16;q.pos=(q.pos+1U)&255U;
q.port=q.pixel&q.ddrc;uz_bitmap_store4(p,q.port);p[4]=(uint8)q.port;p[5]=(uint8)q.port;p[6]=(uint8)q.port;q.compare_pos=q.pos;q.pixel=cu_avr_bitmap_color(&q,1U);
if(q.phase==6U){p+=7;goto AfterTail;}
p[7]=(uint8)q.port;uz_bitmap_store4(p+8,q.port);uz_bitmap_store4(p+12,grid);p+=16;q.pos=(q.pos+1U)&255U;
}
AfterTail:;
}

*c=q;
}

static void __attribute__((noinline,section(".time_critical.cu_avr_bitmap_execute"))) cu_avr_bitmap_execute(cu_avr_bitmap_ctx_t *c){
 if(uz_function_mode && c->phase==4U)uz_bitmap_local(c);else uz_bitmap_original(c);
}

