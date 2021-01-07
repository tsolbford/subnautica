// Based on A2DP specification 1.3.2
// Section 12 Appendix B: Technical Specification of SBC
#include <string>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

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

void writeAudioResource(uint8_t* data, int32_t size)
{
    static const pa_sample_spec spec = { .format = PA_SAMPLE_S16LE, .rate = 44100, .channels = 2 };
    pa_simple* stream = pa_simple_new(NULL, NULL, PA_STREAM_PLAYBACK, NULL, "a2dp_track", &spec, NULL, NULL, NULL);
    if(stream != NULL)
    {
        pa_simple_write(stream, data, size, NULL);
        pa_simple_drain(stream, NULL);
        pa_simple_free(stream);
    }
}

int main(int argc, char** argv)
{
    if(argc < 2)
    {
        printf("subnautica <sbc file>");
        return -1;
    }

    FILE* audio = fopen(argv[1], "rb");
    if(audio)
    {
        fseek(audio, 0, SEEK_END);
        int32_t size = ftell(audio);
        rewind(audio);
        uint8_t* binary = (uint8_t*)malloc(size);
        size_t read = fread(binary, 1, size, audio);
        if(read != size) return -1;

        int32_t ndx = 0;
        bool initialFrame = true;

        int32_t validFrames = 0;
        int32_t invalidFrames = 0;
        while(ndx < size)
        {
            frame_header header;
            header.syncword = binary[ndx];
            if(header.syncword != 0x9C)
            {
                invalidFrames++;
                while(ndx < size && binary[ndx] != 0x9C)
                {
                    ndx++; // Scrub forward
                }

                if(ndx >= size)
                {
                    return -1;
                }
                else
                {
                    header.syncword = binary[ndx];
                }
            }
            else
            {
                validFrames+=2;
            }

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
            if(initialFrame)
            {
                initialFrame = false;
                printf("Sampling frequency: %.1f kHz\n", frequency);
                printf("Block size: %d\n", num_blocks);
                printf("Channel mode: %s\n", mode.c_str());
                printf("Allocation method: %s\n", header.allocation_method ? "SNR": "Loudness");
                printf("Subbands: %s\n", header.subbands ? "8": "4");
                printf("Bitpool: %d\n", header.bitpool);
            }

            const int32_t num_subbands = header.subbands ? 8: 4;
            const int32_t frame_length =
                (4 + (4*num_subbands*num_channels)/8)
                +((1*num_subbands) + (num_blocks*header.bitpool))/8;

            const int32_t count = (num_subbands*num_channels)/2;

            // Could do Crc here
            // audio_samples
            int32_t cursor = ndx + 4 + count;
            while(cursor < (ndx + frame_length*2))
            {
                binary[cursor];
                cursor++;
            }

            ndx = cursor;
        }

        free(binary);
        fclose(audio);

        printf("Processed %d frames, skipped %d\n", validFrames, invalidFrames);
    }

    return 0;
}
