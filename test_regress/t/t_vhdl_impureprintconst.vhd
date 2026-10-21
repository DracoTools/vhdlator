-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- An array constant of the root architecture from an impure function whose
-- only reach outside itself is a line written to OUTPUT: elaboration gives
-- the constant its value, a generate's bounds read from it, and the line is
-- printed once, as the model starts.

library ieee;
use ieee.std_logic_1164.all;
use std.textio.all;
entity t_vhdl_impureprintconst is
  generic (N : positive := 9; K : positive := 2);
end entity;
architecture a of t_vhdl_impureprintconst is
  type integer_vector is array (natural range <>) of integer;
  impure function compute_blocks return integer_vector is
    variable res : integer_vector(K - 1 downto 0);
    variable l : line;
  begin
    for i in res'range loop
      res(i) := ((i + 1) * (N - K * (K - 1) / 2) + K / 2) / K + (i + 1) * i / 2;
    end loop;
    write(l, string'("blocks "));
    write(l, res(0));
    write(l, ' ');
    write(l, res(1));
    writeline(output, l);
    return res;
  end compute_blocks;
  constant BLOCKS : integer_vector(K - 1 downto 0) := compute_blocks;
  signal v : std_logic_vector(N - 1 downto 0) := (others => '0');
begin
  gen : for i in 1 to K - 1 generate
    v(BLOCKS(i) - 1 downto BLOCKS(i - 1)) <= (others => '1');
  end generate;
  process
  begin
    wait for 1 ns;
    assert BLOCKS(0) = 4 and BLOCKS(1) = 9 severity failure;
    assert v = "111110000" report "v " & to_string(v) severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
