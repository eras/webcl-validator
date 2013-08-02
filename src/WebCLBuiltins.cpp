#include "WebCLBuiltins.hpp"

static const char *hashReplacements[] = {
    "2", "3", "4", "8", "16"
};
static const int numHashReplacements =
    sizeof(hashReplacements) / sizeof(hashReplacements[0]);

static const char *unsafeMathBuiltins[] = {
    "fract", "frexp", "lgamma_r", "modf", "remquo", "sincos"
};
static const int numUnsafeMathBuiltins =
    sizeof(unsafeMathBuiltins) / sizeof(unsafeMathBuiltins[0]);

static const char *unsafeVectorBuiltins[] = {
    "vload#", "vload_half", "vload_half#", "vloada_half#",
    "vstore#", "vstore_half", "vstore_half#", "vstorea_#",
    "vstore_half_rte", "vstore_half_rtz", "vstore_half_rtp", "vstore_half_rtn",
    "vstore_half#_rte", "vstore_half#_rtz", "vstore_half#_rtp", "vstore_half#_rtn",
    "vstorea_half_rte", "vstorea_half_rtz", "vstorea_half_rtp", "vstorea_half_rtn",
    "vstorea_half#_rte", "vstorea_half#_rtz", "vstorea_half#_rtp", "vstorea_half#_rtn"
};
static const int numUnsafeVectorBuiltins =
    sizeof(unsafeVectorBuiltins) / sizeof(unsafeVectorBuiltins[0]);

static const char *unsafeAtomicBuiltins[] = {
    "atomic_add", "atomic_sub",
    "atomic_inc", "atomic_dec",
    "atomic_xchg", "atomic_cmpxchg",
    "atomic_min", "atomic_max",
    "atomic_and", "atomic_or", "atomic_xor"
};
static const int numUnsafeAtomicBuiltins =
    sizeof(unsafeAtomicBuiltins) / sizeof(unsafeAtomicBuiltins[0]);

static const char *unsupportedBuiltins[] = {
    "async_work_group_copy", "async_work_group_strided_copy",
    "wait_group_events", "prefetch"
};
static const int numUnsupportedBuiltins =
    sizeof(unsupportedBuiltins) / sizeof(unsupportedBuiltins[0]);

WebCLBuiltins::WebCLBuiltins()
    : unsafeMathBuiltins_()
    , unsafeVectorBuiltins_()
    , unsafeAtomicBuiltins_()
    , unsupportedBuiltins_()
{
    initialize(unsafeMathBuiltins_, unsafeMathBuiltins, numUnsafeMathBuiltins);
    initialize(unsafeVectorBuiltins_, unsafeVectorBuiltins, numUnsafeVectorBuiltins);
    initialize(unsafeAtomicBuiltins_, unsafeAtomicBuiltins, numUnsafeAtomicBuiltins);
    initialize(unsupportedBuiltins_, unsupportedBuiltins, numUnsupportedBuiltins);
}

WebCLBuiltins::~WebCLBuiltins()
{
}

bool WebCLBuiltins::isUnsafe(const std::string &builtin) const
{
    return
        unsafeMathBuiltins_.count(builtin) ||
        unsafeVectorBuiltins_.count(builtin) ||
        unsafeAtomicBuiltins_.count(builtin);
}

bool WebCLBuiltins::isUnsupported(const std::string &builtin) const
{
    return unsupportedBuiltins_.count(builtin);
}

void WebCLBuiltins::initialize(
    BuiltinNames &names, const char *patterns[], int numPatterns)
{
    for (int i = 0; i < numPatterns; ++i) {
        const std::string pattern = patterns[i];
        const size_t hashPosition = pattern.find_last_of('#');
        if (hashPosition == std::string::npos) {
            names.insert(pattern);
        } else {
            for (int j = 0; j < numHashReplacements; ++j) {
                std::string name;
                name += pattern.substr(0,
                                       hashPosition);
                name += hashReplacements[j];
                name += pattern.substr(hashPosition + 1,
                                       pattern.length() - hashPosition - 1);
                names.insert(name);
            }
        }
    }
}