
#include <fstream>
#include <streambuf>

#include "utils.hpp"

using namespace std;
using namespace std::filesystem;

string utils::from_file(const path &path)
{
    ifstream file{path};
    return string{std::istreambuf_iterator<char>(file),
                  std::istreambuf_iterator<char>()};
}