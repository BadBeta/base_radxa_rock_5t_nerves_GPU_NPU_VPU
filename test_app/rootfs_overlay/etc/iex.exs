use Toolshed

if RingLogger in Application.get_env(:logger, :backends, []) do
  IO.puts(
    IO.ANSI.color(5) <>
      """

      RingLogger is collecting log messages from prior boot and the current boot.
      To see messages, run `RingLogger.next` or `RingLogger.attach`.
      """ <>
      IO.ANSI.reset()
  )
end

IO.puts("""
Rock 5T Portable BSP Test
Toolshed helpers: cat, tree, ifconfig, ping, reboot, uname, top
""")
