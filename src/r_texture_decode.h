#ifndef EZ_R_TEXTURE_DECODE_H
#define EZ_R_TEXTURE_DECODE_H
void R_TextureDecodeInit(void);
void R_TextureDecodeBegin(void);
/* Returns whether a source exists, including sources handled by the serial fallback. */
int R_TextureDecodeQueue(const char *filename, int mode);
void R_TextureDecodeRun(void);
void R_TextureDecodeEnd(void);
#endif
