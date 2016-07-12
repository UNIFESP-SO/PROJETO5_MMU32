#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <inttypes.h>
#include <time.h>

#define TRUE 1
#define FALSE 0
#define F 0
#define V 1
#define MIN(a, b) a<b?a:b

int prob(float x){
	float p;
	p = ((float)(rand()%101))/100.0;
	if (p <= x) return V;
	return F;
}

struct virtual_page_t{ // Pagina virtual
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
#define LEN_ADR VTAB_LEN/3

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

void zera_vmem(vtab_t vm[][VTAB_LEN]) {
	memset(vm, 0, VTAB_LEN * VTAB_LEN * sizeof(vtab_t));
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

#define FRAME_NUM 524288

void zera_agetab(age_t at[]) {
	memset(at, 0, FRAME_NUM * sizeof(age_t));
}

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

age_t vet_envelhecimento[FRAME_NUM];
vtab_t virtual_mem[VTAB_LEN][VTAB_LEN];
int frames = 0;

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

void remove_page(vtab_t vt[][VTAB_LEN], int l1, int l2, age_t at[]) {
	memset(&at[vt[l1][l2].frame], 0, sizeof(age_t));
	memset(&vt[l1][l2], 0, sizeof(vtab_t));
}
/////////////////////* PARTE DE PROCESSOS /* ///////////////////////// */

// estados
#define EXECUTANDO 0
#define PRONTO     1
#define BLOQUEADO  2

// probabilidades assumidas
#define PROB_IO_DISPONIVEL  0.9
#define PROB_PREEMP         0.3
#define LEN_MEMO_PROC       8
int imprime_header = F;  // usada apenas na funcao imprime_processo
int total_tempo_cpu = 0; // marcador de tempo discreto, acumula tempos de execucao dos processos.

struct processo {
	unsigned short pid;          // identificador unico do processo
	unsigned int   prio;         // prioridade do processo
	int            quantum;      // fatia de tempo de execucao dadao ao processo para ser executado na cpu
	int            ttotal_exec;  // tempo total de execucao do processo
	unsigned short estado;       // estado do processo
	float          cpu_bound;    // probabilidade de o processo fazer uso apenas de cpu
	float          io_bound;     // probabilidade de o processo fazer IO durante seu usa da cpu

	int            texec;           // recebe um tempo aleatorio de execucao na cpu (texec <= quantum)
	int            total_preemp;    // acumula qtd preempcoes sofridas
	int            total_uso_cpu;   // acumula qtd uso da cpu por todo quntum
	int            total_io;        // acumula qtd numero de interrupcoes por IO
	int            total_tempo_ret; // marca tempo de retorno do processo no momento da execucao do processo: tempo corrente da cpu acrescido do tempo de ingresso na fila
	int            tingresso;       // marca tempo que ingressou na fila

	uint32_t       lvaddr[LEN_ADR];
};
typedef struct processo processo_t;

struct no_t {
	processo_t proc;
	struct no_t *prox;
};
typedef struct no_t no_t;

struct fila_t{
	no_t *inicio;
	no_t *fim;
	int qtd_proc;
};
typedef struct fila_t fila_t;

// criacao fila
void cria_fila(fila_t *f) {
	f->inicio = NULL;
	f->fim = NULL;
	f->qtd_proc = 0;
}

// cria no
no_t *cria_no(processo_t proc) {
	no_t *no = (no_t *)calloc(1, sizeof(no_t));
	if (!no) return NULL;
	no->proc = proc;
	no->prox = NULL;
	return no;
}

// insercao ordenada
int insere_fila_prio(fila_t *f, processo_t proc){
	no_t *no = cria_no(proc);
	if (!no) return F;
	if (f->inicio == NULL) { // fila vazia
		f->inicio = no;
		f->fim = no;
		return V;
	}
	if (no->proc.prio > f->inicio->proc.prio) {
		no->prox = f->inicio;
		f->inicio = no;
		return V;
	}
	if (no->proc.prio <= f->fim->proc.prio) {
		f->fim->prox = no;
		f->fim = no;
		return V;
	}
	no_t *atual, *ant;
	atual = f->inicio;
	ant = NULL;
	while(no->proc.prio <= atual->proc.prio) {
		ant = atual;
		atual = atual->prox;
	}
	ant->prox = no;
	no->prox = atual;
	return V;
}

int fila_vazia(fila_t *f) {
	if (f->inicio == NULL) return V;
	return F;
}

// retira_fila()
int retira_fila(fila_t *f, processo_t *proc) {
	if (fila_vazia(f)) return F;
	no_t *no = f->inicio;
	*proc = no->proc;
	f->inicio = f->inicio->prox;
	if (f->inicio == NULL) f->fim = NULL;
	free(no);
	return V;
}

void imprime_processo(processo_t proc) {
	if (!imprime_header) {
		printf("pid prio estado cpu_bound io_bound ttotal_exec quantum texec total_preemp total_uso_cpu total_io total_tempo_ret tingresso\n");
		imprime_header = V;
	}
	printf("%d %d %d %.2f %.2f %d %d %d %d %d %d %d %d\n",
		proc.pid,
		proc.prio,
		proc.estado,
		proc.cpu_bound,
		proc.io_bound,
		proc.ttotal_exec,
		proc.quantum,
		proc.texec,
        	proc.total_preemp,
        	proc.total_uso_cpu,
        	proc.total_io,
		proc.total_tempo_ret,
		proc.tingresso );
}


// roleta ... para gerar um evento, dada uma probabilidade x.

// obtem tempo aleatorio <= x
int pega_tempo (int x) {
	return(rand()%(x+1));
}

// subtrai a-b, menor valor 0
int sub(int a, int b) {
	int r = a - b;
	if (r < 0) return 0;
	return r;
}

// AO EXECUTAR PROCESSO ELE INICIA ACESSO AOS ENDEREÇOS QUE FORAM INICIALIZADOS COMO FAZENDO PARTE DESSE PROCESSO
void acessando_enderecos(processo_t proc){
    int i = 0;
    uint32_t *faddr[LEN_ADR];
    // va È USADO PARA ACESSAR O ENDEREÇO VIRTUAL REFERENTE AO ENDEREÇO LINEAR faddr[i]
    vaddr_t va = get_virtual_addr(proc.lvaddr[i]);

    // PARA CADA ENDEREÇO CONTIDO NESSE PROCESSO ELE VERIFICA SE O ENDEREÇO JA ESTA MAPEADO E TRATA CADA SITUAÇÂO
    for(i = 0; i < LEN_ADR; i++) {
        if(get_frame_addr(proc.lvaddr[i], faddr[i], virtual_mem) == R_TRAP){
            // TRATAR FALTA DE PAGINA

            // REMOVE PAGINA DA MEMORIA SEGUNDO AGING

            // INSERE PAGINA Q LANÇOU TRAP
            printf("Tratar falta de pagina para o endereco %d\n", proc.lvaddr[i]);
        }
        else{
            // ACESSAR ENDEREÇO FISICO OBTIDO em faddr[LEN_ADR]
            printf("Acessando endereço fisico %d\n", *faddr[i]);
            // COMO O ENDEREÇO ESTA SENDO ACESSADO ELE É REFERENCIADO
            virtual_mem[va.pt1][va.pt2].ref = TRUE;

            // PROBABILIDADE DE ESSE ENDEREÇO SER MODIFICADO
            if (prob(0.2)) virtual_mem[va.pt1][va.pt2].modif = TRUE; // modificada
			else virtual_mem[va.pt1][va.pt2].modif = FALSE;

            printf("PROC %d -> ACESSANDO %d\n", proc.pid, (*faddr[i]));
        }
        if(!(i % 5)){    // A CADA 5 ENDEREÇOS ACESSADOS, ENVELHECE VMEM
            //printf("Envelhecendo\n");
            aging(vet_envelhecimento, virtual_mem);
        }
    }
}

void executa_processo(processo_t *proc) {
	// preempcao
	if (prob(PROB_PREEMP)) {
		proc->texec = pega_tempo(proc->quantum);
		proc->estado = PRONTO;
		proc->total_preemp++;
	}
	else {
		if (prob(proc->cpu_bound)) { // executa cpu
			proc->texec = proc->quantum;
			proc->estado = PRONTO;
			proc->total_uso_cpu++;
		}
		else { // faz IO
			proc->texec = pega_tempo(proc->quantum);
			proc->estado = BLOQUEADO;
			proc->total_io++;
		}
	}
	total_tempo_cpu += MIN(proc->ttotal_exec, proc->texec);     // atualiza tempo da cpu
	proc->ttotal_exec = sub(proc->ttotal_exec, proc->texec);    // atuzliza tempo do processo
	proc->total_tempo_ret = total_tempo_cpu + proc->tingresso;  // marca tempo de retorno corrente do processo
	//imprime_processo(*proc);

	acessando_enderecos(*proc);                                 //Acessa endereços de memoria desse process
}

void escalonador(fila_t *f) {
	processo_t proc;
	while(!fila_vazia(f)) {
		bzero(&proc, sizeof(processo_t));
		retira_fila(f, &proc);
		switch (proc.estado) {
			case PRONTO:
				proc.estado = EXECUTANDO;
				executa_processo(&proc);
				if (proc.ttotal_exec > 0)
					insere_fila_prio(f, proc);
				break;
			case BLOQUEADO:
				if (prob(PROB_IO_DISPONIVEL)) {
					proc.estado = EXECUTANDO;
                                	executa_processo(&proc);
	                                if (proc.ttotal_exec > 0)
						insere_fila_prio(f, proc);
				}
				else {
					 insere_fila_prio(f, proc);
				}
				break;
		}
	}
}


float get_quantum(unsigned int prio) {
	int q;
	switch(prio) {
		case 4:
			q = 100;
			break;
		case 3:
			q = 75;
			break;
		case 2:
			q = 50;
			break;
		case 1:
			q = 25;
			break;
		default:
			q = 10;
	}
	return q;
}


void inicializa_enderecos(processo_t *proc){
    int i = 0;
    vaddr_t va;
    for(i = 0; i < LEN_ADR; i++){
        // Processos são inicializados com endereços fixos de forma aleatoria
        proc->lvaddr[i] = rand()%(VTAB_LEN*VTAB_LEN);
        va = get_virtual_addr(proc->lvaddr[i]);

        // Inicializando Memoria Virtual
        virtual_mem[va.pt1][va.pt2].frame = frames;
        frames++;

        printf("Inicializando endereço %d como parte do processo %d\n", proc->lvaddr[i], proc->pid);
    }
}

processo_t cria_processo(unsigned short pid){
	processo_t proc;
	proc.pid = pid;
	proc.prio = rand()%5;
        proc.quantum = get_quantum(proc.prio);
	proc.ttotal_exec = rand()%10000;
        proc.estado = PRONTO;
        proc.cpu_bound = ((float)(rand()%101))/100.0;
	proc.io_bound = 1 - proc.cpu_bound;
	proc.texec = 0; // recebe tempo de execucao na cpu
        proc.total_preemp = 0; // conta o total de preempcoes sofridas
        proc.total_uso_cpu = 0;
        proc.total_io = 0;
        proc.total_tempo_ret = 0;
	proc.tingresso = total_tempo_cpu;

	inicializa_enderecos(&proc);

	return proc;
}

void imprime_fila(fila_t *f){
	no_t *i;
	for(i=f->inicio; i!=NULL; i=i->prox) {
		imprime_processo(i->proc);
	}
}

void cria_todos_processos(fila_t *f, int np) {
	int i;
	processo_t proc;
	for (i=0; i<np; i++) {
		proc = cria_processo(i);
		imprime_processo(proc);
		insere_fila_prio(f, proc);
	}
}



int main(int argc, char *argv[]){
	if (argc != 2) {
		printf("uso: %s <num_proc>\n", argv[0]);
		return 0;
	}
	int np;
	srand(time(NULL));
	zera_agetab(vet_envelhecimento);
	zera_vmem(virtual_mem);
	fila_t f;
	np = atoi(argv[1]);
	cria_fila(&f);
	cria_todos_processos(&f, np);
	   /*
	         Todos os np processos estao sendo criados no mesmo instante (tingresso = total_tempo_cpu = 0).
	      Portanto, a media do tempo de retorno sera sempre a soma de todos os ttotal_exec dos processos
	      dividida por np.
	        Obs.: para verificar diferentes tempos de retorno, os processos precisam chegar
	      na fila em diferentes momentos (ou seja, o tingresso nao pode ser o mesmo para todos).
	      Nesse caso, um nova funcao cria_todos_processo() deve inserir os processos na fila
              com diferentes tempos de tingresso.
	   */
	escalonador(&f);
	return 0;
}
