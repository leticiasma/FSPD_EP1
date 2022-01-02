#include <pthread.h>
#include <queue>
#include <iostream>

#define MAXITER 32768

using namespace std;

// params for each call to the fractal function
typedef struct {
	int left; int low;  // lower left corner in the screen
	int ires; int jres; // resolution in pixels of the area to compute
	double xmin; double ymin;   // lower left corner in domain (x,y)
	double xmax; double ymax;   // upper right corner in domain (x,y)
} fractal_param_t;


//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
//DECLARAÇÕES GLOBAIS
//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
FILE* input; // descriptor for the list of tiles (cannot be stdin)

unsigned int numThreads = 5; // decide how to choose the color palette //Número total de threads existentes (tabalhadoras + thread mestre)
unsigned int tamMaxFilaFractais; //A fila de fractais terá tamanho de, no máximo, 4 vezes o número de threads trabalhadoras 
unsigned int numThreadsTrabalhadoras;

std::queue<fractal_param_t> filaFractais;

bool encontradoEOW = false; //Não sei se precisa

//----------------------------------------------
//Mutex e variáveis de condição
pthread_mutex_t mutexFilaDeFractais;

pthread_mutex_t mutexFilaDeFractaisPreenchida;
pthread_cond_t varCondFilaDeFractaisPreenchida;

pthread_mutex_t mutexPreencherFilaDeFractais;
pthread_cond_t varCondPreencherFilaDeFractais;


//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
//FUNÇÕES DISPONIBILIZADAS NO CÓDIGO BASE
//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
/****************************************************************
 * Nesta versao, o programa principal le diretamente da entrada
 * a descricao de cada quadrado que deve ser calculado; no EX1,
 * uma thread produtora deve ler essas descricoes sob demanda, 
 * para manter uma fila de trabalho abastecida para as threads
 * trabalhadoras.
 ****************************************************************/
int input_params(fractal_param_t* p){ 
	int n;
	n = fscanf(input,"%d %d %d %d",&(p->left),&(p->low),&(p->ires),&(p->jres));
	if (n == EOF) return n;

	if (n!=4){
		perror("fscanf(left,low,ires,jres)");
		exit(-1);
	}
	n = fscanf(input,"%lf %lf %lf %lf",
		 &(p->xmin),&(p->ymin),&(p->xmax),&(p->ymax));
	if (n!=4){
		perror("scanf(xmin,ymin,xmax,ymax)");
		exit(-1);
	}
	return 8;
}

/****************************************************************
 * A funcao a seguir faz a geracao dos blocos de imagem dentro
 * da area que vai ser trabalhada pelo programa. Para a versao
 * paralela, nao importa quais as dimensoes totais da area, basta
 * manter um controle de quais blocos estao sendo processados
 * a cada momento, para manter as restricoes desritas no enunciado.
 ****************************************************************/
// Function to draw mandelbrot set
void fractal(fractal_param_t* p){
	double dx, dy;
	int i, j, k;
	double x, y, u, v, u2, v2;

	dx = (p->xmax - p->xmin) / p->ires;
	dy = (p->ymax - p->ymin) / p->jres;
	
	// scanning every point in that rectangular area.
	// Each point represents a Complex number (x + yi).
	// Iterate that complex number
	for (j = 0; j < p->jres; j++){
		for (i = 0; i <= p->ires; i++){
			x = i * dx + p->xmin; // c_real
			u = u2 = 0; // z_real
			y = j * dy + p->ymin; // c_imaginary
			v = v2 = 0; // z_imaginary

			// Calculate whether c(c_real + c_imaginary) belongs
			// to the Mandelbrot set or not and draw a pixel
			// at coordinates (i, j) accordingly
			// If you reach the Maximum number of iterations
			// and If the distance from the origin is
			// greater than 2 exit the loop
			for (k=0; (k < MAXITER) && ((u2+v2) < 4); ++k){
				// Calculate Mandelbrot function
				// z = z*z + c where z is a complex number

				// imag = 2*z_real*z_imaginary + c_imaginary
				v = 2 * u * v + y;
				// real = z_real^2 - z_imaginary^2 + c_real
				u  = u2 - v2 + x;
				u2 = u * u;
				v2 = v * v;
			}
		}
	}
}


//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
//FUNÇÕES AUXILIARES
//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

//Verifica se uma thread trabalhadora retirou da fila um fractal com todos os valores zerados, isto é, um registro de fim de tarefas (EOW)
bool encontrouEOW(fractal_param_t* fractal){
    if(fractal->ires == 0 && fractal->jres == 0 && fractal->left == 0 && fractal->low == 0 && fractal->xmax == 0.0 && fractal->xmin == 0.0 && fractal->ymax == 0.0 && fractal->ymin == 0.0){
        return true;
    }

    return false;
}

//Adiciona uma quantidade de fractais especiais (com valores todos zerados) igual ao número de threads trabalhadoras ao chegar no final da leitura do arquivo de entrada 
void registrarEOW(){

    fractal_param_t fractal;

    for (unsigned int i=0; i<numThreadsTrabalhadoras; i++){

        fractal.ires = 0;
        fractal.jres = 0;
        fractal.left = 0;
        fractal.low = 0;
        fractal.xmax = 0.0;
        fractal.xmin = 0.0;
        fractal.ymax = 0.0;
        fractal.ymin = 0.0;

        filaFractais.push(fractal);

    }

}

/*Adiciona os fractais de cada linha do arquivo de entrada à fila de fractais a ser consumida pelas threads trabalhadoras.
Essa função é chamada para a thread mestre (responsável por alimentar a fila) sempre que o número de fractais na fila for 
menor (ou igual) ao número de threads trabalhadoras. Em cada momento da execução do programa o número de threads a ser adicionado
respeita o limite estabelecido por tamMaxFilaFractais. Caso a fila esteja sendo preenchida pela primeira vez, o número de
fractais a ser adicionado é igual ao tamMaxFilaFractais.*/
bool preencherFilaFractais(){

    fractal_param_t fractal;

    unsigned int numFractaisAdicionar = tamMaxFilaFractais - filaFractais.size(); //Verificar se os zerados não ultrapassam o limite quando colocados
    bool acabouArquivo = false;

    /*No momento de adição de fractais, a fila está sob uso exclusivo da thread mestre, isto é, todos os fractais 
    necessários para preencher por completo a fila são adicionados fazendo uso do mutex mutexPreencherFilaDeFractais.
    Ou seja, só depois que todos a fila for preenchida é dado um unlock no mutex para que as threads trabalhadoras
    possam continuar a consumir fractais.*/
    for (unsigned int i = 0; i<numFractaisAdicionar; i++){
        if (input_params(&fractal) == EOF){
            acabouArquivo = true;
            break;
        }
        filaFractais.push(fractal);
    }

    if (acabouArquivo){
        registrarEOW();
    }   

    return acabouArquivo; //Talvez matar a leitora depois

}


//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
//ROTINAS DOS 2 TIPOS DE THREADS EXISTENTES
//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
void* rotinaThreadMestre(void* indexThread){

    bool acabouArquivo = false;

    while(true){

        pthread_mutex_lock(&mutexPreencherFilaDeFractais);

        while (pthread_cond_wait(&varCondPreencherFilaDeFractais, &mutexPreencherFilaDeFractais) != 0);

        acabouArquivo = preencherFilaFractais();

        pthread_mutex_unlock(&mutexPreencherFilaDeFractais);
        pthread_cond_broadcast(&varCondFilaDeFractaisPreenchida);

        if (acabouArquivo){
            break;
        }          
        
    }

}

void* rotinaThreadTrabalhadora(void* indexThread){

    fractal_param_t f;

    while(true){

        pthread_mutex_lock(&mutexFilaDeFractais);

        if (filaFractais.size() < numThreadsTrabalhadoras && !encontradoEOW){ //Ou igual
            pthread_cond_signal(&varCondPreencherFilaDeFractais); //Acordar a thread mestre
            while (pthread_cond_wait(&varCondFilaDeFractaisPreenchida, &mutexFilaDeFractaisPreenchida) != 0);
        }

        f = filaFractais.front();
        filaFractais.pop();

        if (encontrouEOW(&f)){
            encontradoEOW = true;
            pthread_mutex_unlock(&mutexFilaDeFractais);
            break;
        }

        pthread_mutex_unlock(&mutexFilaDeFractais);

        fractal(&f);

    }

}


//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
//FUNÇÃO MAIN
//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*
int main (int argc, char* argv[]){

    if ((argc!=2)&&(argc!=3)){
        fprintf(stderr,"usage %s filename [numThreads]\n", argv[0]);
        exit(-1);
    } 

    //Caso o parâmetro adicional "número de threads trabalhadoras" for passado
    if (argc==3) {
        numThreads = std::stoi(argv[2]) + 1; //número de threads trabalhadoras + 1 thread mestre
    }
    pthread_t threads[numThreads];
    numThreadsTrabalhadoras = numThreads - 1;
    tamMaxFilaFractais = 4*numThreadsTrabalhadoras;

    if ((input=fopen(argv[1],"r"))==NULL){
        perror("fdopen");
        exit(-1);
    }

    pthread_mutex_init(&mutexFilaDeFractais, NULL);
    pthread_mutex_init(&mutexPreencherFilaDeFractais, NULL);
    pthread_mutex_init(&mutexFilaDeFractaisPreenchida, NULL);

    pthread_cond_init(&varCondPreencherFilaDeFractais, NULL);
    pthread_cond_init(&varCondFilaDeFractaisPreenchida, NULL);

    for(long indexThread = 0; indexThread<numThreads; indexThread++){
        if(indexThread == 0){
            pthread_create(&threads[indexThread], NULL, rotinaThreadMestre, (void*) indexThread);
        }
        else{
            pthread_create(&threads[indexThread], NULL, rotinaThreadTrabalhadora, (void*) indexThread);
        }
    }

    for(long indexThread = 0; indexThread<numThreads; indexThread++){
        pthread_join(threads[indexThread], NULL);
    }

        //Destruir todas? Onde?
    pthread_mutex_destroy(&mutexFilaDeFractais);
    pthread_mutex_destroy(&mutexPreencherFilaDeFractais);
    pthread_mutex_destroy(&mutexFilaDeFractaisPreenchida);

    pthread_cond_destroy(&varCondPreencherFilaDeFractais);
    pthread_cond_destroy(&varCondFilaDeFractaisPreenchida);

	return 0;

}

