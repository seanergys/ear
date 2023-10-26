#include <stdlib.h>
#include <pthread.h>
#include <common/system/sockets.h>
#include <data_center_monitor/plugins/serial_feeder.h>

static signature_t     sig;
static serial_params_t sp;
static socket_t        sock;
static pthread_t       thread;

declr_up_get_tag()
{
    *tag = "serial_feeder";
    *tags_deps = NULL;
}

#define serial_params_add_auto(sp, parameter) \
    serial_params_add(sp, #parameter, (void *) &parameter, sizeof(parameter));

static void serial_params_alloc(serial_params_t *sp)
{
    size_t *size_taken;
    sp->data_size = 16384;
    sp->data      = calloc(16384, 1);
    size_taken    = (size_t *) sp->data;
    *size_taken   = sizeof(size_t);
}

static void serial_params_add(serial_params_t *sp, char *tag, void *param, size_t size_param)
{
    size_t  size_tag   = 16;
    size_t *size_taken = ((size_t *) &sp->data[0]);
    size_t  size_after = size_tag + sizeof(size_t) + size_param;
    char   *p           = &sp->data[(*size_taken)];

    // Check and realloc
    if (sp->data_size <= *size_taken+size_after) {
        sp->data_size *= 2;
        sp->data = realloc(sp->data, sp->data_size);
    }
    // Copying
    strcpy(&p[0], tag);
    memcpy(&p[size_tag], &size_param, sizeof(size_t));
    memcpy(&p[size_tag+sizeof(size_t)], param, size_param);
    *size_taken += size_after;
}

static size_t serial_params_size(serial_params_t *sp)
{
    size_t *size_taken = (size_t *) sp->data;
    return *size_taken;
}

static void *sockets_main(void *nothing)
{
    sleep(1);
    if (state_fail(sockets_init(&sock, "localhost", 6969, TCP))) {
        printf("[D] while socket initialization\n");
        exit(0);
    }
    if (state_fail(sockets_socket(&sock)))   {
        printf("[D] while socket initialization\n");
        exit(0);
    }
    if (state_fail(sockets_connect(&sock)))   {
        printf("[D] while socket connecting\n");
        exit(0);
    }
    if (state_fail(sockets_send(sock.fd, 0U, serial_params_data(&sp), serial_params_size(&sp), 0LLU))) {
        printf("[D] while sending data\n");
        exit(0);
    }
    while(1);
}

declr_up_action_init(_serial_feeder)
{
    sig.IO_MBS = 2.0;
    serial_params_alloc(&sp);
    serial_params_add_auto(&sp, sig.DC_power);
    serial_params_add_auto(&sp, sig.DRAM_power);
    serial_params_add_auto(&sp, sig.PCK_power);
    serial_params_add_auto(&sp, sig.EDP);
    serial_params_add_auto(&sp, sig.IO_MBS);
    serial_params_add_auto(&sp, sig.TPI);
    serial_params_add_auto(&sp, sig.FLOPS);

    pthread_create(&thread, NULL, sockets_main, NULL);
    return "Created feeder thread";
}