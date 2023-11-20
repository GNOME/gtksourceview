defmodule MUD.Server do
  def start(port, iex?) do
    IO.puts("Starting on port #{port}...")
    tcp_options = [:binary, packet: 0, active: false]
    case :gen_tcp.listen(port, tcp_options) do
      {:ok, socket} -> listen(socket)
      {:error, :eaddrinuse} when iex? -> :eaddrinuse # this is OK in iex
    end
  end

  defp listen(socket) do
    {:ok, conn} = :gen_tcp.accept(socket)
    spawn(fn -> recv(conn) end)
    listen(socket)
  end

  def recv(conn) do
    case :gen_tcp.recv(conn, 0) do
      {:ok, data} ->
        :gen_tcp.send(conn, "you said: " <> data)
        recv(conn)

      {:error, :closed} ->
        :ok
    end
  end
end

# in the iex repl, you can run this:
# spawn(fn -> MUD.Server.start(6000, true) end)

if !:erlang.function_exported(IEx, :started?, 0) do
  MUD.Server.start(6001, false)
end
