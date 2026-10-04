-- DESCRIPTION: Verilator: VHDL generate ranged by a constant an open port's
-- length gives
--
-- An entity's constants worked out from an unconstrained port's 'length
-- range a for generate and decide an if generate (11.8): they are settled
-- when the instance is, as the port's bounds are -- GHDL gna issue1376's
-- window splitter.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

package t_vhdl_genportlen_pkg is
  type bv_array_t is array (natural range <>) of bit_vector;
end package;

use work.t_vhdl_genportlen_pkg.all;

entity t_vhdl_genportlen_sub is
  generic (WINDOW_SIZE : integer; BIT_WIDTH : integer);
  port (i_in : in bv_array_t;
        o_windows : out bv_array_t(0 to 11)(BIT_WIDTH - 1 downto 0);
        o_count : out integer);
  constant IN_LEN : integer := i_in'length;
  constant WINDOW_COUNT : integer := 1 + (IN_LEN - WINDOW_SIZE);
end entity;

architecture comb of t_vhdl_genportlen_sub is
begin
  gen_w : for c in 0 to WINDOW_COUNT - 1 generate
    gen_j : for j in 0 to WINDOW_SIZE - 1 generate
      o_windows(c * WINDOW_SIZE + j) <= i_in(c + j);
    end generate;
  end generate;
  many : if WINDOW_COUNT > 2 generate
    o_count <= WINDOW_COUNT;
  end generate;
  few : if WINDOW_COUNT <= 2 generate
    o_count <= -1;
  end generate;
end architecture;

use work.t_vhdl_genportlen_pkg.all;

entity t_vhdl_genportlen is
end entity;

architecture test of t_vhdl_genportlen is
  signal inp : bv_array_t(0 to 5)(3 downto 0) := (x"1", x"2", x"3", x"4", x"5", x"6");
  signal win : bv_array_t(0 to 11)(3 downto 0);
  signal count : integer;
begin
  u : entity work.t_vhdl_genportlen_sub
    generic map (WINDOW_SIZE => 3, BIT_WIDTH => 4)
    port map (i_in => inp, o_windows => win, o_count => count);

  process is
  begin
    wait for 1 ns;
    assert count = 4 report "count" severity failure;
    -- Windows of three, stepping by one: 123 234 345 456
    assert win(0) = x"1" and win(2) = x"3" and win(3) = x"2" and win(11) = x"6"
      report "windows" severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
