-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A port of an array of arrays may be partly constrained (5.3.2.2): the index
-- open and the element given, "a(open)(3 downto 0)", or the index given and
-- the element open, "a(1 downto 0)". The open part comes from the actual
-- (6.5.6.3), so each instance is elaborated with the bounds it was handed.

package t_vhdl_partialport_pkg is
  type bit_vec_vec is array (natural range <>) of bit_vector;
end package;

use work.t_vhdl_partialport_pkg.all;
entity t_vhdl_partialport_idx is
  port (i : in bit_vec_vec(open)(3 downto 0);
        o : out bit_vec_vec(open)(3 downto 0);
        n : out natural);
end entity;

architecture behav of t_vhdl_partialport_idx is
begin
  process (i) begin
    for j in i'range loop
      o(j) <= not i(j);
    end loop;
  end process;
  n <= i'length;
end architecture;

use work.t_vhdl_partialport_pkg.all;
entity t_vhdl_partialport_elem is
  port (i : in bit_vec_vec(1 downto 0);
        o : out bit_vec_vec(1 downto 0);
        n : out natural);
end entity;

architecture behav of t_vhdl_partialport_elem is
begin
  process (i) begin
    for j in i'range loop
      o(j) <= i(j) xor (i(j)'range => '1');
    end loop;
  end process;
  n <= i(1)'length;
end architecture;

use work.t_vhdl_partialport_pkg.all;
entity t_vhdl_partialport is
end entity;

architecture behav of t_vhdl_partialport is
  signal a_i, a_o : bit_vec_vec(1 downto 0)(3 downto 0);
  signal b_i, b_o : bit_vec_vec(2 to 4)(3 downto 0);
  signal c_i, c_o : bit_vec_vec(1 downto 0)(7 downto 0);
  signal an, bn, cn : natural;
begin
  ia : entity work.t_vhdl_partialport_idx port map (i => a_i, o => a_o, n => an);
  ib : entity work.t_vhdl_partialport_idx port map (i => b_i, o => b_o, n => bn);
  ic : entity work.t_vhdl_partialport_elem port map (i => c_i, o => c_o, n => cn);

  process begin
    a_i <= (x"3", x"c");
    b_i <= (x"1", x"2", x"4");
    c_i <= (x"0f", x"a5");
    wait for 1 ns;
    assert a_o(1) = x"c" and a_o(0) = x"3" severity failure;
    assert b_o(2) = x"e" and b_o(3) = x"d" and b_o(4) = x"b" severity failure;
    assert c_o(1) = x"f0" and c_o(0) = x"5a" severity failure;
    assert an = 2 and bn = 3 and cn = 8 severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
