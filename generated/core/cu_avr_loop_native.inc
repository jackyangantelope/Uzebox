/* Opt-in closed-loop experiment. Recognition binds register roles, rather
 * than cartridge names, ROM hashes or PCs. A complete fixed-shape loop runs
 * in one native call; the SRAM Thumb template has its own backward branch.
 * No hardware event, interrupt or row boundary may occur inside the batch.
 * Unsupported instructions and overlapping register roles use cu_avr_exec.
 *
 * The tile loop is a 21-instruction LPM/PIXEL/LD/MUL/address/DEC/BRNE idiom.
 * It is deliberately a small semantic-loop translator, not a general AVR
 * JIT. Only flags live at loop exit are materialized. All guest cycles and
 * pixels remain exact, including the one-cycle final BRNE.
 */
#if defined(CU_AVR_LOOP_NATIVE) && defined(CU_AVR_LOOP_REFERENCE)
#error "Choose native or reference closed loops"
#endif
#if defined(CU_AVR_LOOP_NATIVE) && !defined(__thumb__)
#error "Native closed loops require ARM Thumb-2"
#endif

typedef struct {
 uint8 *crom, *sram, *pixels, *marks;
 auint z, x, iterations, pixel, product, work, base, stride, ddrc;
 auint tail_taken, pixel_raw;
} cu_avr_loop_ctx_t;

typedef struct {
 uint16 pc;
 uint8 valid, kind, pixel, work, base, stride, count, pointer;
 uint16 branch_delta;
 uint8 living, cursor_color, grid, cursor, stop, phase;
} cu_avr_loop_binding_t;

static cu_avr_loop_binding_t cu_avr_loop_cache[32U];
static cu_avr_loop_stats_t cu_avr_loop_counters;
static boole cu_avr_loop_disabled;
static void cu_avr_loop_classify(auint base, auint len);

void cu_avr_loop_stats(cu_avr_loop_stats_t *out)
{
 *out = cu_avr_loop_counters;
}

static void cu_avr_loop_invalidate(void)
{
 /* Reclassify the entire ROM after any edit. An instruction anywhere in a
  * loop body can invalidate a head up to 20 words before it, including over
  * the physical ROM wrap; a full rescan keeps the invalidation rule simple. */
 cu_avr_loop_classify(0U, 32768U);
 cu_avr_loop_counters.invalidations ++;
}

/* This independent C path specifies the semantic-loop template and supplies
 * a host oracle. Production ARM tests also compare to cu_avr_exec itself. */
static void cu_avr_loop_reference(cu_avr_loop_ctx_t *c, auint kind)
{
 auint i, j, k, pos = 0U;
 for (i = 0U; i < c->iterations; i++){
  if (kind == 1U){
   for (j = 0U; j < 3U; j++){ c->pixels[pos++] = c->pixel; }
   for (j = 0U; j < 6U; j++){
    c->pixel_raw = c->crom[(c->z + j) & 0xFFFFU];
    c->pixel = c->pixel_raw & c->ddrc;
    for (k = 0U; k < (j == 5U ? 2U : 6U); k++){
     c->pixels[pos++] = c->pixel;
    }
   }
   c->product = c->sram[c->x & 0x0FFFU] * c->stride;
   c->marks[c->x & 0x0FFFU] |= CU_MEM_R;
   c->x = (c->x + 1U) & 0xFFFFU;
   c->work = c->base + c->product;
   c->z = c->work & 0xFFFFU;
  }else{
   c->pixels[pos++] = c->pixel;
   c->pixels[pos++] = c->pixel;
  }
  if (i + 1U < c->iterations || c->tail_taken){
   c->pixels[pos++] = c->pixel;
  }
 }
}

#ifdef CU_AVR_LOOP_NATIVE
_Static_assert(sizeof(uintptr_t) == 4U && sizeof(auint) == 4U,
               "Closed-loop Thumb ABI requires 32-bit pointers and auint");
_Static_assert(sizeof(cu_avr_loop_ctx_t) == 60U,
               "Closed-loop Thumb context layout changed");

/* Generated from the adjacent cu_avr_loop_native_template.s
 * (Cortex-M33, no FPU).
 * The 0x108 entry is the delay kernel. This writable, aligned initialized
 * array lives in SRAM (.data); no flash trampoline executes the guest loop.
 *
 * tile: r0=ROM, r1=SRAM, r2=pixel cursor, r3=scratch, r4=Z, r5=X/Y,
 * r6=batch remaining, r7=PORTC, r8=product, r9=next Z, r10=tile base,
 * r11=stride, r12=context. The backedge retains all these live values.
 * Stores cover old PORTC cycles 1..3, six colors at 4,10,16,22,28,34.
 * All but the sixth occupy six cycles; the sixth occupies three for taken
 * BRNE and two for the final untaken branch. R4 wraps every LPM address.
 */
_Alignas(4) static uint16 cu_avr_loop_thumb[] = {
 0xE92DU,0x4FF0U,0x4684U,0x6904U,0x6945U,0x6986U,0x69C7U,0xF8D0U,
 0xA028U,0xF8D0U,0xB02CU,0x6882U,0x6841U,0x6800U,0x7017U,0x7057U,
 0x7097U,0x5D07U,0x3401U,0xB2A4U,0xF8DCU,0x3030U,0x401FU,0x70D7U,
 0x7117U,0x7157U,0x7197U,0x71D7U,0x7217U,0xF3C5U,0x030BU,0xF811U,
 0x8003U,0xF8DCU,0x900CU,0xF819U,0x7003U,0xF047U,0x0701U,0xF809U,
 0x7003U,0x3501U,0xB2ADU,0x5D07U,0x3401U,0xB2A4U,0xF8DCU,0x3030U,
 0x401FU,0x7257U,0x7297U,0x72D7U,0x7317U,0x7357U,0x7397U,0xFB08U,
 0xF80BU,0x5D07U,0x3401U,0xB2A4U,0xF8DCU,0x3030U,0x401FU,0x73D7U,
 0x7417U,0x7457U,0x7497U,0x74D7U,0x7517U,0xEB08U,0x090AU,0x5D07U,
 0x3401U,0xB2A4U,0xF8DCU,0x3030U,0x401FU,0x7557U,0x7597U,0x75D7U,
 0x7617U,0x7657U,0x7697U,0x5D07U,0x3401U,0xB2A4U,0xF8DCU,0x3030U,
 0x401FU,0x76D7U,0x7717U,0x7757U,0x7797U,0x77D7U,0xF882U,0x7020U,
 0x5D07U,0xF8CCU,0x7038U,0xF8DCU,0x3030U,0x401FU,0xF882U,0x7021U,
 0xF882U,0x7022U,0xFA1FU,0xF489U,0x3E01U,0xD003U,0xF882U,0x7023U,
 0x3224U,0xE79BU,0xF8DCU,0x3034U,0xB10BU,0xF882U,0x7023U,0xF8CCU,
 0x4010U,0xF8CCU,0x5014U,0xF8CCU,0x701CU,0xF8CCU,0x8020U,0xF8CCU,
 0x9024U,0xE8BDU,0x8FF0U,0xBF00U,0xB510U,0x6881U,0x6982U,0x69C3U,
 0x700BU,0x704BU,0x3A01U,0xD002U,0x708BU,0x3103U,0xE7F8U,0x6B44U,
 0xB104U,0x708BU,0xBD10U,0xBF00U,
};
_Static_assert(sizeof(cu_avr_loop_thumb) == 0x128U,
               "Closed-loop template size or delay entry changed");

static void cu_avr_loop_execute(cu_avr_loop_ctx_t *ctx, auint kind)
{
 uintptr_t entry = (uintptr_t)&cu_avr_loop_thumb[0];
 if (kind == 2U){ entry += 0x108U; }
 ((void (*)(cu_avr_loop_ctx_t *))(entry | 1U))(ctx);
}

/* Isolated SRAM execution check. It never touches emulated CPU state. A
 * semantic mismatch permanently disables this experiment until reboot. */
static boole __attribute__((noinline)) cu_avr_loop_tiles_selftest(void)
{
 /* Two iterations read only SRAM indices 256 and 257. */
 uint8 rom[32U], ram[258U], marks[258U], expected_marks[258U];
 uint8 pixels[80U], expected_pixels[80U];
 cu_avr_loop_ctx_t c, e;
 auint i, kind, tail;
 boole ok = TRUE;
 for (i = 0U; i < sizeof(rom); i++){ rom[i] = (uint8)(i * 19U); }
 memset(ram, 0, sizeof(ram));
 ram[256U] = 4U;
 ram[257U] = 7U;
 __asm volatile("dsb sy\n\tisb sy" ::: "memory");
 for (kind = 1U; kind <= 2U; kind++){
  for (tail = 0U; tail <= 1U; tail++){
   memset(marks, 0x82, sizeof(marks));
   memset(expected_marks, 0x82, sizeof(expected_marks));
   memset(pixels, 0xDA, sizeof(pixels));
   memset(expected_pixels, 0xDA, sizeof(expected_pixels));
   memset(&c, 0, sizeof(c));
   c.crom = rom; c.sram = ram; c.pixels = pixels; c.marks = marks;
   c.z = 1U; c.x = 256U; c.iterations = 2U; c.pixel = 0xABU;
   c.base = 3U; c.stride = 1U; c.ddrc = 0x5AU; c.tail_taken = tail;
   e = c; e.pixels = expected_pixels; e.marks = expected_marks;
   cu_avr_loop_reference(&e, kind);
   cu_avr_loop_execute(&c, kind);
   ok = ok && c.z == e.z && c.x == e.x && c.pixel == e.pixel &&
        c.product == e.product && c.work == e.work &&
        c.pixel_raw == e.pixel_raw &&
        memcmp(pixels, expected_pixels, sizeof(pixels)) == 0 &&
        memcmp(marks, expected_marks, sizeof(marks)) == 0;
  }
 }
 if (!ok){ cu_avr_loop_disabled = TRUE; }
 return ok;
}
#else
static void cu_avr_loop_execute(cu_avr_loop_ctx_t *ctx, auint kind)
{
 cu_avr_loop_reference(ctx, kind);
}
#endif

static auint cu_avr_loop_a(uint32 op) { return (op >> 8) & 0xFFU; }
static auint cu_avr_loop_b(uint32 op) { return (op >> 16) & 0xFFFFU; }

#include "uzebox_mode1.inc"
#include "uzebox_poll.inc"
#include "cu_avr_bitmap_native.inc"
#include "cu_avr_wait_native.inc"
#include "cu_avr_pure_native.inc"

#ifdef CU_AVR_LOOP_NATIVE
boole cu_avr_loop_selftest(void)
{
 boole ok;
 if(!pflags_done){cu_avrfg_fill(cpu_pflags);pflags_done=TRUE;}
 ok=cu_avr_loop_tiles_selftest() && cu_avr_bitmap_selftest() && cu_avr_pure_selftest() && cu_avr_span_selftest();
#ifdef CU_AVR_REGION
 if(ok){ok=cu_avr_region_selftest();}
#ifdef CU_AVR_FUSED
 if(ok){ok=cu_avr_fused_selftest();}
#endif
#endif
 if (!ok){ cu_avr_loop_disabled=TRUE; }
 return ok;
}
#endif

static void cu_avr_loop_bind(cu_avr_loop_binding_t *b, auint pc)
{
 static uint8 const ids[21U] = {
  0x19U,0x49U,0x23U,0x19U,0x49U,0x37U,0x19U,0x49U,
  0x01U,0x09U,0x19U,0x49U,0x0DU,0x00U,0x19U,0x49U,
  0x19U,0x01U,0x2BU,0x49U,0x43U
 };
 static uint8 const loads[6U] = {0U,3U,6U,10U,14U,16U};
 static uint8 const outs[6U] = {1U,4U,7U,11U,15U,19U};
 uint32 op[21U], used = 3U;
 auint i, r[11U], n, stride;
 memset(b, 0, sizeof(*b));
 b->pc = (uint16)pc;
 b->valid = 1U;
 op[0] = cpu_code[pc];
 if((op[0]&127U)==0x19U && cu_avr_mode1_bind(b,pc))return;
 if ((op[0]&0x7FU)==0x2DU){ if(!cu_poll_bind(b,pc))cu_avr_wait_bind(b,pc); return; }
 if ((op[0]&0x7FU)==0x18U){ cu_avr_bitmap_bind(b,pc); return; }
 if ((op[0] & 0x7FU) == 0x2BU){
  op[1] = cpu_code[(pc + 1U) & 0x7FFFU];
  if (cu_avr_loop_a(op[0]) >= 32U || (op[1] & 0x7FU) != 0x43U ||
      cu_avr_loop_a(op[1]) != SREG_ZM ||
      ((2U + cu_avr_loop_b(op[1])) & 0x7FFFU) != 0U){ return; }
  b->kind = 2U;
  b->count = (uint8)cu_avr_loop_a(op[0]);
  b->branch_delta = (uint16)cu_avr_loop_b(op[1]);
  cu_avr_loop_counters.builds ++;
  return;
 }
 for (i = 0U; i < 21U; i++){
  op[i] = cpu_code[(pc + i) & 0x7FFFU];
  if ((op[i] & 0x7FU) != ids[i]){ return; }
 }
 b->pixel = (uint8)cu_avr_loop_a(op[0]);
 b->work = (uint8)cu_avr_loop_a(op[2]);
 b->pointer = (uint8)cu_avr_loop_b(op[2]);
 b->base = (uint8)cu_avr_loop_b(op[8]);
 b->count = (uint8)cu_avr_loop_a(op[18]);
 if (cu_avr_loop_b(op[2]) != b->pointer ||
     cu_avr_loop_b(op[8]) != b->base){ return; }
 if (cu_avr_loop_a(op[5]) == b->work){ stride = cu_avr_loop_b(op[5]); }
 else if (cu_avr_loop_b(op[5]) == b->work){ stride = cu_avr_loop_a(op[5]); }
 else{ return; }
 if (stride >= 32U){ return; }
 b->stride = (uint8)stride;
 for (i = 0U; i < 6U; i++){
  if (cu_avr_loop_a(op[loads[i]]) != b->pixel ||
      cu_avr_loop_b(op[outs[i]]) != b->pixel){ return; }
 }
 if ((b->work & 1U) != 0U || b->work >= 31U ||
     (b->base & 1U) != 0U || b->base >= 31U ||
     (b->pointer != 26U && b->pointer != 28U) ||
     cu_avr_loop_a(op[8]) != b->work ||
     cu_avr_loop_a(op[9]) != b->work || cu_avr_loop_b(op[9]) != 0U ||
     cu_avr_loop_a(op[12]) != b->work + 1U || cu_avr_loop_b(op[12]) != 1U ||
     cu_avr_loop_a(op[17]) != 30U || cu_avr_loop_b(op[17]) != b->work ||
     cu_avr_loop_a(op[20]) != SREG_ZM ||
     ((21U + cu_avr_loop_b(op[20])) & 0x7FFFU) != 0U){ return; }
 /* Every role must be disjoint: MUL writes r1:r0, LD updates its pointer,
  * LPM updates Z, work doubles as the LD destination and address pair. */
 r[0]=b->pixel; r[1]=b->work; r[2]=b->work+1U;
 r[3]=b->base; r[4]=b->base+1U; r[5]=b->stride;
 r[6]=b->count; r[7]=b->pointer; r[8]=b->pointer+1U;
 r[9]=30U; r[10]=31U;
 for (i = 0U; i < 11U; i++){
  n = r[i];
  if (n >= 32U || (used & ((uint32)1U << n)) != 0U){ return; }
  used |= (uint32)1U << n;
 }
 b->branch_delta = (uint16)cu_avr_loop_b(op[20]);
 b->kind = 1U;
 cu_avr_loop_counters.builds ++;
}

/* Word-range helper also permits small synthetic fixtures to classify only
 * their candidate heads. Production invalidation always scans the full ROM.
 * Bit31 belongs to ignored arg2 for these two opcodes; branch offsets and
 * skip-length bit7 retain all their original bits. No guest state changes. */
static void cu_avr_loop_classify(auint base, auint len)
{
 cu_avr_loop_binding_t binding;
 auint i, pc, id;
 auint const builds = cu_avr_loop_counters.builds;
 /* The packed code table is private to the compiler. Validate every pure
  * operand before allowing unchecked register-array indices in SRAM code;
  * any invalid compiled operand disables only pure regions until rescan. */
 cu_avr_pure_ok=TRUE;
 for(i=0U;i<32768U;i++){
  id=cpu_code[i]&127U;
  if(cu_avr_pure_types[id] && !cu_avr_pure_supported(cpu_code[i])){cu_avr_pure_ok=FALSE;break;}
 }
 memset(cu_avr_loop_cache, 0, sizeof(cu_avr_loop_cache));
 if (len > 32768U){ len = 32768U; }
 for (i = 0U; i < len; i++){
  pc = (base + i) & 0x7FFFU;
  id = cpu_code[pc] & 0x7FU;
  if (id == 0x11U || id == 0x18U || id == 0x19U || id == 0x2BU){ cpu_code[pc] &= ~CU_AVR_LOOP_HEAD_MARKER; }
  if (id == 0x2DU){ cpu_code[pc] &= ~CU_AVR_LOOP_CALL_MARKER; }
 }
 for (i = 0U; i < len; i++){
  pc = (base + i) & 0x7FFFU;
  id = cpu_code[pc] & 0x7FU;
  if (id == 0x18U || id == 0x19U || id == 0x2BU || id == 0x2DU){
   cu_avr_loop_bind(&binding, pc);
   if (binding.kind != 0U){ cpu_code[pc] |= id==0x2DU?CU_AVR_LOOP_CALL_MARKER:CU_AVR_LOOP_HEAD_MARKER; }
  }
 }
 if(CU_AVR_ALU_ENABLED){
 for(i=0U;i<len;i++){
  auint target;
  pc=(base+i)&32767U;id=cpu_code[pc]&127U;
  if((id==0x40U||id==0x42U||id==0x43U) && (cpu_code[pc]&0x80000000U)){
   target=(pc+1U+cu_avr_loop_b(cpu_code[pc]))&32767U;
   if((cpu_code[target]&127U)==0x11U && cu_avr_pure_qualify(target)){
    cpu_code[target]|=CU_AVR_LOOP_HEAD_MARKER;
   }
  }
  /* Long straight register regions are also safe reentry points after an
   * event. Eligibility still requires twelve supported packed words, and
   * the executor discards dynamic paths shorter than eight instructions. */
  if(id==0x11U && cu_avr_pure_qualify(pc)){cpu_code[pc]|=CU_AVR_LOOP_HEAD_MARKER;}
 }
 }
 /* Build statistics count runtime bindings, not compile-time eligibility. */
#ifdef CU_AVR_REGION
 cu_avr_region_classify(base,len);
#endif
 uz_aot_classify();
 cu_avr_loop_counters.builds = builds;
}

__attribute__((noinline,noclone,no_icf,section(".aa_flash_loop")))
boole cu_avr_loop_run(void)
{
 auint const pc = cpu_state.pc & 0x7FFFU;
 auint const id = cpu_code[pc] & 0x7FU;
 cu_avr_loop_binding_t *b;
 cu_avr_loop_ctx_t ctx;
 auint count, remaining, budget, event_budget, per, n, cycles, flags;
 auint src, dst, length;
 if (cu_avr_loop_disabled || (id != 0x11U && id != 0x18U && id != 0x19U && id != 0x2BU && id != 0x2DU)){ return FALSE; }
 if(id==0x11U){
  return (CU_AVR_ALU_ENABLED && (cpu_code[pc]&CU_AVR_LOOP_HEAD_MARKER))?cu_avr_pure_run():FALSE;
 }
 b = &cu_avr_loop_cache[(pc ^ (pc >> 5)) & 31U];
 if (!b->valid || b->pc != pc){ cu_avr_loop_bind(b, pc); }
 if (b->kind == 0U){ return FALSE; }
 if (event_it || event_it_enter){
  cu_avr_loop_counters.event_rejects ++; return FALSE;
 }
 if (video_cycle >= VIDEO_CY_MAX){
  cu_avr_loop_counters.row_rejects ++; return FALSE;
 }
 if (b->kind==7U){return cu_poll_run(b);}
 if (b->kind==6U){return cu_avr_mode1_run(b);}
 if (b->kind==3U){ return cu_avr_bitmap_run(b); }
 if (b->kind==4U){ return cu_avr_wait_run(b); }
 count = cpu_state.iors[b->count];
 remaining = count != 0U ? count : 256U;
 per = b->kind == 1U ? 36U : 3U;
 length = b->kind == 1U ? 21U : 2U;
 event_budget = WRAP32(cycle_next_event - cpu_state.cycle);
 if (event_budget == 0U){
  cu_avr_loop_counters.event_rejects ++; return FALSE;
 }
 budget = VIDEO_CY_MAX - video_cycle;
 if (budget >= event_budget){ budget = event_budget - 1U; }
 n = budget / per;
 if (n >= remaining){ n = remaining; }
 else if (n + 1U == remaining && budget >= remaining * per - 1U){ n++; }
 if (n == 0U){
  if (event_budget <= per){ cu_avr_loop_counters.event_rejects ++; }
  else{ cu_avr_loop_counters.row_rejects ++; }
  return FALSE;
 }
 /* A two-op delay has little scalar work. The full native call costs more
  * for one iteration and only breaks even for two in ARM work-count tests.
  * Require four to retain a useful margin; the raw-kernel selftest still
  * exercises short terminal batches independently of this admission rule. */
 if (b->kind == 2U && n < 4U){
  cu_avr_loop_counters.guard_rejects ++; return FALSE;
 }
 if(UZ_FILL_MODE && b->kind==2U){
  ctx.tail_taken=n!=remaining;cycles=n*per-(ctx.tail_taken?0U:1U);
  uz_fill_constant(&video_row.pixels[video_cycle],cpu_state.iors[CU_IO_PORTC],cycles);
  goto uz_fill_done;
 }
 memset(&ctx, 0, sizeof(ctx));
 ctx.crom = cpu_state.crom; ctx.sram = cpu_state.sram;
 ctx.pixels = &video_row.pixels[video_cycle]; ctx.marks = access_mem;
 ctx.pixel = cpu_state.iors[CU_IO_PORTC];
 if (b->kind == 1U){
  ctx.x = cpu_state.iors[b->pointer] + ((auint)cpu_state.iors[b->pointer+1U] << 8);
  /* Mirror SRAM semantics above 0x100; never accidentally read an I/O
   * register, including after a 16-bit pointer wrap. No effects precede
   * these guards. A safe prefix may end exactly at address 0xFFFF. */
  if (ctx.x < 0x100U){ cu_avr_loop_counters.guard_rejects ++; return FALSE; }
  if (n > 0x10000U - ctx.x){ n = 0x10000U - ctx.x; }
  ctx.z = cpu_state.iors[30U] + ((auint)cpu_state.iors[31U] << 8);
  ctx.base = cpu_state.iors[b->base] + ((auint)cpu_state.iors[b->base+1U] << 8);
  ctx.stride = cpu_state.iors[b->stride];
  ctx.ddrc = cpu_state.iors[CU_IO_DDRC];
 }
 ctx.iterations = n;
 ctx.tail_taken = n != remaining;
 cycles = n * per - (ctx.tail_taken ? 0U : 1U);
 cu_avr_loop_execute(&ctx, b->kind);
uz_fill_done:
 flags = cpu_state.iors[CU_IO_SREG];
 count = (count - n) & 0xFFU;
 if (b->kind == 1U){
  cpu_state.iors[b->pointer] = (uint8)ctx.x;
  cpu_state.iors[b->pointer+1U] = (uint8)(ctx.x >> 8);
  cpu_state.iors[30U] = (uint8)ctx.z;
  cpu_state.iors[31U] = (uint8)(ctx.z >> 8);
  cpu_state.iors[0U] = (uint8)ctx.product;
  cpu_state.iors[1U] = (uint8)(ctx.product >> 8);
  cpu_state.iors[b->work] = (uint8)ctx.work;
  cpu_state.iors[b->work+1U] = (uint8)(ctx.work >> 8);
  cpu_state.iors[b->pixel] = (uint8)ctx.pixel_raw;
  cpu_state.iors[CU_IO_PORTC] = (uint8)ctx.pixel;
  src = ctx.product >> 8;
  dst = ctx.base >> 8;
  flags = (flags & (SREG_IM | SREG_TM)) |
   cpu_pflags[CU_AVRFG_ADD + ((ctx.work >> 8) & 0x1FFU) +
              ((src & 0x90U) << 5) + ((dst & 0x90U) << 6)];
  cu_avr_loop_counters.tile_entries ++;
 }else{ cu_avr_loop_counters.delay_entries ++; }
 cpu_state.iors[b->count] = (uint8)count;
 cpu_state.iors[CU_IO_SREG] = (flags & (SREG_IM | SREG_TM | SREG_HM | SREG_CM)) |
                             cpu_pflags[CU_AVRFG_DEC + count];
 /* The scalar decoder zero-extends compiled relative offsets to 16 bits.
  * Preserve its affine 32-bit PC growth, not merely the low ROM address. */
 cpu_state.pc = WRAP32(cpu_state.pc +
  (n - (ctx.tail_taken ? 0U : 1U)) * (length + b->branch_delta) +
  (ctx.tail_taken ? 0U : length));
 cpu_state.cycle = WRAP32(cpu_state.cycle + cycles);
 video_cycle += cycles;
 cu_avr_loop_counters.entries ++;
 cu_avr_loop_counters.iterations += n;
 cu_avr_loop_counters.cycles += cycles;
 return TRUE;
}

#ifdef CU_AVR_REGION
#include "cu_avr_region.inc"
#endif
