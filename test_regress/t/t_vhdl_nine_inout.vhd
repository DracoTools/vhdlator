-- DESCRIPTION: Verilator: INOUT ports of the nine values, a bus two instances share
--
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

-- An INOUT port drives one source of its actual and reads what the actual
-- resolves to (14.7.3): an open-drain line with a weak pull-up, and a
-- vector bus each instance drives in turn, read back through the ports

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_nine_inout_dev is
  port (sda : inout std_logic;
        bus_io : inout std_logic_vector(3 downto 0);
        pull : in std_logic;
        drive : in boolean;
        val : in std_logic_vector(3 downto 0);
        seen_sda : out std_logic;
        seen_bus : out std_logic_vector(3 downto 0));
end entity;

architecture a of t_vhdl_nine_inout_dev is
begin
  sda <= '0' when pull = '1' else 'Z';
  bus_io <= val when drive else "ZZZZ";
  seen_sda <= sda;
  seen_bus <= bus_io;
end architecture;

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_nine_inout is
end entity;

architecture a of t_vhdl_nine_inout is
  signal sda : std_logic;
  signal bus_io : std_logic_vector(3 downto 0);
  signal pull_a, pull_b : std_logic := '0';
  signal drive_a, drive_b : boolean := false;
  signal val_a : std_logic_vector(3 downto 0) := "1010";
  signal val_b : std_logic_vector(3 downto 0) := "0110";
  signal seen_a, seen_b : std_logic;
  signal bus_a, bus_b : std_logic_vector(3 downto 0);
begin
  sda <= 'H';
  ua : entity work.t_vhdl_nine_inout_dev
    port map (sda => sda, bus_io => bus_io, pull => pull_a, drive => drive_a, val => val_a,
              seen_sda => seen_a, seen_bus => bus_a);
  ub : entity work.t_vhdl_nine_inout_dev
    port map (sda => sda, bus_io => bus_io, pull => pull_b, drive => drive_b, val => val_b,
              seen_sda => seen_b, seen_bus => bus_b);

  process
  begin
    wait for 1 ns;
    report "1: sda=" & std_logic'image(sda) & " seen=" & std_logic'image(seen_a)
      & " bus=" & to_string(bus_io);
    assert sda = 'H' and to_x01(seen_a) = '1' and to_x01(seen_b) = '1' severity failure;
    assert bus_io = "ZZZZ" severity failure;
    pull_b <= '1';
    drive_a <= true;
    wait for 1 ns;
    report "2: sda=" & std_logic'image(sda) & " seen=" & std_logic'image(seen_a)
      & " bus=" & to_string(bus_io) & " read=" & to_string(bus_b);
    assert sda = '0' and seen_a = '0' severity failure;
    assert bus_io = "1010" and bus_b = "1010" severity failure;
    drive_b <= true;
    wait for 1 ns;
    report "3: bus=" & to_string(bus_io);
    assert bus_io = "XX10" severity failure;
    drive_a <= false;
    pull_b <= '0';
    wait for 1 ns;
    report "4: sda=" & std_logic'image(sda) & " bus=" & to_string(bus_io);
    assert sda = 'H' and bus_io = "0110" and bus_a = "0110" severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
