// Every XAM export that takes an XOVERLAPPED completes it. On the console a
// XAM call given an XOVERLAPPED returns ERROR_IO_PENDING and reports its
// result through the overlapped (its event, its completion routine). Games
// rely on that: Deathly Hallows - Part 1's save loader (sub_8215A1C0) asks
// XamContentGetCreator with an overlapped and treats any return but
// ERROR_IO_PENDING, even success, as a failed load. The runtime's version
// returned 0 and ignored the overlapped, so every Continue and every
// checkpoint restart said "Load Failed. The save file appears to be
// damaged" (and the restart then froze on one frame).
//
// The runtime's XAM sources are read (ctest passes their paths): each Xam*
// function with an XXOVERLAPPED parameter must complete it (XamComplete,
// CompleteXamCall, CompleteOverlapped, imports_user's Finish) or pass it on
// to another such function. From the repo root:
//   clang++ -std=c++20 -O2 tests/xam_overlapped_test.cpp -o build/xam_overlapped_test
//   build/xam_overlapped_test runtime/kernel/xam.cpp runtime/kernel/imports_unleashed.cpp runtime/kernel/imports_user.cpp
#include <cstdio>
#include <fstream>
#include <map>
#include <regex>
#include <sstream>
#include <string>

static int g_failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { g_failures++; printf("  FAIL %s:%d: %s: ", __FILE__, __LINE__, #cond); printf(__VA_ARGS__); printf("\n"); } } while (0)

// name -> body of each Xam* function definition with an XXOVERLAPPED parameter.
static void Collect(const std::string& source, std::map<std::string, std::string>& out)
{
    static const std::regex head(R"(\nuint32_t (Xam\w+)\(([^)]*)\)\s*\{)");
    for (auto it = std::sregex_iterator(source.begin(), source.end(), head); it != std::sregex_iterator(); ++it)
    {
        const std::smatch& m = *it;
        if (m[2].str().find("XXOVERLAPPED") == std::string::npos)
            continue;
        size_t i = size_t(m.position(0) + m.length(0));
        int depth = 1;
        size_t start = i;
        while (i < source.size() && depth > 0)
        {
            depth += source[i] == '{';
            depth -= source[i] == '}';
            i++;
        }
        out[m[1].str()] = source.substr(start, i - start);
    }
}

static bool Completes(const std::string& body, const std::map<std::string, std::string>& all)
{
    for (const char* call : { "XamComplete(", "CompleteXamCall(", "CompleteOverlapped(", "Finish(" })
        if (body.find(call) != std::string::npos)
            return true;
    for (const auto& [name, other] : all)
        if (body.find(name + "(") != std::string::npos)
            return true;  // passes the overlapped on (XamContentCreate -> XamContentCreateEx)
    return false;
}

int main(int argc, char** argv)
{
    std::map<std::string, std::string> functions;
    for (int i = 1; i < argc; i++)
    {
        std::ifstream f(argv[i]);
        CHECK(f.good(), "can't read %s", argv[i]);
        std::stringstream ss;
        ss << f.rdbuf();
        Collect(ss.str(), functions);
    }
    if (argc > 1)
    {
        CHECK(functions.count("XamContentGetCreator") == 1, "XamContentGetCreator found (%zu functions)", functions.size());
        CHECK(functions.count("XamContentCreateEx") == 1, "XamContentCreateEx found");
    }
    for (const auto& [name, body] : functions)
        CHECK(Completes(body, functions), "%s takes an XXOVERLAPPED but never completes it", name.c_str());
    printf("%zu XAM functions with an overlapped checked: %s\n", functions.size(), g_failures ? "FAILED" : "ok");
    return g_failures ? 1 : 0;
}
