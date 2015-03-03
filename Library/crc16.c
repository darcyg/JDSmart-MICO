unsigned short tCRCData[] =
{
    0x0000, 0x1081, 0x2102, 0x3183,
    0x4204, 0x5285, 0x6306, 0x7387,
    0x8408, 0x9489, 0xa50a, 0xb58b,
    0xc60c, 0xd68d, 0xe70e, 0xf78f
};

unsigned short crc16_calc(unsigned char* ptr, unsigned int len)
{
    unsigned short vCRC = 0xFFFF;

    while (len-- != 0)
    {
        vCRC = ((vCRC >> 4)^tCRCData[(vCRC & 0x0f) ^ ((*ptr) & 0x0f)]);
        vCRC = ((vCRC >> 4)^tCRCData[(vCRC & 0x0f) ^ ((*ptr) >> 4)]);
        ptr++;
    }

    return ~vCRC;
}

