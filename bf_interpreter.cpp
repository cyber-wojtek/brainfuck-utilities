#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstddef>
#include <cstring>

int main(const int argc, char* argv[])
{
    char path[32767];

    char array[30000] = {0}, *ptr = array;

    if (argc > 1)
    {
        strcpy_s(path, argv[1]);
    }
    else
    {
        fprintf(stderr, "Enter the path to the file: ");
    }

    scanf_s("%32767s", path, 32767);

    FILE* bf_file = fopen(path, "r");

    if (!bf_file)
    {
        fprintf(stderr, "Error: Could not open file %s\n", path);
        return EXIT_FAILURE;
    }

    char line[1024];

    while (fgets(line, 1024, bf_file))
    {
        for (const char i : line)
        {
            if (i == '>')
                ++ptr;
            else if (i == '<')
                --ptr;
            else if (i == '+')
                ++*ptr;
            else if (i == '-')
                --*ptr;
            else if (i == '.')
                putchar(*ptr);
            else if (i == ',')
                (*ptr) = getchar();
        }
    }
}