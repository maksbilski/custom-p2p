/**
 * @brief Structure representing resource download request
 *
 * Used for sending requests over TCP to download resources from remote nodes.
 * The structure has a variable-length field (resourceName) which must always
 * be placed at the end of the structure.
 */
#pragma pack(1)
#include <cstdint>
struct ResourceRequest {
  uint32_t messageLength;
  uint32_t resourceNameLength;
  uint64_t offset;
  char resourceName[];
};
#pragma pack()
