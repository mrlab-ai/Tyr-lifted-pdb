#include "json_loader.hpp"

#include <fstream>
#include <stdexcept>

namespace tyr::profiling
{
std::filesystem::path root_path()
{
    return std::filesystem::path(std::string(ROOT_DIR));
}

std::filesystem::path data_path(std::string_view relative_path)
{
    return root_path() / "data" / relative_path;
}

std::filesystem::path profiling_path(std::string_view relative_path)
{
    return root_path() / "profiling" / relative_path;
}

std::string read_file(const std::filesystem::path& path)
{
    auto stream = std::ifstream(path);
    if (!stream)
        throw std::runtime_error("Could not open file: " + path.string());

    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

boost::json::value load_json_file(const std::filesystem::path& path)
{
    return boost::json::parse(read_file(path));
}
}
