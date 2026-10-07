#pragma once

#include "djmeta/normalizer.h"

class file_info;

namespace djmeta_foobar {

djmeta::MetadataDocument metadata_from_file_info(const file_info& info);

} // namespace djmeta_foobar
