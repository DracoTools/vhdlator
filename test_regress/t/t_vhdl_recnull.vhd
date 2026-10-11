-- DESCRIPTION: Verilator: VHDL recursion over a generic array, down to a
-- null slice
--
-- "img(v(v'low + 1 to v'high))" in the copy made for an array whose length
-- a generic gives: the slice's ends are only known per instance, and the
-- last call's slice is null (5.2.1)
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

package t_vhdl_recnull_pkg is
  function img(v : integer_vector) return string;
end package;

package body t_vhdl_recnull_pkg is
  function img(v : integer_vector) return string is
  begin
    if v'length = 1 then
      return integer'image(v(v'low));
    end if;
    return integer'image(v(v'low)) & "," & img(v(v'low + 1 to v'high));
  end function;
end package body;

use work.t_vhdl_recnull_pkg.all;

entity t_vhdl_recnull_sub is
  generic (dim : positive; w : integer_vector(0 to dim - 1); expect : string);
end entity;

architecture a of t_vhdl_recnull_sub is
begin
  process
  begin
    assert img(w) = expect report "img " & img(w) severity failure;
    wait;
  end process;
end architecture;

use work.t_vhdl_recnull_pkg.all;

entity t_vhdl_recnull is
end entity;

architecture a of t_vhdl_recnull is
  constant c : integer_vector(1 to 4) := (7, 8, 9, 10);
begin
  u3 : entity work.t_vhdl_recnull_sub generic map (dim => 3, w => (3, 4, 5), expect => "3,4,5");
  u1 : entity work.t_vhdl_recnull_sub generic map (dim => 1, w => (0 => 6), expect => "6");

  process
  begin
    wait for 1 ns;
    assert img(c) = "7,8,9,10" report "img " & img(c) severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
