-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- What a generate body declares stays in it, and what the architecture
-- declares stays the architecture's however a generate first reads it:
-- a branch's own width constants size a RESIZE of a product in a branch
-- one instance elaborates and another does not; and a constant of the
-- architecture compared in two for-generates (as neorv32's multiplier and
-- PMP are written)

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_genscope_mul is
  generic (W : natural; SPLIT : boolean);
  port (a, b : in signed(W - 1 downto 0); p : out signed(2 * W - 1 downto 0));
end entity;

architecture a of t_vhdl_genscope_mul is
begin
  whole : if not SPLIT generate
    p <= a * b;
  end generate;

  halves : if SPLIT generate
    constant lo_c : natural := W / 2;
    constant pw_c : natural := 2 * W;
    signal lo_prod : signed(pw_c - 1 downto 0);
  begin
    lo_prod <= resize(signed('0' & std_ulogic_vector(a(lo_c - 1 downto 0))) * b, pw_c);
    p <= lo_prod + shift_left(resize(a(W - 1 downto lo_c) * b, pw_c), lo_c);
  end generate;
end architecture;

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_genscope is
end entity;

architecture a of t_vhdl_genscope is
  constant mode_c : std_ulogic_vector(1 downto 0) := "01";
  type cfg_t is array (0 to 3) of std_ulogic_vector(1 downto 0);
  signal cfg : cfg_t := ("01", "10", "01", "11");
  signal hit_a, hit_b : std_ulogic_vector(0 to 3);
  signal a, b : signed(7 downto 0) := to_signed(-37, 8);
  signal p1, p2 : signed(15 downto 0);
begin
  m1 : entity work.t_vhdl_genscope_mul generic map (8, false) port map (a, b, p1);
  m2 : entity work.t_vhdl_genscope_mul generic map (8, true) port map (a, b, p2);

  ga : for i in 0 to 3 generate
    hit_a(i) <= '1' when cfg(i) = mode_c else '0';
  end generate;
  gb : for i in 0 to 3 generate
    hit_b(i) <= '1' when cfg(i) /= mode_c else '0';
  end generate;

  process
  begin
    wait for 1 ns;
    report to_string(hit_a) & " " & to_string(hit_b) & " " & integer'image(to_integer(p1))
      & " " & integer'image(to_integer(p2));
    assert hit_a = "1010" and hit_b = "0101" severity failure;
    assert to_integer(p1) = 1369 and to_integer(p2) = 1369 severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
