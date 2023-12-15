#include "PicProcess.h"

#define NO_RGB_COMPONENTS 3
#define BLUR_REGION_SIZE 9
#define MAX_THREAD 8

void invert_picture(struct picture *pic)
{
  // iterate over each pixel in the picture
  for (int i = 0; i < pic->width; i++)
  {
    for (int j = 0; j < pic->height; j++)
    {
      struct pixel rgb = get_pixel(pic, i, j);

      // invert RGB values of pixel
      rgb.red = MAX_PIXEL_INTENSITY - rgb.red;
      rgb.green = MAX_PIXEL_INTENSITY - rgb.green;
      rgb.blue = MAX_PIXEL_INTENSITY - rgb.blue;

      // set pixel to inverted RBG values
      set_pixel(pic, i, j, &rgb);
    }
  }
}

void grayscale_picture(struct picture *pic)
{
  // iterate over each pixel in the picture
  for (int i = 0; i < pic->width; i++)
  {
    for (int j = 0; j < pic->height; j++)
    {
      struct pixel rgb = get_pixel(pic, i, j);

      // compute gray average of pixel's RGB values
      int avg = (rgb.red + rgb.green + rgb.blue) / NO_RGB_COMPONENTS;
      rgb.red = avg;
      rgb.green = avg;
      rgb.blue = avg;

      // set pixel to gray-scale RBG value
      set_pixel(pic, i, j, &rgb);
    }
  }
}

void rotate_picture(struct picture *pic, int angle)
{
  // capture current picture size
  int new_width = pic->width;
  int new_height = pic->height;

  // adjust output picture size as necessary
  if (angle == 90 || angle == 270)
  {
    new_width = pic->height;
    new_height = pic->width;
  }

  // make new temporary picture to work in
  struct picture tmp;
  init_picture_from_size(&tmp, new_width, new_height);

  // iterate over each pixel in the picture
  for (int i = 0; i < new_width; i++)
  {
    for (int j = 0; j < new_height; j++)
    {
      struct pixel rgb;
      // determine rotation angle and execute corresponding pixel update
      switch (angle)
      {
      case (90):
        rgb = get_pixel(pic, j, new_width - 1 - i);
        break;
      case (180):
        rgb = get_pixel(pic, new_width - 1 - i, new_height - 1 - j);
        break;
      case (270):
        rgb = get_pixel(pic, new_height - 1 - j, i);
        break;
      default:
        printf("[!] rotate is undefined for angle %i (must be 90, 180 or 270)\n", angle);
        clear_picture(&tmp);
        clear_picture(pic);
        exit(IO_ERROR);
      }
      set_pixel(&tmp, i, j, &rgb);
    }
  }

  // clean-up the old picture and replace with new picture
  clear_picture(pic);
  overwrite_picture(pic, &tmp);
}

void flip_picture(struct picture *pic, char plane)
{
  // make new temporary picture to work in
  struct picture tmp;
  init_picture_from_size(&tmp, pic->width, pic->height);

  // iterate over each pixel in the picture
  for (int i = 0; i < tmp.width; i++)
  {
    for (int j = 0; j < tmp.height; j++)
    {
      struct pixel rgb;
      // determine flip plane and execute corresponding pixel update
      switch (plane)
      {
      case ('V'):
        rgb = get_pixel(pic, i, tmp.height - 1 - j);
        break;
      case ('H'):
        rgb = get_pixel(pic, tmp.width - 1 - i, j);
        break;
      default:
        printf("[!] flip is undefined for plane %c\n", plane);
        clear_picture(&tmp);
        clear_picture(pic);
        exit(IO_ERROR);
      }
      set_pixel(&tmp, i, j, &rgb);
    }
  }

  // clean-up the old picture and replace with new picture
  clear_picture(pic);
  overwrite_picture(pic, &tmp);
}

void blur_picture(struct picture *pic)
{
  // make new temporary picture to work in
  struct picture tmp;
  init_picture_from_size(&tmp, pic->width, pic->height);

  // iterate over each pixel in the picture
  for (int i = 0; i < tmp.width; i++)
  {
    for (int j = 0; j < tmp.height; j++)
    {

      // set-up a local pixel on the stack
      struct pixel rgb = get_pixel(pic, i, j);

      // don't need to modify boundary pixels
      if (i != 0 && j != 0 && i != tmp.width - 1 && j != tmp.height - 1)
      {

        // set up running RGB component totals for pixel region
        int sum_red = rgb.red;
        int sum_green = rgb.green;
        int sum_blue = rgb.blue;

        // check the surrounding pixel region
        for (int n = -1; n <= 1; n++)
        {
          for (int m = -1; m <= 1; m++)
          {
            if (n != 0 || m != 0)
            {
              rgb = get_pixel(pic, i + n, j + m);
              sum_red += rgb.red;
              sum_green += rgb.green;
              sum_blue += rgb.blue;
            }
          }
        }

        // compute average pixel RGB value
        rgb.red = sum_red / BLUR_REGION_SIZE;
        rgb.green = sum_green / BLUR_REGION_SIZE;
        rgb.blue = sum_blue / BLUR_REGION_SIZE;
      }

      // set pixel to computed region RBG value (unmodified if boundary)
      set_pixel(&tmp, i, j, &rgb);
    }
  }

  // clean-up the old picture and replace with new picture
  clear_picture(pic);
  overwrite_picture(pic, &tmp);
}

void blur_helper(void *work_arg)
{
  struct work_item *work = (struct work_item *)work_arg;
  struct picture *pic = work->pic;
  struct picture *tmp = work->tmp;
  int i = work->row_index;
  int j = work->col_index;

  struct pixel rgb = get_pixel(pic, i, j);

  // don't need to modify boundary pixels
  if (i != 0 && j != 0 && i != tmp->width - 1 && j != tmp->height - 1)
  {

    // set up running RGB component totals for pixel region
    int sum_red = rgb.red;
    int sum_green = rgb.green;
    int sum_blue = rgb.blue;

    // check the surrounding pixel region
    for (int n = -1; n <= 1; n++)
    {
      for (int m = -1; m <= 1; m++)
      {
        if (n != 0 || m != 0)
        {
          rgb = get_pixel(pic, i + n, j + m);
          sum_red += rgb.red;
          sum_green += rgb.green;
          sum_blue += rgb.blue;
        }
      }
    }

    // compute average pixel RGB value
    rgb.red = sum_red / BLUR_REGION_SIZE;
    rgb.green = sum_green / BLUR_REGION_SIZE;
    rgb.blue = sum_blue / BLUR_REGION_SIZE;
  }
  // set pixel to computed region RBG value (unmodified if boundary)
  set_pixel(tmp, i, j, &rgb);
}

/*
   Creates threads for parallel execution of a specified thread function.
   Parameters:
     - threads: Array of pthread_t representing the threads.
     - thread_counter: Pointer to an integer storing the current thread count.
     - thread_function: Pointer to the function to be executed in a thread.
     - item: Pointer to the item to be processed by the thread function.
*/

void create_join_thread(pthread_t *threads, int *thread_counter, void *(*thread_function)(void *), void *item)
{
  /* Create a new thread for the given thread function and item. */
  pthread_create(&threads[*thread_counter], NULL, thread_function, item);
  /* Increment the thread counter to manage thread creation. */
  (*thread_counter)++;

  /* Check if the maximum thread limit is reached. */
  if (*thread_counter >= MAX_THREAD)
  {
    /* Wait for threads to finish before starting new ones. */
    for (int k = 0; k < MAX_THREAD; k++)
    {
      pthread_join(threads[k], NULL);
    }
    /* Reset the thread counter to manage new threads. */
    *thread_counter = 0;
  }
}

/*
   Waits for all active threads in the given array to complete.
   Parameters:
     - threads: Array containing thread IDs for parallel execution.
     - thread_count: The number of active threads in the array.
*/
void wait_for_threads(pthread_t *threads, int thread_count)
{
  for (int k = 0; k < thread_count; k++)
  {
    pthread_join(threads[k], NULL);
  }
}

/*
   Blurs a single pixel of the picture by invoking the blur_helper function.
   Parameters:
     - work_arg: Pointer to the work item containing information about the pixel to be blurred.
*/
void blur_pixel(struct work_item *work_arg)
{
  blur_helper(work_arg);
  /* Free the memory allocated for the work item after processing the pixel. */
  free(work_arg);
}

/*
   Apply a parallel blur effect to every pixel of the input picture.
   Parameters:
     - pic: Pointer to the original picture structure to be blurred.
*/
void parallel_blur_picture(struct picture *pic)
{
  /* Create a temporary picture struct to hold the blurred image, and initialise it using pic. */
  struct picture tmp;
  init_picture_from_size(&tmp, pic->width, pic->height);

  /* Array to store thread IDs for parallel execution. */
  pthread_t threads[MAX_THREAD];

  /* Counter to keep track of the number of active threads. */
  int thread_counter = 0;

  /* Iterate through each pixel of the picture for blurring. */
  for (int i = 0; i < tmp.width; i++)
  {
    for (int j = 0; j < tmp.height; j++)
    {
      /* Allocate memory for a work item to process the current pixel. */
      struct work_item *item = (struct work_item *)malloc(sizeof(struct work_item));
      if (item == NULL)
      {
        break;
      }
      /* Initialize work item with necessary pixel coordinates and picture information. */
      item->pic = pic;
      item->tmp = &tmp;
      item->row_index = i;
      item->col_index = j;

      /* Create and join a thread to blur the current pixel. */
      create_join_thread(threads, &thread_counter, (void *(*)(void *))blur_helper, item);
    }
  }
  /* Wait for all threads to complete before further processing. */
  wait_for_threads(threads, thread_counter);

  /* Clear the original picture and overwrite it with the blurred image. */
  clear_picture(pic);
  overwrite_picture(pic, &tmp);
}