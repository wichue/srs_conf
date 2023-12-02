#include "Test_ReloadWork.h"

Test_ReloadWork::Test_ReloadWork()
{

}

srs_error_t Test_ReloadWork::on_reload_testwork()
{
    printf("\non_reload_testwork\n");
    return srs_success;
}
