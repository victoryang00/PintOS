#include <random.h>
#include <stdlib.h>
#include <stdio.h>
#include <syscall.h>
#include "tests/lib.h"
#include "tests/filesys/base/syn-write.h"

/* run two flush consecutively, the first flush
 cache number should be greater than 0, and the
 second flush cache number should be 0.
  */
int
test_main (int argc, char *argv[])
{
  int tmp = 0;
  CHECK (mkdir ("a"), "mkdir \"a\"");
  CHECK (create ("a/b", 512), "create \"a/b\"");
  CHECK (cache_flush()>=0, "fist flush sucess");
  CHECK (cache_flush()>=0, "second flush nothing");
}