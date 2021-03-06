// This file is distributed under a BSD license. See LICENSE.txt for details.

#include "loadpng.hpp"
#include "png.h"
#include <stdio.h>

#pragma warning (disable: 4996) // deprecated BS
#ifdef _DEBUG
#pragma comment (linker, "/nodefaultlib:libcmt.lib")
#endif

// Small helper to make cleanup code nicer
struct CLibFile
{
  FILE *fp;

  CLibFile()
  {
    fp = 0;
  }

  ~CLibFile()
  {
    if(fp)
      fclose(fp);

    fp = 0;
  }

  sBool open(const char *filename,const char *mode)
  {
    fp = fopen(filename, mode);
    return fp != 0;
  }

  operator FILE *()
  {
    return fp;
  }
};

struct PNGReadContext
{
  const sU8 *data;
  sInt pos;
  sInt size;
};

static void PNGReadFunction(png_structp png,png_bytep data,png_size_t length)
{
  PNGReadContext *ctx = (PNGReadContext *) png_get_io_ptr(png);
  sInt len = sMin<sInt>(ctx->size - ctx->pos,(sInt) length);

  memcpy(data,ctx->data + ctx->pos,len);
  ctx->pos += len;
}

// png loading routines
sBool LoadPNG(const sU8 *data,sInt size,sInt &xout,sInt &yout,sU8 *&dataout)
{
  PNGReadContext ctx;

  dataout = 0;

  // png validity check first
  if(size<8 || png_sig_cmp((png_bytep) data,0,size) != 0)
    return sFALSE;

  // create png read+info structures
  png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING,0,0,0);
  if(!png)
    return sFALSE;

  png_infop info = png_create_info_struct(png);
  if(!info)
  {
    png_destroy_read_struct(&png,0,0);
    return sFALSE;
  }

  png_infop endinfo = png_create_info_struct(png);
  if(!endinfo)
  {
    png_destroy_read_struct(&png,&info,0);
    return sFALSE;
  }

  // gross setjmp error handling...
  if(setjmp(png_jmpbuf(png)))
  {
    delete[] dataout;
    dataout = 0;
    png_destroy_read_struct(&png,&info,&endinfo);
    return sFALSE;
  }

  // setup reading
  ctx.data = data;
  ctx.pos = 0;
  ctx.size = size;

  png_set_read_fn(png,&ctx,PNGReadFunction);

  // read png info
  png_read_info(png,info);
  xout = png_get_image_width(png,info);
  yout = png_get_image_height(png,info);
  sInt colorType = png_get_color_type(png,info);

  // setup transforms
  if(colorType & PNG_COLOR_MASK_PALETTE)
    png_set_palette_to_rgb(png);
  else if(!(colorType & PNG_COLOR_MASK_COLOR))
    png_set_gray_to_rgb(png);

  if(colorType & PNG_COLOR_MASK_COLOR)
    png_set_bgr(png);

  if(!(colorType & PNG_COLOR_MASK_ALPHA))
    png_set_add_alpha(png,0xff,PNG_FILLER_AFTER);

  png_set_interlace_handling(png);
  png_read_update_info(png,info);

  // allocate image buffer and read image
  dataout = new sU8[xout * yout * 4];

  for(sInt y=0;y<yout;y++)
  {
    png_bytep row = dataout + y * (xout * 4);
    png_read_row(png,row,0);
  }

  // cleanup
  png_destroy_read_struct(&png,&info,&endinfo);

  return sTRUE;
}

// png saving
sBool SavePNG(const sChar *filename,const sU8 *data,sInt xSize,sInt ySize)
{
  CLibFile file;
  if(!file.open(filename, "wb"))
    return sFALSE;

  png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING,0,0,0);
  if(!png)
    return sFALSE;

  png_infop info = png_create_info_struct(png);
  if(!info)
  {
    png_destroy_write_struct(&png,0);
    return sFALSE;
  }

  png_init_io(png,file);
  png_set_compression_level(png,5);
  png_set_IHDR(png,info,xSize,ySize,8,PNG_COLOR_TYPE_RGB_ALPHA,
    PNG_INTERLACE_NONE,PNG_COMPRESSION_TYPE_DEFAULT,PNG_FILTER_TYPE_DEFAULT);
  png_write_info(png, info);
  png_set_bgr(png);

  for(sInt y=0;y<ySize;y++)
    png_write_row(png, (png_bytep) (data+y*(xSize*4)));

  png_write_end(png, info);
  png_destroy_write_struct(&png,0);

  return sTRUE;
}
