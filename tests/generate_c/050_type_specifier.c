const int const_int = 0;
static int static_int = 0;
volatile int volatile_int = 0;
bool bool_ = 0;
char char_ = 0;
short short_ = 0;
short int short_int = 0;
long int long_int = 0;
long long int long_long_int = 0;
long long long_long = 0;
void* void_ptr = 0;
signed signed_ = 0;
unsigned unsigned_ = 0;

signed int signed_int = 0;
unsigned int unsigned_int = 0;
signed char signed_char = 0;
unsigned char unsigned_char = 0;

inline int inline_func() { return 0; }
static int static_func() { return 0; }
static inline int static_inline_func() { return 0; }
inline static int inline_static_func() { return 0; }
int inline static int_inline_static() { return 0; }
const int inline static const_int_inline_static() { return 0; }
int inline const static int_inline_const_static() { return 0; }
int inline const long static long int_inline_const_long_static_long() { return 0; }

void* ptr = 0;

void restrict_(char* restrict a, char* restrict b);

int main()
{
    auto a = 0;
    register int b = 0;
    volatile int c = 0;
	return a;
}
