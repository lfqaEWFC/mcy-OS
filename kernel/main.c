#include "init.h"
#include "print.h"
#include "debug.h"
#include "string.h"

int main(void) {
   put_str("I am kernel\n");
   init_all();
   ASSERT(strcmp("bbb","bbb"));
   while(1);
}
