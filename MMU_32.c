#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <inttypes.h>

struct virtual_page_t{
    uint32_t cache:1;
    uint32_t ref:1;
    uint32_t modif:1;
    uint32_t prote:3;
    uint32_t exist:1;
    uint32_t frame:19;
    uint32_t pad:6;
};
typedef struct virtual_page_t vtab_t;

#define VTAB_LEN 1024
vtab_t virtual_mem[VTAB_LEN][VTAB_LEN];

struct vaddr{
    uint32_t pt1:10;
    uint32_t pt2:10;
    uint32_t desloc:12;
};
typedef struct vaddr vaddr_t;

#define PT1 0xFFC00000 // 4290772992 // 1111111111 0000000000 000000000000
#define PT2 0x3FF000   // 4190208    // 0000000000 1111111111 000000000000
#define DESLOC 0xFFF   // 4095       //                       111111111111

vaddr_t get_virtual_addr(uint32_t lvaddr){
    vaddr_t va;
    va.pt1 = (PT1 & lvaddr) >> 22;
    va.pt2 = (PT2 & lvaddr) >> 12;
    va.desloc = (DESLOC & lvaddr);
    return (va);
}

#define R_TRAP -1
#define R_OK 0

int get_frame_addr(uint32_t lvaddr, uint32_t *faddr, vtab_t virtual_tab[][VTAB_LEN]){
    vaddr_t va = get_virtual_addr(lvaddr);
    if(!virtual_tab[va.pt1][va.pt2].exist) return R_TRAP;

    *faddr = 0;
    *faddr = (*faddr) | ( virtual_tab[va.pt1][va.pt2].frame << 12 );
    *faddr = (*faddr) | va.desloc;
    return R_OK;
}


int main(){

}
/*
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
}*/

