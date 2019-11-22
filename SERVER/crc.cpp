#include"crc.h"

#define WIDTH (8*sizeof(crc))
#define TOPBIT (1UL <<(WIDTH - 1UL))
#if (REFLECT_DATA == TRUE)
#undef  REFLECT_DATA
#define REFLECT_DATA(X)			((unsigned char) reflect((X), 8))
#else
#undef  REFLECT_DATA
#define REFLECT_DATA(X)			(X)
#endif

#if (REFLECT_REMAINDER == TRUE)
#undef  REFLECT_REMAINDER
#define REFLECT_REMAINDER(X)	((crc) reflect((X), WIDTH))
#else
#undef  REFLECT_REMAINDER
#define REFLECT_REMAINDER(X)	(X)
#endif

crc crcTable[256];


static unsigned long
reflect(unsigned long data, unsigned char nBits)
{
	unsigned long  reflection = 0x00000000;
	unsigned char  bit;

	/*
	 * Reflect the data about the center bit.
	 */
	for (bit = 0; bit < nBits; ++bit)
	{
		/*
		 * If the LSB bit is set, set the reflection of it.
		 */
		if (data & 0x01)
		{
			reflection |= (1 << ((nBits - 1) - bit));
		}

		data = (data >> 1);
	}

	return (reflection);

}	


void crcInit(void)
{
    crc remainder;
    int dividend;
    unsigned char bit;
    for (dividend = 0; dividend < 256; ++dividend)
    {
        remainder = (crc)dividend<<(WIDTH - 8);
        for (bit=8 ; bit>0; --bit)
        {
            if (remainder & TOPBIT)
            {
                remainder = (remainder<<1)^POLYNOMIAL;
            }
            else
            {
                remainder = (remainder << 1);
            }
            
        }
        crcTable[dividend] = remainder;
    }

}

crc compute_crc(unsigned char const mess[],int nBytes)
{
    crcInit();
    crc remainder = INITIAL_REMAINDER;
    unsigned char data;
    int byte;

    for(byte=0; byte < nBytes; ++byte)
    {
        data = REFLECT_DATA(mess[byte])^(unsigned char)(remainder>>(WIDTH-8));
        remainder=crcTable[data]^(remainder<<8);

    }
    return (REFLECT_REMAINDER(remainder) ^ FINAL_XOR_VALUE);
}


