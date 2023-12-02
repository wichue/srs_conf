#include "srs_app_config.hpp"
#include "Test_ReloadWork.h"

SrsConfig* _srs_config = NULL;

int main(int argc, char *argv[])
{
    setbuf(stdout,NULL);

    _srs_config = new SrsConfig();
    _srs_config->parse_options(argc, argv);
    _srs_config->check_config();

    _srs_config->get_http_server_listen();

    Test_ReloadWork* pTest_ReloadWork = new Test_ReloadWork;
    _srs_config->subscribe(pTest_ReloadWork);
    _srs_config->reload();

    while(1){}
}
