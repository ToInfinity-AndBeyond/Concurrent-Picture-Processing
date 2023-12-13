#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>
#include "Utils.h"
#include "Picture.h"
#include "PicProcess.h"

#define BLUR_REGION_SIZE 9
#define MAX_THREAD 100

void execute(void (*blur)(struct picture *), struct picture *pic, const char *blur_file);
long long get_current_time(void);
void sequentially_blur_picture(struct picture *pic);
void blur_row(struct work_item *work_arg);
void row_blur_picture(struct picture *pic);
void blur_col(struct work_item *work_arg);
void column_blur_picture(struct picture *pic);
void blur_sector(struct work_item *work_arg);
void sector_blur_picture(struct picture *pic);
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

// ---------- MAIN PROGRAM ---------- \\

int main(int argc, char **argv)
{
  const char *file = "images/ducks1.jpg";
  const char *blur_file = "images/duck_seq.jpg";
  struct picture pic;

  if (!init_picture_from_file(&pic, file))
  {
    exit(IO_ERROR);
  }
  // printf("%d, %d", pic.width, pic.height);

  printf("Sequential Blurring Executed\n");
  execute(sequentially_blur_picture, &pic, "images/duck_seq.jpg");
  clear_picture(&pic);

  if (!init_picture_from_file(&pic, file))
  {
    exit(IO_ERROR);
  }

  printf("Row by row Blurring Executed\n");
  execute(row_blur_picture, &pic, "images/duck_row.jpg");
  clear_picture(&pic);

  if (!init_picture_from_file(&pic, file))
  {
    exit(IO_ERROR);
  }

  printf("Column by column Blurring Executed\n");
  execute(column_blur_picture, &pic, "images/duck_col.jpg");
  clear_picture(&pic);

  if (!init_picture_from_file(&pic, file))
  {
    exit(IO_ERROR);
  }

  printf("Sector by sector Blurring Executed\n");
  execute(sector_blur_picture, &pic, "images/duck_sector.jpg");
  clear_picture(&pic);

  if (!init_picture_from_file(&pic, file))
  {
    exit(IO_ERROR);
  }

  printf("Pixel by pixel Blurring Executed\n");
  execute(parallel_blur_picture, &pic, "images/duck_pixel.jpg");
  clear_picture(&pic);

  return 0;
}

void execute(void (*blur)(struct picture *), struct picture *pic, const char *blur_file)
{
  long long start = get_current_time();
  blur(pic);
  printf("Blurring Completed\n");
  long long end = get_current_time();
  printf("Time has passed: %lld\n", end - start);
  save_picture_to_file(pic, blur_file);
}

long long get_current_time(void)
{
  struct timeval currentTimeMicro;
  long long currentTimeMicros;

  gettimeofday(&currentTimeMicro, NULL);
  currentTimeMicros = currentTimeMicro.tv_sec * 1000000LL + currentTimeMicro.tv_usec;
  return currentTimeMicros;
}

bool add_thread(struct thread_list *list, pthread_t thread)
{
  struct node *node = malloc(sizeof(struct node));
  if (node == NULL)
  {
    printf("Not enough memory for creating a new thread. Abort...");
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

  pthread_t threads[MAX_THREAD];

  int thread_counter = 0;

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
    pthread_t thread;
    if (pthread_create(&thread, NULL, (void *(*)(void *))blur_row, item) != 0)
    {
      join_free_finished_thread(&list);
    }
    add_thread(&list, thread);
  }
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
    if (pthread_create(&thread, NULL, (void *(*)(void *))blur_col, item) != 0)
    {
      join_free_finished_thread(&list);
    }
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

void sector_blur_picture(struct picture *pic)
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
    if (pthread_create(&thread, NULL, (void *(*)(void *))blur_sector, item) != 0)
    {
      join_free_finished_thread(&list);
    }
    add_thread(&list, thread);
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

  pthread_t threads[10];
  int thread_counter = 0;

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
      if (thread_counter >= 10)
      {
        pthread_join(threads[thread_counter % 10], NULL);

        // Modify the list to remove finished threads
        join_free_finished_thread(&list);
      }
      pthread_t thread;
      if (pthread_create(&threads[thread_counter % 10], NULL, (void *(*)(void *))blur_helper, item) != 0)
      {
        join_free_finished_thread(&list);
      }
      add_thread(&list, threads[thread_counter % 10]);
    }
  }
  for (int i = 0; i < thread_counter % 10; i++)
  {
    pthread_join(threads[i], NULL);

    // Modify the list to remove finished threads
    join_free_finished_thread(&list);
  }
  clear_picture(pic);
  overwrite_picture(pic, &tmp);
}
