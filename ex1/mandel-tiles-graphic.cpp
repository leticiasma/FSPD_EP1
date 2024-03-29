// C implementation for mandelbrot set fractals using libgraph, a simple
// library similar to the old Turbo C graphics library.

//#include <graphics.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#include <string>
#include <queue>
#include <iostream>

#define MAXX 640
#define MAXY 480
#define MAXITER 32768

using namespace std;

FILE* input; // descriptor for the list of tiles (cannot be stdin)
int numThreads = 5; // decide how to choose the color palette

// params for each call to the fractal function
typedef struct {
	int left; int low;  // lower left corner in the screen
	int ires; int jres; // resolution in pixels of the area to compute
	double xmin; double ymin;   // lower left corner in domain (x,y)
	double xmax; double ymax;   // upper right corner in domain (x,y)
} fractal_param_t;

std::queue<fractal_param_t> filaDeFractais;

pthread_mutex_t mutexFilaDeFractais;
pthread_cond_t varCondFilaDeFractais;

pthread_mutex_t mutexPreencheuFilaDeFractais;
pthread_cond_t varCondPreencheuFilaDeFractais;

bool inicializouFilaDeFractais = false; //Acho que isso só é de preocupação das trabalhadoras

/****************************************************************
 * Nesta versao, o programa principal le diretamente da entrada
 * a descricao de cada quadrado que deve ser calculado; no EX1,
 * uma thread produtora deve ler essas descricoes sob demanda, 
 * para manter uma fila de trabalho abastecida para as threads
 * trabalhadoras.
 ****************************************************************/
int input_params(fractal_param_t* p) { 
	int n;
	n = fscanf(input,"%d %d %d %d",&(p->left),&(p->low),&(p->ires),&(p->jres));
	if (n == EOF) return n;

	if (n!=4) {
		perror("fscanf(left,low,ires,jres)");
		exit(-1);
	}
	n = fscanf(input,"%lf %lf %lf %lf",
		 &(p->xmin),&(p->ymin),&(p->xmax),&(p->ymax));
	if (n!=4) {
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
	for (j = 0; j < p->jres; j++) {
		for (i = 0; i <= p->ires; i++) {
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
			for (k=0; (k < MAXITER) && ((u2+v2) < 4); ++k) {
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

bool ehFractalZerado(fractal_param_t* fractal){
    if(fractal->ires == 0 && fractal->jres == 0 && fractal->left == 0 && fractal->low == 0 && fractal->xmax == 0.0 && fractal->xmin == 0.0 && fractal->ymax == 0.0 && fractal->ymin == 0.0){
        return true;
    }

    return false;
}

void* rotinaThreadEntrada(void* indexThread){
    //cout<<"Entrou na rotinaThreadEntrada"<<endl;

    int tamMaxFilaDeFractais = 4*(numThreads-1);
    int numFractaisAAdicionar = tamMaxFilaDeFractais; //Verificar se os zerados não ultrapassam o limite quando colocados

    int numThreadsTrabalhadoras = numThreads - 1;

    bool acabouOArquivo = false;

    fractal_param_t fractal;

    int num = 1;

    //while (input_params(&fractal)!=EOF){ //Supondo que sempre tem ao menos 1
    //while(input_params(&fractal) != EOF){ //Até o final do arquivo
    while(true){

        while (pthread_cond_wait(&varCondPreencheuFilaDeFractais, &mutexPreencheuFilaDeFractais) != 0);

        pthread_mutex_lock(&mutexFilaDeFractais);

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        numFractaisAAdicionar = tamMaxFilaDeFractais - filaDeFractais.size();

        //cout<<"Precisa adicionar "<<numFractaisAAdicionar<<" fractais"<<endl;

        for (int i = 0; i<numFractaisAAdicionar; i++){
            if (input_params(&fractal) == EOF){
                acabouOArquivo = true;
                break;
            }
            filaDeFractais.push(fractal);
            cout<<"Adicionou o fractal "<<fractal.ires<<" "<<fractal.jres<<" "<<fractal.left<<" "<<fractal.low<<" "<<fractal.xmax<<" "<<fractal.xmin<<" "<<fractal.ymax<<" "<<fractal.ymin<<" na fila"<<endl;
        }

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        if (acabouOArquivo){
            cout<<"Acabou o arquivo"<<endl;

            fractal.ires = 0;
            fractal.jres = 0;
            fractal.left = 0;
            fractal.low = 0;
            fractal.xmax = 0.0;
            fractal.xmin = 0.0;
            fractal.ymax = 0.0;
            fractal.ymin = 0.0;

            //pthread_mutex_lock(&mutexFilaDeFractais);
            //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

            for (int i=0; i<numThreadsTrabalhadoras; i++){
                filaDeFractais.push(fractal);
            }

            //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
            pthread_mutex_unlock(&mutexFilaDeFractais);
            cout<<"Vai dar break no while"<<endl;
            break; //Talvez matar a leitora depois
        }

        pthread_mutex_unlock(&mutexFilaDeFractais);

        //Não sei se estou usando a var e o mutex certos ou se tem que criar outros 
        //while (pthread_cond_wait(&varCondFilaDeFractais, &mutexFilaDeFractais) != 0); //Quando acaba o arquivo, de fato não dorme de novo e coloca os 4 zerados, será? Pois mudei o while para loop infinito.
        cout<<"Thread de Entrada dormindo "<<num<<endl;
        num++;


        // if (!inicializouFilaDeFractais){
        //     pthread_cond_broadcast(&varCondInicializouFilaDeFractais);                

        //     inicializouFilaDeFractais = true;
        // }

        // if (filaDeFractais.size() == tamMaxFilaDeFractais){ //Acho que não precisa desse if, pois quando ela é acordada ela simplesmente preenche e pronto
        //     //pthread_cond_wait(&varCondFilaDeFractais, &mutexFilaDeFractais);
        // }
        
    }
    //Destruir a thread leitora ou dar um join na main

}

void* rotinaThreadTrabalhadora(void* indexThread){
    //cout<<"Entrou na rotinaThreadTrabalhadora"<<endl;
    int numThreadsTrabalhadoras = numThreads - 1;

    // if(!inicializouFilaDeFractais){ //Acho que pode travar se ele ver o valor desatualizado
    //     while (pthread_cond_wait(&varCondInicializouFilaDeFractais, &mutexInicializouFilaDeFractais) != 0);
    // }
    // else{

        //while (pthread_cond_wait(&varCondPreencheuFilaDeFractais, &mutexPreencheuFilaDeFractais) != 0);

        fractal_param_t f;

        while(true){

            pthread_mutex_lock(&mutexFilaDeFractais);
            //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

                f = filaDeFractais.front(); //Acho que é assim
                filaDeFractais.pop();

                if (filaDeFractais.size() == numThreadsTrabalhadoras){
                    //Acordar a thread de entrada
                    pthread_cond_signal(&varCondPreencheuFilaDeFractais);
                }

            //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
            pthread_mutex_unlock(&mutexFilaDeFractais);

            if (ehFractalZerado(&f)){
                break;
            }

            fractal(&f);

        }
        
    //}




    // std::cout<<"Vou dormir"<<std::endl;
    // if (inicializouFilaDeFractais == false){
    //     while (pthread_cond_wait(&varCondInicializouFilaDeFractais, &mutexInicializouFilaDeFractais) != 0);
    // }
    // std::cout<<"Acordei"<<std::endl;
}

void* func2(void* rank){
	//Verifica se está liberado buscar na fila
	//Tira um fractal da fila
	//Se fila de fractais .size < (numThreads-1)
	//acorda a thread 0

	//enquanto fractal não é o EOW
	//marcar inicio tempo
	//chama fractal
	//marca final tempo
	//preencher o array de tempo das threads
	//consegue um fractal para trabalhar com

}

/****************************************************************
 * Na versao paralela com 
 * pthreads, como indicado no enunciado, devem ser criados tres
 * tipos de threads: uma thread de entrada que le a descricao dos
 * blocos e alimenta uma fila para os trabalhadores, diversas
 * threads trabalhadoras que retiram um descritor de bloco da fila
 * e fazem o calculo da imagem e depositam um registro com os
 * dados sobre o processamento do bloco, como descrito no enunciado,
 * e uma thread final que recebe os registros sobre o processamento
 * e computa as estatisticas do sistema (que serao a unica saida
 * visivel do seu programa na versao que segue o enunciado.
 ****************************************************************/
int main (int argc, char* argv[]){

    if (argc > 1){

        //---Não sei para que serve---
        if ((argc!=2)&&(argc!=3)){
            fprintf(stderr,"usage %s filename [numThreads]\n", argv[0]);
            exit(-1);
        } 

        //Caso o parâmetro adicional "número de threads trabalhadoras" for passado
        if (argc==3) {
            numThreads = std::stoi(argv[2]) + 1; //Trabalhadoras + Thread de Entrada
        }
        pthread_t threads[numThreads];

        //---Não sei para que serve---
        if ((input=fopen(argv[1],"r"))==NULL) {
            perror("fdopen");
            exit(-1);
        }

        pthread_mutex_init(&mutexFilaDeFractais, NULL);
        pthread_mutex_init(&mutexPreencheuFilaDeFractais, NULL);

        pthread_cond_init(&varCondFilaDeFractais, NULL);
        pthread_cond_init(&varCondPreencheuFilaDeFractais, NULL);

        for(long indexThread = 0; indexThread<numThreads; indexThread++){
            if(indexThread == 0){
                pthread_create(&threads[indexThread], NULL, rotinaThreadEntrada, (void*) indexThread);
                cout<<"Criou thread de entrada"<<endl;
            }
            else{
                pthread_create(&threads[indexThread], NULL, rotinaThreadTrabalhadora, (void*) indexThread);
                cout<<"Criou thread trabalhadora"<<endl;
            }
        }

        for(long indexThread = 0; indexThread<numThreads; indexThread++){
            pthread_join(threads[indexThread], NULL);
            cout<<"Fez join"<<endl;
        }

        //Destruir todas? Onde?

    }
    else{
        cout<<"ERRO: Nenhum arquivo de fractais passado."<<endl;
    }

    pthread_mutex_destroy(&mutexFilaDeFractais);
    pthread_mutex_destroy(&mutexPreencheuFilaDeFractais);

    pthread_cond_destroy(&varCondFilaDeFractais);
    pthread_cond_destroy(&varCondPreencheuFilaDeFractais);

	return 0;

}

