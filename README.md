# P2P Resource Sync

A distributed peer-to-peer system for resource discovery and file sharing across networked nodes.

## Overview

This project implements a complete peer-to-peer file sharing system in C++ with automatic resource discovery, reliable file transfers with resume capability, and network resilience. The system enables nodes to:

- Broadcast available resources over UDP
- Discover resources on other nodes in the network
- Share files via TCP connections
- Resume interrupted downloads
- Manage concurrent transfers

## Features

- **Resource Discovery**: Automatic broadcasting and discovery of shared files
- **Reliable Transfer**: Robust file download with resume capability
- **Concurrent Operations**: Multi-threaded design for simultaneous file sharing
- **Network Resilience**: Handling of dropped connections and network instability
- **Resource Management**: Thread-safe tracking of local and remote resources

## Building and Running

### Prerequisites

- C++23 compatible compiler
- CMake 3.14 or higher
- Docker and Docker Compose (optional, for containerized deployment)

### Build from Source

```bash
# Clone the repository
git clone https://github.com/yourusername/p2p-resource-sync.git
cd p2p-resource-sync

# Build the project
./build.sh

# Build with tests
./build.sh ON
```

### Running with Docker

```bash
# Create test files
./create_test_files.sh

# Start the network with three nodes
docker-compose up -d

# Attach to a node
docker attach p2p-resource-sync_p2p_node1_1
```

## Usage

After starting the application, you'll see a menu with the following options:

```
P2P File Sharing System
1. List local resources
2. List network resources
3. Add local resource
4. Remove local resource
5. Download resource
6. Exit
Enter command:
```

### Adding Resources

To share a file, select option 3 and enter the file path and resource name.

### Downloading Resources

Select option 2 to see resources available on the network, then option 5 to download a file by name. If multiple nodes have the requested resource, you can choose which node to download from.

## Architecture

The system consists of several key components:

- **LocalResourceManager**: Manages resources available locally for sharing
- **RemoteResourceManager**: Tracks resources available from remote nodes
- **AnnouncementBroadcaster**: Periodically broadcasts information about local resources
- **AnnouncementReceiver**: Listens for broadcasted resource announcements
- **TcpServer**: Handles incoming file download requests
- **ResourceDownloader**: Manages downloading resources from remote nodes

## Testing

The system includes comprehensive test coverage with Google Test:

```bash
# Build with tests enabled
./build.sh ON

# Run the tests
cd build && ctest --output-on-failure
```

## Docker Configuration

The project includes a `docker-compose.yml` file that sets up a network with three nodes:

- Each node has its own shared and download directories
- Ports are configured to allow both internal and external connections
- Network simulation includes periodic connection drops to test resilience

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Implementation Details

- Uses C++23 features for modern, efficient code
- Thread safety with shared mutexes
- Socket-based networking with BSD sockets
- Efficient buffered file transfers
- Automatic recovery from network failures
