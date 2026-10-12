#pragma once

#include "QGCMAVLink.h"

/// MAV_CMD_THACO_EXTERNAL_XYZ (44000) is generated from thaco_common.xml; this alias keeps
/// existing call sites (SimpleMissionItem.cc, CustomFirmwarePlugin.cc) unchanged.
static constexpr MAV_CMD MAV_CMD_THACO_EXTERNAL_XYZ_ENUM = MAV_CMD_THACO_EXTERNAL_XYZ;
