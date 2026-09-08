#include "projectjson.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <set>
#include <sstream>
#include <stdexcept>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {
constexpr int kLatestProjectSchemaVersion = 6;

const ProjectJson::Json emptyObject = ProjectJson::Json::object();
const ProjectJson::Json emptyArray = ProjectJson::Json::array();

const ProjectJson::Json& objectMember(const ProjectJson::Json& value, const char* key) {
    if (!value.is_object()) return emptyObject;
    const auto found = value.find(key);
    return found != value.end() && found->is_object() ? *found : emptyObject;
}

const ProjectJson::Json& arrayMember(const ProjectJson::Json& value, const char* key) {
    if (!value.is_object()) return emptyArray;
    const auto found = value.find(key);
    return found != value.end() && found->is_array() ? *found : emptyArray;
}

std::filesystem::path executableDirectory() {
#ifdef _WIN32
    std::vector<char> buffer(32768, '\0');
    const DWORD length = GetModuleFileNameA(nullptr, buffer.data(),
                                            static_cast<DWORD>(buffer.size()));
    if (length > 0 && length < buffer.size())
        return std::filesystem::path(std::string(buffer.data(), length)).parent_path();
#endif
    return std::filesystem::current_path();
}

float clean(float value, float fallback) {
    return std::isfinite(value) ? value : fallback;
}
}

ProjectJson ProjectJson::load(const std::string& path) {
    if (path.empty()) throw std::invalid_argument("project.json path is required");
    ProjectJson result;
    result.m_inputPath = std::filesystem::absolute(path).lexically_normal();
    std::string fileName = result.m_inputPath.filename().string();
    std::transform(fileName.begin(), fileName.end(), fileName.begin(),
                   [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
    if (fileName != "project.json")
        throw std::invalid_argument("HiStream accepts project.json only");
    std::ifstream input(result.m_inputPath, std::ios::binary);
    if (!input) throw std::runtime_error("Cannot open project.json: " + result.m_inputPath.string());
    try { input >> result.m_root; }
    catch (const std::exception& error) {
        throw std::runtime_error(std::string("Invalid project.json: ") + error.what());
    }
    if (!result.m_root.is_object()) throw std::runtime_error("project.json root must be an object");
    const std::string format = string(result.m_root, "format", "streamsim-project");
    if (format != "streamsim-project") throw std::runtime_error("Unsupported project format: " + format);
    const int version = integer(result.m_root, "schemaVersion", 1);
    if (version < 1 || version > kLatestProjectSchemaVersion)
        throw std::runtime_error("Unsupported project schemaVersion; expected 1-" +
                                 std::to_string(kLatestProjectSchemaVersion));
    result.m_configuration = objectMember(result.m_root, "configuration");
    if (result.m_configuration.empty()) throw std::runtime_error("project.json has no configuration object");
    return result;
}

const ProjectJson::Json& ProjectJson::scene() const { return objectMember(m_configuration, "scene"); }
const ProjectJson::Json& ProjectJson::light() const { return objectMember(m_configuration, "light"); }
const ProjectJson::Json& ProjectJson::sensor() const { return objectMember(m_configuration, "sensor"); }
const ProjectJson::Json& ProjectJson::control() const { return objectMember(m_configuration, "control"); }
const ProjectJson::Json& ProjectJson::objects() const { return arrayMember(objectMember(m_configuration, "objects"), "items"); }
const ProjectJson::Json& ProjectJson::spectra() const { return arrayMember(m_configuration, "spectra"); }
const ProjectJson::Json& ProjectJson::canopies() const { return arrayMember(m_configuration, "canopies"); }
const ProjectJson::Json& ProjectJson::thermals() const { return arrayMember(m_configuration, "thermals"); }
const ProjectJson::Json& ProjectJson::materials() const { return arrayMember(m_configuration, "materials"); }
const ProjectJson::Json& ProjectJson::meteorology() const { return objectMember(m_configuration, "meteo"); }
const ProjectJson::Json& ProjectJson::atmosphere() const { return objectMember(m_configuration, "atmosphere"); }
const ProjectJson::Json& ProjectJson::fluid() const { return objectMember(m_configuration, "fluid"); }

std::filesystem::path ProjectJson::resolve(const std::string& value) const {
    if (value.empty()) return {};
    const std::filesystem::path path(value);
    if (path.is_absolute()) return path.lexically_normal();
    const std::filesystem::path local = (projectDirectory() / path).lexically_normal();
    if (std::filesystem::exists(local) || path.begin() == path.end() || *path.begin() != "assets")
        return local;
    for (std::filesystem::path parent = executableDirectory(); !parent.empty(); parent = parent.parent_path()) {
        const std::filesystem::path candidate = (parent / path).lexically_normal();
        if (std::filesystem::exists(candidate)) return candidate;
        if (parent == parent.root_path()) break;
    }
    return local;
}

std::filesystem::path ProjectJson::outputDirectory() const {
    return resolve(string(m_configuration, "outDir", "output"));
}

std::filesystem::path ProjectJson::runtimeDefinedDirectory() const {
    return executableDirectory() / "defined";
}

std::filesystem::path ProjectJson::meteorologyPath() const {
    const std::string value = string(meteorology(), "path", "defined/meteo.txt");
    if (value.empty() || value == "defined/meteo.txt" || value == "HiStream 内置气象数据")
        return runtimeDefinedDirectory() / "meteo.txt";
    return resolve(value);
}

std::vector<float> ProjectJson::numbers(const Json& value) {
    std::vector<float> result;
    if (value.is_array()) {
        for (const auto& item : value) if (item.is_number()) result.push_back(item.get<float>());
        return result;
    }
    std::string text;
    if (value.is_string()) text = value.get<std::string>();
    else if (value.is_number()) text = value.dump();
    std::replace(text.begin(), text.end(), ';', ',');
    std::replace(text.begin(), text.end(), ' ', ',');
    std::istringstream input(text);
    std::string token;
    while (std::getline(input, token, ',')) {
        if (token.empty()) continue;
        try {
            const float parsed = std::stof(token);
            if (std::isfinite(parsed)) result.push_back(parsed);
        } catch (...) {}
    }
    return result;
}

std::vector<std::string> ProjectJson::names(const Json& value) {
    std::vector<std::string> result;
    if (value.is_array()) {
        for (const auto& item : value) if (item.is_string()) result.push_back(item.get<std::string>());
        return result;
    }
    if (!value.is_string()) return result;
    std::istringstream input(value.get<std::string>());
    std::string token;
    while (std::getline(input, token, ',')) if (!token.empty()) result.push_back(token);
    return result;
}

std::string ProjectJson::string(const Json& object, const char* key, const std::string& fallback) {
    if (!object.is_object()) return fallback;
    const auto found = object.find(key);
    if (found == object.end()) return fallback;
    if (found->is_string()) return found->get<std::string>();
    return found->is_null() ? fallback : found->dump();
}

float ProjectJson::number(const Json& object, const char* key, float fallback) {
    if (!object.is_object()) return fallback;
    const auto found = object.find(key);
    if (found == object.end()) return fallback;
    try {
        if (found->is_number()) return clean(found->get<float>(), fallback);
        if (found->is_string()) return clean(std::stof(found->get<std::string>()), fallback);
    } catch (...) {}
    return fallback;
}

int ProjectJson::integer(const Json& object, const char* key, int fallback) {
    return static_cast<int>(std::lround(number(object, key, static_cast<float>(fallback))));
}

bool ProjectJson::boolean(const Json& object, const char* key, bool fallback) {
    if (!object.is_object()) return fallback;
    const auto found = object.find(key);
    if (found == object.end()) return fallback;
    if (found->is_boolean()) return found->get<bool>();
    if (found->is_number_integer()) return found->get<int>() != 0;
    if (found->is_string()) return found->get<std::string>() == "true" || found->get<std::string>() == "1";
    return fallback;
}

std::vector<float> ProjectJson::sensorBands() const {
    std::vector<float> values;
    std::set<long long> keys;
    const auto add = [&](float value) {
        if (!(value > 0.0f) || !std::isfinite(value)) return;
        const long long key = std::llround(value * 1000000.0);
        if (keys.insert(key).second) values.push_back(value);
    };
    const std::vector<float> custom = numbers(sensor().contains("bands") ? sensor()["bands"] : Json{});
    if (boolean(sensor(), "continuousBands", false)) {
        const float start = number(sensor(), "bandStart", 400.0f);
        const float end = number(sensor(), "bandEnd", 2500.0f);
        const float step = number(sensor(), "bandStep", 10.0f);
        if (start > 0.0f && end >= start && step > 0.0f) {
            for (int index = 0; index < 800 && start + index * step <= end + 1e-5f; ++index)
                add(start + index * step);
        }
    }
    for (float value : custom) add(value);
    if (values.empty()) values.push_back(10500.0f);
    std::sort(values.begin(), values.end());
    if (values.size() > 800) values.resize(800);
    return values;
}

std::vector<std::array<float, 2>> ProjectJson::viewAngles() const {
    std::vector<std::array<float, 2>> result;
    std::set<std::pair<int, int>> keys;
    const auto add = [&](float vza, float vaa) {
        vza = std::clamp(vza, 0.0f, 89.999f);
        vaa = std::fmod(std::fmod(vaa, 360.0f) + 360.0f, 360.0f);
        const auto key = vza < 1e-6f ? std::pair<int, int>{0, 0}
                                     : std::pair<int, int>{static_cast<int>(std::lround(vza * 10000)), static_cast<int>(std::lround(vaa * 10000))};
        if (keys.insert(key).second) result.push_back({vza, vaa});
    };
    if (string(sensor(), "projection", "parallel") == "perspective") {
        add(number(sensor(), "vza", 0.0f), number(sensor(), "vaa", 0.0f));
        return result;
    }
    const bool principal = boolean(sensor(), "principalPlane", false);
    const bool hemisphere = boolean(sensor(), "hemisphere", false);
    const float solarAzimuth = number(light(), "azimuth", 0.0f);
    if (!principal && !hemisphere) add(number(sensor(), "vza", 0.0f), number(sensor(), "vaa", 0.0f));
    if (principal) for (float offset : {0.0f, 90.0f})
        for (int angle = -75; angle <= 75; angle += 5)
            add(static_cast<float>(std::abs(angle)), solarAzimuth + offset + (angle < 0 ? 180.0f : 0.0f));
    if (hemisphere) {
        const float golden = static_cast<float>(3.14159265358979323846 * (3.0 - std::sqrt(5.0)));
        const float minimumCosine = std::cos(75.0f * 3.14159265358979323846f / 180.0f);
        for (int index = 0; index < 128; ++index) {
            // Match TiRT-EB: sample the VZA 0..75 degree spherical cap, not
            // the complete hemisphere down to the 90-degree horizon.
            const float cosine = 1.0f - (1.0f - minimumCosine)
                * static_cast<float>(index) / 127.0f;
            add(std::acos(cosine) * 180.0f / 3.14159265358979323846f,
                index * golden * 180.0f / 3.14159265358979323846f);
        }
    }
    return result;
}
