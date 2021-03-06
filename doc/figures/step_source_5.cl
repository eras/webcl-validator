struct _Wcl2Struct {
    int a;
};

struct _WclStruct {
    struct _Wcl2Struct b;
};

typedef struct {
    struct _WclStruct _wcl_c;
    int _wcl_index;
} __attribute__ ((aligned (_WCL_ADDRESS_SPACE_private_ALIGNMENT))) _WclPrivates;

void fix_index(
	int index, int *fixed_index) {
	*fixed_index = index*1;
}

kernel foo_kernel(
	global float *in, 
	global float4 *out) {
    struct _WclStruct c;

    int i = get_global_id(0);
    int index;
    fix_index(i, &index);
    out[i] = vload4(index, in);
}

