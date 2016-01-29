#include <parg.h>
#include "internal.h"
#include "khash.h"

// Mapping from tokens to C strings.
// There's no need for a mapping from C strings to tokens because of math.
KHASH_MAP_INIT_INT(parstr, sds)

static khash_t(parstr)* _token_registry = 0;

sds parg_token_to_sds(parg_token token)
{
    parg_assert(_token_registry, "Uninitialized token registry");
    khiter_t iter = kh_get(parstr, _token_registry, token);
    parg_assert(iter != kh_end(_token_registry), "Unknown token");
    return kh_value(_token_registry, iter);
}

const char* parg_token_to_string(parg_token token)
{
    return parg_token_to_sds(token);
}

parg_token parg_token_from_string(const char* cstring)
{
    parg_token token = kh_str_hash_func(cstring);
    if (!_token_registry) {
        _token_registry = kh_init(parstr);
    }
    int ret;
    int iter = kh_put(parstr, _token_registry, token, &ret);
    kh_value(_token_registry, iter) = sdsnew(cstring);
    return token;
}
