// error_recalc.h

#ifndef uint8_t
#define uint8_t unsigned char
#endif

extern void eccedc_init(void);
extern void eccedc_generate(uint8_t* sector);
extern int edc_verify(const uint8_t* sector);
