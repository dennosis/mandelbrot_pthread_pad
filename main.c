/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   main.c
 * Author: Dennis Aguiar
 *
 * Created on 13 de Junho de 2019, 18:50
 */

#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <omp.h>
#include <sys/time.h>
//#define _OPEN_THREADS
//#define _UNIX03_THREADS
#include <pthread.h>
#include <assert.h>
//pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
//parametros de entrada
static const int Image_Width = 5000;
static const int Image_Height = 5000;
static const int Max_Iterations = 1000;
static const int Div_Width = 2;
static const int Div_Height = 2;
int rc;

typedef struct Complex {
    long double real;
    long double imaginary;
} Complex;

typedef unsigned char RGB_Pixel[3];

static const unsigned char MAX_RGB_VAL = 255;

typedef struct Thread_args {
       int x;
       int y;
       int x_end;
       int y_end;
       double min_bounds_real;
       double min_bounds_imaginary;
       double scale_real;
       double scale_imaginary;
       RGB_Pixel *colors;
       RGB_Pixel *pixels;
}Thread_args;



static const Complex Focus_Point = {.real = -0.5, .imaginary = 0};
static const long double Zoom = 2;

void calc_colors(RGB_Pixel *colors) {
    for (int i = 0; i < Max_Iterations; i++) {
        double t = (double) i / Max_Iterations;

        colors[i][0] = (unsigned char) (9 * (1 - t) * t * t * t * MAX_RGB_VAL);
        colors[i][1] = (unsigned char) (15 * (1 - t) * (1 - t) * t * t * MAX_RGB_VAL);
        colors[i][2] = (unsigned char) (8.5 * (1 - t) * (1 - t) * (1 - t) * t * MAX_RGB_VAL);
    }
};


void * func_thread(void *_args){
    struct Thread_args *args = (struct Thread_args *) _args;
    
    
    for (int y = args->y; y < args->y_end &&  y < Image_Height; y++) {
        for (int x = args->x; x < args->x_end && x < Image_Width; x++) {
                
            ///struct Thread_args *args = (struct Thread_args *) _args;

            Complex c = {
                    .real = args->min_bounds_real + x * args->scale_real,
                    .imaginary = args->min_bounds_imaginary + y * args->scale_imaginary
            };

            Complex z = {.real = 0, .imaginary = 0};
            Complex z_squared = {.real = 0, .imaginary = 0};

            int iterations = 0;
            while (z_squared.real + z_squared.imaginary <= 4 && iterations < Max_Iterations) {
                z.imaginary = z.real * z.imaginary;
                z.imaginary += z.imaginary;
                z.imaginary += c.imaginary;

                z.real = z_squared.real - z_squared.imaginary + c.real;

                z_squared.real = z.real * z.real;
                z_squared.imaginary = z.imaginary * z.imaginary;

                iterations++;
            }
            //pthread_mutex_lock(&mutex);
            args->pixels[y * Image_Width + x][0] = args->colors[iterations][0];
            args->pixels[y * Image_Width + x][1] = args->colors[iterations][1];
            args->pixels[y * Image_Width + x][2] = args->colors[iterations][2];
           //pthread_mutex_unlock(&mutex);
        }
    }
    pthread_exit (0);
}

int main(int argc, const char **argv) {
    RGB_Pixel *pixels = malloc(sizeof(RGB_Pixel) * Image_Width * Image_Height);

    RGB_Pixel colors[Max_Iterations + 1];

    struct timeval start, end;

    printf("Timing...\n");
    gettimeofday(&start, NULL);

    calc_colors(colors);
    colors[Max_Iterations][0] = MAX_RGB_VAL;
    colors[Max_Iterations][1] = MAX_RGB_VAL;
    colors[Max_Iterations][2] = MAX_RGB_VAL;

    const Complex min_bounds = {.real = Focus_Point.real - Zoom, .imaginary = Focus_Point.imaginary - Zoom};
    const Complex max_bounds = {.real = Focus_Point.real + Zoom, .imaginary = Focus_Point.imaginary + Zoom};
    const Complex scale = {
            .real = (max_bounds.real - min_bounds.real) / Image_Width,
            .imaginary = (max_bounds.real - min_bounds.real) / Image_Height
    };

  
    int qtd_x  =   (Image_Width / Div_Width);
    int qtd_y  =   (Image_Height / Div_Height);
    
    int tmpCont = 0;
    int qtd_total = Div_Width * Div_Height;
    int num_thread[qtd_total][4];
    int img_y = 0;
    int img_x = 0;
    
    while (img_y < Image_Height) {
        while (img_x < Image_Width) {
            num_thread[tmpCont][0] = img_x;
            num_thread[tmpCont][1] = img_y;
            num_thread[tmpCont][2] = img_x+qtd_x;
            num_thread[tmpCont][3] = img_y+qtd_y;
            tmpCont++;
            img_x = img_x+qtd_x;
        }
     
        img_y = img_y+qtd_y;
    }
     
   // pthread_t* threads = (pthread_t*)malloc(qtd_total  * sizeof(pthread_t));
    
    pthread_t threads[qtd_total];
    
    
    for(int i = 0; i < qtd_total; i++) {
       
         
        struct Thread_args thread_args = {
                .x = num_thread[i][0],
                .y = num_thread[i][1],
                .x_end = num_thread[i][2],
                .y_end = num_thread[i][3],
                .min_bounds_real = min_bounds.real,
                .min_bounds_imaginary = min_bounds.imaginary,
                .scale_real = scale.real,
                .scale_imaginary = scale.imaginary,
                .colors = colors,
                .pixels = pixels
        };
        
        rc = pthread_create(&threads[i],NULL,func_thread,&thread_args);
        assert(rc == 0); 
    }

    for(int i = 0; i < qtd_total; i++) {
       rc = pthread_join(threads[i], NULL);
       assert(rc == 0); 
    }
    
    gettimeofday(&end, NULL);
    printf("Took %f seconds\n\n", end.tv_sec - start.tv_sec + (double) (end.tv_usec - start.tv_usec) / 1000000);

    FILE *fp = fopen("MandelbrotSet.ppm", "wb");
    fprintf(fp, "P6\n %d %d\n %d\n", Image_Width, Image_Height, MAX_RGB_VAL);
    fwrite(pixels, sizeof(RGB_Pixel), Image_Width * Image_Width, fp);
    fclose(fp);

    free(pixels);

    pthread_exit(0);
    
    return 0;
}



