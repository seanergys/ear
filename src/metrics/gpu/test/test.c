#define SHOW_DEBUGS 1

#include <common/output/debug.h>
#include <common/system/monitor.h>
#include <metrics/gpu/gpuproc.h>

static gpuproc_t *data1;
static gpuproc_t *data2;
static gpuproc_t *dataD;

static int update(void *x)
{
    gpuproc_read_diff(no_ctx, data2, data1, dataD); 

    gpuproc_data_print(dataD, fderr);
}

void __main(int argc, char **argv, char **envp)
{
    static int protected = 0;
    uint api;
    
    if (protected) {
        return;
    }
    protected = 1;
    
    debug("__main");
    
    monitor_init();

    gpuproc_load(NO_EARD);
    gpuproc_get_api(&api);
    gpuproc_init(no_ctx);
    gpuproc_data_alloc(&data1);
    gpuproc_data_alloc(&data2);
    gpuproc_data_alloc(&dataD);
    gpuproc_read(no_ctx, data1); 
    apis_print(api, "API: ");
  
	suscription_t *sus = suscription();
	sus->call_init     = NULL;
	sus->call_main     = update;
	sus->time_relax    = 10000;
	sus->time_burst    = 10000;
	sus->suscribe(sus);
 
    return;
}

__attribute__((section(".init_array"))) typeof(__main) *__init = __main;
