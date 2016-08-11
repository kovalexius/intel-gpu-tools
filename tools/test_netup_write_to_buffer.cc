#include <iostream>
#include <string.h>

extern "C"
{
#include "netup_get_statistics.h"
extern size_t len_buffer;
extern size_t pos_to_write;
extern void write_to_buffer(const char* str, size_t n);
extern void empty_buffer();
}

int main()
{
    // TEST
    std::cout << "Test of write_to_buffer" << std::endl;

    char phrase1[] = "Go home! ";
    write_to_buffer( phrase1, strlen(phrase1));
    char phrase2[] = "Go Job! ";
    write_to_buffer( phrase2, strlen(phrase2));
    std::cout << "out_buffer = " << out_buffer << " pos_to_write = " << pos_to_write << " len_buffer = " << len_buffer << std::endl;
    
    empty_buffer();
    char phrase3[] = "Go home buster! ";
    write_to_buffer( phrase3, strlen(phrase3));
    char phrase4[] = "Go Job buster! ";
    write_to_buffer( phrase4, strlen(phrase4));
    std::cout << "out_buffer = " << out_buffer << " pos_to_write = " << pos_to_write << " len_buffer = " << len_buffer << std::endl;
    
    empty_buffer();
    char phrase5[] = "Get out! ";
    write_to_buffer( phrase5, strlen(phrase5));
    char phrase6[] = "Come here! ";
    write_to_buffer( phrase6, strlen(phrase6));
    std::cout << "out_buffer = " << out_buffer << " pos_to_write = " << pos_to_write << " len_buffer = " << len_buffer << std::endl;
}