#ifndef __YOLOv2_H__
#define __YOLOv2_H__


/** all floats */
#define NY2_OUT_W           13
#define NY2_OUT_H           13
#define NY2_B_CELL          5
#define NY2_B_TOTAL         (NY2_OUT_W*NY2_OUT_H*NY2_B_CELL)
#define NY2_COORDS          4
#define NY2_CLASSES         80
#define NY2_INPUT_W         416
#define NY2_INPUT_H         416
#define NY2_INPUT_C         3
#define NY2_INPUT_SIZE      (NY2_INPUT_W*NY2_INPUT_H*NY2_INPUT_C)
#define NY2_OUTPUT_SIZE     (NY2_OUT_W*NY2_OUT_H*NY2_B_CELL*(NY2_COORDS + 1 + NY2_CLASSES))


extern float*       ny2_input;
extern int          ny2_names[80];
extern const char   ny2_categories[626];

typedef struct box{
    float x, y, w, h;
} box;

typedef struct detection{
    box bbox;
    int classes;
    float *prob;
    float objectness;
    int sort_class;
} detection;

typedef struct rec_object{
    box bbox;
    float objectness;
    float prob;
    int cindex;
} rec_object;


int  ny2_init();
int  ny2_inference_byte(unsigned char *image, rec_object *robj, float thresh, int const imw, int const imh, int const imc);
int  ny2_inference(rec_object *robj, float thresh, int const imw, int const imh);
void ny2_destroy();


#endif