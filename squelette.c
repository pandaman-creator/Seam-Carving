#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define STBI_NO_FAILURE_STRINGS
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_FAILURE_USERMSG
#include "stb_image.h"
#include "stb_image_write.h"
#include "seam_carving.h"
#include <assert.h>

image *image_load(char *filename){
    int w, h, channels;
    uint8_t *data = stbi_load(filename, &w, &h, &channels, 0);
    if (!data) {
        fprintf(stderr, "Erreur de lecture.\n");
        stbi_failure_reason();
        exit(EXIT_FAILURE);
    }
    if (channels != 1){
        fprintf(stdout, "Pas une image en niveaux de gris.\n");
        exit(EXIT_FAILURE);
    }
    image *im = image_new(h, w);
    for (int i = 0; i < h; i++){
        for (int j = 0; j < w; j++){
            im->at[i][j] = data[j + i * w];
        }
    }
    free(data);
    return im;
}

void image_save(image *im, char *filename){
    int h = im->h;
    int w = im->w;
    int stride_length = w;
    uint8_t *data = malloc(w * h * sizeof(uint8_t));
    for (int i = 0; i < h; i++){
        for (int j = 0; j < w; j++){
            data[j + i * w] = im->at[i][j];
        }
    }
    if (!stbi_write_png(filename, w, h, 1, data, stride_length)){
        fprintf(stderr, "Erreur d'écriture.\n");
        image_delete(im);
        free(data);
        exit(EXIT_FAILURE);
    }
    free(data);
}


image *image_new(int h, int w){
    image *im = malloc(sizeof(image));
    im->h = h ;
    im->w = w ;
    im->at = malloc(h* sizeof(uint8_t *));
    for (int i = 0 ; i < h ; i++){
        im->at[i] = malloc(w*sizeof(uint8_t));
    }
    return im;
}

void image_delete(image *im){
    for (int i = 0; i < im->h ; i++){
        free(im->at[i]);
    }
    free(im->at);
    free(im);
    return ;
}

void invert(image *im){
    for (int i = 0 ; i < im->h ; i++){
        for (int j = 0 ; j < im->w ; j++){
            im->at[i][j] = 255 - im->at[i][j];
        }
    }
    return ;
}

void binarize(image *im){
    for (int i = 0 ; i < im->h ; i++){
        for (int j = 0 ; j < im->w ; j++){
            if (im->at[i][j] <  128) im->at[i][j] = 0 ;
            else im->at[i][j] = 255 ;
            // + rapide opérateur ternaire (pas au programme) :
            // im->at[i][j] = (im->at[i][j] < 128) ? 0 : 255 ;
        }
    }
    return ;
}

void echange(uint8_t *t, int len, int i, int j){
    assert ((i < len) & (j < len));
    uint8_t temp = t[i];
    t[i] = t[j];
    t[j] = temp;
    return ;
}

void flip_horizontal(image *im){
    for (int i = 0 ; i < im->h ; i++){
        for (int j = 0 ; j < (im->w / 2) ; j++){
            echange(im->at[i] , im->w, j, im->w - j - 1);
        }
    }
    return ;
}



// 2 poss quand on fait des malloc comme as ; soit on fait en->at = malloc(...) ; 
//soit on malloc un espace mémoire (du type adapté (= plus chiant car on dans l'autre cas le castage se fait automatiquement
// alors que la il faut y réfléchir)) et on fait pointer en->at dessus. 
// 2eme plus formel, rigoureux je pense
energy *energy_new(int h, int w){
    energy *en = malloc(sizeof(energy));
    en->h = h ;
    en->w = w ;
    en->at = malloc(h* sizeof(double *));
    for (int i = 0 ; i < h ; i++){
        en->at[i] = malloc(w*sizeof(double));
    }
    return en;
}

void energy_delete(energy *e){
    for (int i = 0; i < e->h ; i++){
        free(e->at[i]);
    }
    free(e->at);
    free(e);
    return ;
}

double absolue(uint8_t x){
    // il éxiste une fonction préétabli pr la val abs d'un float : fabs(...) ; youpi
    if ((double)x < 0){ return -(double)x;}
    else { return (double)x;}
}

//c'est elle qui foire ma gueule...
void compute_energy(image *im, energy *e){
    for (int i = 0 ; i < im->h ; i++){
        for (int j = 0 ; j < im->w ; j++){
            int jr = (j < (im->w) - 1) ? j + 1 : j ;
            int jl = (j > 0) ? j - 1 : j ;
            int ib = (i < (im->h) - 1) ? i + 1 : i ;
            int it = (i > 0) ? i - 1 : i ;
            
            double eij = (absolue(im->at[i][jr] - im->at[i][jl]) / (double)(jr - jl)) +
                         (absolue(im->at[ib][j] - im->at[it][j]) / (double)(ib - it));
            e->at[i][j] = eij;
        }
    }
    return ;
}



image *energy_to_image(energy *e){
    // trouvage du min et max
    double min = e->at[0][0];
    double max = e->at[0][0];
    for (int i = 0 ; i < e->h; i++){
        for (int j = 0 ; j < e->w ; j++){
            min = (e->at[i][j] < min) ? e->at[i][j] : min ;
            max = (e->at[i][j] > max) ? e->at[i][j] : max ;
        }
    }
    // génération de l'image correspondante
    image* im = image_new(e->h , e->w);
    for (int i = 0 ; i < im->h; i++){
        for (int j = 0 ; j < im->w ; j++){
            im->at[i][j] = (uint8_t)(e->at[i][j] - min)*255 / (max - min);
        }
    }
    return im;
}



void remove_pixel(uint8_t *line, double *e, int w){
    // trouvage du min
    int imin = 0 ; 
    for (int i = 0 ; i < w ; i++){
        imin = (e[imin] < e[i]) ? imin : i ;
    }
    // décalage à partir du min
    for (int j = imin ; j < w - 1 ; j++){
        line[j] = line[j + 1] ;
        e[j] = e[j + 1] ;
    }
    return ;
}


void reduce_one_pixel(image *im, energy *e){
    for (int i = 0 ; i < im->h ; i++){
        remove_pixel(im->at[i] , e->at[i] , im->w);
    }
    im->w -- ;
    e->w -- ;
}

void reduce_pixels(image *im, int n){
    energy* en = energy_new(im->h , im->w);
    for (int i = 0 ; i < n ; i++){
        compute_energy(im, en);
        reduce_one_pixel(im, en);
    }
    energy_delete(en);
    return ;
}



int best_column(energy *e){
    //grave pas benef de ccommencer avec la somme_col de la première colonne. technique dan sla correction ;
    // on initialise a une valuer impossible et on ajoute la condition dans le if
    int i_colmin = 0 ;
    double colmin = 0 ;
    for (int k = 0 ; k < e->h ; k++){
        colmin += e->at[k][0] ;
    }
    double s ;
    for (int i = 1 ; i < e->w ; i++){
        s = 0;
        for (int j = 0 ; j < e->h ; j++){
            s += e->at[j][i] ;
        }
        if (s < colmin){ 
            colmin = s ;
            i_colmin = i ;
        }
    }
    return i_colmin ;
}

void reduce_one_column(image *im, energy *e){
    int i_colmin = best_column(e) ;
    for (int i = 0 ; i < im->h ; i++){
        for (int j = i_colmin; j < (im->w) - 1 ; j++){
                im->at[i][j] = im->at[i][j+1] ;
                e->at[i][j] = e->at[i][j+1] ;
        }
    }
    im->w -- ;
    e->w -- ;
    return ;
}

void reduce_column(image *im, int n){
    energy* en = energy_new(im->h , im->w) ;
    for (int i = 0 ; i < n ; i++){
        compute_energy (im, en) ;
        reduce_one_column(im, en) ;
    }
    energy_delete(en);
    return ;
}

double min_3_poss (double gauche, double milieu, double droite){
    if (gauche < milieu){
        if (droite < gauche){
            return droite ;
        }
        else{
            return gauche ;
        }
    }
    else {
        if (droite < milieu){
            return droite ;
        }
        else{
            return milieu ;
        }
    }
}

double min_2_poss (double gauche , double droite){
    if (gauche < droite){
        return gauche ;
    }
    else return droite ;
}

// pour la concision du texte, bcp mieux la corr !
void energy_min_path(energy *e){
    for (int i = 1 ; i < e->h ; i++){
        for (int j = 0 ; j < e->w ; j++){
            if (j == 0 || j == (e->w) - 1){
                if (j == 0){
                    e->at[i][j] = e->at[i][j] + min_2_poss(e->at[i - 1][j], e->at[i - 1][j + 1]) ;
                }
                if (j == (e->w) - 1){
                e->at[i][j] = e->at[i][j] + min_2_poss(e->at[i - 1][j - 1], e->at[i - 1][j]) ;
                }
            }
            else{
                e->at[i][j] = e->at[i][j] + min_3_poss(e->at[i - 1][j - 1], e->at[i - 1][j], e->at[i - 1][j + 1]) ;
            }
        }
    }
}

path *path_new(int n){
    path *pathate = malloc(sizeof(path)) ;
    pathate->size = n ;
    pathate->at = malloc(n*sizeof(int)) ;
    return pathate ;
}

void path_delete(path *p){
    free(p->at);
    free(p) ;
    return ;
}

int min_tab(double *tab, int len){
    // renvoie l'indice du min du tab
    int min = 0 ;
    for (int i = 1 ; i < len ; i++){
        min = (tab[i] < tab[min]) ? i : min ;
    }
    return min ;
}

int min_3_indice(double a, double b, double c){
    // renvoie 0 si a est le min, 1 si b est le min, 2 si c est le min
    if (a < b){
        return (c < a) ? 2 : 0 ;
    }
    else{
        return (c < b) ? 2 : 1 ;        
    }
}

// sol bcp mieux en concision du texte. technique du bord gauche bord droit avec operateur ternaire évite les 3 poss c cool !
void compute_min_path(energy *e, path *p){
    assert(e->w >= 2) ;
    assert(e->h == p->size) ;

    // trouvage du dernier min de la derniere ligne du tab e
    int actu_min = min_tab(e->at[(e->h) - 1], e->w) ;
    p->at[(e->h) - 1] = actu_min ;

    // trouvage de tout les autres en remontant 
    for (int i = (e->h) - 2 ; i >= 0 ; i--){
        // bord gauche
        if (actu_min == 0){
            actu_min = (e->at[i][0] < e->at[i][1]) ? 0 : 1 ;
            p->at[i] = actu_min ;
        }
        //bord droite
        else if(actu_min == (e->w) - 1){
            actu_min = (e->at[i][(e->w) - 2] < e->at[i][(e->w) - 1]) ? (e->w) - 2 : (e->w) - 1 ;
            p->at[i] = actu_min ;
        }
        // milieu quelconque
        else{
            actu_min = actu_min - 1 + min_3_indice(e->at[i][actu_min - 1], e->at[i][actu_min], e->at[i][actu_min + 1]) ;
            p->at[i] = actu_min ;
        }
    }
    return ;
}

void reduce_seam_carving(image *im, int n){
    assert(n < im->w) ;

    energy *en = energy_new(im->h, im->w) ;
    path *p = path_new(en->h) ;

    for (int k = 0 ; k < n ; k++){
        compute_energy(im, en) ;
        energy_min_path(en) ;
        compute_min_path(en, p) ;
        for (int i = 0 ; i < im->h ; i++){
            for (int j = p->at[i] ; j < (im->w) - 1 ; j++){
                im->at[i][j] = im->at[i][j + 1] ;
                //pas utile d'enlever les en aussi car on recompute a chaque fois
            }
        }
        im->w -- ;
        en->w -- ;
    } 
    path_delete(p) ;
    energy_delete(en) ;
    return ;
}

void affichage_im(image *im){
    printf("============================\n");
    for (int i = 0 ; i < im->h ; i++){
        for (int j = 0 ; j < im->w ; j++){
            printf("%d | ", im->at[i][j]);
        }
        printf("\n");
    }
    printf("============================\n");
    return ;
}

void affichage_en(energy *en){
    printf("============================\n");
    for (int i = 0 ; i < en->h ; i++){
        for (int j = 0 ; j < en->w ; j++){
            printf("%f | ", en->at[i][j]);
        }
        printf("\n");
    }
    printf("============================\n");
    return ;
}

void affichage_path(path *p){
    printf("============================\n") ;
    for (int i = 0 ; i < p->size ; i++){
            printf("%d , ", p->at[i]) ;
    }
    printf("\n");
    printf("============================\n");
    return ;
}



int main(int argc, char *argv[]) {
    if (argc < 3){
        printf("Fournir le fichier d'entrée et de sortie.\n");
        exit(EXIT_FAILURE);
    }
    char *f_in = argv[1];
    char *f_out = argv[2];
    image *im = image_load(f_in);
    // triffouillis
    
    reduce_seam_carving(im, 300) ;

    //fin trifouillis
    image_save(im, f_out) ;
    image_delete(im) ;
    return 0 ;
}