// C implementation for mandelbrot set fractals using libgraph, a simple
// library similar to the old Turbo C graphics library.

//#include <graphics.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#define MAXX 640
#define MAXY 480
#define MAXITER 32768

FILE* input; // descriptor for the list of tiles (cannot be stdin)
int   numThreads = 5; // decide how to choose the color palette

// params for each call to the fractal function
typedef struct {
	int left; int low;  // lower left corner in the screen
	int ires; int jres; // resolution in pixels of the area to compute
	double xmin; double ymin;   // lower left corner in domain (x,y)
	double xmax; double ymax;   // upper right corner in domain (x,y)
} fractal_param_t;

/****************************************************************
 * Nesta versao, o programa principal le diretamente da entrada
 * a descricao de cada quadrado que deve ser calculado; no EX1,
 * uma thread produtora deve ler essas descricoes sob demanda, 
 * para manter uma fila de trabalho abastecida para as threads
 * trabalhadoras.
 ****************************************************************/
int input_params( fractal_param_t* p )
{ 
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
void fractal( fractal_param_t* p )
{
	double dx, dy;
	int i, j, k;
	double x, y, u, v, u2, v2;

	dx = ( p->xmax - p->xmin ) / p->ires;
	dy = ( p->ymax - p->ymin ) / p->jres;
	
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

/****************************************************************
 * Essa versao do programa, sequencial le a descricao de um bloco
 * de imagem do conjunto de mandelbrot por vez e faz a geracao
 * e exibicao do mesmo na janela grafica. Na versao paralela com 
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
int main ( int argc, char* argv[] )
{
	int i,j,k;
	fractal_param_t p;

	if ((argc!=2)&&(argc!=3)) {
		fprintf(stderr,"usage %s filename [numThreads]\n", argv[0] );
		exit(-1);
	} 
	if (argc==3) {
		numThreads = atoi(argv[2]);
	} 
	if ((input=fopen(argv[1],"r"))==NULL) {
		perror("fdopen");
		exit(-1);
	}

	while (input_params(&p)!=EOF) {
		fractal(&p); // No exercicio a funcao nao vai exibir nada! :-(
	}

	return 0;
}

