#include "image_decode_memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <png.h>
#include <SDL.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "check failed at %d: %s\n", __LINE__, #x); exit(1); } } while (0)
static const unsigned char rgba[] = {255,0,0,255, 0,255,0,128, 0,0,255,0, 17,29,43,255};
typedef struct { unsigned char bytes[8192]; size_t size; } encoded_t;
static void WritePNG(png_structp p, png_bytep data, png_size_t count)
{
    encoded_t *out = png_get_io_ptr(p);
    CHECK(count <= sizeof(out->bytes) - out->size);
    memcpy(out->bytes + out->size, data, count); out->size += count;
}
static void FlushPNG(png_structp p) { (void)p; }

static void PNGCase(int color, int depth, int interlace)
{
    encoded_t out = {{0},0};
    png_structp p = png_create_write_struct(PNG_LIBPNG_VER_STRING,NULL,NULL,NULL);
    png_infop info = png_create_info_struct(p);
    unsigned char raw[32] = {0}, expected[16], alpha[] = {255,128,0,255};
    png_color palette[] = {{255,0,0},{0,255,0},{0,0,255},{17,29,43}};
    png_bytep rows[2];
    unsigned char *decoded;
    int stride = 0, i, w, h;
    CHECK(p && info);
    CHECK(!setjmp(png_jmpbuf(p)));
    png_set_write_fn(p,&out,WritePNG,FlushPNG);
    png_set_IHDR(p,info,2,2,depth,color,interlace,PNG_COMPRESSION_TYPE_DEFAULT,PNG_FILTER_TYPE_DEFAULT);
    memcpy(expected,rgba,sizeof(expected));
    if (color == PNG_COLOR_TYPE_PALETTE) {
        png_set_PLTE(p,info,palette,4); png_set_tRNS(p,info,alpha,4,NULL);
        raw[0]=0; raw[1]=1; raw[2]=2; raw[3]=3; stride=2;
    }
    else if (color == PNG_COLOR_TYPE_GRAY && depth == 1) {
        raw[0]=0x40; raw[1]=0x80; stride=1;
        for(i=0;i<4;++i) { memset(expected+i*4,(i==1||i==2)?255:0,3); expected[i*4+3]=255; }
    }
    else {
        int channels = color == PNG_COLOR_TYPE_RGBA ? 4 : color == PNG_COLOR_TYPE_RGB ? 3 : color == PNG_COLOR_TYPE_GRAY_ALPHA ? 2 : 1;
        stride = 2 * channels * (depth/8);
        for(i=0;i<4;++i) {
            int c;
            if(channels < 3) expected[i*4+1]=expected[i*4+2]=expected[i*4];
            if(channels == 1 || channels == 3) expected[i*4+3]=255;
            for(c=0;c<channels;++c) {
                int v = channels == 2 && c == 1 ? rgba[i*4+3] : rgba[i*4+c];
                raw[(i*channels+c)*(depth/8)]=(unsigned char)v;
                if(depth==16) raw[(i*channels+c)*2+1]=73;
            }
        }
    }
    rows[0]=raw; rows[1]=raw+stride;
    png_write_info(p,info); png_write_image(p,rows); png_write_end(p,info);
    png_destroy_write_struct(&p,&info);
    CHECK(Image_MemoryDecodeSize(out.bytes,out.size,1)==16);
    decoded=Image_DecodeMemory(out.bytes,out.size,1,&w,&h);
    CHECK(decoded && w==2 && h==2 && !memcmp(decoded,expected,16)); free(decoded);
    decoded=Image_DecodeMemory(out.bytes,out.size/2,1,&w,&h);
    CHECK(!decoded);
}

static void TGACase(int depth, int top, int rle)
{
    unsigned char data[64]={0}, expected[16];
    unsigned char *decoded;
    int i,w,h,offset=18,bpp=depth/8;
    data[2]=rle?10:2; data[12]=2; data[14]=2; data[16]=(unsigned char)depth; data[17]=(unsigned char)(top?32:0);
    memcpy(expected,rgba,16);
    if(rle) data[offset++]=3; /* literal packet crosses a row boundary */
    for(i=0;i<4;++i) {
        int src=(top?i:(i+2)%4)*4;
        data[offset++]=rgba[src+2]; data[offset++]=rgba[src+1]; data[offset++]=rgba[src];
        if(bpp==4) data[offset++]=rgba[src+3]; else expected[i*4+3]=255;
    }
    decoded=Image_DecodeMemory(data,offset,0,&w,&h);
    CHECK(decoded && w==2 && h==2 && !memcmp(decoded,expected,16)); free(decoded);
    CHECK(!Image_DecodeMemory(data,offset-1,0,&w,&h));
    if(rle) {
        data[18]=131; /* repeat four pixels, crossing row boundary */
        decoded=Image_DecodeMemory(data,19+bpp,0,&w,&h);
        CHECK(decoded);
        for(i=1;i<4;++i) CHECK(!memcmp(decoded,decoded+i*4,4));
        free(decoded);
        data[18]=132; CHECK(!Image_DecodeMemory(data,19+bpp,0,&w,&h));
    }
    data[17]|=16; CHECK(!Image_MemoryDecodeSize(data,offset,0));
}

static int SDLCALL Concurrent(void *unused)
{
    int i; (void)unused;
    for(i=0;i<10;++i) {
        PNGCase(PNG_COLOR_TYPE_RGBA,8,PNG_INTERLACE_ADAM7);
        PNGCase(PNG_COLOR_TYPE_PALETTE,8,PNG_INTERLACE_NONE);
        TGACase(32,0,1);
    }
    return 0;
}

int main(int argc, char **argv)
{
    int i,j,k;
    if(argc==4) {
        FILE *f=fopen(argv[1],"rb"); long length; unsigned char *data,*pixels; int w,h;
        CHECK(f); CHECK(!fseek(f,0,SEEK_END)); length=ftell(f); CHECK(length>0); rewind(f);
        data=malloc(length); CHECK(data && fread(data,1,length,f)==(size_t)length); fclose(f);
        pixels=Image_DecodeMemory(data,length,atoi(argv[2]),&w,&h); free(data); CHECK(pixels);
        f=fopen(argv[3],"wb"); CHECK(f); CHECK(fwrite(pixels,4,(size_t)w*h,f)==(size_t)w*h); fclose(f);free(pixels);
        return 0;
    }
    for(i=24;i<=32;i+=8) for(j=0;j<2;++j) for(k=0;k<2;++k) TGACase(i,j,k);
    PNGCase(PNG_COLOR_TYPE_GRAY,1,PNG_INTERLACE_NONE);
    PNGCase(PNG_COLOR_TYPE_PALETTE,8,PNG_INTERLACE_NONE);
    for(i=8;i<=16;i+=8) for(j=0;j<=1;++j) {
        PNGCase(PNG_COLOR_TYPE_RGB,i,j); PNGCase(PNG_COLOR_TYPE_RGBA,i,j);
        PNGCase(PNG_COLOR_TYPE_GRAY,i,j); PNGCase(PNG_COLOR_TYPE_GRAY_ALPHA,i,j);
    }
    {
        SDL_Thread *workers[8];
        for(i=0;i<8;++i) { workers[i]=SDL_CreateThread(Concurrent,"decoder-test",NULL); CHECK(workers[i]); }
        for(i=0;i<8;++i) SDL_WaitThread(workers[i],NULL);
    }
    puts("Memory decoder: PNG/TGA pixels, alpha, orientation, truncation and concurrency passed");
    return 0;
}
