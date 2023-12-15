#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>
#include "Utils.h"
#include "Picture.h"

#define BLUR_REGION_SIZE 9
#define MAX_THREAD 8
#define NUM_OF_BLUR_METHODS 6

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

/* Array of blur method names. */
const char *methods[] = {
    "Sequential Blurring",
    "Row by row Blurring",
    "Column by column Blurring",
    "Sector by sector 4 Blurring",
    "Sector by sector 8 Blurring",
    "Pixel by pixel Blurring"};

/* Struct that ties method with its average runtime. */
struct MethodTime
{
  const char *method;
  long long average_time;
};

/* Array of file names produced by each blur method. */
const char *blurred_files[] = {
    "_seq.jpg",
    "_row.jpg",
    "_col.jpg",
    "_sector_4.jpg",
    "_sector_8.jpg",
    "_pixel.jpg"};

int compare_method_time(const void *a, const void *b);
int execute(void (*blur)(struct picture *), struct picture *pic);
long long get_current_time(void);
void sequentially_blur_picture(struct picture *pic);
void blur_row(struct work_item *work_arg);
void row_blur_picture(struct picture *pic);
void blur_col(struct work_item *work_arg);
void column_blur_picture(struct picture *pic);
void blur_sector(struct work_item *work_arg);
void sector_blur_picture_by_8(struct picture *pic);
void sector_blur_picture_by_4(struct picture *pic);
void pixel_blur_picture(struct picture *pic);

// ---------- MAIN PROGRAM ---------- \\

/* Compare run time of each blurring method - to be used in qsort
   by sorting the blur method based ont the runtime. */
int compare_method_time(const void *a, const void *b)
{
  struct MethodTime *method_time_a = (struct MethodTime *)a;
  struct MethodTime *method_time_b = (struct MethodTime *)b;

  /* According to the average time, return -1, 1, or 0
     to determine the order of blur method. */
  if (method_time_a->average_time < method_time_b->average_time)
    return -1;
  else if (method_time_a->average_time > method_time_b->average_time)
    return 1;
  else
    return 0;
}

int main(int argc, char **argv)
{

  /* Default image to be tested is set as "images/kensington.jpg"*/
  char *file = "images/kensington.jpg";

  /* The second argument specifies the number of iterations.
     Ex) './blur_opt_exprmt 10' -> perform blurring 10 times on the given image: ducks1.jpg . */
  int iterations = atoi(argv[1]);

  /* If three arguments are provided, the second argument signifies the number of iterations,
     and the third argument specifies the image file path. This allows experimenting with different images.
     Ex) './blur_opt_exprmt 10 images/keepcalm.png' will blur 'keepcalm.png' 10 times. */
  if (argc == 3)
  {
    file = argv[2];
  }
  struct picture pic;

  long long average_times[NUM_OF_BLUR_METHODS] = {0};

  /* Loop through each blur method to measure execution time. */
  for (int i = 0; i < NUM_OF_BLUR_METHODS; i++)
  {
    long long execution_time = 0;
    /* Initialize the picture from a file. */
    if (!init_picture_from_file(&pic, file))
    {
      exit(EXIT_FAILURE);
    }
    /* Perform blurring 'iterations' times for the current method. */
    for (int j = 0; j < iterations; j++)
    {
      /* Execute the appropriate blur method and accumulate execution time. */
      execution_time += execute(
          /* Select the blur method based on index 'i'. */
          i == 0   ? sequentially_blur_picture
          : i == 1 ? row_blur_picture
          : i == 2 ? column_blur_picture
          : i == 3 ? sector_blur_picture_by_4
          : i == 4 ? sector_blur_picture_by_8
                   : pixel_blur_picture,
          &pic);
    }
    /* Generate the file path dynamically based on the given file name. */
    char dynamic_path[500];           // Adjust the size as needed to accommodate the path
    char file_without_extension[500]; // Adjust the size based on your maximum file path length

    char *extension_position = strstr(file, ".jpg");

    if (extension_position != NULL)
    {
      strncpy(file_without_extension, file, extension_position - file);
      file_without_extension[extension_position - file] = '\0'; // Null-terminate the string
    }
    sprintf(dynamic_path, "%s%s", file_without_extension, blurred_files[i]);

    /* Save the resulting picture to a file using the dynamically generated path. */
    save_picture_to_file(&pic, dynamic_path); /* Clear the picture after blurring. */
    clear_picture(&pic);
    /* Calculate and store the average execution time for the current blurring method. */
    average_times[i] = execution_time / iterations;
  }

  /* Create an array of 'MethodTime' struct to hold method names and average execution time. . */
  struct MethodTime method_times[NUM_OF_BLUR_METHODS];

  /* Populate the 'method_times' array with method names and their average times. */
  for (int i = 0; i < NUM_OF_BLUR_METHODS; i++)
  {
    method_times[i].method = methods[i];
    method_times[i].average_time = average_times[i];
  }

  /* Quicksort the 'method_times' array based on average execution times. */
  qsort(method_times, NUM_OF_BLUR_METHODS, sizeof(struct MethodTime), compare_method_time);

  /* Display results: Iteration count, sorted method names, their average times, and ranks. */
  printf("%23Iterated for %d time%s\n", iterations, iterations != 1 ? "s" : "");
  printf("---------------------------------------------------------------------\n");
  printf("%-5s%-28s%-32s%-12s\n", " ", "Blurring Method", "Average Blurring Time", "Rank");
  for (int i = 0; i < sizeof(methods) / sizeof(methods[0]); ++i)
  {
    printf("%-33s%6lld milliseconds %14d\n", method_times[i].method,
           method_times[i].average_time, i + 1);
  }
  return 0;
}

/*======================HELPER FUNCTIONS=======================*/

/*
   Executes the specified blur function on the given picture, measures its execution time,
   and saves the resulting picture to a file. Returns the time taken in milliseconds.
   Parameters:
     - blur: Function pointer to the blur method.
     - pic: Pointer to the picture struct to be blurred.
     - blur_file: File name for the resulting blurred picture.
*/
int execute(void (*blur)(struct picture *), struct picture *pic)
{
  /* Records the start time before applying the blur. */
  long long start = get_current_time();
  /* Apply the specified blur function to the picture. */
  blur(pic);
  /* Record the end time after applying the blur. */
  long long end = get_current_time();
  /* Calculate and return the time taken for blurring in milliseconds. */
  return (end - start);
}

/* Retrieves the current time in milliseconds. */
long long get_current_time(void)
{
  struct timeval currentTime;
  long long currentTimeMilliSeconds;

  /* Get the current time from the system. */
  gettimeofday(&currentTime, NULL);
  /* Convert the obtained time to milliseconds. */
  currentTimeMilliSeconds = currentTime.tv_sec * 1000LL + currentTime.tv_usec / 1000;
  return currentTimeMilliSeconds;
}

void blur_helper_exprmt(void *work_arg)
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

void create_join_thread_exprmt(pthread_t *threads, int *thread_counter, void *(*thread_function)(void *), void *item)
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
void wait_for_threads_exprmt(pthread_t *threads, int thread_count)
{
  for (int k = 0; k < thread_count; k++)
  {
    pthread_join(threads[k], NULL);
  }
}

/*======================BLUR SEQUENTIALLY=======================*/

/*
   Executes a sequential blurring operation on the entire picture.
   Parameters:
     - pic: Pointer to the picture struct to be blurred.
*/

void sequentially_blur_picture(struct picture *pic)
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
}
/*======================BLUR ROW BY ROW=======================*/

/*
   Blurs a single row of the picture by invoking the blur_helper_exprmt function.
   Parameters:
     - work_arg: Pointer to the work item containing information about the row to be blurred.
*/

void blur_row(struct work_item *work_arg)
{
  /* Iterate through each pixel in the row and apply the blur operation. */
  for (int i = 0; i < work_arg->tmp->width; i++)
  {
    /* Set the current row index in the work item. */
    work_arg->row_index = i;
    blur_helper_exprmt(work_arg);
  }
  /* Free the memory allocated for the work item after processing the column. */
  free(work_arg);
}

/*
   Initiates row-wise blurring of the picture using multiple threads.
   Parameters:
     - pic: Pointer to the original picture struct.
*/

void row_blur_picture(struct picture *pic)
{
  /* Create a temporary picture struct to hold the blurred image, and initialise it using pic. */
  struct picture tmp;
  init_picture_from_size(&tmp, pic->width, pic->height);

  /* Array to store thread IDs for parallel execution. */
  pthread_t threads[MAX_THREAD];
  /* Counter to keep track of the number of active threads. */
  int thread_counter = 0;

  /* Iterate through each row of the picture. */
  for (int i = 0; i < tmp.height; i++)
  {
    /* Allocate memory for a work item to process a row. */
    struct work_item *item = (struct work_item *)malloc(sizeof(struct work_item));
    if (item == NULL)
    {
      break;
    }
    /*  Initialize work item with necessary information for blurring a row. */
    item->pic = pic;
    item->tmp = &tmp;
    item->col_index = i;

    /* Create and join a thread to blur the current row. */
    create_join_thread_exprmt(threads, &thread_counter, (void *(*)(void *))blur_row, item);
  }
  /* Wait for all threads to complete before further processing. */
  wait_for_threads_exprmt(threads, thread_counter);

  /* Clear the original picture and overwrite it with the blurred image. */
  clear_picture(pic);
  overwrite_picture(pic, &tmp);
}

/*======================BLUR COLUMN BY COLUMN=======================*/

/*
   Blurs a single column of the picture by invoking the blur_helper_exprmt function.
   Parameters:
     - work_arg: Pointer to the work item containing information about the column to be blurred.
*/
void blur_col(struct work_item *work_arg)
{
  /* Iterate through each pixel in the column and apply the blur operation. */
  for (int i = 0; i < work_arg->pic->height; i++)
  {
    /* Set the current column index in the work item. */
    work_arg->col_index = i;
    /* Call the blur_helper_exprmt function to perform the blur operation. */
    blur_helper_exprmt(work_arg);
  }
  /* Free the memory allocated for the work item after processing the column. */
  free(work_arg);
}

/*
   Initiates column-wise blurring of the picture using multiple threads.
   Parameters:
     - pic: Pointer to the original picture struct.
*/
void column_blur_picture(struct picture *pic)
{
  /* Create a temporary picture struct to hold the blurred image, and initialise it using pic. */
  struct picture tmp;
  init_picture_from_size(&tmp, pic->width, pic->height);

  /* Array to store thread IDs for parallel execution. */
  pthread_t threads[MAX_THREAD];

  /* Counter to keep track of the number of active threads. */
  int thread_counter = 0;

  /* Iterate through each column of the picture. */
  for (int i = 0; i < pic->width; i++)
  {
    /* Allocate memory for a work item to process a column. */
    struct work_item *item = (struct work_item *)malloc(sizeof(struct work_item));
    if (item == NULL)
    {
      break;
    }
    /* Initialize work item with necessary information for blurring a column. */
    item->pic = pic;
    item->tmp = &tmp;
    item->row_index = i;

    /* Create and join a thread to blur the current column. */
    create_join_thread_exprmt(threads, &thread_counter, (void *(*)(void *))blur_col, item);
  }

  /* Wait for all threads to complete before further processing. */
  wait_for_threads_exprmt(threads, thread_counter);
  /* Clear the original picture and overwrite it with the blurred image. */
  clear_picture(pic);
  overwrite_picture(pic, &tmp);
}

/*======================BLUR SECTOR BY SECTOR=======================*/

/*
   Blurs a specific sector of the picture by invoking the blur_helper_exprmt function on the specified region.
   Parameters:
     - work_arg: Pointer to the work item containing information about the sector to be blurred.
*/
void blur_sector(struct work_item *work_arg)
{
  /* Retrieve parameters for the sector to be blurred. */
  int start_row = work_arg->start_row;
  int start_col = work_arg->start_col;
  int sector_width = work_arg->sector_width;
  int sector_height = work_arg->sector_height;

  /* Iterate through the pixels within the specified sector and apply the blur operation. */
  for (int i = start_row; i < start_row + sector_width; i++)
  {
    for (int j = start_col; j < start_col + sector_height; j++)
    {
      /* Set the current row and column indices in the work item. */
      work_arg->row_index = i;
      work_arg->col_index = j;
      blur_helper_exprmt(work_arg);
    }
  }
  /* Free the memory allocated for the work item after processing the sector blurring. */
  free(work_arg);
}

/*
   Initiates sector-wise blurring of the picture divided into 4 sectors using multiple threads.
   Parameters:
     - pic: Pointer to the original picture struct.
*/

void sector_blur_picture_by_4(struct picture *pic)
{
  /* Create a temporary picture struct to hold the blurred image, and initialise it using pic. */
  struct picture tmp;
  init_picture_from_size(&tmp, pic->width, pic->height);

  /* Array to store thread IDs for parallel execution. */
  pthread_t threads[MAX_THREAD];
  /* Counter to keep track of the number of active threads. */
  int thread_counter = 0;

  /* Calculate sector dimensions for 4 sectors. */
  int sector_width = tmp.width / 2;
  int sector_height = tmp.height / 2;

  /* Iterate through each of the 4 sectors. */
  for (int sector = 0; sector < 4; sector++)
  {
    /* Calculate starting row and column indices for the current sector. */
    int start_row = (sector / 2) * sector_width;
    int start_col = (sector % 2) * sector_height;

    /* Allocate memory for a work item to process the current sector. */
    struct work_item *item = (struct work_item *)malloc(sizeof(struct work_item));
    if (item == NULL)
    {
      break;
    }
    /* Initialize work item with necessary information for blurring a sector. */
    item->pic = pic;
    item->tmp = &tmp;

    item->sector_height = sector_height;
    item->sector_width = sector_width;
    item->start_col = start_col;
    item->start_row = start_row;

    /* Create and join a thread to blur the current sector. */
    create_join_thread_exprmt(threads, &thread_counter, (void *(*)(void *))blur_sector, item);
  }
  /* Wait for all threads to complete before further processing. */
  wait_for_threads_exprmt(threads, thread_counter);
  /* Clear the original picture and overwrite it with the blurred image. */
  clear_picture(pic);
  overwrite_picture(pic, &tmp);
}

void sector_blur_picture_by_8(struct picture *pic)
{
  /* Create a temporary picture struct to hold the blurred image, and initialise it using pic. */

  struct picture tmp;
  init_picture_from_size(&tmp, pic->width, pic->height);

  /* Array to store thread IDs for parallel execution. */
  pthread_t threads[MAX_THREAD];
  /* Counter to keep track of the number of active threads. */
  int thread_counter = 0;

  /* Calculate sector dimensions for 8 sectors based on picture width and height. */
  int sector_width, sector_height;
  if (tmp.width > tmp.height)
  {
    sector_width = tmp.width / 4;
    sector_height = tmp.height / 2;
  }
  else
  {
    sector_height = tmp.height / 4;
    sector_width = tmp.width / 2;
  }
  /* Iterate through each of the 8 sectors. */
  for (int sector = 0; sector < 8; sector++)
  {
    /* Calculate starting row and column indices for the current sector. */
    int start_row, start_col;
    if (tmp.width > tmp.height)
    {
      start_row = (sector / 2) * sector_width;
      start_col = (sector % 2) * sector_height;
    }
    else
    {
      start_row = (sector % 2) * sector_width;
      start_col = (sector / 2) * sector_height;
    }

    /* Allocate memory for a work item to process the current sector. */
    struct work_item *item = (struct work_item *)malloc(sizeof(struct work_item));
    if (item == NULL)
    {
      break;
    }
    /* Initialize work item with necessary information for blurring a sector. */
    item->pic = pic;
    item->tmp = &tmp;

    item->sector_height = sector_height;
    item->sector_width = sector_width;
    item->start_col = start_col;
    item->start_row = start_row;

    /* Create and join a thread to blur the current sector. */
    create_join_thread_exprmt(threads, &thread_counter, (void *(*)(void *))blur_sector, item);
  }
  wait_for_threads_exprmt(threads, thread_counter);
  /* Clear the original picture and overwrite it with the blurred image. */
  clear_picture(pic);
  overwrite_picture(pic, &tmp);
}

/*======================BLUR PIXEL BY PIXEL=======================*/

/*
   Blurs a single pixel of the picture by invoking the blur_helper_exprmt function.
   Parameters:
     - work_arg: Pointer to the work item containing information about the pixel to be blurred.
*/
void blur_pixel_exprmt(struct work_item *work_arg)
{
  blur_helper_exprmt(work_arg);
  /* Free the memory allocated for the work item after processing the pixel. */
  free(work_arg);
}

/*
   Apply a parallel blur effect to every pixel of the input picture.
   Parameters:
     - pic: Pointer to the original picture structure to be blurred.
*/

void pixel_blur_picture(struct picture *pic)
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
      create_join_thread_exprmt(threads, &thread_counter, (void *(*)(void *))blur_pixel_exprmt, item);
    }
  }
  /* Wait for all threads to complete before further processing. */
  wait_for_threads_exprmt(threads, thread_counter);

  /* Clear the original picture and overwrite it with the blurred image. */
  clear_picture(pic);
  overwrite_picture(pic, &tmp);
}
