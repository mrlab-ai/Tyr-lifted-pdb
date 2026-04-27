#ifndef TYR_PROFILING_JSON_LOADER_HPP_
#define TYR_PROFILING_JSON_LOADER_HPP_

#include <boost/json.hpp>

#include <filesystem>
#include <string>
#include <string_view>

namespace tyr::profiling
{
std::filesystem::path root_path();

std::filesystem::path data_path(std::string_view relative_path);

std::filesystem::path profiling_path(std::string_view relative_path);

std::string read_file(const std::filesystem::path& path);

boost::json::value load_json_file(const std::filesystem::path& path);
}

#endif
