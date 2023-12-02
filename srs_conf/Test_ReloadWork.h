#ifndef TEST_RELOADWORK_H
#define TEST_RELOADWORK_H

#include "srs_app_reload.hpp"
class Test_ReloadWork : public ISrsReloadHandler
{
public:
    Test_ReloadWork();

    virtual srs_error_t on_reload_testwork();
};

#endif // TEST_RELOADWORK_H
