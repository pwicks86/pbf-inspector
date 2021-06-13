#include <sstream>
#include <stack>
#include <set>
#include <regex>
#include <fstream>
#include <iterator>
#include <cstddef>
#include <vector>
#include <cstring>
#include <string>
#include "linenoise.h"
#include <nlohmann/json.hpp>
#include <cxxopts.hpp>
#include <osmium/handler.hpp>
#include <osmium/io/any_input.hpp>
#include <osmium/io/writer.hpp>
#include <osmium/osm/node.hpp>
#include <osmium/osm/way.hpp>
#include <osmium/visitor.hpp>
#include <osmium/tags/matcher.hpp>
#include <osmium/tags/tags_filter.hpp>
#include <osmium/memory/buffer.hpp>
#include <iostream>

static const size_t KB = 1024;
static const size_t MB = KB * 1024;
static const size_t GB = MB * 1024;

static const size_t INITIAL_BUFFER_SIZE = 256 * MB;

std::vector<std::string> split(const std::string &in_str, const std::string &delim = " ") {
    std::string str{in_str};
    std::vector<std::string> words{};

    size_t pos = 0;
    while ((pos = str.find(delim)) != std::string::npos) {
        words.push_back(str.substr(0, pos));
        str.erase(0, pos + delim.length());
    }
    if (str != "") {
        words.push_back(str);
    }
    return words;
}

class FilterHandler : public osmium::handler::Handler {
private:
    osmium::TagsFilter filter;
public:
    FilterHandler(osmium::TagsFilter tf) : filter(tf), buf(INITIAL_BUFFER_SIZE, osmium::memory::Buffer::auto_grow::yes) {}
    uint64_t count = 0;
    osmium::memory::Buffer buf;
    void node(const osmium::Node& node) {
        for(auto &tag : node.tags()) {
            if (filter(tag)) {
                count++;
                buf.add_item(node);
                buf.commit();
                break;
            }
        }
    }
};

class PrintHandler : public osmium::handler::Handler {
public:
    void node(const osmium::Node& node) {
        auto location = node.location();
        std::cout << "Node [\n";
        std::cout << "\t" << "location: " << location << "\n";
        std::cout << "\t" << "tags: [\n";
        for(auto &tag : node.tags()) {
            std::cout << "\t\t" << tag.key() << " = " << tag.value() << "\n";
        }
        std::cout << "\t" << "]\n";
        std::cout << "]\n";
    }
};
class KeyGatherHandler : public osmium::handler::Handler {
public:
    std::set<std::string> key_set;
    void node(const osmium::Node& node) {
        for(auto &tag : node.tags()) {
            key_set.insert(tag.key());
        }
    }
};

using json = nlohmann::json;
class GeoJsonHandler : public osmium::handler::Handler {
private:   
    json geojson;
    std::vector<json> features;
public:
    GeoJsonHandler() {
        geojson["type"] = "FeatureCollection";
    }
    void node(const osmium::Node& node) {
        json feature;
        feature["type"] = "Feature";
        for(auto &tag : node.tags()) {
            feature["properties"][tag.key()] = tag.value();
        }
        feature["geometry"]["type"] = "Point";
        feature["geometry"]["coordinates"] = { node.location().lat(), node.location().lon() };
        features.push_back(feature);
    }

    json get_json() {
        geojson["features"] = features;
        return geojson;
    }
};

struct Context {
    Context() : base_file(nullptr) {}
    Context(osmium::io::Reader &reader) : base_file(&reader) {} 
    osmium::io::Reader *base_file;
    std::stack<osmium::memory::Buffer> buffer_stack;
};

void to_lower(std::string &s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c)
                   { return std::tolower(c); });
}

bool ask(std::string prompt) {
    std::string result;
    while(true) {
        std::cout << prompt << " (y/n)?\n";
        std::getline(std::cin, result);
        to_lower(result);
        if (result == "y") {
            return true;
        }
        if (result == "n") {
            return false;
        }
    }
}

bool handle_line_parts(const std::vector<std::string> &parts, Context &ctx) {
    if (parts.size() == 0) {
        return true;
    }
    std::string cmd = parts[0];
    if (cmd == "filter") {
        // Build a tags filter out of the rest
        std::string filter_str = parts[1];
        auto filters = split(filter_str, ";");
        osmium::TagsFilter tf{false};
        for(auto &filter: filters) {
            bool invert = filter[0] == '!';
            std::string to_split = invert ? filter.substr(1, filter.size()) : filter;
            auto filter_parts = split(to_split, "=");
            std::cout << "Invert: " << invert << " ";
            if (filter_parts.size() == 2) {
                // match key and value here
                std::cout << "key = " << filter_parts[0] << ", item = " << filter_parts[1] << "\n";
                tf.add_rule(!invert, osmium::TagMatcher{std::regex(filter_parts[0]), std::regex(filter_parts[1])});
            } else {
                std::cout << "key = " << filter_parts[0] << "\n";
                // Just match key here
                tf.add_rule(!invert, osmium::TagMatcher{filter_parts[0]});
            }
        }
        // Build a handler
        FilterHandler fh{tf};
        if (ctx.buffer_stack.size() == 0) {
            // Then search the file
            osmium::apply(*ctx.base_file, fh);
        } else {
            // Search the last buffer
            osmium::apply(ctx.buffer_stack.top(), fh);
        }
        if (ask(std::string("Found ") + std::to_string(fh.count) + " results, push to stack")) {
            ctx.buffer_stack.push(std::move(fh.buf));
        }
    } else if (cmd == "print") {
        PrintHandler ph;
        if (ctx.buffer_stack.size() == 0) {
            osmium::apply(*ctx.base_file, ph);
        } else {
            osmium::apply(ctx.buffer_stack.top(), ph);
        }
    } else if (cmd == "tagkeys") {
        KeyGatherHandler kgh;
        if (ctx.buffer_stack.size() == 0) {
            osmium::apply(*ctx.base_file, kgh);
        } else {
            osmium::apply(ctx.buffer_stack.top(), kgh);
        }
        for(const std::string &key : kgh.key_set) {
            std::cout << key << ",";
        }
        std::cout << "\n";

    } else if (cmd == "save") {
        // Write the buffer straight to disk
        if (ctx.buffer_stack.size() == 0) {
            std::cout << "No buffer on the stack to write out!\n";
            return true;
        }
        std::string outfile = parts.size() > 1 ? parts[1] : "";
        if (outfile == "") {
            std::cout << "Filename ?\n";
            std::getline(std::cin, outfile);
        }
        auto fp = ::fopen(outfile.c_str(), "w");
        auto &top_buffer = ctx.buffer_stack.top();
        ::fwrite(top_buffer.data(), 1, top_buffer.committed(),fp);
        ::fclose(fp);
    } else if (cmd == "export") {
        std::string outfile;
        std::cout << "name ?\n";
        std::getline(std::cin, outfile);
        std::string outfile_name = outfile + ".geojson";
        GeoJsonHandler gjh;
        if (ctx.buffer_stack.size() == 0) {
            osmium::apply(*ctx.base_file, gjh);
        } else {
            osmium::apply(ctx.buffer_stack.top(), gjh);
        }
        auto json = gjh.get_json();
        std::ofstream of(outfile_name);
        of << json;
    } else if (cmd == "pop") {
        if (ctx.buffer_stack.size() > 1) {
            ctx.buffer_stack.pop();
            std::cout << ctx.buffer_stack.size() << " entries remaining on stack\n";
        } else {
            if (ctx.base_file == nullptr) {
                std::cout << "No file to fall back to!\n";
            } else if (ctx.buffer_stack.size() > 0) {
                ctx.buffer_stack.pop();
                std::cout << ctx.buffer_stack.size() << " entries remaining on stack\n";
            }
        }

    } else if (cmd == "quit") {
        return false;
    } else {
        std::cout << "Invalid cmd\n";
    }    
    return true;
}

void completion(const char *buf, linenoiseCompletions *lc) {
    if (buf[0] == 'f') {
        linenoiseAddCompletion(lc,"filter");
    } else if (buf[0] == 'p') {
        linenoiseAddCompletion(lc,"print");
        linenoiseAddCompletion(lc,"pop");
    } else if (buf[0] == 't') {
        linenoiseAddCompletion(lc,"tagkeys");
    } else if (buf[0] == 's') {
        linenoiseAddCompletion(lc,"save");
    } else if (buf[0] == 'e') {
        linenoiseAddCompletion(lc,"export");
    } else if (buf[0] == 'q') {
        linenoiseAddCompletion(lc,"quit");
    }
}

int main(int argc, char* argv[]) {
    cxxopts::Options options("pbf-inspector", "inspect pbf files");

    options.add_options() 
        ("b,buffer", "buffer file", cxxopts::value<std::string>());
    options.parse_positional({"file"});
    auto result = options.parse(argc, argv);

    auto pos_args = result.arguments();
    std::string path;
    if (pos_args.size() > 0) {
       path = pos_args[0].as<std::string>(); 
    }

    Context ctx;
    if (result.count("buffer")) {
        std::string buffer_file = result["buffer"].as<std::string>();
        FILE *fp = ::fopen(buffer_file.c_str(), "r");
        ::fseek(fp, 0, SEEK_END);
        size_t num_bytes = ::ftell(fp);
        ::fseek(fp, 0, SEEK_SET);
        void *data_ptr = std::malloc(num_bytes);
        ::fread(data_ptr, 1, num_bytes, fp);
        ::fclose(fp);
        osmium::memory::Buffer buf{(uint8_t*)data_ptr, num_bytes};
        ctx.buffer_stack.push(std::move(buf));
    } else if (path != "") {
        auto otypes = osmium::osm_entity_bits::node;
        osmium::io::Reader reader{path, otypes};
        Context temp_ctx{reader};
        ctx = std::move(temp_ctx);
    } else {
        std::cout << "oops" << std::endl;
        return 1;
    }

    char *line;
    linenoiseSetCompletionCallback(completion);
    while((line = linenoise("pbf-inspector> ")) != nullptr) {
        // Split string into parts
        std::string line_str = line;
        auto line_parts = split(line_str);
        if (!handle_line_parts(line_parts, ctx)) {
            break;
        }

        // if (line[0] != '\0' && line[0] != '/') {
        //     printf("echo: '%s'\n", line);
        //     linenoiseHistoryAdd(line); /* Add to the history. */
        //     linenoiseHistorySave("history.txt"); /* Save the history on disk. */
        // } else if (!strncmp(line,"/historylen",11)) {
        //     /* The "/historylen" command will change the history len. */
        //     int len = atoi(line+11);
        //     linenoiseHistorySetMaxLen(len);
        // } else if (!strncmp(line, "/mask", 5)) {
        //     linenoiseMaskModeEnable();
        // } else if (!strncmp(line, "/unmask", 7)) {
        //     linenoiseMaskModeDisable();
        // } else if (line[0] == '/') {
        //     printf("Unreconized command: %s\n", line);
        // }
        free(line);
    }
    return 0;
}