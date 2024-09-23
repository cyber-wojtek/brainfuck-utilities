#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <algorithm>

#define MEMORY_SIZE 30000
#define BUFFER_SIZE 1000000

inline void substring(const char *inputString, int startPos, int length, char *outputString) 
{
    int i;
    
    for(i = 0; i < length && inputString[startPos + i] != '\0'; i++) {
        outputString[i] = inputString[startPos + i];
    }
    
    outputString[i] = '\0';
}

inline constexpr char special_to_escaped(const char& chr)
{
    switch (chr)
    {
        case '\n':
            return 'n';
        case '\r':
            return 'r';
        case '\t':
            return 't';
        case '\\':
            return '\\';
        case '\"':
            return '\"';
        default:
            return '\0';
    }
}

inline constexpr bool is_special(const char& chr)
{
    if (chr == '\n' || chr == '\r' || chr == '\t' || chr == '\\' || chr == '\"')
        return true;
    return false;
}

int main(int argc, const char *argv[]) 
{
    /*
        Flags in target program
        -O[0-2] 0 does nothing, 1 enables code-logic optimizations, 2 enables compile-time evaluation
        -Opf enables putchar to printf optimization (at least most of times optimization) (Only to be used with -O2)
        -Oc[0-3, fast] specifies internal GCC's optimization flag for C code
    */

    if (argc < 2) {
        printf("Usage: %s {filename}.bf [-O[0-2], -Opf, -Oc[0-3, fast], -o {filename}.exe\n", argv[0]);
        return 1;
    }

    const char *input_filename = argv[1];
    char output_filename[256] = "out.exe", c_output_filename[256] = "out.c";
    bool printf_optimized = false;
    uint8_t optimization_level = 0;
    char c_optimized[5] = {0};

    // Parse command-line arguments
    for (int i = 1; i < argc - 1; i++) 
    {
        if (strcmp(argv[i], "-o") == 0) 
        {
            strcpy(output_filename, argv[i + 1]);
            // Copy the output filename without the .exe extension
            strcpy(c_output_filename, argv[i + 1]); // Copy the output filename without the .exe extension
            char* dot;
            if ((dot = std::find(c_output_filename, c_output_filename + 256, '.')) != c_output_filename + 256) {
                *dot = '\0'; // Null-terminate the string at the position of the last '.'
            }
            strcat(c_output_filename, ".c"); // Append ".c" extension
        }
        else if (strncmp(argv[i], "-Oc", 3) == 0)
        {
            substring(argv[i], 3, 5, c_optimized);
        }
        else if (strcmp(argv[i], "-Opf") == 0)
        {
            printf_optimized = true;
        }
        else if (strncmp(argv[i], "-O", 2) == 0)
        {
            optimization_level = argv[i][2] - '0';
        }
    }
    
    printf("Optimization: %i\n", optimization_level);

    FILE *file = fopen(input_filename, "r"); // Open the input file in read mode
    if (file == NULL) {
        perror("Error opening input file");
        return 1;
    }

    // Allocate memory for a buffer
    char buffer[BUFFER_SIZE];
    uint64_t bytesRead;

    // Read the file into the buffer
    bytesRead = fread(buffer, sizeof(char), BUFFER_SIZE, file);
    if (!bytesRead) {
        perror("Error reading input file");
        fclose(file);
        return 1;
    }
    // Close the input file
    fclose(file);  

    // Create new file for C output
    FILE *outFile = fopen(c_output_filename, "w");

    // Check if output file opened successfully
    if (!outFile) {
        perror("Error creating output file");
        return 1;
    }

    buffer[bytesRead] = '\0';
    char *buf_ptr = buffer - 1;

    bool printf_printed = false;
    char printf_args[500000] = {0}; 
    int index_pa = 0;

    if (optimization_level == 2)
    {
        // Print the C code onto the file

        bool found_comma = false, found_dot = false;
        
        while (*++buf_ptr)
        {
            switch (*buf_ptr)
            {
                case ',':
                    found_comma = true;
                    break;
                case '.':
                    found_dot = true;
                    break;
                default:
                    break;
            }
        }
        buf_ptr = buffer;

        if (found_dot || found_comma)
            fprintf(outFile, "#include <stdio.h>\n");

        fprintf(outFile, "int main(){");

        if (found_comma)
        {
            fprintf(outFile, "char array[");
            fprintf(outFile, "%li", MEMORY_SIZE);
            fprintf(outFile, "]={0},*ptr=array;");
        }
        char array[MEMORY_SIZE] = {0};
        int32_t array_ptr_diff = 0, loop_stack_ptr = -1;
        bool not_constant_value[MEMORY_SIZE] = {false};

        if (found_dot)
        {
            int32_t loop_stack[MEMORY_SIZE];
            int32_t array_ptr = 0;
            buf_ptr = buffer;

            while (*++buf_ptr)
            {
                switch (*buf_ptr)
                {
                    case '>':
                        ++array_ptr;
                        break;
                    case '<':
                        --array_ptr;
                        break;
                    case '+':
                        ++array[array_ptr];
                        break;
                    case '-':
                        --array[array_ptr];
                        break;
                    case '.':
                        if (printf_optimized)
                        {
                            if (!printf_printed)
                            {
                                fprintf(outFile, "printf(\"");
                                printf_printed = true;
                            }
                            if (!not_constant_value[array_ptr])
                            {
                                // NULL character handling
                                if (!array[array_ptr])
                                {
                                    fprintf(outFile, "%%c");
                                    snprintf(printf_args + index_pa, sizeof(printf_args) - index_pa, ",0");
                                    index_pa += 2;
                                }
                                // percent character handling
                                else if (array[array_ptr] == '%')
                                    fprintf(outFile, "%%%%");
                                // other special characters handling
                                else if (is_special(array[array_ptr]))
                                    fprintf(outFile, "\\%c", special_to_escaped(array[array_ptr]));
                                // characters that don't need to be escaped in any way
                                else
                                    fprintf(outFile, "%c", array[array_ptr]);
                            }
                            else
                            {
                                fprintf(outFile, "%%c");
                                snprintf(printf_args + index_pa, sizeof(printf_args) - index_pa, ",*(ptr+%i)+=%i", array_ptr, array[array_ptr]);
                                index_pa += 10;
                                char num_str[20];
                                ltoa(array_ptr, num_str, 10);
                                index_pa += strlen(num_str);
                                memset(num_str, 0, 20);
                                ltoa(array[array_ptr], num_str, 10);
                                index_pa += strlen(num_str);
                                array[array_ptr] = 0; // Reset the value
                            }
                        }
                        else
                        {
                            if (!not_constant_value[array_ptr])
                                fprintf(outFile, "putchar(%i);", array[array_ptr]);
                            else {
                                fprintf(outFile, "putchar(*(ptr+%i)+=%i);", array_ptr, array[array_ptr]);
                                array[array_ptr] = 0; // Reset the value
                            }
                        }
                        break;
                    case ',':
                        not_constant_value[array_ptr] = true;
                        if (printf_optimized && printf_printed)
                        {
                            fprintf(outFile, "\"%s);*(ptr+%i)=getchar();", printf_args, array_ptr);
                            index_pa = 0;
                            memset(printf_args, 0, sizeof(printf_args));
                            printf_printed = false;
                        }
                        else
                            fprintf(outFile, "*(ptr+%i)=getchar();", array_ptr);
                        break;
                    case '[':
                        if (!array[array_ptr]) 
                        {
                            // Jump to matching ']'
                            int loop_count = 1;
                            while (loop_count) 
                            {
                                if (*++buf_ptr == '[') ++loop_count;
                                else if (*buf_ptr == ']') --loop_count;
                            }
                        } 
                        else
                            loop_stack[++loop_stack_ptr] = buf_ptr - buffer; // Push position of '[' onto loop stack
                        break;
                    case ']':
                        if (array[array_ptr]) 
                            buf_ptr = buffer + loop_stack[loop_stack_ptr]; // Jump back to corresponding '['
                        else
                            --loop_stack_ptr; // Pop from loop stack
                        break;
                    case '@':
                        goto exitwhile;
                        break;
                    default:
                        break;
                }
            }
            exitwhile:
            if (printf_optimized)
                fprintf(outFile, "\"%s);", printf_args);
        }
    }
    else if (optimization_level == 1)
    {
        // Print the C code onto the file
        char array[MEMORY_SIZE] = {0};
        int array_ptr = 0, array_ptr_diff = 0;



        fprintf(outFile, "#include <stdio.h>\nint main(){");
        fprintf(outFile, "char array[");
        fprintf(outFile, "%li", MEMORY_SIZE);
        fprintf(outFile, "]={0},*ptr=array;");

        while (*++buf_ptr)
        {
            if (buf_ptr + 2 < buffer + bytesRead && *buf_ptr == '[' && (*(buf_ptr + 1) == '+' || *(buf_ptr + 1) == '-') && *buf_ptr == ']')
            {
                fprintf(outFile, "*ptr=0;");
                buf_ptr += 3;
            }
            switch (*buf_ptr)
            {
                // Handling BF statements
                case '>':
                    if (printf_optimized && printf_printed)
                    {
                        fprintf(outFile, "\"%s);", printf_args);
                        index_pa = 0;
                        memset(printf_args, 0, sizeof(printf_args));
                        printf_printed = false;
                    }
                    if (array[array_ptr])
                    {
                        if (array[array_ptr] > 1)
                            fprintf(outFile, "*ptr+=%i;", array[array_ptr]);
                        else if (array[array_ptr] == 1)
                            fprintf(outFile, "++*ptr;");
                        else if (array[array_ptr] == -1)
                            fprintf(outFile, "--*ptr;");
                        else
                            fprintf(outFile, "*ptr-=%i;", -array[array_ptr]);
                        array[array_ptr] = 0;
                    }
                    ++array_ptr;
                    ++array_ptr_diff;
                    break;
                case '<':
                    if (printf_optimized && printf_printed)
                    {
                        fprintf(outFile, "\"%s);", printf_args);
                        index_pa = 0;
                        memset(printf_args, 0, sizeof(printf_args));
                        printf_printed = false;
                    }
                    if (array[array_ptr])
                    {
                        if (array[array_ptr] > 1)
                            fprintf(outFile, "*ptr+=%i;", array[array_ptr]);
                        else if (array[array_ptr] == 1)
                            fprintf(outFile, "++*ptr;");
                        else if (array[array_ptr] == -1)
                            fprintf(outFile, "--*ptr;");
                        else
                            fprintf(outFile, "*ptr-=%i;", -array[array_ptr]);
                        array[array_ptr] = 0;
                    }
                    --array_ptr;
                    --array_ptr_diff;
                    break;
                case '+':
                    if (printf_optimized && printf_printed)
                    {
                        fprintf(outFile, "\"%s);", printf_args);
                        index_pa = 0;
                        memset(printf_args, 0, sizeof(printf_args));
                        printf_printed = false;
                    }
                    if (array_ptr_diff)
                    {
                        if (array_ptr_diff > 1)
                            fprintf(outFile, "ptr+=%i;", array_ptr_diff);
                        else if (array_ptr_diff == 1)
                            fprintf(outFile, "++ptr;");
                        else if (array_ptr_diff == -1)
                            fprintf(outFile, "--ptr;");
                        else
                            fprintf(outFile, "ptr-=%i;", -array_ptr_diff);
                        array_ptr_diff = 0;
                    }
                    //fprintf(outFile, "++*ptr;");
                    ++array[array_ptr];
                    break;
                case '-':
                    if (printf_optimized && printf_printed)
                    {
                        fprintf(outFile, "\"%s);", printf_args);
                        index_pa = 0;
                        memset(printf_args, 0, sizeof(printf_args));
                        printf_printed = false;
                    }
                    if (array_ptr_diff)
                    {
                        if (array_ptr_diff > 1)
                            fprintf(outFile, "ptr+=%i;", array_ptr_diff);
                        else if (array_ptr_diff == 1)
                            fprintf(outFile, "++ptr;");
                        else if (array_ptr_diff == -1)
                            fprintf(outFile, "--ptr;");
                        else
                            fprintf(outFile, "ptr-=%i;", -array_ptr_diff);
                        array_ptr_diff = 0;
                    }
                    //fprintf(outFile, "--*ptr;");
                    --array[array_ptr];
                    break;
                case '.':                    
                    if (array_ptr_diff)
                    {
                        if (array_ptr_diff > 1)
                            fprintf(outFile, "ptr+=%i;", array_ptr_diff);
                        else if (array_ptr_diff == 1)
                            fprintf(outFile, "++ptr;");
                        else if (array_ptr_diff == -1)
                            fprintf(outFile, "--ptr;");
                        else
                            fprintf(outFile, "ptr-=%i;", -array_ptr_diff);
                        array_ptr_diff = 0;
                    }
                    if (array[array_ptr])
                    {
                        if (array[array_ptr] > 1)
                            fprintf(outFile, "*ptr+=%i;", array[array_ptr]);
                        else if (array[array_ptr] == 1)
                            fprintf(outFile, "++*ptr;");
                        else if (array[array_ptr] == -1)
                            fprintf(outFile, "--*ptr;");
                        else
                            fprintf(outFile, "*ptr-=%i;", -array[array_ptr]);
                        array[array_ptr] = 0;
                    }
                    if (printf_optimized)
                    {
                        if (!printf_printed)
                        {
                            fprintf(outFile, "printf(\"");
                            printf_printed = true;
                        }
                        fprintf(outFile, "%%c");
                        snprintf(printf_args + index_pa, sizeof(printf_args) - index_pa, ",*ptr");
                        index_pa += 5;
                    }
                    else
                        fprintf(outFile, "putchar(*ptr);");
                    break;
                case ',':
                    if (printf_optimized && printf_printed)
                    {
                        fprintf(outFile, "\"%s);", printf_args);
                        index_pa = 0;
                        memset(printf_args, 0, sizeof(printf_args));
                        printf_printed = false;
                    }
                    if (array_ptr_diff)
                    {
                        if (array_ptr_diff > 1)
                            fprintf(outFile, "ptr+=%i;", array_ptr_diff);
                        else if (array_ptr_diff == 1)
                            fprintf(outFile, "++ptr;");
                        else if (array_ptr_diff == -1)
                            fprintf(outFile, "--ptr;");
                        else
                            fprintf(outFile, "ptr-=%i;", -array_ptr_diff);
                        array_ptr_diff = 0;
                    }
                    array[array_ptr] = 0;
                    fprintf(outFile, "*ptr=getchar();");
                    break;      
                case '[':
                    if (printf_optimized && printf_printed)
                    {
                        fprintf(outFile, "\"%s);", printf_args);
                        index_pa = 0;
                        memset(printf_args, 0, sizeof(printf_args));
                        printf_printed = false;
                    }
                    if (array_ptr_diff)
                    {
                        if (array_ptr_diff > 1)
                            fprintf(outFile, "ptr+=%i;", array_ptr_diff);
                        else if (array_ptr_diff == 1)
                            fprintf(outFile, "++ptr;");
                        else if (array_ptr_diff == -1)
                            fprintf(outFile, "--ptr;");
                        else
                            fprintf(outFile, "ptr-=%i;", -array_ptr_diff);
                        array_ptr_diff = 0;
                    }
                    if (array[array_ptr])
                    {
                        if (array[array_ptr] > 1)
                            fprintf(outFile, "*ptr+=%i;", array[array_ptr]);
                        else if (array[array_ptr] == 1)
                            fprintf(outFile, "++*ptr;");
                        else if (array[array_ptr] == -1)
                            fprintf(outFile, "--*ptr;");
                        else
                            fprintf(outFile, "*ptr-=%i;", -array[array_ptr]);
                        array[array_ptr] = 0;
                    }
                    fprintf(outFile, "while(*ptr){");
                    break;
                case ']':
                    if (printf_optimized && printf_printed)
                    {
                        fprintf(outFile, "\"%s);", printf_args);
                        index_pa = 0;
                        memset(printf_args, 0, sizeof(printf_args));
                        printf_printed = false;
                    }
                    if (array_ptr_diff)
                    {
                        if (array_ptr_diff > 1)
                            fprintf(outFile, "ptr+=%i;", array_ptr_diff);
                        else if (array_ptr_diff == 1)
                            fprintf(outFile, "++ptr;");
                        else if (array_ptr_diff == -1)
                            fprintf(outFile, "--ptr;");
                        else
                            fprintf(outFile, "ptr-=%i;", -array_ptr_diff);
                        array_ptr_diff = 0;
                    }
                    if (array[array_ptr])
                    {
                        if (array[array_ptr] > 1)
                            fprintf(outFile, "*ptr+=%i;", array[array_ptr]);
                        else if (array[array_ptr] == 1)
                            fprintf(outFile, "++*ptr;");
                        else if (array[array_ptr] == -1)
                            fprintf(outFile, "--*ptr;");
                        else
                            fprintf(outFile, "*ptr-=%i;", -array[array_ptr]);
                        array[array_ptr] = 0;
                    }
                    fprintf(outFile, "}");
                    break;
                case '@':
                    goto exitwhile3;
                    break;
                default:
                break;
            }
        }
        exitwhile3:
        if (printf_optimized && printf_printed)
            fprintf(outFile, "%s);", printf_args);
    }
    else
    {
        // Print the C code onto the file

        fprintf(outFile, "#include <stdio.h>\nint main(){");
        fprintf(outFile, "char array[");
        fprintf(outFile, "%li", MEMORY_SIZE);
        fprintf(outFile, "]={0},*ptr=array;");

        while (*++buf_ptr)
        {
            switch (*buf_ptr)
            {
                // Handling BF statements
                case '>':
                    if (printf_optimized && printf_printed)
                    {
                        fprintf(outFile, "\"%s);", printf_args);
                        index_pa = 0;
                        memset(printf_args, 0, sizeof(printf_args));
                        printf_printed = false;
                    }
                    fprintf(outFile, "++ptr;");
                    break;
                case '<':
                    if (printf_optimized && printf_printed)
                    {
                        fprintf(outFile, "\"%s);", printf_args);
                        index_pa = 0;
                        memset(printf_args, 0, sizeof(printf_args));
                        printf_printed = false;
                    }
                    fprintf(outFile, "--ptr;");
                    break;
                case '+':
                    if (printf_optimized && printf_printed)
                    {
                        fprintf(outFile, "\"%s);", printf_args);
                        index_pa = 0;
                        memset(printf_args, 0, sizeof(printf_args));
                        printf_printed = false;
                    }
                    fprintf(outFile, "++*ptr;");
                    break;
                case '-':
                    if (printf_optimized && printf_printed)
                    {
                        fprintf(outFile, "\"%s);", printf_args);
                        index_pa = 0;
                        memset(printf_args, 0, sizeof(printf_args));
                        printf_printed = false;
                    }
                    fprintf(outFile, "--*ptr;");
                    break;
                case '.':
                    if (printf_optimized)
                    {
                        if (!printf_printed)
                        {
                            fprintf(outFile, "printf(\"");
                            printf_printed = true;
                        }
                        fprintf(outFile, "%%c");
                        snprintf(printf_args + index_pa, sizeof(printf_args) - index_pa, ",*ptr");
                        index_pa += 5;
                    }
                    else
                        fprintf(outFile, "putchar(*ptr);");
                    break;
                case ',':
                    if (printf_optimized && printf_printed)
                    {
                        fprintf(outFile, "\"%s);", printf_args);
                        index_pa = 0;
                        memset(printf_args, 0, sizeof(printf_args));
                        printf_printed = false;
                    }
                    fprintf(outFile, "*ptr = getchar();");
                    break;      
                case '[':
                    if (printf_optimized && printf_printed)
                    {
                        fprintf(outFile, "\"%s);", printf_args);
                        index_pa = 0;
                        memset(printf_args, 0, sizeof(printf_args));
                        printf_printed = false;
                    }
                    fprintf(outFile, "while(*ptr){");
                    break;
                case ']':
                    if (printf_optimized && printf_printed)
                    {
                        fprintf(outFile, "\"%s);", printf_args);
                        index_pa = 0;
                        memset(printf_args, 0, sizeof(printf_args));
                        printf_printed = false;
                    }
                    fprintf(outFile, "}");
                    break;
                case '@':
                    goto exitwhile2;
                    break;
                default:
                break;
            }
        }    
        exitwhile2:
        if (printf_optimized && printf_printed)
            fprintf(outFile, "\"%s);", printf_args);
    }


    // Closing bracket for 'int main()' function
    fprintf(outFile, "return 0;}");

    fclose(outFile);

    // Compile the file using GCC, which is hopefully on the %PATH%.

    char compile_command[550];
    if (c_optimized[0])
        snprintf(compile_command, sizeof(compile_command), "gcc -std=c23 -O%s %s -o %s", c_optimized, c_output_filename, output_filename);
    else
        snprintf(compile_command, sizeof(compile_command), "gcc -std=c23 %s -o %s", c_output_filename, output_filename);
        
    printf("%s\n%s\n%s\n%s", c_output_filename, output_filename, compile_command, c_optimized);

    const int compile_result = system(compile_command);

    if (compile_result) {
        printf("\nError compiling C code with GCC: %i", compile_result);
        return 1;
    }

    printf("\nGCC compilation successful. Output executable: %s\n", output_filename);

    // Delete the generated C file
    /*if (unlink(c_output_filename) == -1) {
        perror("Error deleting C file");
        return 1;
    }*/

    return 0;
}