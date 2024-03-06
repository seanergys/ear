#include <common/system/sockets.h>
#include <data_center_monitor/plugins/serial_feeder.h>

static socket_t         sock;
static int              fd_read;
static char             buffer_recv[16384];
static serial_buffer_t  sp;
static signature_t      sig;

declr_up_get_tag()
{
    *tag = "serial_eater";
    *tags_deps = NULL;
}

static char *reterr(char *msg)
{
    static char buffer[512];
    sprintf(buffer, "%s (%s)", msg, state_msg);
    return buffer;
}

declr_up_fds_register()
{
    afd_init(set);

    if (state_fail(sockets_init(&sock, "localhost", 6969, TCP))) {
        return reterr("[X] while socket initialization");
    }
    if (state_fail(sockets_socket(&sock)))   {
        return reterr("[X] while socket initialization");
    }
    if (state_fail(sockets_bind(&sock, 10))) {
        return reterr("[X] while socket binding");
    }
    if (state_fail(sockets_listen(&sock)))   {
        return reterr("[X] while socket listening");
    }
    if (!afd_set(sock.fd, set, POLLIN))       {
        return reterr("[X] while setting FD in poll");
    }
    return_print(state_msg, "prepared to receive socket FD%d", sock.fd);
}

declr_up_fds_attend()
{
    size_t data_size = 0LU;
    ullong extra     = 0LLU;
    uint type        = 0U;
    int i;

    sp.data = buffer_recv;
    // Accept
    for(i = set->fd_min; i <= set->fd_max; i++) {
        if (AFD_ISSET(sock.fd, set)) {
            if (state_fail(sockets_accept(i, &fd_read, NULL))) {
                return reterr("[X] while accepting socket");
            }
            AFD_SET(fd_read, set);
            return_print(state_msg, "accepted FD%d", fd_read);
        }
        if (AFD_ISSET(fd_read, set)) {
            if (state_fail(sockets_recv_header(fd_read, &type, &data_size, &extra, 1))) {
                return reterr("[X] while reading socket");
            }
            if (state_fail(sockets_recv(fd_read, sp.data, data_size, 1))) {
                return reterr("[X] while reading socket");
            }
            serial_dictionary_pop_auto(&sp, sig.DC_power);
            serial_dictionary_pop_auto(&sp, sig.DRAM_power);
            serial_dictionary_pop_auto(&sp, sig.PCK_power);
            serial_dictionary_pop_auto(&sp, sig.FLOPS);
            printf("sig.DC_power  : %lf\n", sig.DC_power);
            printf("sig.DRAM_power: %lf\n", sig.DRAM_power);
            printf("sig.PCK_power : %lf\n", sig.PCK_power);
            printf("sig.FLOPS     : %llu\n", sig.FLOPS[0]);
            return "[X] Finished correctly";
        }
    }
    return NULL;
}