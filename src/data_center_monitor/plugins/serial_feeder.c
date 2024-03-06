#include <stdlib.h>
#include <pthread.h>
#include <common/system/sockets.h>
#include <data_center_monitor/plugins/serial_feeder.h>

static signature_t     sig;
static serial_buffer_t sp;
static socket_t        sock;
static pthread_t       thread;

declr_up_get_tag()
{
    *tag = "serial_feeder";
    *tags_deps = NULL;
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
    if (state_fail(sockets_send(sock.fd, 0U, serial_data(&sp), serial_size(&sp), 0LLU))) {
        printf("[D] while sending data\n");
        exit(0);
    }
    while(1);
}

declr_up_action_init(_serial_feeder)
{
    sig.DC_power   = 1.0;
    sig.DRAM_power = 2.0;
    sig.PCK_power  = 3.0;
    sig.FLOPS[0]   = 4.0;

    serial_alloc(&sp, SIZE_8KB);
    serial_dictionary_push_auto(&sp, sig.DC_power);
    serial_dictionary_push_auto(&sp, sig.DRAM_power);
    serial_dictionary_push_auto(&sp, sig.PCK_power);
    serial_dictionary_push_auto(&sp, sig.FLOPS);

    pthread_create(&thread, NULL, sockets_main, NULL);
    return "Created feeder thread";
}