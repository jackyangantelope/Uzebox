/* Included only in the generated, port-private hstx.c.
 * Ring production/consumption is confined to core0. Core1 consumes only
 * the existing HDMI queue; its publication protocol is unchanged. */
static size_t __attribute__((noinline)) __not_in_flash_func(uzebox_audio_batch_drain)(jack_audio_ring *ring,size_t count)
{
    const size_t requested=count;
    while(count){
        size_t take=4u-(unsigned)uzebox_audio_acc_count;
        if(take>count)take=count;
        size_t real=ring->frames?ring->count:0u;
        if(real>take)real=take;
        size_t tail=ring->tail;
        const unsigned start=(unsigned)uzebox_audio_acc_count;
        for(size_t i=0;i<real;++i){
            uzebox_audio_acc_buf[start+i].left=ring->frames[tail].left;
            uzebox_audio_acc_buf[start+i].right=ring->frames[tail].right;
            if(++tail==ring->capacity)tail=0u;
        }
        ring->tail=tail;ring->count-=real;
        for(size_t i=real;i<take;++i){
            uzebox_audio_acc_buf[start+i].left=0;
            uzebox_audio_acc_buf[start+i].right=0;
        }
        ring->underruns+=(uint32_t)(take-real);
        uzebox_audio_acc_count+=(int)take;count-=take;
        if(uzebox_audio_acc_count==4){
            /* GENERATED_EMIT_PACKET */
        }
batch_next:;
    }
    return requested;
}
static void uzebox_audio_scalar_sink(void *context,int16_t left,int16_t right)
{
    (void)context;hstx_push_audio_sample(left,right);
}
size_t __attribute__((noinline)) uzebox_audio_drain(jack_audio_ring *ring,size_t count)
{
    if(!ring||!count)return 0u;
    size_t real=ring->frames?ring->count:0u;if(real>count)real=count;
    uzebox_audio_data.calls++;uzebox_audio_data.samples+=(uint32_t)count;
    uzebox_audio_data.real+=(uint32_t)real;uzebox_audio_data.zero+=(uint32_t)(count-real);
    if(count>uzebox_audio_data.peak)uzebox_audio_data.peak=(uint32_t)count;
    if(uzebox_audio_batch_mode()==1u){
        uzebox_audio_data.batches++;
        return uzebox_audio_batch_drain(ring,count);
    }
    return jack_audio_drain(ring,count,uzebox_audio_scalar_sink,NULL);
}
void uzebox_audio_batch_stats(uzebox_audio_batch_stats_t *out)
{
    *out=uzebox_audio_data;
    out->pending=(uint32_t)uzebox_audio_acc_count;
    out->phase=(uint32_t)uzebox_audio_phase;
}
