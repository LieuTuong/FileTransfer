#ifndef __CRC_H__
#define __CRC_H__

typedef unsigned long crc;

#define POLYNOMIAL      0x04C11DB7
#define INITIAL_REMAINDER	0xFFFFFFFF
#define FINAL_XOR_VALUE		0xFFFFFFFF
#define REFLECT_DATA		TRUE
#define REFLECT_REMAINDER	TRUE
#define CHECK_VALUE			0xCBF43926


void crcInit(void);

crc compute_crc (unsigned char const mess[], int nBytes);
#endif