#include <sys/stat.h>

#include <boost/filesystem.hpp>
#include <boost/functional/hash.hpp>

#include <libexif/exif-data.h>
#include <libexif/exif-tag.h>

#include <cstdio>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <memory>
#include <numeric>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>

namespace fs = boost::filesystem;

auto defer(std::function<void()>&& action) {
    class deferred_action {
    private:
        std::function<void()> _action;
        bool _cancelled = false;
    public:
        deferred_action(std::function<void()>&& action) noexcept
                : _action(std::move(action)) {
        }
        deferred_action(deferred_action&& o) noexcept 
                : _action(std::move(o._action))
                , _cancelled(o._cancelled) {
            o._cancelled = true;
        }
        deferred_action& operator=(deferred_action&& o) noexcept {
            if (this != &o) {
                this->~deferred_action();
                new (this) deferred_action(std::move(o));
            }
            return *this;
        }
        ~deferred_action() {
            if (!_cancelled) {
                _action(); 
            }
        }
    };
    return deferred_action{std::move(action)};
}

static fs::path normalize(fs::path path) {
    return !path.empty() && path.string().at(0) == '~'
            ? std::accumulate(std::next(path.begin()), path.end(), fs::path(getenv("HOME")), fs::operator/)
            : path;
}

static fs::path time_to_path(std::tm& time) {
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d_%H-%M-%S", &time);
    return fs::path(buf);
}

static fs::path time_to_path(char* time_str) {
    std::tm tm;
    std::istringstream ss(time_str);
    ss >> std::get_time(&tm, "%Y:%m:%d %H:%M:%S");    
    return time_to_path(tm);
}

static fs::path new_stem_from_stat(const fs::directory_entry& dir_entry) {
    struct stat st;
    if (stat(dir_entry.path().c_str(), &st) != 0) {
        std::cerr << "Failed to stat " << dir_entry.path() << '\n';
        std::terminate();
    }
    std::tm* time = std::localtime(&st.st_birthtime);
    return time_to_path(*time);
}

static fs::path new_stem_from_exif(const fs::directory_entry& dir_entry) {
    auto* exif_data = exif_data_new_from_file(dir_entry.path().c_str());
    if (!exif_data) {
        return new_stem_from_stat(dir_entry);
    }
    auto free_exif_data = defer([&] { exif_data_unref(exif_data); });

    auto* shot_at = exif_content_get_entry(exif_data->ifd[EXIF_IFD_EXIF], EXIF_TAG_DATE_TIME_ORIGINAL);
    if (!shot_at) {
        return new_stem_from_stat(dir_entry);
    }

    auto buf = std::unique_ptr<char, decltype(free)*>(static_cast<char*>(malloc(sizeof(char) * shot_at->size)), free);
    exif_entry_get_value(shot_at, buf.get(), shot_at->size);
    return time_to_path(buf.get()); 
}

int main(int argc, const char* argv[]) {
    if (argc != 2) {
        std::cout << "Please specify the directory containing the photos to rename" << '\n';
        return 1;
    }

    auto path = fs::system_complete(normalize(fs::path(argv[1])));
    if (!fs::exists(path)) {
        std::cout << "\nNot found: " << path << '\n';
        return 1;
    }

    if (!fs::is_directory(path)) {
        std::cout << "The specified path (" << path << ") is not a directory\n";
        return 1;
    }

    std::cout << "Processing photos in " << path << '\n';

    struct path_hash {
        size_t operator()(const fs::path& p) const noexcept {
            return fs::hash_value(p);
        }
    };
    std::unordered_map<fs::path, size_t, path_hash> new_paths;
    uint32_t num_files = 0;
    for (auto& dir_entry : fs::directory_iterator(path)) {
        if (!fs::is_regular_file(dir_entry.status())) {
            continue;
        }

        auto make_path_from_stem = [] (const fs::path old_path, fs::path stem) { 
            return old_path.parent_path() / stem.replace_extension(old_path.extension());
        };

        auto stem = new_stem_from_exif(dir_entry);
        auto s = stem;
        s += "_1";
        auto new_path = make_path_from_stem(dir_entry.path(), std::move(s));
        
        if (new_path == dir_entry.path()) {
            continue;
        }

        auto [it, inserted] = new_paths.try_emplace(new_path, 1);
        it->second += !inserted || fs::exists(new_path);
        if (it->second > 1) {
            stem += "_" + std::to_string(it->second);
            new_path = make_path_from_stem(new_path, std::move(stem));
        }

        fs::rename(dir_entry.path(), new_path);
        num_files += 1;
    }

    std::cout << "Processed " << num_files << " files\n";
    return 0;
}
