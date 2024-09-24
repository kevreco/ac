#include <stdio.h>

#include <re/dstr.h>

int main(int argc, const char* args[]) {
   
   dstr s;
   dstr_init(&s);
   dstr_append_str(&s, "Hello, World! (from tester) using dstr from re.lib \n");
   printf(s.data);
   
   dstr_destroy(&s);
   return 0;
}