// Based on A2DP specification 1.3.2
// Section 12 Appendix B: Technical Specification of SBC
#include <string>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

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

        frame_header header;
        header.syncword = binary[0];

        if(header.syncword != 0x9C)
        {
            printf("Invalid frame!");
            return -1;
        }

        header.sampling_frequency = binary[1] >> 6;
        header.blocks = (binary[1] >> 4) & 0x3;
        header.channel_mode = (binary[1] >> 2) & 0x3;
        header.allocation_method = (binary[1] >> 1) & 0x1;
        header.subbands = binary[1] & 0x1;
        header.bitpool = binary[2];
        header.crc_check = binary[3];

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

        std::string mode = "Unknown";
        switch(header.channel_mode)
        {
            case 0:
                mode = "Single";
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
        printf("Block size: %d\n", (header.blocks+1)*4);
        printf("Channel mode: %s\n", mode.c_str());
        printf("Allocation method: %s\n", header.allocation_method ? "SNR": "Loudness");
        printf("Subbands: %s\n", header.subbands ? "8": "4");
        printf("Bitpool: %d\n", header.bitpool);

        int32_t i = 0;
        const int32_t count = (header.subbands ? 8: 4)/2;
        binary[5 + count];

        // audio_samples

        free(binary);
        fclose(audio);
    }

    return 0;
}
