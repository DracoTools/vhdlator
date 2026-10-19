-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- Each iteration of a for generate is a driver of a resolved signal it drives (11.8, 14.7.2),
-- also where the range is a generic's, which only the instance settles: a carry driven from
-- outside and from every iteration (GHDL gna bug15993), and a bus every iteration may drive.

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_nine_genloopdrv_sub is
  generic (n : integer);
  port (a, b : in std_logic_vector(n - 1 downto 0);
        cin : in std_logic;
        en : in std_logic_vector(n - 1 downto 0);
        co : out std_logic;
        bus_o : out std_logic_vector(7 downto 0));
end entity;

architecture a of t_vhdl_nine_genloopdrv_sub is
  signal carry : std_logic;
  signal bus_s : std_logic_vector(7 downto 0);
begin
  carry <= cin;
  g : for i in 0 to n - 1 generate
    carry <= (a(i) and b(i)) or (carry and (a(i) xor b(i)));
    bus_s <= std_logic_vector(to_unsigned(i + 1, 8)) when en(i) = '1' else (others => 'Z');
  end generate;
  co <= carry;
  bus_o <= bus_s;
end architecture;

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_nine_genloopdrv is
end entity;

architecture a of t_vhdl_nine_genloopdrv is
  signal a, b : std_logic_vector(3 downto 0) := "0000";
  signal cin : std_logic := '0';
  signal en : std_logic_vector(3 downto 0) := "0010";
  signal co : std_logic;
  signal bus_o : std_logic_vector(7 downto 0);
begin
  u : entity work.t_vhdl_nine_genloopdrv_sub
    generic map (n => 4)
    port map (a => a, b => b, cin => cin, en => en, co => co, bus_o => bus_o);
  process is
  begin
    wait for 1 ns;
    report "co " & std_logic'image(co) & " bus " & to_string(bus_o);
    assert co = '0' and bus_o = "00000010" severity failure;
    a <= "0001";
    b <= "0001";
    cin <= '1';
    en <= "0110";
    wait for 1 ns;
    report "co " & std_logic'image(co) & " bus " & to_string(bus_o);
    assert co = 'X' and bus_o = "0000001X" severity failure;
    en <= "0000";
    wait for 1 ns;
    report "co " & std_logic'image(co) & " bus " & to_string(bus_o);
    assert bus_o = "ZZZZZZZZ" severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
