-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- A function nested in a procedure, reading an alias the procedure declares
-- of an element of one of its parameters: UVVM's this_vvc_completed and
-- "alias wanted_idx : integer is command.gen_integer_array(0)"
-- (uvvm_vvc_framework/src_target_dependent/td_vvc_entity_support_pkg.vhd).
-- The nested function is passed what it reaches out of its own region for,
-- but an alias has no object of its own and was not followed to the one it
-- denotes, so the function read the procedure's parameter directly. With
-- the procedure copied per bound signal into two entities, that was a
-- reference into another copy, and Verilator stopped with an internal error.
library ieee;
use ieee.std_logic_1164.all;

package t_vhdl_nestalias_pkg is
  type t_cmd is record
    gen_integer_array : integer_vector(0 to 1);
    name : string(1 to 4);
  end record;
  procedure interp(constant command : in t_cmd; signal busy : in boolean;
                   signal last_idx : in natural; constant expect : in boolean);
end package;

package body t_vhdl_nestalias_pkg is
  procedure interp(constant command : in t_cmd; signal busy : in boolean;
                   signal last_idx : in natural; constant expect : in boolean) is
    alias wanted_idx : integer is command.gen_integer_array(0);
    alias limit : integer is wanted_idx;
    impure function completed(dummy : integer) return boolean is
    begin
      if wanted_idx = -1 then
        return busy = false;
      else
        return last_idx >= limit;
      end if;
    end function;
  begin
    assert completed(0) = expect
      report command.name & " completed " & boolean'image(completed(0)) severity failure;
  end procedure;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_nestalias_pkg.all;

entity t_vhdl_nestalias_vvc is
  generic (IDX : natural);
end entity;

architecture a of t_vhdl_nestalias_vvc is
  signal busy : boolean := IDX = 2;
  signal last_idx : natural := 5;
begin
  process
    variable c : t_cmd := ((IDX * 3, 0), "vvc" & character'val(48 + IDX));
  begin
    wait for IDX * 1 ns;
    -- 5 >= 3 for the first instance, 5 >= 6 not for the second
    interp(c, busy, last_idx, IDX = 1);
    c.gen_integer_array(0) := -1;
    interp(c, busy, last_idx, IDX /= 2);
    wait;
  end process;
end architecture;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_nestalias_pkg.all;

entity t_vhdl_nestalias_uvc is
end entity;

architecture a of t_vhdl_nestalias_uvc is
  signal busy2 : boolean := true;
  signal last2 : natural := 1;
begin
  process
    variable c : t_cmd := ((3, 0), "uvc3");
  begin
    wait for 3 ns;
    interp(c, busy2, last2, false);
    c.gen_integer_array(0) := 1;
    interp(c, busy2, last2, true);
    wait;
  end process;
end architecture;

entity t_vhdl_nestalias is
end entity;

architecture a of t_vhdl_nestalias is
begin
  u1 : entity work.t_vhdl_nestalias_vvc generic map (IDX => 1);
  u2 : entity work.t_vhdl_nestalias_vvc generic map (IDX => 2);
  u3 : entity work.t_vhdl_nestalias_uvc;

  process
  begin
    wait for 5 ns;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
