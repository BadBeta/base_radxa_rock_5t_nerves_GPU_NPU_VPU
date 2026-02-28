import Config

Application.start(:nerves_bootstrap)

config :test_app, target: Mix.target()

config :nerves, :firmware, rootfs_overlay: "rootfs_overlay"

config :nerves, source_date_epoch: "1704067200"

if Mix.target() != :host do
  import_config "target.exs"
end
