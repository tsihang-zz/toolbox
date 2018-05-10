#ifndef _RC4_H_
#define _RC4_H_
#include<stdio.h>
#include<string.h>

void rc4_init(unsigned char *, unsigned char *, unsigned long );
void rc4_crypt(unsigned char *, unsigned char *, unsigned long );

#endif
