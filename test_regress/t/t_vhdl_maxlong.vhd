-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- MAXIMUM and MINIMUM over a long array, and a record subelement resolved by
-- a function that takes the MAXIMUM of its drivers, forty of them. Lowered as
-- one conditional over the elements, nested in itself at each one, the copy
-- of the function for forty drivers was two to the fortieth nodes.

library ieee;
use ieee.std_logic_1164.all;
package t_vhdl_maxlong_pkg is
  function rmax(s : integer_vector) return integer;
  subtype int_max is rmax integer;
  type rec_t is record
    Count : int_max;
    Level : int_max;
  end record;
end package;
package body t_vhdl_maxlong_pkg is
  function rmax(s : integer_vector) return integer is
  begin
    if s'length = 0 then
      return integer'low;
    end if;
    return maximum(s);
  end function;
end package body;

use work.t_vhdl_maxlong_pkg.all;
entity t_vhdl_maxlong is end;
architecture t of t_vhdl_maxlong is
  constant N : integer := 40;
  signal r : rec_t;
  signal v : integer_vector(1 to N);
begin
  g : for i in 1 to N generate
    r.Count <= i * 3 mod 50;
    r.Level <= 100 - i;
  end generate;
  process
    variable w : integer_vector(1 to N);
  begin
    for i in w'range loop
      w(i) := (i * 7) mod 31 - 15;
    end loop;
    v <= w;
    wait for 1 ns;
    report "max=" & integer'image(maximum(w)) & " min=" & integer'image(minimum(w))
      & " smax=" & integer'image(maximum(v)) & " r=" & integer'image(r.Count) & " "
      & integer'image(r.Level);
    assert maximum(w) = 15 and minimum(w) = -15 and maximum(v) = 15 and minimum(v) = -15
      severity failure;
    assert r.Count = 49 and r.Level = 99 severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end;
