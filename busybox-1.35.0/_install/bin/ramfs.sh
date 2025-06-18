#!/bin/sh

# Automated Test Script for ramfs FUSE filesystem
#
# This script should be run inside the QEMU/BusyBox environment
# after ramfs has been mounted on /mnt/myramfs.

# --- Configuration ---
MOUNT_POINT="/mnt/ramfs"
TEST_FAILED=0

# --- Helper Functions ---
# Function to print a test header
print_header() {
    echo "\n================================================="
    echo "  TEST: $1"
    echo "================================================="
}

# Function to check if a command succeeded
check_success() {
    # $? is the exit code of the last command
    if [ $? -ne 0 ]; then
        echo "  [FAIL] $1"
        TEST_FAILED=1
    else
        echo "  [PASS] $1"
    fi
}

# Function to check if a command failed (as expected)
check_failure() {
    if [ $? -eq 0 ]; then
        echo "  [FAIL] $1 (Command was expected to fail but succeeded)"
        TEST_FAILED=1
    else
        echo "  [PASS] $1 (Command failed as expected)"
    fi
}


# --- Main Test Execution ---

# 0. Pre-Test Check
print_header "TEST2"
cd "$MOUNT_POINT"
cd $MOUNT_POINT
echo "  Successfully changed to $MOUNT_POINT"
mkdir -p link-stress
echo "Initial test content" > /mnt/ramfs/link-stress/original_file
LINKCOUNT=1000

for i in $(seq 1 $LINKCOUNT); do
    ln /mnt/ramfs/link-stress/original_file /mnt/ramfs/link-stress/link_$i
done

echo "Created $LINKCOUNT hard links."

ls -l /mnt/ramfs/link-stress/original_file

for i in $(seq 1 $LINKCOUNT); do
    rm /mnt/ramfs/link-stress/link_$i
done

echo "Removed $LINKCOUNT hard links."

ls -l /mnt/ramfs/link-stress/original_file
rm /mnt/ramfs/link-stress/original_file

print_header "TEST3"
DEPTH=100
BASE_DIR="${MOUNT_POINT}/deep_test"
mkdir -p deep_test
current_path=$BASE_DIR

for i in $(seq 1 $DEPTH); do
    current_path="$current_path/dir_$i"
    mkdir -p $current_path
    echo "Test at depth $i" > "$current_path/file_$i.txt"
done

echo "Deep nested directories of depth $DEPTH created."

# Verify
find $BASE_DIR -type f | wc -l

print_header "TEST4"
SMALL_FILES_DIR="${MOUNT_POINT}/small_files_test"
FILE_COUNT=100

mkdir -p small_files_test

for i in $(seq 1 $FILE_COUNT); do
    echo "small data $i" > "$SMALL_FILES_DIR/small$i.txt"
done

echo "Created $FILE_COUNT small files."

ls $SMALL_FILES_DIR | wc -l

for i in $(seq 1 $FILE_COUNT); do
    rm "$SMALL_FILES_DIR/small$i.txt"
done

echo "Removed $FILE_COUNT small files."

ls $SMALL_FILES_DIR | wc -l

