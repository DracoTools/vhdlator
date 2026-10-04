-- DESCRIPTION: Verilator: VHDL record port constraining one element and
-- leaving another open
--
-- "o : out my_rec(v1(open), v2(l - 1 downto 0))": the element left open
-- takes its bounds from the actual, the one constrained keeps what the
-- constraint says (5.3.3, 6.5.6.3) -- GHDL gna bug0136. Two instances give
-- the open element two lengths.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

package t_vhdl_recportopen_pkg is
  type my_rec is record
    v1 : bit_vector;
    v2 : bit_vector;
  end record;
end package;

use work.t_vhdl_recportopen_pkg.all;

entity t_vhdl_recportopen_sub is
  generic (l : natural := 4);
  port (o : out my_rec(v1(open), v2(l - 1 downto 0));
        len1 : out natural);
end entity;

architecture behav of t_vhdl_recportopen_sub is
begin
  o.v2 <= b"0101";
  g : for i in o.v1'range generate
    o.v1(i) <= '1' when i = o.v1'left else '0';
  end generate;
  len1 <= o.v1'length;
end architecture;

use work.t_vhdl_recportopen_pkg.all;

entity t_vhdl_recportopen is
end entity;

architecture behav of t_vhdl_recportopen is
  signal s : my_rec(v1(1 downto 0), v2(4 downto 1));
  signal t : my_rec(v1(5 downto 0), v2(3 downto 0));
  signal ls, lt : natural;
begin
  dut1 : entity work.t_vhdl_recportopen_sub port map (o => s, len1 => ls);
  dut2 : entity work.t_vhdl_recportopen_sub port map (o => t, len1 => lt);

  process is
  begin
    wait for 1 ns;
    assert ls = 2 and lt = 6 report "lengths" severity failure;
    assert s.v1 = "10" and s.v2 = "0101" report "s" severity failure;
    assert t.v1 = "100000" and t.v2 = "0101" report "t" severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
