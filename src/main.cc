#include <nexus/run.hh>

#include <rich-log/logger.hh>

int main(int argc, char** argv)
{
    rlog::set_console_log_style(rlog::console_log_style::brief);

    return nx::run(argc, argv);
}
