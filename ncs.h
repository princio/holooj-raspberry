#ifndef __MOVIDIUS_H__
#define __MOVIDIUS_H__

int ncs_init();
int ncs_load_nn(const char*, const char*);

int ncs_inference(float *in, unsigned int in_size_bytes, float *out, int out_size_bytes);

int ncs_destroy();

typedef enum { 
    NCSDevCreateError,
    NCSDevOpenError,
	NCSGraphCreateError,
	NCSGraphAllocateError,
	NCSInferenceError,
	NCSGetOptError,
	NCSFifoReadError,
	NCSDestroyError,
    NCSReadGraphFileError,
    NCSTooFewBytesError
} NCSerror;

#endif //__MOVIDIUS_H__