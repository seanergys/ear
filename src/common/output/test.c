#include <common/output/verbose.h>

int main(int argc, char *argv[])
{
	log_open(".", "ejke");
	verbose(0, "SUPUESTAMENTE");
	verbose(0, "ESTO ES UNA");
	verbose(0, "PRUEBA");
	return 0;
}
