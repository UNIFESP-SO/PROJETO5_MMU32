#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

// espaco virtual de 2^32 = 4GB
// 	endereco virtual de 32 bits: pt1 10 bits, pt2 10 bits, deslocamento de 12 bits

// espaco real de 2^31 = 2GB... metade da virtual
//	endereco real de 31 bits: frame base 19 bits, deslocamentos de 12 bits

struct virtual_page_t {
	uint32_t cache:1;
	uint32_t ref:1;
	uint32_t modif:1;
	uint32_t protec:3;
	uint32_t exist:1;
	uint32_t frame:19; // endereco base da moldura
	uint32_t pad:6;    // preenchimento
};
typedef struct virtual_page_t virtual_page_t;
typedef virtual_page_t vtab_t;

#define VTAB_LEN 1024

// --------------------------
void zera_vmem(virtual_page_t vm[][VTAB_LEN]) {
	memset(vm, 0, VTAB_LEN * VTAB_LEN * sizeof(virtual_page_t));
}

#define PT1 0xFFC00000 // 4290772992 = 1111111111 0000000000 000000000000
#define PT2 0x3FF000   // 4190208    = 0000000000 1111111111 000000000000
#define DESLOC 0xFFF   // 4095       =                       111111111111

struct vaddr_t {
	uint32_t pt1:10;
	uint32_t pt2:10;
	uint32_t desloc:12;
};
typedef struct vaddr_t vaddr_t;

vaddr_t get_virtual_addr(uint32_t lvaddr) { //linear virtual addr
	vaddr_t va;
	va.pt1 = (PT1 & lvaddr) >> 22;
	va.pt2 = (PT2 & lvaddr) >> 12;
	va.desloc = (DESLOC & lvaddr);
	return (va);
}

void print_vaddr(uint32_t lvaddr){
	vaddr_t va = get_virtual_addr(lvaddr);
	printf("addr=%u: pt1=%u, pt2=%u, desloc=%u\n", lvaddr, va.pt1, va.pt2, va.desloc);
}

#define R_TRAP -1
#define R_OK    0

int get_frame_addr(uint32_t lvaddr, uint32_t *faddr, vtab_t virtual_tab[][VTAB_LEN]) {
	vaddr_t va = get_virtual_addr(lvaddr);
	if (!virtual_tab[va.pt1][va.pt2].exist) return R_TRAP;
	*faddr = 0;
	*faddr = (*faddr) | (virtual_tab[va.pt1][va.pt2].frame << 12);
	*faddr = (*faddr) | va.desloc;
	return R_OK;
}

// algoritmo de envelhecimento (aging) para subsitutuicao de paginas

#define FRAME_NUM 524288 //2^19
struct age_t {
	uint8_t age;
	uint8_t alloc;
	uint16_t l1; // tab nivel 1
	uint16_t l2; // tab nivel 2
};
typedef struct age_t age_t;

void zera_agetab(age_t at[]) {
	memset(at, 0, FRAME_NUM * sizeof(age_t));
}

#define TRUE  1
#define FALSE 0
// em algum momento (na interrupcao do relogio ...)
void aging(age_t at[], vtab_t vt[][VTAB_LEN]) {
	int i, j;
	for (i=0; i<VTAB_LEN; i++) {
		for (j=0; j<VTAB_LEN; j++) {
			if (vt[i][j].exist) {
				at[vt[i][j].frame].alloc = TRUE;
				at[vt[i][j].frame].age = at[vt[i][j].frame].age >> 1;
				at[vt[i][j].frame].age = at[vt[i][j].frame].age | (vt[i][j].ref << 7);
			}
		}
	}
}

int get_frame_NUF (age_t at[]) { // algoritmo de substituicao: NUF/Aging nao utilizado frequentemente
	int i, imin;
	int min = 256; //maior valor possivel para um byte + 1
	for (i=0; i<FRAME_NUM; i++) {
		if (!at[i].alloc) return i;
		if (at[i].alloc) {
			if (at[i].age < min) {
				min = at[i].age;
				imin = i;
			}
		}
	}
	return imin;
}

// -------------------------------------------

int prob(float x) {
	float v = (rand()%101)/100.0;
	if (v < x) return TRUE;
	return FALSE;
}

void random_fill_vt_at(vtab_t vt[][VTAB_LEN], age_t at[]) {
	int i, j, frames = 0;
	do {
		i = rand()%1024;
		j = rand()%1024;
		//if (!vt[i][j].exist) {
			vt[i][j].exist = TRUE;
			if (prob(1.0)) vt[i][j].ref = TRUE; // referenciada
			else vt[i][j].ref = FALSE;
			if (prob(0.5)) vt[i][j].modif = TRUE; // modificada
			else vt[i][j].modif = FALSE;
			vt[i][j].frame = frames;
			at[frames].alloc = TRUE;
			at[frames].age = 0;
			at[frames].l1 = i;
			at[frames].l2 = j;
			frames++;
		//}
	} while (frames < FRAME_NUM);
}

void remove_page(vtab_t vt[][VTAB_LEN], int l1, int l2, age_t at[]) {
	memset(&at[vt[l1][l2].frame], 0, sizeof(age_t));
	memset(&vt[l1][l2], 0, sizeof(vtab_t));
}

int main(int argc, char **argv) {
	//uint32_t addr = atoi(argv[1]);
	//print_vaddr(addr);
	int sub;
	virtual_page_t virtual_mem[VTAB_LEN][VTAB_LEN];
	age_t age_tab[FRAME_NUM];
	zera_vmem(virtual_mem);
	zera_agetab(age_tab);
	while (1) {
		random_fill_vt_at(virtual_mem, age_tab);
		aging(age_tab, virtual_mem);
		sub = get_frame_NUF(age_tab);
		printf("pt1:%u, pt2:%u, frame:%u, age:%d \n", age_tab[sub].l1, age_tab[sub].l2, sub, age_tab[sub].age);
		remove_page(virtual_mem, age_tab[sub].l1, age_tab[sub].l2, age_tab);
	}

	return 0;
}
