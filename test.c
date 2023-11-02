
#include <metrics/common/popen.h>
    
static popen_t p;

int main(int argc, char *argv[])
{
    if (state_fail(popen_open("dcgmi dmon --list", 3, 1, &p))) {
        printf("Failed to open: %s\n", state_msg);
        return 0;
    }
    char *event_name = NULL;
    int event_id = 0;
    while (popen_read(&p, "sai", &event_name, &event_id)) {
//        printf("%s: %d\n", event_name, event_id);
    }
    popen_close(&p);
    return 0;
}
