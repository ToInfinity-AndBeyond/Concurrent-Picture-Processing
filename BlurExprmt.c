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

  printf("Sequential Blurring Executed\n");
  execute(sequentially_blur_picture, &pic, blur_file);
  clear_picture(&pic);

  if (!init_picture_from_file(&pic, file))
  {
    exit(IO_ERROR);
  }

  printf("Row by row Blurring Executed\n");
  execute(row_blur_picture, &pic, blur_file);
  clear_picture(&pic);

  if (!init_picture_from_file(&pic, file))
  {
    exit(IO_ERROR);
  }

  printf("Column by column Blurring Executed\n");
  execute(column_blur_picture, &pic, blur_file);
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
  for (int i = 1; i < work_arg->pic->width - 1; i++)
  {
    work_arg->col_index = i;
    blur_helper(work_arg);
  }
  free(work_arg);
}

void column_blur_picture(struct picture *pic)
{
  struct picture tmp;
  tmp.img = copy_image(pic->img);
  tmp.width = pic->width;
  tmp.height = pic->height;
  struct thread_list list;
  init_thread_list(&list);

  for (int i = 1; i < pic->width - 1; i++)
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
