
#include "../jpeg/jpeg.h"

typedef mjpg_destination_mgr *mjpg_dest_ptr;

METHODDEF(void) init_destination(j_compress_ptr cinfo) {
  mjpg_dest_ptr dest = (mjpg_dest_ptr)cinfo->dest;

  dest->buffer = (JOCTET *)(*cinfo->mem->alloc_small)(
      (j_common_ptr)cinfo, JPOOL_IMAGE, OUTPUT_BUF_SIZE * sizeof(JOCTET));

  *(dest->written) = 0;

  dest->pub.next_output_byte = dest->buffer;

  dest->pub.free_in_buffer = OUTPUT_BUF_SIZE;
}

METHODDEF(boolean) empty_output_buffer(j_compress_ptr cinfo) {
  mjpg_dest_ptr dest = (mjpg_dest_ptr)cinfo->dest;

  memcpy(dest->outbuffer_cursor, dest->buffer, OUTPUT_BUF_SIZE);

  dest->outbuffer_cursor += OUTPUT_BUF_SIZE;

  *(dest->written) += OUTPUT_BUF_SIZE;

  dest->pub.next_output_byte = dest->buffer;

  dest->pub.free_in_buffer = OUTPUT_BUF_SIZE;

  return TRUE;
}

METHODDEF(void) term_destination(j_compress_ptr cinfo) {
  mjpg_dest_ptr dest = (mjpg_dest_ptr)cinfo->dest;

  size_t datacount = OUTPUT_BUF_SIZE - dest->pub.free_in_buffer;

  /* Write any data remaining in the buffer */

  memcpy(dest->outbuffer_cursor, dest->buffer, datacount);

  dest->outbuffer_cursor += datacount;

  *(dest->written) += datacount;
}

void dest_buffer(j_compress_ptr cinfo, unsigned char *buffer, int size,
                 int *written) {
  mjpg_dest_ptr dest;

  if (cinfo->dest == NULL) {
    cinfo->dest = (struct jpeg_destination_mgr *)(*cinfo->mem->alloc_small)(
        (j_common_ptr)cinfo, JPOOL_PERMANENT, sizeof(mjpg_destination_mgr));
  }

  dest = (mjpg_dest_ptr)cinfo->dest;

  dest->pub.init_destination = init_destination;

  dest->pub.empty_output_buffer = empty_output_buffer;

  dest->pub.term_destination = term_destination;

  dest->outbuffer = buffer;

  dest->outbuffer_size = size;

  dest->outbuffer_cursor = buffer;

  dest->written = written;
}

//......YUYV.....JPEG..

int compress_yuyv_to_jpeg(unsigned char *buf, unsigned char *buffer, int size,
                          int quality) {
  struct jpeg_compress_struct cinfo;

  struct jpeg_error_mgr jerr;

  JSAMPROW row_pointer[1];

  unsigned char *line_buffer, *yuyv;

  int z;

  static int written;

  // int count = 0;

  // printf("%s\n", buf);

  line_buffer = (unsigned char *)calloc(WIDTH * 3, 1);

  yuyv = buf;
  // printf("compress start...\n");

  cinfo.err = jpeg_std_error(&jerr);

  jpeg_create_compress(&cinfo);

  /* jpeg_stdio_dest(&cinfo, file); */

  dest_buffer(&cinfo, buffer, size, &written);

  cinfo.image_width = WIDTH;

  cinfo.image_height = HEIGHT;

  cinfo.input_components = 3;

  cinfo.in_color_space = JCS_RGB;

  jpeg_set_defaults(&cinfo);

  jpeg_set_quality(&cinfo, quality, TRUE);

  jpeg_start_compress(&cinfo, TRUE);

  z = 0;

  while (cinfo.next_scanline < HEIGHT) {
    int x;

    unsigned char *ptr = line_buffer;

    for (x = 0; x < WIDTH; x++) {
      int r, g, b;

      int y, u, v;

      if (!z)

        y = yuyv[0] << 8;

      else

        y = yuyv[2] << 8;

      u = yuyv[1] - 128;

      v = yuyv[3] - 128;

      r = (y + (359 * v)) >> 8;

      g = (y - (88 * u) - (183 * v)) >> 8;

      b = (y + (454 * u)) >> 8;

      *(ptr++) = (r > 255) ? 255 : ((r < 0) ? 0 : r);

      *(ptr++) = (g > 255) ? 255 : ((g < 0) ? 0 : g);

      *(ptr++) = (b > 255) ? 255 : ((b < 0) ? 0 : b);

      if (z++) {
        z = 0;

        yuyv += 4;
      }
    }

    row_pointer[0] = line_buffer;

    jpeg_write_scanlines(&cinfo, row_pointer, 1);
  }

  jpeg_finish_compress(&cinfo);

  jpeg_destroy_compress(&cinfo);

  free(line_buffer);

  return (written);
}
