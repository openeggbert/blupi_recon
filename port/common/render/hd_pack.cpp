#include "render/hd_pack.h"

#include <cctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>

namespace blupi::render
{
namespace
{
namespace fs = std::filesystem;

std::string lower(std::string s)
{
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

// `name` in `dir`, any case.
bool child(const fs::path& dir, const std::string& name, fs::path& out)
{
    std::error_code error;
    const fs::path exact = dir / name;
    if (fs::exists(exact, error)) { out = exact; return true; }
    const std::string want = lower(name);
    for (fs::directory_iterator it(dir, error), end; !error && it != end; it.increment(error))
    {
        if (lower(it->path().filename().string()) == want) { out = it->path(); return true; }
    }
    return false;
}
}  // namespace

PackResult load_pack_image(const std::string& root, const std::string& gamePath, int scale, int origWidth,
                           int origHeight, Bitmap& out, std::string& found)
{
    found.clear();
    if (root.empty() || scale < 2) return PackResult::Missing;
    std::vector<std::string> parts;
    std::string part;
    for (char c : gamePath)
    {
        if (c == '\\' || c == '/') { if (!part.empty()) parts.push_back(part); part.clear(); }
        else part += c;
    }
    if (!part.empty()) parts.push_back(part);
    if (parts.empty()) return PackResult::Missing;
    fs::path at = root;
    for (size_t i = 0; i + 1 < parts.size(); i++)
    {
        fs::path next;
        if (!child(at, parts[i], next)) return PackResult::Missing;
        at = next;
    }
    const std::string file = parts.back();
    const std::string stem = file.substr(0, file.find_last_of('.'));
    fs::path image;
    if (!child(at, file, image) && !child(at, stem + ".bmp", image) && !child(at, stem + ".blp", image))
        return PackResult::Missing;
    found = image.string();
    std::ifstream in(image, std::ios::binary);
    const std::vector<uint8_t> data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (data.empty() || !decode_bmp(data.data(), data.size(), out)) return PackResult::Unreadable;
    if (out.width != origWidth * scale || out.height != origHeight * scale) return PackResult::WrongSize;
    return PackResult::Loaded;
}
}  // namespace blupi::render
