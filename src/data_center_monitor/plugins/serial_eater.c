#include <common/system/sockets.h>
#include <data_center_monitor/plugins/serial_feeder.h>

static socket_t         sock;
static int              fd_read;
static char             buffer_recv[16384];
static serial_params_t sp;
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

#define serial_params_unpack_auto(sp, parameter) \
    serial_params_unpack(sp, #parameter, (void *) &parameter, sizeof(parameter));


static void serial_params_unpack(serial_params_t *sp, char *tag, void *param, size_t size_param)
{
    size_t  size_tag   = 16;
    size_t *size_taken = (size_t *) &sp->data[0];
    size_t  size_after = size_tag + sizeof(size_t) + size_param;
    size_t  size_index = 8;
    char   *p          = &sp->data[size_index];

    while (size_index < *size_taken) {
        printf("size indes %lu < size_taken %lu\n", size_index, *size_taken);
        char   *under_tag  = &p[size_index];
        size_t *under_size = (size_t *) &p[size_index+size_tag];
        void   *under_data = &p[size_index+size_tag+sizeof(size_t)];

        // If equal
        if (strcmp(under_tag, tag) == 0) {
            memcpy(param, under_data, *under_size);
            return;
        }
        size_index += size_tag + sizeof(size_t) + *under_size;
    }
    return;
}

declr_up_fds_attend()
{
    size_t data_size = 0LU;
    ullong extra     = 0LLU;
    uint type        = 0U;
    int i;

    //
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
            printf("Reading %lu bytes\n", data_size);
            if (state_fail(sockets_recv(fd_read, sp.data, data_size, 1))) {
                return reterr("[X] while reading socket");
            }
            printf("Read %lu bytes\n", data_size);
            serial_params_unpack_auto(&sp, sig.TPI);
            serial_params_unpack_auto(&sp, sig.IO_MBS);
            serial_params_unpack_auto(&sp, sig.EDP);
            serial_params_unpack_auto(&sp, sig.PCK_power);
            serial_params_unpack_auto(&sp, sig.DRAM_power);
            serial_params_unpack_auto(&sp, sig.DC_power);
            printf("sig.DC_power: %lf\n", sig.DC_power);
            printf("sig.DRAM_power: %lf\n", sig.DRAM_power);
            printf("sig.PCK_power: %lf\n", sig.PCK_power);
            printf("sig.EDP: %lf\n", sig.EDP);
            printf("sig.IO_MBS: %lf\n", sig.IO_MBS);
            printf("sig.TPI: %lf\n", sig.TPI);
        }
    }
    return "[X] Finished correctly";
}