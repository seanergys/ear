%.so: %.c
	$(CC) $(CFLAGS)  -o $@ $^

dummy.so: dummy.c
	$(CC) $(CFLAGS)  -o $@ $^

kernel_cuda.so: kernel_cuda.c kernel_cuda_ptx.cu
	$(CUDA_BASE)/bin/nvcc  $(CFLAGS)  --compiler-options '-fPIC'  -o $@ $^

kernel_cl.so: kernel_cl.c
	$(CC) $(CFLAGS) $(OPENCL_CFLAGS) -D CL_TARGET_OPENCL_VERSION=100  -o $@ $^ $(OPENCL_LDFLAGS)
