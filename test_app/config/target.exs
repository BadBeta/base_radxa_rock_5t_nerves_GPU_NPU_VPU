import Config

config :shoehorn,
  init: [:nerves_runtime, :nerves_pack],
  app: Mix.Project.config()[:app]

config :nerves_runtime, :kernel, use_system_registry: false

config :logger, backends: [RingLogger]

config :nerves_ssh,
  authorized_keys: [],
  user_passwords: [{"root", "nerves"}]

config :vintage_net,
  regulatory_domain: "00",
  config: [
    {"eth0", %{type: VintageNetEthernet, ipv4: %{method: :dhcp}}},
    {"usb0", %{type: VintageNetDirect}}
  ]

config :mdns_lite,
  hosts: [:hostname, "nerves"]
