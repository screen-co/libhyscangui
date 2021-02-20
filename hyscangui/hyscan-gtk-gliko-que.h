typedef struct _que_t
{
  int64_t count;
  char *data;
  int size;
  int length;
  int index;
} que_t;

static void
initque (que_t *que, void *data, const int item_size, const int que_length)
{
  que->data = data;
  que->size = item_size;
  que->length = que_length;
  que->index = 0;
  que->count = 0;
}

static void
enque (que_t *que, const void *srcv, const int items)
{
  int i, k, n, s;
  const char *src = srcv;

  k = items;
  if (k > que->length)
    {
      k = que->length;
    }
  i = que->index;
  if ((i + k) > que->length)
    {
      n = que->length - i;
      s = n * que->size;
      memcpy (que->data + i * que->size, src, s);
      k -= n;
      i = 0;
      que->count += n;
      src += s;
    }
  if (k != 0)
    {
      memcpy (que->data + i * que->size, src, que->size * k);
      i += k;
      if (i >= que->length)
        {
          i = 0;
        }
      que->count += k;
    }
  que->index = i;
}

static int
deque (que_t *que, void *dstv, const int items, int64_t count)
{
  int64_t d;
  int i, k, n, r, s;
  char *dst = dstv;

  d = que->count - count;
  if (d == 0)
    return 0;
  if (d >= que->length)
    return -1;
  i = que->index - d;
  if (i < 0)
    {
      i += que->length;
    }
  k = items;
  if (k > d)
    {
      k = d;
    }
  r = 0;
  if ((i + k) > que->length)
    {
      n = que->length - i;
      k -= n;
      s = n * que->size;
      if (dstv != NULL)
        {
          memcpy (dst, que->data + i * que->size, s);
        }
      i = 0;
      r += n;
      dst += s;
    }
  if (k != 0)
    {
      if (dstv != NULL)
        {
          memcpy (dst, que->data + i * que->size, que->size * k);
        }
      r += k;
    }
  return r;
}

/*static int lenque( que_t *que, int64_t count )
{
  return (int)(que->count - count);
}*/
