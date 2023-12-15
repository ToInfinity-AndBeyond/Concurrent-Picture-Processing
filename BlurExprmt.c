#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>
#include "Utils.h"
#include "Picture.h"
#include "PicProcess.h"

#define BLUR_REGION_SIZE 9
#define MAX_THREAD 8
#define NUM_OF_BLUR_METHODS 6

int execute(void (*blur)(struct picture *), struct picture *pic, const char *blur_file);
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
void blur_pixel(struct work_item *work_arg);

struct thread_list
{
  struct node *head;
};

struct node
{
  pthread_t thread;
  struct node *next;
};

const char *methods[] = {
    "Sequential Blurring",
    "Row by row Blurring",
    "Column by column Blurring",
    "Sector by sector 4 Blurring",
    "Sector by sector 8 Blurring",
    "Pixel by pixel Blurring"};

struct MethodTime
{
  const char *method;
  long long average_time;
};

const char *file_names[] = {
    "images/duck_seq.jpg",
    "images/duck_row.jpg",
    "images/duck_col.jpg",
    "images/duck_sector_4.jpg",
    "images/duck_sector_8.jpg",
    "images/duck_pixel.jpg"};

// ---------- MAIN PROGRAM ---------- \\


int compare_method_time(const void *a, const void *b)
{
  struct MethodTime *method_time_a = (struct MethodTime *)a;
  struct MethodTime *method_time_b = (struct MethodTime *)b;

  if (method_time_a->average_time < method_time_b->average_time)
    return -1;
  else if (method_time_a->average_time > method_time_b->average_time)
    return 1;
  else
    return 0;
}

int main(int argc, char **argv)
{

  int iterations = atoi(argv[1]);

  const char *file = "images/ducks1.jpg";
  struct picture pic;

  long long average_times[NUM_OF_BLUR_METHODS] = {0};

  for (int i = 0; i < NUM_OF_BLUR_METHODS; i++)
  {
    long long execution_time = 0;
    for (int j = 0; j < iterations; j++)
    {
      if (!init_picture_from_file(&pic, file))
      {
        exit(EXIT_FAILURE);
      }

      // printf("%s Executed\n", methods[i]);
      execution_time += execute(
          i == 0   ? sequentially_blur_picture
          : i == 1 ? row_blur_picture
          : i == 2 ? column_blur_picture
          : i == 3 ? sector_blur_picture_by_4
          : i == 4 ? sector_blur_picture_by_8
                   : parallel_blur_picture,
          &pic,
          file_names[i]);

      clear_picture(&pic);
    }
    average_times[i] = execution_time / iterations;
  }

  struct MethodTime method_times[NUM_OF_BLUR_METHODS];

  for (int i = 0; i < NUM_OF_BLUR_METHODS; i++)
  {
    method_times[i].method = methods[i];
    method_times[i].average_time = average_times[i];
  }

  qsort(method_times, NUM_OF_BLUR_METHODS, sizeof(struct MethodTime), compare_method_time);

  printf("Iterated for %d times\n\n", iterations);
  printf("%-5s%-28s%-32s%-10s\n", " ", "Blurring Method", "Average Iteration Time", "Rank");
  for (int i = 0; i < sizeof(methods) / sizeof(methods[0]); ++i)
  {
    printf("%-35s%4lld milliseconds %15d\n", method_times[i].method,
           method_times[i].average_time, i + 1);
  }

  return 0;
}

int execute(void (*blur)(struct picture *), struct picture *pic, const char *blur_file)
{
  long long start = get_current_time();
  blur(pic);
  long long end = get_current_time();
  save_picture_to_file(pic, blur_file);
  return (end - start);
}

long long get_current_time(void)
{
  struct timeval currentTime;
  long long currentTimeMilliSeconds;

  gettimeofday(&currentTime, NULL);
  currentTimeMilliSeconds = currentTime.tv_sec * 1000LL + currentTime.tv_usec / 1000;
  return currentTimeMilliSeconds;
}

bool add_thread(struct thread_list *list, pthread_t thread)
{
  struct node *node = malloc(sizeof(struct node));
  if (node == NULL)
  {
    return false;
  }
  node->thread = thread;
  node->next = list->head;
  list->head = node;
  return true;
}

bool init_thread_list(struct thread_list *list)
{
  list->head = NULL;
  return true;
}

void join_free_thread(struct thread_list *list)
{
  struct node *current = list->head;
  while (current != NULL)
  {
    struct node *temp = current;
    current = current->next;
    pthread_join(temp->thread, NULL);
    free(temp);
  }
  list->head = NULL;
}

void join_free_finished_thread(struct thread_list *list)
{
  struct node *current = list->head;
  struct node *prev = NULL;
  while (current != NULL)
  {
    if (pthread_join(current->thread, NULL) == 0)
    {
      if (prev == NULL)
      {
        list->head = current->next;
      }
      else
      {
        prev->next = current->next;
      }
      struct node *temp = current;
      current = current->next;
      free(temp);
    }
    else
    {
      prev = current;
      current = current->next;
    }
  }
}

void sequentially_blur_picture(struct picture *pic)
{
  blur_picture(pic);
}

void blur_row(struct work_item *work_arg)
{
  for (int i = 0; i < work_arg->tmp->width; i++)
  {
    work_arg->row_index = i;
    blur_helper(work_arg);
  }
  free(work_arg);
}

void row_blur_picture(struct picture *pic)
{
  struct picture tmp;
  init_picture_from_size(&tmp, pic->width, pic->height);
  struct thread_list list;
  init_thread_list(&list);

  // struct ThreadPool pool;
  // init_thread_pool(&pool, MAX_THREAD);
  // int thread_counter = 0;

  for (int i = 0; i < tmp.height; i++)
  {
    struct work_item *item = (struct work_item *)malloc(sizeof(struct work_item));
    if (item == NULL)
    {
      break;
    }
    item->pic = pic;
    item->tmp = &tmp;
    item->col_index = i;

    // pthread_create(&pool.threads[thread_counter], NULL, (void *(*)(void *))blur_row, item);
    // thread_counter++;

    // if (thread_counter >= pool.thread_count)
    // {
    //   pthread_join(pool.threads[thread_counter % pool.thread_count], NULL);
    //   thread_counter--;
    // }

    pthread_t thread;
    pthread_create(&thread, NULL, (void *(*)(void *))blur_row, item);
    add_thread(&list, thread);
  }
  // free(pool.threads);
  join_free_thread(&list);
  clear_picture(pic);
  overwrite_picture(pic, &tmp);
}

void blur_col(struct work_item *work_arg)
{
  for (int i = 0; i < work_arg->pic->height; i++)
  {
    work_arg->col_index = i;
    blur_helper(work_arg);
  }
  free(work_arg);
}

void column_blur_picture(struct picture *pic)
{
  struct picture tmp;
  init_picture_from_size(&tmp, pic->width, pic->height);
  struct thread_list list;
  init_thread_list(&list);

  for (int i = 0; i < pic->width; i++)
  {
    struct work_item *item = (struct work_item *)malloc(sizeof(struct work_item));
    if (item == NULL)
    {
      break;
    }
    item->pic = pic;
    item->tmp = &tmp;
    item->row_index = i;
    pthread_t thread;
    pthread_create(&thread, NULL, (void *(*)(void *))blur_col, item);
    add_thread(&list, thread);
  }
  join_free_thread(&list);
  clear_picture(pic);
  overwrite_picture(pic, &tmp);
}

void blur_sector(struct work_item *work_arg)
{
  int start_row = work_arg->start_row;
  int start_col = work_arg->start_col;
  int sector_width = work_arg->sector_width;
  int sector_height = work_arg->sector_height;

  for (int i = start_row; i < start_row + sector_width; i++)
  {
    for (int j = start_col; j < start_col + sector_height; j++)
    {
      work_arg->row_index = i;
      work_arg->col_index = j;
      blur_helper(work_arg);
    }
  }
  free(work_arg);
}

void sector_blur_picture_by_4(struct picture *pic)
{
  struct picture tmp;
  init_picture_from_size(&tmp, pic->width, pic->height);
  struct thread_list list;
  init_thread_list(&list);

  int sector_width;
  int sector_height;

  sector_width = tmp.width / 2;
  sector_height = tmp.height / 2;

  for (int sector = 0; sector < 4; sector++)
  {
    int start_row = (sector / 2) * sector_width;
    int start_col = (sector % 2) * sector_height;

    struct work_item *item = (struct work_item *)malloc(sizeof(struct work_item));
    if (item == NULL)
    {
      break;
    }
    item->pic = pic;
    item->tmp = &tmp;

    item->sector_height = sector_height;
    item->sector_width = sector_width;
    item->start_col = start_col;
    item->start_row = start_row;
    pthread_t thread;
    pthread_create(&thread, NULL, (void *(*)(void *))blur_sector, item);
    add_thread(&list, thread);
    // if (pthread_create(&thread, NULL, (void *(*)(void *))blur_sector, item) != 0)
    // {
    //   join_free_finished_thread(&list);
    // }
    // add_thread(&list, thread);
  }
  join_free_thread(&list);
  clear_picture(pic);
  overwrite_picture(pic, &tmp);
}

void sector_blur_picture_by_8(struct picture *pic)
{
  struct picture tmp;
  init_picture_from_size(&tmp, pic->width, pic->height);
  struct thread_list list;
  init_thread_list(&list);

  int sector_width;
  int sector_height;
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
  for (int sector = 0; sector < 8; sector++)
  {
    int start_row;
    int start_col;
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

    struct work_item *item = (struct work_item *)malloc(sizeof(struct work_item));
    if (item == NULL)
    {
      break;
    }
    item->pic = pic;
    item->tmp = &tmp;

    item->sector_height = sector_height;
    item->sector_width = sector_width;
    item->start_col = start_col;
    item->start_row = start_row;
    pthread_t thread;
    pthread_create(&thread, NULL, (void *(*)(void *))blur_sector, item);
    add_thread(&list, thread);
    // if (pthread_create(&thread, NULL, (void *(*)(void *))blur_sector, item) != 0)
    // {
    //   join_free_finished_thread(&list);
    // }
    // add_thread(&list, thread);
  }
  join_free_thread(&list);
  clear_picture(pic);
  overwrite_picture(pic, &tmp);
}

void blur_pixel(struct work_item *work_arg)
{
  blur_helper(work_arg);
  free(work_arg);
}
void pixel_blur_picture(struct picture *pic)
{
  struct picture tmp;
  init_picture_from_size(&tmp, pic->width, pic->height);
  struct thread_list list;
  init_thread_list(&list);

  // iterate over each pixel in the picture
  for (int i = 0; i < tmp.width; i++)
  {
    for (int j = 0; j < tmp.height; j++)
    {
      struct work_item *item = (struct work_item *)malloc(sizeof(struct work_item));
      if (item == NULL)
      {
        break;
      }
      item->pic = pic;
      item->tmp = &tmp;
      item->row_index = i;
      item->col_index = j;

      pthread_t thread;
      while (pthread_create(&thread, NULL, (void *(*)(void *))blur_helper, item) != 0)
      {
        join_free_finished_thread(&list);
      }
      add_thread(&list, thread);
    }
  }
  join_free_thread(&list);
  clear_picture(pic);
  overwrite_picture(pic, &tmp);
}
