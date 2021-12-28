// C implementation for mandelbrot set fractals using libgraph, a simple
// library similar to the old Turbo C graphics library.
/*****************************************************************
 * ATENCAO: a biblioteca libgraph nao funciona com pthreads, entao
 * a solucao do exercicio nao deve chamar as funcoes graficas 
 * NADA SERA EXIBIDO, INFELIZMENTE. :-(
 *****************************************************************/

#include <graphics.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#define MAXX 640
#define MAXY 480
#define MAXITER 32768

FILE* input; // descriptor for the list of tiles (cannot be stdin)
int   color_pick = 5; // decide how to choose the color palette

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
	int i, j, k, color;
	double x, y, u, v, u2, v2;

    /*************************************************************
     * a funcao rectangle deve ser removida na versao com pthreads,
     * ja que as bibliotecas sao conflitantes.
     *************************************************************/
	rectangle(p->left, p->low, p->left+p->ires-1, p->low+p->jres-1);

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
			if (k==MAXITER) {
				// converging areas are black
				color = 0;
			} else {
				// graphics mode has only 16 colors;
				// choose a range and recycle!
				color = ( k >> color_pick ) %16;
			}
			// To display the created fractal
			/******************************************************
                         * a funcao putpixel deve ser removida na versao com pthreads,
                         * ja que as bibliotecas sao conflitantes. Infelizmente,
                         * isso significa que sua versao final nao tera uma saida
                         * grafica real.
                         ******************************************************/
			//putpixel(p->left+i, p->low+j, color);
		}
	}
}

/****************************************************************
 * as funcoes init_gr e end_gr devem ser removidas na versao
 * com pthreads, ja que a biblioteca libgraph nao e compativel.
 ****************************************************************/
void init_gr(void)
{
    // This starts the graphics mode in its simple form (auto detect)
	int gd = DETECT, gm, errorcode;

	char driver[] = "";
	initgraph(&gd, &gm, driver);

	// getting maximum value of x-axis of screen
	if ((getmaxx()+1) != MAXX) {
		fprintf(stderr,"ERR: input expects maxx==640! (not %d)\n",MAXX);
		exit(-1);
	}
	if ((getmaxy()+1) != MAXY) {
		fprintf(stderr,"ERR: input expects maxy==480 (not %d)!\n",MAXY);
		exit(-1);
	}
}

/****************************************************************
 * as funcoes init_gr e end_gr devem ser removidas na versao
 * com pthreads, ja que a biblioteca libgraph nao e compativel.
 ****************************************************************/
void end_gr(void)
{
    //Wait for a key press before closing the window
    int in = 0;
    while (in == 0) {
        in = getchar();
    }

    // closegraph function closes the graphics mode and deallocates
	// all memory allocated by graphics system
	closegraph();
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
		fprintf(stderr,"usage %s filename [color_pick]\n", argv[0] );
		exit(-1);
	} 
	if (argc==3) {
		color_pick = atoi(argv[2]);
	} 
	if ((input=fopen(argv[1],"r"))==NULL) {
		perror("fdopen");
		exit(-1);
	}

	init_gr();  // Essa biblioteca nao funciona com Pthreads! REMOVA
	while (input_params(&p)!=EOF) {
		fractal(&p); // No exercicio a funcao nao vai exibir nada! :-(
	}
	end_gr();  // Essa biblioteca nao funciona com Pthreads! REMOVA

	return 0;
}

