defmodule TestApp.MixProject do
  use Mix.Project

  @app :test_app
  @version "0.1.0"
  @all_targets [:rock5t]

  def project do
    [
      app: @app,
      version: @version,
      elixir: "~> 1.17",
      archives: [nerves_bootstrap: "~> 1.13"],
      start_permanent: Mix.env() == :prod,
      deps: deps(),
      releases: [{@app, release()}],
      preferred_cli_target: [run: :host, test: :host]
    ]
  end

  def application do
    [
      mod: {TestApp.Application, []},
      extra_applications: [:logger, :runtime_tools, :iex]
    ]
  end

  defp deps do
    [
      {:nerves, "~> 1.10", runtime: false},
      {:shoehorn, "~> 0.9.1"},
      {:ring_logger, "~> 0.11.0"},
      {:toolshed, "~> 0.4.0"},
      {:nerves_runtime, "~> 0.13.0", targets: @all_targets},
      {:nerves_pack, "~> 0.7.1", targets: @all_targets},
      {:nerves_system_rock5t,
       path: "..",
       runtime: false,
       targets: :rock5t,
       nerves: [compile: true]}
    ]
  end

  def release do
    [
      overwrite: true,
      include_erts: &Nerves.Release.erts/0,
      steps: [&Nerves.Release.init/1, :assemble],
      strip_beams: Mix.env() == :prod or [keep: ["Docs"]]
    ]
  end
end
