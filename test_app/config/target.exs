import Config

# Use shoehorn to start the main application
config :shoehorn,
  init: [:nerves_runtime, :nerves_pack],
  app: Mix.Project.config()[:app]

config :nerves,
  erlinit: [
    hostname_pattern: "rock5t-%s",
    uniqueid_exec: "/usr/bin/boardid"
  ]

config :logger,
  backends: [RingLogger],
  level: :info

config :ring_logger,
  max_size: 4096

# SSH configuration for remote access
config :nerves_ssh,
  shell: :elixir,
  authorized_keys: [
    "ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIKEmDYGHTWFic5O1ZmqFX2bU+7qyEVwJSqPP/pcRjZT7 vidar@Hidar"
  ],
  user_passwords: [{"root", "nerves"}],
  fwup_devpath: "/dev/mmcblk0"

# SSH subsystem for firmware updates
config :ssh_subsystem_fwup,
  devpath: "/dev/mmcblk0"

# Networking
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
