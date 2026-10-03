#include <stddef.h>
/* Freestanding compiler support; volatile stores prevent loop-to-libcall folding. */
void *memcpy(void *dest,const void *source,size_t n)
{
    volatile unsigned char *d=dest; const unsigned char *s=source;
    for(size_t i=0;i<n;++i) d[i]=s[i];
    return dest;
}
void *memset(void *dest,int value,size_t n)
{
    volatile unsigned char *d=dest;
    for(size_t i=0;i<n;++i) d[i]=(unsigned char)value;
    return dest;
}
