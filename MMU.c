#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <inttypes.h>

#define FLAG_HX 0x1     //1
#define MAP_HX   0x7     //111
#define DESLOC_HX  0xfff   //111111111111
#define NPAG 16

struct pagina_t{
    uint16_t flag:1;
    uint16_t mapa:3;
    uint16_t desloc:12;
};
typedef struct pagina_t pagina_t;

//struct

pagina_t *get_pagina_virtual(pagina_t memo_virtual[], uint16_t end_virtual){
    pagina_t end;
    end.desloc = (end_virtual & DESLOC_HX);
    end.mapa = (end_virtual & (MAP_HX << 12)) >> 12;
    end.flag = (end_virtual & (FLAG_HX << 15)) >> 15;

    if(memo_virtual[end.mapa].flag) return &memo_virtual[end.mapa];

    return NULL;
}

int get_pagina_quadro(pagina_t memo_virtual[], uint16_t end_virtual){
    uint16_t quadro = -1;
    pagina_t *pag = get_pagina_virtual(memo_virtual, end_virtual);

    if(pag){
        quadro = 0;
        quadro = quadro | (pag->mapa << 12);
        quadro = quadro | pag->desloc;
    }
    return quadro;
}

pagina_t mv[NPAG];

int main(int argc, char **argv){
    memset(mv, 0, NPAG * sizeof(pagina_t));
    mv[2].flag = 1;
    mv[2].mapa = 6;
    mv[2].desloc = 4;

    int res = get_pagina_quadro(mv, 8196);

    printf("%d", res);
}

