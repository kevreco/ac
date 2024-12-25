int main()
{
    char *func = __func__;
    char *FUNCTION = __FUNCTION__;
    char *PRETTY = __PRETTY_FUNCTION__;
    int a = *func == 'm' && *FUNCTION == 'm' && *PRETTY == 'm';
    return !a;
}

