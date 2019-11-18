#include <random.h>
#include <stdlib.h>
#include <stdio.h>
#include <syscall.h>
#include "tests/lib.h"
#include "tests/filesys/base/syn-write.h"


/* This test is to check the flush number is always less than 64 */
int
test_main (int argc, char *argv[])
{
  int tmp = 0;
  // fist check
  CHECK (mkdir ("a"), "mkdir \"a\"");
  tmp = cache_flush();
  CHECK (tmp <= 64, "cache number <= 64");

  // second check
  CHECK (create ("a/b", 512), "create \"a/b\"");
  tmp = cache_flush();
  CHECK (tmp <= 64, "cache number <= 64");
}
