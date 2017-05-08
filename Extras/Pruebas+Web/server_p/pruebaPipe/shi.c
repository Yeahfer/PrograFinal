#include <stdio.h>
#include <string.h>
#include <time.h>

char buffFecha[1000];

void calcularFecha(){
    time_t now = time(0);
    struct tm tm = *gmtime(&now);
    strftime(buffFecha, sizeof buffFecha, "%a, %d %b %Y %H:%M:%S %Z", &tm);
}

int main(void) {
    calcularFecha();
    printf("FECHA sdsd: %s\n",buffFecha);
    return 0;
}