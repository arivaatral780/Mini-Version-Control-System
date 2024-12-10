#include <string>
#include <vector>
using namespace std;
std::string compute_sha1(const std::string& filename);
void write_blob(const std::string& sha, const std::string& filename);
void print_blob_content(const std::string& sha);
size_t get_blob_size(const std::string& sha);
std::string store_tree_structure();
void list_tree_names(const std::string& sha);
void list_tree_details(const std::string& sha);
void stage_file(const std::string& filename);
std::string create_commit(const std::string& message);
void show_commit_log(const string &latest_commit_sha);
void restore_commit(const std::string& sha);
string get_system_name();
