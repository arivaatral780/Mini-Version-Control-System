Requirements:
    - C++17: Required for filesystem operations and certain C++17 features.
    - zlib: Essential for file compression and decompression.

Code Structure:
    -mygit.cpp: Entry point for MyGit, handling command parsing and function calls.Core functions for init, add, commit, and checkout functionalities.
    -utils.cpp: Helper functions for file and directory handling, compression, and decompression.
    -.mygit Directory Structure:
        objects: Stores compressed blobs of committed files.
        HEAD: Points to the current commit.
        
        
Features Implemented:
-Initialize Repository (init)
Creates a .mygit directory to store objects, metadata, and references for version control.

-Hash-Object (hash-object)
Calculates the SHA-1 hash of a file and, with the -w flag, writes the file as a blob object into the repository.

-Cat-File (cat-file)
Displays information about a stored object, such as its content (-p), size (-s), or type (-t).

-Write Tree (write-tree)
Captures the current directory's structure as a tree object and stores it in the repository.

-List Tree (ls-tree)
Lists the contents of a tree object (directory) by its SHA-1 hash, displaying either detailed info or just the names of files and subdirectories.

-Add Files (add)
Stages files or directories for the next commit.

-Commit Changes (commit)
Creates a commit object representing a snapshot of the staged changes. Updates the repository’s history and points the HEAD to the new commit.

-Log (log)
Displays the commit history, including commit SHA, parent SHA, commit message, timestamp, and committer info.

-Checkout (checkout)
Restores the project to a specific commit, effectively checking out the repository to a given commit’s state.


Compilation & Execution:
-Enter "make" in the terminal in the project directory.
-This will compile all source files and generate the mygit executable.

Running the Commands:
-To run any of the commands, use the following syntax:
    ./mygit <command> <options>

Example Commands:
    Initialize a new repository:
        ./mygit init

    Calculate and hash a file:
        ./mygit hash-object -w test.txt

    View a stored file:
        ./mygit cat-file -p <file_sha>

    Stage all files in the current directory:
        ./mygit add .

    Commit the changes with a message:
        ./mygit commit -m "Initial commit"

    View commit history:
        ./mygit log

    Checkout a specific commit:
        ./mygit checkout <commit_sha>
    

Implementation logic of following commands in code:
- ./mygit init:
    -Create .mygit directory.
    -Inside ./mygit directory create a directory called objects which stores tree and blob objects.
    -Create HEAD file which stores the hash value of latest commmit.
    -Create index file which the staging area.

- ./mygit hash-object [-w] <file_name>:
    -Compute SHA1 value for the given file which is of 40 bytes.
    -If -w flag is set then, create the directory name inside objects folder with first 2 characters of SHA1 value,go inside the created directory and store a compressed format of the file with the last 38 characters of SHA1 value.
    -compression is done using zlib.
    -Stores the compression file in the format <object type> <file size> <fiel content>

-  ./mygit cat-file <flag> <file_sha>:
    -If flag is p: Print the actual content of the file.
    -Using file_sha retieves the compressed file from objects directory and decompresses the file to extract the contents of the file. 
    -If flag is s: Display the file size in bytes.
    -Using file_sha retieves the compressed file from objects directory and decompresses the file to extract the size of the file. 
    -If flag is t: Show the type of object.
    -Using file_sha retieves the compressed file from objects directory and decompresses the file to extract the object type of the file. 

- ./mygit write-tree:
    -Recursively parse through the directories and store the metadata as well as its hash value in the required format for accessing in future.


- ./mygit ls-tree [--name-only] <tree_sha>:
    -Using tree_sha extract the compressed file from objects directory and decompress to get the metada of the tree that contains all metadata about the files and directories present in the tree.

- ./mygit add .:
    -Add all the file into the staging area.
    -Hash and write the directory and file the tree into objects folder.
    -Store in index file(staging area) in the format <mode> <path> <sha>.

- ./mygit commit -m "Commit message" (or) ./mygit commit:
    -Return SHA for the commit.
    -Commit stores info sucha as tree_sha of staging area, parent_sha, timestamp of commit,committer_name, commit_sha.
    -Stores the commit_sha in objects folder and the data of the commits in it.
    -Update the HEAD file to the curent commit SHA. 

- ./mygit log:
    -Display commit history from the latest to the oldest.
    -It recursively travels from on commit_sha to its parent from the metadata parent_commit_sha stored in current commit_sha until intial commit_sha is reached.

- ./mygit checkout <commit_sha>:
    -Using commit_sha retrieve the tree_sha from its directory in objects.
    -Using tree_sha retrieve the tree_structure files from the object and write it into the home directory.
    -Before these process remove the already existing files and folder except .mygit folder.












