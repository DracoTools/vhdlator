-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;
use ieee.std_logic_arith.all;
use ieee.std_logic_unsigned.all;

entity t_vhdl_synopsys is
end entity;

architecture a of t_vhdl_synopsys is

  -- A table over the nine values, which is how the Synopsys packages and a
  -- good deal of legacy VHDL write one
  type tbl_t is array (std_ulogic) of std_ulogic;
  constant tbl_binary : tbl_t := ('X', 'X', '0', '1', 'X', 'X', '0', '1', 'X');

begin

  main : process is
    variable x, y, z : std_logic_vector(7 downto 0);
    variable s : std_ulogic;
  begin
    x := conv_std_logic_vector(5, 8);
    y := conv_std_logic_vector(3, 8);
    assert conv_integer(x) = 5 report "x is " & integer'image(conv_integer(x)) severity failure;

    z := x + y;
    assert conv_integer(z) = 8 report "sum is " & integer'image(conv_integer(z)) severity failure;
    z := x - y;
    assert conv_integer(z) = 2 report "difference" severity failure;
    assert conv_integer(x * y) = 15 report "product" severity failure;

    -- An array indexed by a value of a logic type is indexed by the
    -- position of that value, while the value itself is carried as one bit
    s := '1';
    assert tbl_binary(s) = '1' report "table at one" severity failure;
    s := '0';
    assert tbl_binary(s) = '0' report "table at zero" severity failure;

    report "*-* All Finished *-*";
    std.env.finish;
  end process;

end architecture;
