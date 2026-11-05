void __not_in_flash_func(hstx_push_audio_sample)(const int left, const int right)
{
    static int g_hdmi_audio_frame_counter = 0;
    static audio_sample_t acc_buf[4];
    static int acc_count = 0;
    static  uint32_t video_frame_count = 0;
    // Masked index + >= reset: if two producers ever race this function
    // (menu wavplayer on core0 vs a core1 mixer), a lost update could step
    // acc_count past 4 — with an exact == check that would never reset
    // again and acc_buf[acc_count] would write through adjacent memory at
    // the audio sample rate. The mask bounds every write; >= guarantees
    // the reset. Costs one AND per sample.
    acc_buf[acc_count & 3].left = left;
    acc_buf[acc_count & 3].right = right;
    acc_count++;
    if (acc_count >= 4)
    {
        if (hstx_di_queue_get_level() >= HSTX_AUDIO_DI_HIGH_WATERMARK)
        {
            acc_count = 0;
            return;
        }
        hstx_packet_t packet;
        // _cs variant: carries IEC 60958 channel status with a valid
        // sample-frequency code. Strict HDMI sinks mute all-zero channel
        // status even with a correct Audio InfoFrame; lax sinks (capture
        // cards) decode it but with glitches.
        g_hdmi_audio_frame_counter = hstx_packet_set_audio_samples_cs(&packet, acc_buf, 4, g_hdmi_audio_frame_counter);

        hstx_data_island_t island;
        hstx_encode_data_island(&island, &packet, false, true);
        (void)hstx_di_queue_push(&island);
        acc_count = 0;
    }
}