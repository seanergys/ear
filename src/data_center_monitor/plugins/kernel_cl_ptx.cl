__kernel void cl_kernel(__global int *C) {
    
    int id = get_global_id(0);
    unsigned long long i;

    C[id] = id;
    for (i = 0; i < 1000000000LLU; ++i) {
        C[id] += id * ((int) i % 9);
    }
}
