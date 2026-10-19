-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- Two methods of a protected type, each declaring a constant of the same
-- name whose value folds (UVVM's rand_pkg, two "C_MIN_RANGE"): two objects,
-- not one declared twice.

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

package t_vhdl_protconstname_pkg is
  type prot is protected
    impure function lo_u return unsigned;
    impure function lo_s return signed;
  end protected;
end package;

package body t_vhdl_protconstname_pkg is
  type prot is protected body
    variable st : natural := 1;
    impure function lo_u return unsigned is
      constant C_MIN_RANGE : unsigned(7 downto 0) := (others => '0');
      variable v : unsigned(7 downto 0);
    begin
      v := C_MIN_RANGE + st;
      return v;
    end function;
    impure function lo_s return signed is
      constant C_MIN_RANGE : signed(3 downto 0) := (others => '1');
      variable v : signed(3 downto 0);
    begin
      v := C_MIN_RANGE - st;
      return v;
    end function;
  end protected body;
end package body;

library ieee;
use ieee.numeric_std.all;
use work.t_vhdl_protconstname_pkg.all;

entity t_vhdl_protconstname is
end entity;

architecture behav of t_vhdl_protconstname is
  shared variable sv : prot;
begin
  process begin
    assert to_integer(sv.lo_u) = 1 severity failure;
    assert to_integer(sv.lo_s) = -2 severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
