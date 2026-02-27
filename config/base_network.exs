# Base network configuration for Rock 5T BSP
#
# Provides default VintageNet setup for eth0 (DHCP) and wlan0 (WiFi).
# WiFi credentials are managed at runtime via the app's Settings module.
#
# Import this from your app's config/target.exs:
#   import_config "../../nerves_system_rock5t/config/base_network.exs"

import Config

config :vintage_net,
  regulatory_domain: "NO",
  config: [
    {"eth0", %{type: VintageNetEthernet, ipv4: %{method: :dhcp}}},
    {"wlan0", %{type: VintageNetWiFi, ipv4: %{method: :dhcp}}}
  ]

config :mdns_lite,
  hosts: [:hostname, "rock5t"],
  ttl: 120,
  services: [
    %{protocol: "ssh", transport: "tcp", port: 22},
    %{protocol: "http", transport: "tcp", port: 80}
  ]
