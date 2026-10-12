# CC Telemetry MAVLink mapping

## Audited source

QGC custom configuration selects:

- Repository: `https://github.com/ThongTruong24/agridrone-mavlink.git`
- Configured revision: `main`
- Resolved local commit during this audit: `4e7fe193d5408d1c3e3c7ca8889645cc50ac8eec`
- Dialect definition: `.cache/CPM/mavlink/2831/message_definitions/v1.0/thaco.xml`
- Selected aggregate dialect: `all.xml`, which includes `thaco.xml`

The configured repository and dialect live in `custom/cmake/CustomOverrides.cmake`. The four IDs also appear in the generated Desktop header under `_deps/mavlink-build/include/mavlink/all/all.h`.

## CC_TELEMETRY_LINKS (ID 42010)

| MAVLink field | UI label | Tab / card | Treatment |
|---|---|---|---|
| `fc_baudrate` | Baudrate | Links / FC Link | Decimal plus `baud` |
| `siyi_baudrate` | Baudrate | Links / SIYI Link | Decimal plus `baud` |
| `fc_bytes_rx` | FC Bytes RX | Links / metric | Exact counter |
| `fc_bytes_tx` | FC Bytes TX | Links / metric | Exact counter |
| `fc_bitrate_kbps` | Live Bitrate | Links / metric | One decimal, kbps |
| `link_status_flags` bit 0 | Connected, FC connected | Links / FC Link and Link Flags | Bit test |
| `link_status_flags` bit 1 | Connected, SIYI connected | Links / SIYI Link and Link Flags | Bit test |
| `link_status_flags` | Raw value | Links / Link Flags | Hex badge |
| `fc_port` | Port | Links / FC Link | Exact string |
| `siyi_port` | Port | Links / SIYI Link | Exact string |

## CC_TELEMETRY_CAMERA (ID 42011)

| MAVLink field | UI label | Tab / card | Treatment |
|---|---|---|---|
| `video_width` | RGB Stream | Camera / RGB Stream | Combined with height |
| `video_height` | RGB Stream | Camera / RGB Stream | Combined with width |
| `video_fps` | RGB Stream FPS | Camera / RGB Stream | fps suffix |
| `depth_width` | Depth Stream | Camera / Depth Stream | Combined with height |
| `depth_height` | Depth Stream | Camera / Depth Stream | Combined with width |
| `depth_fps` | Depth Stream FPS | Camera / Depth Stream | fps suffix |
| `rotation` | Rotation | Camera / Stream Settings | Degree suffix |
| `profile_mode` | Profile Mode | Camera / Stream Settings | 0 RGB only; 1 RGB + depth; 2 RGB + depth + point cloud |
| `enable_emitter` | Emitter | Camera / Stream Settings | 0 Disabled; nonzero Enabled |
| `camera_type` | Camera | Camera / Camera | Exact driver/type string; the dialect has no separate friendly camera-name field |
| `serial_number` | Serial | Camera / Camera | Exact string |
| `codec` | Codec | Camera / Encoder | Uppercase display only |
| `encoder_mode` | Mode | Camera / Encoder | Exact string |
| `bitrate_kbps` | Bitrate | Camera / Encoder | kbps suffix |
| `bitrate_max_kbps` | Max Bitrate | Camera / Encoder | kbps suffix |
| `vbv_buffer_kb` | VBV Buffer | Camera / Encoder | kb suffix |
| `rtsp_url` | URL | Camera / RTSP | Read-only selectable field with copy button |
| No protocol field | Live status | Camera / RTSP | **QGC displays N/A.** Preview alone derives a visual state from non-empty `rtsp_url` |

## CC_TELEMETRY_NETWORK (ID 42012)

| MAVLink field | UI label | Tab / card | Treatment |
|---|---|---|---|
| `ap_channel` | Channel | Network / Access Point Settings | Exact integer |
| `ap_ieee80211n` | 802.11n | Network / Access Point Settings | 0 Disabled; nonzero Enabled |
| `ap_wmm_enabled` | WMM | Network / Access Point Settings | 0 Disabled; nonzero Enabled |
| `ap_wpa` | WPA | Network / Access Point Settings | Value 2 displayed as WPA2 per dialect description |
| `ap_client_count` | Clients | Network / metric | Exact count |
| `wlan0_dhcp` | Wi-Fi Client DHCP | Network / Security | 0 Inactive; nonzero Active |
| `dnsmasq_status` | DHCP, dnsmasq status | Network / DHCP and Security | 0 Inactive; nonzero Active |
| `eth0_ip` | eth0 | Network / IP Addresses | Exact string |
| `wlan0_ip` | wlan0 | Network / IP Addresses | Exact string |
| `ap_ip` | uap0 | Network / IP Addresses | Friendly interface label from field description |
| `ap_ssid` | Access Point | Network / Access Point | Exact SSID |
| `ap_wpa_passphrase` | Passphrase | Network / Security | Masked; value is never shown in clear text |
| `ap_key_mgmt` | Key Mgmt | Network / Access Point Settings | Exact string |
| `ap_hw_mode` | Mode | Network / Access Point Settings | Exact string |
| No protocol field | AP online status | Network / Access Point | **QGC displays N/A.** Preview alone derives a visual state from SSID + dnsmasq |

## CC_TELEMETRY_VISION (ID 42013)

| MAVLink field | UI label | Tab / card | Treatment |
|---|---|---|---|
| `confidence_thresh` | Confidence Threshold | Vision / Runtime | Two decimals |
| `inference_fps` | Inference FPS | Vision / metric | One decimal, fps |
| `input_width` | Input Size | Vision / Model & Input | Combined with height |
| `input_height` | Input Size | Vision / Model & Input | Combined with width |
| `video_fps` | Camera FPS | Vision / Model & Input | fps suffix |
| `detections_count` | Detections | Vision / metric | Exact count |
| `status_flags` bit 0 | Running | Vision / Pipeline and Status Flags | Bit test |
| `status_flags` bit 1 | Depth Enabled | Vision / Runtime and Status Flags | Bit test |
| `status_flags` | Status Flags | Vision / Runtime | Hex display |
| `model_name` | Model | Vision / Model & Input | Exact string |
| `input_source` | Source | Vision / Model & Input | Exact string |

No telemetry labels in the four tabs require an invented mock field. `Live` and `Online` have no corresponding protocol fields, so the integrated QGC page reports `N/A`. Their previous derived behavior is retained only when `CcTelemetryAdapter.previewMode` is true. Before the first message, each tab shows `NO DATA`; after five seconds without a new message it shows `STALE` while retaining the last received values.
