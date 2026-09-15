#pragma once

#include "QGCMAVLink.h"

/// THACO custom MAVLink command definitions
/// These commands are not in the standard MAVLink dialect but are used by THACO/PX4 integration

// THACO External XYZ mission marker - private command in the 44000 range
// This is a non-position marker command used for external XYZ position handover
#define MAV_CMD_THACO_EXTERNAL_XYZ 44000

// Symbolic enum value for use in C++ code
// Note: This is a macro since we cannot extend the generated MAV_CMD enum
// The numeric value 44000 is used directly in MAVLink messages
static constexpr MAV_CMD MAV_CMD_THACO_EXTERNAL_XYZ_ENUM = static_cast<MAV_CMD>(MAV_CMD_THACO_EXTERNAL_XYZ);