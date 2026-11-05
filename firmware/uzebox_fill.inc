/* GPL-3.0-or-later. All caller admission/cycle arithmetic remains original.
 * No I/O/event/IRQ occurs inside the already accepted constant-port batch. */
static void __attribute__((noinline,noclone,section(".time_critical.uz_fill_constant")))
uz_fill_constant(uint8 *pixels,auint pixel,auint length){
 memset(pixels,(uint8)pixel,length);
 uz_fill_calls++;uz_fill_bytes+=length;
}
