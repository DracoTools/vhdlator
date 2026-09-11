-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- "r.elem(a to b)" of a record object slices its element, even where a
-- function of the element's name is visible too, as UVVM's func_cov_pkg has
-- a variable's bin_vector beside its function bin_vector.

package t_vhdl_recslice_pkg is
  type int_vector is array (natural range <>) of integer;
  type rec is record
    bin_vector : int_vector(0 to 7);
    num_bins : natural;
  end record;
  function bin_vector(n : integer) return int_vector;
end package;

package body t_vhdl_recslice_pkg is
  function bin_vector(n : integer) return int_vector is
    variable v : int_vector(0 to 0);
  begin
    v(0) := n;
    return v;
  end function;
end package body;

use work.t_vhdl_recslice_pkg.all;

entity t_vhdl_recslice is
end entity;

architecture a of t_vhdl_recslice is
begin
  process
    variable r : rec;
    variable src : rec;
  begin
    r.bin_vector(0 to 3) := src.bin_vector(4 to 7);
    r.bin_vector(4 to 4) := bin_vector(5);
    wait;
  end process;
end architecture;
