#ifndef PICLIB_H
#define PICLIB_H

#include "Picture.h"
#include "Utils.h"
#include <pthread.h>

/* Stores information about the picture. */
struct work_item
{
  struct picture *pic;
  struct picture *tmp;
  int row_index;
  int col_index;

  int start_row;
  int start_col;
  int sector_width;
  int sector_height;
};

// picture transformation routines
void invert_picture(struct picture *pic);
void grayscale_picture(struct picture *pic);
void rotate_picture(struct picture *pic, int angle);
void flip_picture(struct picture *pic, char plane);
void blur_picture(struct picture *pic);
void parallel_blur_picture(struct picture *pic);
#endif
