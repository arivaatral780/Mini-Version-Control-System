#include <openssl/sha.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <filesystem>
#include <iostream>
#include <string>
#include <zlib.h>
#include <sys/stat.h>   
#include<vector>
#include<zlib.h>
#include <cstdlib> 
using namespace std;
namespace fs = std::filesystem;

struct Commit {
    std::string sha;
    std::string parent_sha;
    std::string committer;
    std::string timestamp;
    std::string message;
};


std::string get_system_name() {
    #ifdef _WIN32
    const char* username = getenv("USERNAME");
    #else
    const char* username = getenv("USER");
    #endif
    return username ? username : "Unknown User";
}

void write_blob(const std::string& sha, const std::string& filename) {
    // Read file content
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        std::cerr << "Error: Cannot open file" << filename << std::endl;
        return;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    file.close();

    

    // Add blob header (uncompressed content)
    std::string uncompressed_content = "blob " + std::to_string(content.size()) + '\0' + content;

   
    // Compress content using zlib
    uLong source_size = uncompressed_content.size();
    uLongf compressed_size = compressBound(source_size);
    std::vector<char> compressed_content(compressed_size);

    int result = compress(reinterpret_cast<Bytef*>(compressed_content.data()), &compressed_size,
                          reinterpret_cast<const Bytef*>(uncompressed_content.data()), source_size);

    if (result != Z_OK) {
        std::cerr << "Error: Compression failed with error code " << result << std::endl;
        return;
    }

    // Resize compressed content to actual size after compression
    compressed_content.resize(compressed_size);
   

    // Store the object in .mygit/objects/<first two characters of hash>/<remaining characters>
    std::string dir = ".mygit/objects/" + sha.substr(0, 2);
    std::string filepath = dir + "/" + sha.substr(2);

    // Create the subdirectory if it doesn't exist
    if (mkdir(dir.c_str(), 0755) && errno != EEXIST) {
        std::cerr << "Error: Failed to create directory " << dir << std::endl;
        return;
    }

    // Write the compressed content to the blob file
    std::ofstream blob_file(filepath, std::ios::binary);
    if (!blob_file) {
        std::cerr << "Error: Cannot create blob file at " << filepath << std::endl;
        return;
    }

    blob_file.write(compressed_content.data(), compressed_content.size());
    blob_file.close();
    
}


void print_blob_content(const std::string& sha) {
    // Construct the path to the blob file
    std::string filepath = ".mygit/objects/" + sha.substr(0, 2) + "/" + sha.substr(2);

    // Open the compressed blob file
    std::ifstream file(filepath, std::ios::binary);
    if (!file) {
        std::cerr << "Error: Cannot open blob file at " << filepath << std::endl;
        return;
    }

    // Read the compressed content into a string
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string compressed_content = buffer.str();
    file.close();

    
    // Decompress the content using zlib
    uLongf decompressed_size = compressed_content.size() * 10; // Initial guess for decompressed size
    std::vector<char> decompressed_content(decompressed_size); // Vector to hold the decompressed data

    int result = uncompress(reinterpret_cast<Bytef*>(decompressed_content.data()), &decompressed_size,
                            reinterpret_cast<const Bytef*>(compressed_content.data()), compressed_content.size());

    // If the decompression buffer was too small, reattempt with a larger buffer
    while (result == Z_BUF_ERROR) {
        std::cout << "Buffer too small, increasing size and retrying..." << std::endl;
        decompressed_size *= 2;  // Try with double the size
        decompressed_content.resize(decompressed_size);
        result = uncompress(reinterpret_cast<Bytef*>(decompressed_content.data()), &decompressed_size,
                            reinterpret_cast<const Bytef*>(compressed_content.data()), compressed_content.size());
    }

    // Check if the decompression was successful
    if (result != Z_OK) {
        std::cerr << "Error: Decompression failed with error code " << result << std::endl;
        return;
    }


    // Resize the vector to match the actual size of the decompressed content
    decompressed_content.resize(decompressed_size);

    // Print the decompressed content, skipping the "blob <size>\0" header
    std::string blob_content(decompressed_content.begin(), decompressed_content.end());


    size_t header_end = blob_content.find('\0'); // Find the null-terminator of the blob header

    if (header_end != std::string::npos) {
        std::cout <<blob_content.substr(header_end + 1) << std::endl;
    } else {
        std::cerr << "Error: Blob header not found" << std::endl;
    }
}




size_t get_blob_size(const std::string& sha) {
    // Ensure the SHA is at least 2 characters long
    if (sha.length() < 2) {
        std::cerr << "Error: SHA must be at least 2 characters long." << std::endl;
        return 0;
    }

    // Construct the path to the blob file using the first 2 characters of the SHA
    std::string path = ".mygit/objects/" + sha.substr(0, 2) + "/" + sha.substr(2);

    // Open the blob file in binary mode and seek to the end to get the size
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        std::cerr << "Error: Cannot open blob file at " << path << std::endl;
        return 0;
    }

    return file.tellg(); // Returns the size of the file
}



std::string store_tree_structure() {
    // Dummy implementation that would store the tree structure
    std::string tree_sha = "dummy_tree_sha"; // Compute actual tree SHA
    std::ofstream tree_file(".mygit/objects/" + tree_sha);
    // Store the tree structure here
    tree_file.close();
    return tree_sha;
}


void list_tree_names(const std::string& tree_sha) {
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
       
       
        std::istringstream stream(entry_header);
    	std::string word;
    
	    // Skip the first word
	    if (stream >> word) {
		// Get the second word
		if (stream >> word) {
		     // Return the second word
		}
	    }
        std::cout << word<< std::endl;
        
        i = null_pos + 21;  // Move to next entry
    }
}



void list_tree_details(const std::string& tree_sha) {
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
        std::string type;
        if (entry_header.find("100644") != std::string::npos) {
            type = "blob";  // Regular file
        } else if (entry_header.find("40000") != std::string::npos) {
            type = "tree";  // Directory
        } else {
            type = "unknown"; // For modes that are not recognized
        }
        
        std::cout << entry_header << " " << hex_sha <<" "<<type<< std::endl;
        
        i = null_pos + 21;  // Move to next entry
    }
}


void stage_file(const std::string& filename) {
    // Staging the file (this is a dummy implementation)
    std::cout << "Staging file: " << filename << std::endl;
}


std::string create_commit(const std::string& message) {
    // Create commit with a dummy SHA
    std::string commit_sha = "dummy_commit_sha";
    std::ofstream commit_file(".mygit/objects/" + commit_sha);
    commit_file << message << std::endl;
    commit_file.close();
    return commit_sha;
}

bool starts_with(const std::string& str, const std::string& prefix) {
    return str.size() >= prefix.size() && str.compare(0, prefix.size(), prefix) == 0;
}

Commit parse_commit(const std::string& commit_sha) {
    std::string path = ".mygit/objects/" + commit_sha.substr(0, 2) + "/" + commit_sha.substr(2);
    std::ifstream file(path,std::ios::binary);
    Commit commit;
    commit.sha = commit_sha;
    std::string line;

    while (std::getline(file, line)) {
        if (starts_with(line,"parent ")) {
            commit.parent_sha = line.substr(7);
        } else if (starts_with(line,"timestamp ")) {
            commit.timestamp = line.substr(10);
        } else if (starts_with(line,"committer ")) {
            size_t last_space = line.find_last_of(' ');
            commit.committer = line.substr(10, last_space - 10);
        } else if (line.empty()) {
            // The rest is the commit message
            std::getline(file, commit.message);
            break;
        }
    }

    return commit;
}

void show_commit_log(const std::string &latest_commit_sha) {
    std::string current_sha = latest_commit_sha;

    while (!current_sha.empty()) {
        Commit commit = parse_commit(current_sha);

        // Display commit information
        std::cout << "commit " << commit.sha << "\n";
        if (!commit.parent_sha.empty())
            std::cout << "Parent: " << commit.parent_sha << "\n";
        std::cout << "Committer: " << commit.committer << "\n";
        std::cout << "Date: " << commit.timestamp << "\n\n";
        std::cout << "Message: " << commit.message << "\n\n";

        // Move to the parent commit
        current_sha = commit.parent_sha;
    }
}






std::string decompress_file(const std::string& compressed_file_path) {
    std::ifstream file(compressed_file_path, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + compressed_file_path);
    }

    // Read the compressed data into a buffer
    std::string compressed_data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    // Prepare the stream for decompression
    z_stream stream{};
    stream.next_in = reinterpret_cast<Bytef*>(compressed_data.data());
    stream.avail_in = compressed_data.size();

    int window_bits = 15 + 32;  // Window size for gzip headers
    if (inflateInit2(&stream, window_bits) != Z_OK) {
        throw std::runtime_error("Failed to initialize zlib for decompression.");
    }

    std::string decompressed_data;
    char out_buffer[4096];

    int ret;
    do {
        stream.next_out = reinterpret_cast<Bytef*>(out_buffer);
        stream.avail_out = sizeof(out_buffer);

        ret = inflate(&stream, Z_NO_FLUSH);
        if (ret == Z_STREAM_ERROR || ret == Z_DATA_ERROR || ret == Z_MEM_ERROR) {
            inflateEnd(&stream);
            std::ostringstream oss;
            oss << "Failed to decompress data. Zlib error code: " << ret;
            throw std::runtime_error(oss.str());
        }

        decompressed_data.append(out_buffer, sizeof(out_buffer) - stream.avail_out);
    } while (ret != Z_STREAM_END);

    inflateEnd(&stream);

    if (ret != Z_STREAM_END) {
        throw std::runtime_error("Incomplete decompression.");
    }

    return decompressed_data;
}

// The rest of the restore_commit function remains the same

std::string decompress_data(const std::string& compressed_data) {
    // Initial estimate: 4 times the size of compressed data (for highly compressed files)
    uLongf decompressed_size = compressed_data.size() * 4;
    std::vector<Bytef> decompressed_data(decompressed_size);

    int ret = uncompress(decompressed_data.data(), &decompressed_size,
                         reinterpret_cast<const Bytef*>(compressed_data.data()),
                         compressed_data.size());

    // If buffer error, retry with a larger buffer up to 8 times the compressed size
    if (ret == Z_BUF_ERROR) {
        decompressed_size = compressed_data.size() * 8;
        decompressed_data.resize(decompressed_size);
        ret = uncompress(decompressed_data.data(), &decompressed_size,
                         reinterpret_cast<const Bytef*>(compressed_data.data()),
                         compressed_data.size());
    }

    if (ret != Z_OK) {
        throw std::runtime_error("Decompression failed with error code: " + std::to_string(ret));
    }

    // Resize to actual decompressed size in case it was smaller
    return std::string(decompressed_data.begin(), decompressed_data.begin() + decompressed_size);
}


// Function to create a file from entry header and SHA hash
void create_file(const std::string& entry_header, const std::string& hex_sha) {
    // Split entry_header to get the mode and relative path
    std::istringstream iss(entry_header);
    std::string mode, relative_path;
    iss >> mode >> relative_path;

    // Skip files that should not be restored
    if (mode != "100644" || relative_path.rfind(".mygit", 0) == 0 ||
        relative_path == "mygit.o" || relative_path == "utils.o" ||
        relative_path == ".vscode" || relative_path == "Makefile" || relative_path == "utils.cpp" || relative_path=="mygit" || relative_path=="mygit.cpp" || relative_path=="mygit.h") {
        return;
    }

    // Create necessary subdirectories for the relative path
    fs::path file_path = relative_path;
    if (file_path.has_parent_path()) {
        fs::create_directories(file_path.parent_path());

    }

    // Locate the blob file using hex_sha
    std::string blob_file_path = ".mygit/objects/" + hex_sha.substr(0, 2) + "/" + hex_sha.substr(2);
    if (!fs::exists(blob_file_path)) {
        throw std::runtime_error("Blob not found for SHA: " + hex_sha);
    }

    // Read the compressed blob data
    std::ifstream blob_file(blob_file_path, std::ios::binary);
    std::string compressed_data((std::istreambuf_iterator<char>(blob_file)), std::istreambuf_iterator<char>());
    blob_file.close();

    // Decompress the blob data
    std::string file_content = decompress_data(compressed_data);
     size_t pos = 0;
    while ((pos = file_content.find("blob ", pos)) == 0) {
        size_t null_pos = file_content.find('\0', pos);
        if (null_pos == std::string::npos) {
            throw std::runtime_error("Invalid blob header format");
        }
        file_content = file_content.substr(null_pos + 1);
    }
    cout<<file_content<<endl;
    // Open file in binary mode and write decompressed content to the target file
    std::ofstream output_file(relative_path, std::ios::binary);
    if (!output_file.is_open()) {
        throw std::runtime_error("Failed to create file: " + relative_path);
    }
    
    output_file.write(file_content.data(), file_content.size());
    output_file.close();

    std::cout << "Restored file: " << relative_path << std::endl;
}

// Function to restore the repository state to a specific commit
void restore_commit(const std::string& sha) {

    std::string commit_file = ".mygit/objects/" + sha.substr(0, 2) + "/" + sha.substr(2);
    if (!fs::exists(commit_file)) {
        throw std::runtime_error("Commit not found: " + sha);
    }

    // Open and read the commit file as plain text
    std::ifstream file(commit_file,std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open commit file: " + commit_file);
    }
    std::string tree_sha;
    std::getline(file, tree_sha);
    tree_sha = tree_sha.substr(5);  // Assuming the first line is "tree <sha>"

    std::string object_path = ".mygit/objects/" + tree_sha.substr(0, 2) + "/" + tree_sha.substr(2);
    
    // Read compressed data
    std::ifstream file1(object_path, std::ios::binary);
    std::string compressed_data((std::istreambuf_iterator<char>(file1)),
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
        
        
        create_file(entry_header,hex_sha);
        
        i = null_pos + 21;  // Move to next entry
    }


    std::cout << "Repository restored to commit " << sha << std::endl;
}



std::string compute_sha1(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + filename);
    }

    SHA_CTX sha_ctx;
    SHA1_Init(&sha_ctx);

    char buffer[8192];
    while (file.read(buffer, sizeof(buffer))) {
        SHA1_Update(&sha_ctx, buffer, file.gcount());
    }
    if (file.gcount() > 0) {
        SHA1_Update(&sha_ctx, buffer, file.gcount());
    }

    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1_Final(hash, &sha_ctx);

    std::stringstream ss;
    for (int i = 0; i < SHA_DIGEST_LENGTH; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }
    return ss.str();
}

