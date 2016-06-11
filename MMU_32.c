#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <inttypes.h>

#define TRUE 1
#define FALSE 0

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
#define R_OK    0

int get_frame_addr(uint32_t lvaddr, uint32_t *faddr, vtab_t virtual_tab[][VTAB_LEN]){
    vaddr_t va = get_virtual_addr(lvaddr);
    if(!virtual_tab[va.pt1][va.pt2].exist) return R_TRAP;

    *faddr = 0;
    *faddr = (*faddr) | ( virtual_tab[va.pt1][va.pt2].frame << 12 );
    *faddr = (*faddr) | va.desloc;
    return R_OK;
}

struct age_t {
    uint8_t age;
    uint8_t alloc;
    uint16_t l1;    // tab nivel 1
    uint16_t l2;    // tab nivel 2
};
typedef struct age_t age_t;

void aging(age_t at[], vtab_t vt[][VTAB_LEN]){
    int i, j;
    for(i = 0; i < VTAB_LEN; i++){
        for(j = 0; j < VTAB_LEN; j++){
            if(vt[i][j].exist) {
                int k = vt[i][j].frame;
                at[k].alloc = TRUE;
                at[k].age = at[k].age >> 1;
                at[k].age = at[k].age | (vt[i][j].ref << 7);
            }
        }
    }
}

#define FRAME_NUM 524288    //  2^19
// NUR -> NOT UTILIZED RECENTLY
int get_frame_NUR(age_t at[]){
    int i, imin = -1;
    int min = 256;

    for(i = 0; i < FRAME_NUM; i++){
        if( !at[i].alloc) return i;
        else{
            if(at[i].age < min){
                min = at[i].age;
                imin = i;
            }
        }
    }
    return imin;
}

int main(){

}


