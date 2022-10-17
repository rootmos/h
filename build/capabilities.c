#include <sys/capability.h>

void drop_capabilities(void)
{
    if(!CAP_IS_SUPPORTED(CAP_SETPCAP)) {
        failwith("SETPCAP not supported");
    }

    cap_t c = cap_get_proc();
    CHECK_NOT(c, NULL, "cap_get_proc");

    cap_flag_value_t allowed;
    int r = cap_get_flag(c, CAP_SETPCAP, CAP_EFFECTIVE, &allowed);
    CHECK(r, "cap_get_flag");

    if(allowed) {
        r = cap_set_mode(CAP_MODE_NOPRIV);
        CHECK(r, "cap_set_mode(CAP_MODE_NOPRIV)");
    } else {
        debug("capability SETPCAP not in effective set; unable to drop capabilities");
    }
}
