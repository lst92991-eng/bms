#include "Common_Debug.h"

int fputc(int c,FILE* f){

    HAL_UART_Transmit(&huart1,(uint8_t*)&c,1,2000);

    return c;
    
}
