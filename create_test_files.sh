#!/bin/bash

for i in {1..3}; do
    mkdir -p "node${i}_data"
    mkdir -p "node${i}_shared"
done

generate_test_files() {
    local node_num=$1
    local shared_dir="node${node_num}_shared"
    
    echo "This is a test file for node ${node_num}" > "${shared_dir}/test_${node_num}.txt"
    
    dd if=/dev/urandom of="${shared_dir}/random_${node_num}.bin" bs=1M count=10
    
    for i in {1..1000}; do
        echo "Line $i: This is some test content for node ${node_num}" >> "${shared_dir}/large_file_${node_num}.txt"
    done
    
    cat > "${shared_dir}/config_${node_num}.json" << EOF
{
    "nodeId": ${node_num},
    "name": "Test Node ${node_num}",
    "settings": {
        "maxConnections": 100,
        "timeout": 30,
        "bufferSize": 8192
    }
}
EOF
}

for i in {1..3}; do
    generate_test_files $i
done

echo "Test environment initialized successfully!"
echo "Created directories and sample files for 3 nodes:"
for i in {1..3}; do
    echo "Node $i:"
    ls -lh "node${i}_shared/"
done
