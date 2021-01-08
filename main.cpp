// Based on A2DP specification 1.3.2
// Section 12 Appendix B: Technical Specification of SBC
#include <string>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#include "decoder/include/oi_codec_sbc.h"
#include <pulse/simple.h>

#pragma pack(push, 1)
struct frame_header
{
    uint8_t syncword;
    uint8_t sampling_frequency: 2;
    uint8_t blocks: 2;
    uint8_t channel_mode: 2;
    uint8_t allocation_method: 1;
    uint8_t subbands: 1;
    uint8_t bitpool;
    uint8_t crc_check;
};
#pragma pack(pop)

#define SYNC_WORD 0x9C
#define PCM_DATA_BUFFER_SIZE 1024
#define BUFFERED_FRAMES 2048

pa_simple* stream = nullptr;

void queueAudioFrame(void* data, int32_t size)
{
    static const pa_sample_spec spec = { .format = PA_SAMPLE_S16LE, .rate = 44100, .channels = 2 };
    if(stream == nullptr)
    {
        stream = pa_simple_new(nullptr, nullptr, PA_STREAM_PLAYBACK, nullptr, "a2dp_track", &spec, nullptr, nullptr, nullptr);
    }
    pa_simple_write(stream, data, size, nullptr);
}

void flushAudio()
{
    pa_simple_drain(stream, nullptr);
    pa_simple_free(stream);

    stream = nullptr;
}

int main(int argc, char** argv)
{
    if(argc < 2)
    {
        printf("subnautica <sbc file>");
        return -1;
    }

    setvbuf(stdout, nullptr, _IONBF, 0);

    FILE* audio = fopen(argv[1], "rb");
    if(audio)
    {
        fseek(audio, 0, SEEK_END);
        int32_t size = ftell(audio);
        rewind(audio);
        OI_BYTE* binary = (OI_BYTE*)malloc(size);
        size_t read = fread(binary, 1, size, audio);
        if(read != size) return -1;

        int32_t ndx = 0;

        frame_header header;
        header.syncword = binary[ndx];
        header.sampling_frequency = binary[ndx + 1] >> 6;
        header.blocks = (binary[ndx + 1] >> 4) & 0x3;
        header.channel_mode = (binary[ndx + 1] >> 2) & 0x3;
        header.allocation_method = (binary[ndx + 1] >> 1) & 0x1;
        header.subbands = binary[ndx + 1] & 0x1;
        header.bitpool = binary[ndx + 2];
        header.crc_check = binary[ndx + 3];

        float frequency = 0.0f;
        switch(header.sampling_frequency)
        {
            case 0:
                frequency = 16;
                break;
            case 1:
                frequency = 32;
                break;
            case 2:
                frequency = 44.1;
                break;
            case 3:
                frequency = 48;
                break;
        }

        const int32_t num_blocks = (header.blocks+1)*4;

        int32_t num_channels = 2;
        std::string mode = "Unknown";
        switch(header.channel_mode)
        {
            case 0:
                mode = "Single";
                num_channels = 1;
                break;
            case 1:
                mode = "Dual channel";
                break;
            case 2:
                mode = "Stereo";
                break;
            case 3:
                mode = "Joint stereo";
                break;
        }

        // Current Bluetooth stacks usually negotiate the following parameters,
        // Joint Stereo, 8 bands, 16 blocks, Loudness and a bitpool of 2 to 53.

        printf("Sampling frequency: %.1f kHz\n", frequency);
        printf("Block size: %d\n", num_blocks);
        printf("Channel mode: %s\n", mode.c_str());
        printf("Allocation method: %s\n", header.allocation_method ? "SNR": "Loudness");
        printf("Subbands: %s\n", header.subbands ? "8": "4");
        printf("Bitpool: %d\n", header.bitpool);

        OI_BYTE* start = binary;
        uint8_t previousSettings = binary[1];

        OI_CODEC_SBC_DECODER_CONTEXT context; // Prime the decoder
        uint32_t context_data[CODEC_DATA_WORDS(2, SBC_CODEC_FAST_FILTER_BUFFERS)];
        int16_t decode_buf[15 * SBC_MAX_SAMPLES_PER_FRAME * SBC_MAX_CHANNELS];
        OI_STATUS status = OI_CODEC_SBC_DecoderReset(&context, context_data, sizeof(context_data), 2, 2, false);

        int32_t valid_frames = 0;
        int32_t invalid_frames = 0;

        int16_t pcm_data[PCM_DATA_BUFFER_SIZE];
        uint32_t pcm_bytes = PCM_DATA_BUFFER_SIZE*sizeof(int16_t);
        uint32_t frame_bytes = size;
        while(frame_bytes > 0)
        {
            status = OI_CODEC_SBC_DecodeFrame(&context, &binary, &frame_bytes, pcm_data, &pcm_bytes);
            if(OI_SUCCESS(status))
            {
                valid_frames++;
                queueAudioFrame(pcm_data, pcm_bytes);
                if(valid_frames % BUFFERED_FRAMES == 0)
                {
                    flushAudio();
                }
            }
            else
            {
                invalid_frames++;
                memset(pcm_data, '\0', pcm_bytes);
                queueAudioFrame(pcm_data, pcm_bytes); // Filler data
                queueAudioFrame(pcm_data, pcm_bytes);
                binary++; // Nudge off magic number in case compressed data was at fault
                while(frame_bytes &&
                     !(binary[0] == SYNC_WORD
                    && binary[1] == previousSettings
                    && binary[2] == header.bitpool)
                )
                {
                    binary++;
                    frame_bytes--;
                }
                status = OI_CODEC_SBC_DecoderReset(&context, context_data, sizeof(context_data), 2, 2, false);
            }
            pcm_bytes = PCM_DATA_BUFFER_SIZE*sizeof(int16_t); // Reset available
        }

        free(start);
        fclose(audio);

        printf("Processed %d frames, skipped %d\n", valid_frames, invalid_frames);
    }

    return 0;
}
