#include "mygit.h"
#include <iostream>
#include <filesystem>
#include <vector>
#include <fstream>       
#include <sys/stat.h>    
#include <openssl/sha.h>
#include <zlib.h>
#include <algorithm>
#include<stack>

using namespace std;
namespace fs = std::filesystem;

void verify_tree_object(const std::string& tree_sha) {
    std::string object_path = ".mygit/objects/" + tree_sha.substr(0, 2) + "/" + tree_sha.substr(2);
    
    // Read compressed data
    std::ifstream file(object_path, std::ios::binary);
    std::string compressed_data((std::istreambuf_iterator<char>(file)),
                               std::istreambuf_iterator<char>());
    
    // Decompress using zlib
    uLongf decompressed_size = compressed_data.length() * 2;  // Estimate size
    std::vector<Bytef> decompressed_data(decompressed_size);
    
    if (uncompress(decompressed_data.data(), &decompressed_size,
                  reinterpret_cast<const Bytef*>(compressed_data.data()),
                  compressed_data.length()) != Z_OK) {
        throw std::runtime_error("Decompression failed");
    }

    // Convert to string for parsing
    std::string content(reinterpret_cast<char*>(decompressed_data.data()), decompressed_size);
    
    // Parse and print tree content
    size_t pos = content.find('\0');
    std::string header = content.substr(0, pos);
    std::string tree_content = content.substr(pos + 1);
    
    std::cout << "Header: " << header << std::endl;
    std::cout << "Tree entries:" << std::endl;
    
    // Parse entries
    size_t i = 0;
    while (i < tree_content.length()) {
        // Find next null byte
        size_t null_pos = tree_content.find('\0', i);
        if (null_pos == std::string::npos) break;
        
        // Extract entry header (mode + filename)
        std::string entry_header = tree_content.substr(i, null_pos - i);
        
        // Extract SHA (20 bytes after null)
        std::string binary_sha = tree_content.substr(null_pos + 1, 20);
        std::string hex_sha;
        for (char c : binary_sha) {
            char hex[3];
            sprintf(hex, "%02x", static_cast<unsigned char>(c));
            hex_sha += hex;
        }
        
        std::cout << entry_header << " " << hex_sha << std::endl;
        
        i = null_pos + 21;  // Move to next entry
    }
}


void checkout_commit(const std::string& sha) {
    restore_commit(sha);  // Restores the commit
}

std::string hash_blob(const std::string &blob_content) {
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char*>(blob_content.c_str()), blob_content.size(), hash);

    std::string sha1;
    for (int i = 0; i < SHA_DIGEST_LENGTH; ++i) {
        char buf[3];
        snprintf(buf, sizeof(buf), "%02x", hash[i]);
        sha1 += buf;
    }
    return sha1;
}


void init_repository() {
    // Create the .mygit directory structure
    mkdir(".mygit", 0755);
    mkdir(".mygit/objects", 0755);
    mkdir(".mygit/refs", 0755);

    // Initialize HEAD pointing to the default branch (master)
    std::ofstream headFile(".mygit/HEAD");
    headFile << "ref: refs/heads/master";
    headFile.close();

    // Optionally create an empty index file
    std::ofstream indexFile(".mygit/index");
    indexFile.close();

    std::cout << "Initialized empty MyGit repository in .mygit/" << std::endl;
}

std::string hex_to_binary(const std::string& hex) {
    std::string binary;
    for (size_t i = 0; i < hex.length(); i += 2) {
        std::string byte = hex.substr(i, 2);
        char chr = static_cast<char>(std::stoul(byte, nullptr, 16));
        binary.push_back(chr);
    }
    return binary;
}

std::string hash_tree_object(const std::string& tree_content) {
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char*>(tree_content.c_str()), 
         tree_content.length(), 
         hash);
    
    std::stringstream ss;
    for (int i = 0; i < SHA_DIGEST_LENGTH; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    }
    return ss.str();
}

std::string compute_file_sha(const fs::path& file_path) {
    // Read file content
    std::ifstream file(file_path, std::ios::binary);
    std::string content((std::istreambuf_iterator<char>(file)), 
                        std::istreambuf_iterator<char>());
    
    // Create blob header
    std::string blob_content = "blob " + std::to_string(content.length()) + '\0' + content;
    
    // Calculate SHA-1
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char*>(blob_content.c_str()),
         blob_content.length(),
         hash);
    
    std::stringstream ss;
    for (int i = 0; i < SHA_DIGEST_LENGTH; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    }
    return ss.str();
}

void update_index(const fs::path& file_path, const std::string& sha1) {
    std::ofstream index_file(".mygit/index", std::ios::app);
    std::string relative_path = fs::relative(file_path, fs::current_path()).string();
    std::string mode = "100644";  // Assuming regular file

    // Write an entry to the index (mode, relative path, SHA-1)
    index_file << mode << " " << relative_path << " " << sha1 << "\n";
}

void verify_index() {
    std::ifstream index_file(".mygit/index",std::ios::binary);
    if (!index_file) {
        std::cerr << "Error: Unable to open index file." << std::endl;
        return;
    }

    std::string line;
    std::cout << "\nCurrent Staging Area (Index):\n";
    while (std::getline(index_file, line)) {
        std::cout << line << std::endl; // Print each entry in the index
    }
    index_file.close();
}


void add_to_stage(const std::vector<std::string>& files) {
std::ofstream index_file(".mygit/index", std::ios::trunc);
    index_file.close();  // Close the file after clearing its contents
    for (const auto& file : files) {
        fs::path file_path = file;
        if (fs::exists(file_path) && fs::is_regular_file(file_path)) {
            std::string sha1 = compute_file_sha(file_path);  // Get SHA-1 of the file
            write_blob(sha1,file_path);  // Write blob to .mygit/objects
            update_index(file_path, sha1);  // Update .mygit/index
        } else if (fs::is_directory(file_path)) {
            for (const auto& entry : fs::recursive_directory_iterator(file_path)) {
                if (fs::is_regular_file(entry.path())) {
                    std::string sha1 = compute_file_sha(entry.path());
                    write_blob(entry.path(), sha1);
                    update_index(sha1,entry.path());
                }
            }
        }
        
        else {
            std::cerr << "Warning: " << file_path << " is not a valid file or directory." << std::endl;
        }
        
    }
    verify_index();
}


std::string write_tree_recursive(const fs::path& root_path, const fs::path& dir_path) {
    std::unordered_map<fs::path, std::string> directory_sha_map;  // To store SHA for each directory
    std::stack<fs::path> dir_stack;  // Stack to process directories

    dir_stack.push(dir_path);

    while (!dir_stack.empty()) {
        fs::path current_dir = dir_stack.top();
        dir_stack.pop();

        std::string current_tree_content;
        std::vector<fs::path> entries;

        // Collect directory entries and sort them
        for (const auto& entry : fs::directory_iterator(current_dir)) {
            if (entry.path().filename() == ".mygit") continue;  // Skip .mygit directory
            entries.push_back(entry.path());
        }
        std::sort(entries.begin(), entries.end());

        // Process each entry in the directory
        for (const auto& entry : entries) {
            std::string mode;
            std::string name = fs::relative(entry, root_path).string();  // Full relative path from root
            std::string sha;

            if (fs::is_regular_file(entry)) {
                mode = "100644";  // Mode for regular files
                sha = compute_file_sha(entry);  // Compute SHA for the file
            } else if (fs::is_directory(entry)) {
                mode = "40000";  // Mode for directories

                // If subdirectory SHA already computed, use it
                if (directory_sha_map.find(entry) != directory_sha_map.end()) {
                    sha = directory_sha_map[entry];
                } else {
                    // Process subdirectory first, then revisit current directory
                    dir_stack.push(current_dir);  // Re-add current directory for later
                    dir_stack.push(entry);  // Process subdirectory first
                    break;  // Recurse to process subdirectory
                }
            } else {
                continue;  // Skip other types (e.g., symlinks, special files)
            }

            // Convert SHA from hex to binary format
            std::string binary_sha = hex_to_binary(sha);

            // Append entry to the current tree's content:
            // <mode> <space> <full relative path> <null> <SHA-1 in binary>
            current_tree_content += mode + " " + name + '\0' + binary_sha;
        }

        // Once the directory has been fully processed, compute its SHA
        if (!current_tree_content.empty()) {
            std::string tree_sha = hash_tree_object(current_tree_content);  // Compute tree SHA

            // Store the tree object in .mygit/objects
            std::string object_dir = ".mygit/objects/" + tree_sha.substr(0, 2);
            std::string object_file = object_dir + "/" + tree_sha.substr(2);

            if (!fs::exists(object_file)) {
                fs::create_directories(object_dir);

                // Prepare the content with header (tree + length) for compression
                std::string full_content = "tree " + std::to_string(current_tree_content.length()) + '\0' + current_tree_content;

                // Compress using zlib
                uLongf compressed_size = compressBound(full_content.length());
                std::vector<Bytef> compressed_data(compressed_size);

                if (compress(compressed_data.data(), &compressed_size,
                             reinterpret_cast<const Bytef*>(full_content.data()),
                             full_content.length()) != Z_OK) {
                    throw std::runtime_error("Compression failed");
                }

                // Write the compressed tree object to file
                std::ofstream outfile(object_file, std::ios::binary);
                outfile.write(reinterpret_cast<const char*>(compressed_data.data()), compressed_size);
            }

            // Save tree SHA for the current directory
            directory_sha_map[current_dir] = tree_sha;
        }
    }

    // Return the SHA for the top-level directory
    return directory_sha_map[dir_path];
}

void write_tree() {
    try {
        std::string tree_sha = write_tree_recursive(".", ".");  // Call with root and current dir as "."
        std::cout << "Tree SHA: " << tree_sha << std::endl;
        
        // Optionally, verify the written tree object
        verify_tree_object(tree_sha);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}




std::string hash_object(const std::string& filename, bool write) {
    // Hash file content using SHA-1, compress it, and return hash
    // If write flag is set, store the file as a blob object
    std::string sha = compute_sha1(filename);
    if (write) {
        write_blob(sha, filename);
    }
    return sha;
}

string get_type(const string &sha){
    // Construct path to the object
    std::string path = ".mygit/objects/" + sha.substr(0, 2) + "/" + sha.substr(2);

    // Open the object file in binary mode
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Error: Cannot open object file.");
    }

    // Read the compressed content into a string
    std::string compressed_content((std::istreambuf_iterator<char>(file)),std::istreambuf_iterator<char>());
    file.close();

    // Prepare for decompression
    uLongf decompressed_size = compressed_content.size() * 10; // Initial guess for decompressed size
    std::vector<char> decompressed_content(decompressed_size);

    int result = uncompress(reinterpret_cast<Bytef*>(decompressed_content.data()), &decompressed_size,
                            reinterpret_cast<const Bytef*>(compressed_content.data()), compressed_content.size());

    // Increase buffer size if decompression buffer was too small
    while (result == Z_BUF_ERROR) {
        decompressed_size *= 2;
        decompressed_content.resize(decompressed_size);
        result = uncompress(reinterpret_cast<Bytef*>(decompressed_content.data()), &decompressed_size,
                            reinterpret_cast<const Bytef*>(compressed_content.data()), compressed_content.size());
    }

    if (result != Z_OK) {
        throw std::runtime_error("Error: Decompression failed.");
    }

    // Resize to actual size after decompression
    decompressed_content.resize(decompressed_size);
    std::string content(decompressed_content.begin(), decompressed_content.end());

    // Parse the object header
    size_t header_end = content.find('\0');
    if (header_end == std::string::npos) {
        throw std::runtime_error("Error: Invalid object format.");
    }
    string object_type;
    // Extract object type (blob or tree) and size
    std::string header = content.substr(0, header_end);
    size_t space_pos = header.find(' ');
    if (space_pos != std::string::npos) {
        object_type = header.substr(0, space_pos); // Set object type
    }
    

    return object_type;
}

void cat_file(const std::string& sha, const std::string& flag) {
    // Read and print file content, size, or type based on SHA
    if (flag == "-p") {
        print_blob_content(sha);
    } else if (flag == "-s") {
        std::cout << "Size: " << get_blob_size(sha) << " bytes\n";
    } else if (flag == "-t") {
        std::cout << "Type: "<< get_type(sha)<<"\n";
    }
}



void ls_tree(const std::string& sha, bool name_only) {
    // List the contents of a tree object
    if (name_only) {
        list_tree_names(sha);
    } else {
        list_tree_details(sha);
    }
}

void add(const std::vector<std::string>& files) {
    // Stage files for the next commit by adding them to the index
    for (const auto& file : files) {
        stage_file(file);
    }
}



std::string get_latest_commit_sha() {
    std::ifstream head_file(".mygit/HEAD",std::ios::binary);
    if (!head_file) {
        std::cerr << "Error: Unable to open .mygit/HEAD file." << std::endl;
        return "";
    }

    std::string head_content;
    std::getline(head_file, head_content);
    head_file.close();
    return head_content;   
}

void log_commits() {
    // Display commit history from the latest to the oldest
    string sha=get_latest_commit_sha();
    show_commit_log(sha);
}




// Function to get current timestamp
std::string get_current_timestamp() {
    std::time_t now = std::time(nullptr);
    return std::ctime(&now);  // Returns a string representation of the current time
}

// Function to read the parent commit SHA (if exists)
std::string get_parent_commit() {
    std::ifstream head_file(".mygit/HEAD",std::ios::binary);
    std::string parent_sha;
    if (head_file) {
        std::getline(head_file, parent_sha);
    }
    head_file.close();
    return parent_sha;
}

// Function to write the new commit SHA to the HEAD
void update_head(const std::string& commit_sha) {
    std::ofstream head_file(".mygit/HEAD");
    head_file << commit_sha;
    head_file.close();
}

// Function to generate the tree object (representing the current staging area)

std::string generate_tree_object() {
    std::ifstream index_file(".mygit/index",std::ios::binary);
    if (!index_file) {
        std::cerr << "Error: Could not open .mygit/index file" << std::endl;
        return "";
    }
    
    std::stringstream tree_content;
    std::string line;

    while (std::getline(index_file, line)) {
        std::istringstream iss(line);
        std::string mode, name, sha;
        
        // Assuming each line is in the format "mode name sha"
        if (!(iss >> mode >> name >> sha)) {
            std::cerr << "Error: Invalid entry in .mygit/index" << std::endl;
            continue;
        }

        // Convert SHA from hex to binary
        std::string binary_sha = hex_to_binary(sha);

        // Append entry to tree content in the specified format
        tree_content << mode << " " << name << '\0' << binary_sha;
    }
    index_file.close();

    std::string tree_data = tree_content.str();
    std::string tree_sha1 = hash_tree_object(tree_data);

    // Compress the tree data using zlib
    uLong source_size = tree_data.size();
    uLongf compressed_size = compressBound(source_size);
    std::vector<char> compressed_data(compressed_size);

    int result = compress(reinterpret_cast<Bytef*>(compressed_data.data()), &compressed_size,
                          reinterpret_cast<const Bytef*>(tree_data.data()), source_size);
    
    if (result != Z_OK) {
        std::cerr << "Error: Compression failed with error code " << result << std::endl;
        return "";
    }

    // Resize vector to the actual size of the compressed data
    compressed_data.resize(compressed_size);

    // Write the compressed tree object to .mygit/objects directory
    std::string dir = ".mygit/objects/" + tree_sha1.substr(0, 2);
    std::string filepath = dir + "/" + tree_sha1.substr(2);

    if (!fs::exists(dir)) {
        fs::create_directories(dir);
    }

    std::ofstream tree_file(filepath, std::ios::binary);
    if (!tree_file) {
        std::cerr << "Error: Could not create file " << filepath << std::endl;
        return "";
    }

    // Write compressed data to the file
    tree_file.write(compressed_data.data(), compressed_data.size());
    tree_file.close();
    
    return tree_sha1;
}


std::string compute_sha1_string(const std::string& input) {
    // Create an array to hold the SHA-1 hash (20 bytes for SHA-1)
    unsigned char hash[SHA_DIGEST_LENGTH];
    
    // Compute SHA-1 hash
    SHA1(reinterpret_cast<const unsigned char*>(input.c_str()), input.size(), hash);

    // Convert hash to a hex string
    std::stringstream ss;
    for (int i = 0; i < SHA_DIGEST_LENGTH; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    }

    return ss.str();  // Return hex string representation of the hash
}

// Function to generate the commit object
std::string create_commit_object(const std::string& tree_sha, const std::string& message) {
    // Get parent commit SHA
    std::string parent_sha = get_parent_commit();

    // Get current timestamp
    std::string timestamp = get_current_timestamp();

    // Construct the commit content
    std::stringstream commit_content;
    string committer=get_system_name();
    commit_content << "tree " << tree_sha << "\n";
    if (!parent_sha.empty()) {
        commit_content << "parent " << parent_sha << "\n";
    }

    commit_content<<"committer "<<committer<<"\n";

    commit_content<<"timestamp "<<timestamp<<"\n";
    
    commit_content << message << "\n";
	
    // Compute the SHA-1 for the commit object
    std::string commit_data = commit_content.str();
    
    std::string commit_sha1 = compute_sha1_string("commit " + std::to_string(commit_data.size()) + '\0' + commit_data);

    commit_content<<"commit sha "<<commit_sha1<<"\n";

    commit_data = commit_content.str();

    // Write the commit object to the objects directory
    std::string dir = ".mygit/objects/" + commit_sha1.substr(0, 2);
    std::string filepath = dir + "/" + commit_sha1.substr(2);

    if (!fs::exists(dir)) {
        fs::create_directories(dir);
    }
	
    std::ofstream commit_file(filepath, std::ios::binary);
    commit_file << commit_data;
    commit_file.close();

    // Update HEAD to the new commit
    update_head(commit_sha1);

    return commit_sha1;
}

// Main commit function
void commit_changes(const std::string& message) {
    // Step 1: Generate the tree object
    std::string tree_sha = generate_tree_object();
    
	
    // Step 2: Create the commit object
    std::string commit_sha = create_commit_object(tree_sha, message);

    // Step 3: Display the new commit SHA
    std::cout << "Commit created: " << commit_sha << std::endl;
}

// Command handling function
void handle_commit(int argc, char* argv[]) {
    std::string commit_message = "Default commit message";  // Default commit message

    if (argc == 3 && std::string(argv[2]) == "-m") {
        if (argc == 4) {
            commit_message = argv[3];  // Use provided message
        } else {
            std::cerr << "Error: No commit message provided after -m flag." << std::endl;
            return;
        }
    }
    }



int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Please provide a valid command.\n";
        return 1;
    }

    std::string command = argv[1];

    if (command == "init") {
        init_repository();
    } else if (command == "hash-object") {
        bool write = (argc > 2 && std::string(argv[2]) == "-w");
        std::string filename = argv[write ? 3 : 2];
        std::string sha = hash_object(filename, write);
        std::cout << sha << "\n";
    } else if (command == "cat-file") {
        if (argc < 4) {
            std::cerr << "Usage: ./mygit cat-file <flag> <file_sha>\n";
            return 1;
        }
        cat_file(argv[3], argv[2]);
    } else if (command == "write-tree") {
        write_tree();
        
    } else if (command == "ls-tree") {
        bool name_only = (argc > 2 && std::string(argv[2]) == "--name-only");
        std::string sha = argv[name_only ? 3 : 2];
        ls_tree(sha, name_only);
    } else if (command == "add") {
        if (argc < 2) {
        std::cerr << "Usage: ./mygit add <files...> or ./mygit add ." << std::endl;
        return 1;
    }

    std::vector<std::string> files;
    
   if (std::string(argv[2]) == ".") {
    auto current_path = fs::current_path();
    for (const auto& entry : fs::recursive_directory_iterator(current_path)) {
        if (fs::is_regular_file(entry.path())) {
            // Convert the absolute path to a relative path
            std::string relative_path = fs::relative(entry.path(), current_path).string();
            files.push_back(relative_path);
            //std::cout << relative_path << std::endl; // Output the relative path
        }
    }
} else {
    // If specific files are provided, use them directly
    for (int i = 2; i < argc; ++i) {
        files.push_back(argv[i]);
    }
}

    add_to_stage(files);
    } else if (command == "commit") {
        std::string message = (argc > 2 && std::string(argv[2]) == "-m") ? argv[3] : "No message";
        commit_changes(message);
       
    } else if (command == "log") {
        log_commits();
    } else if (command == "checkout") {
	
        if (argc < 3) {
            std::cerr << "Usage: ./mygit checkout <commit_sha>\n";
            return 1;
        }
        checkout_commit(argv[2]);
    } else {
        std::cerr << "Unknown command\n";
    }

    return 0;
}

