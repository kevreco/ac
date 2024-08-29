#include <stdio.h>

#include <re/dstr.h>
#include <dyn_lib_a.h>
#include <dyn_lib_b.h>

int main(int argc, const char* args[]) {
   
   struct dstr s;
   dstr_init(&s);
   dstr_append_str(&s, "Hello, World! (from tester) using dstr from re.lib \n");
   printf(s.data);
   
   printf(dyn_lib_a_get_string());
   printf(dyn_lib_b_get_string());
   
   return 0;
}